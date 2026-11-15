#include <Arduino.h>
#include <anniversary_33_inferencing.h>
#include <driver/i2s.h>

// ==========================================
// Pin Configurations
// ==========================================
// INMP441 I2S Pins
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const int PIN_I2S_SD  = 4;   // Data Out from Mic
static const int PIN_I2S_WS  = 5;   // Word Select / LRCK
static const int PIN_I2S_SCK = 6;   // Clock / BCLK

// Output LED Pins (External 3 LEDs + Onboard Status LED)
static const int PIN_LED_1   = 11;  // LED 1
static const int PIN_LED_2   = 12;  // LED 2
static const int PIN_LED_3   = 13;  // LED 3
static const int PIN_LED_48  = 48;  // Onboard Red LED

// Wake Word Threshold & Hold Duration
static const float WAKE_WORD_THRESHOLD = 0.70f;
static const unsigned long LED_ON_DURATION_MS = 5000; // Hold for 5 seconds

// ==========================================
// Audio Inference Buffers & State
// ==========================================
typedef struct {
  signed short *buffers[2];
  unsigned char buf_select;
  unsigned char buf_ready;
  unsigned int  buf_count;
  unsigned int  n_samples;
} inference_t;

static inference_t inference;
static const uint32_t SAMPLE_BUFFER_SIZE = 512;
static int32_t raw_i2s_buffer[SAMPLE_BUFFER_SIZE];
static signed short sample_buffer[SAMPLE_BUFFER_SIZE];
static volatile bool record_status = true;

static unsigned long led_turn_off_time = 0;
static bool leds_active = false;

// Custom printf for Edge Impulse
void ei_printf(const char *format, ...) {
  static char print_buf[1024] = { 0 };
  va_list args;
  va_start(args, format);
  int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
  va_end(args);
  if (r > 0) {
    Serial.write(print_buf);
  }
}



// Set all LEDs
static void set_leds(bool state) {
  uint8_t val = state ? HIGH : LOW;
  digitalWrite(PIN_LED_1, val);
  digitalWrite(PIN_LED_2, val);
  digitalWrite(PIN_LED_3, val);
  digitalWrite(PIN_LED_48, val);
  leds_active = state;
}

// Romantic lighting sequence when wake word triggers
static void trigger_anniversary_lighting() {
  Serial.println("\n********************************************************");
  Serial.println("  >>> [SURPRISE!] ครบรอบ 33 ปี (33_years Detected!) <<< ");
  Serial.println("  >>> Turning ON LEDs! Happy 33rd Anniversary! ❤️      ");
  Serial.println("********************************************************\n");

  // Step 1: Sequential light-up
  digitalWrite(PIN_LED_1, HIGH);
  delay(150);
  digitalWrite(PIN_LED_2, HIGH);
  delay(150);
  digitalWrite(PIN_LED_3, HIGH);
  digitalWrite(PIN_LED_48, HIGH);

  leds_active = true;
  led_turn_off_time = millis() + LED_ON_DURATION_MS;
}

// Push PCM samples into double-buffered inference queue
static void audio_inference_callback(uint32_t n_samples) {
  for (uint32_t i = 0; i < n_samples; i++) {
    inference.buffers[inference.buf_select][inference.buf_count++] = sample_buffer[i];

    if (inference.buf_count >= inference.n_samples) {
      inference.buf_select ^= 1;
      inference.buf_count = 0;
      inference.buf_ready = 1;
    }
  }
}

// Background FreeRTOS task capturing I2S samples from INMP441
static void capture_samples_task(void *arg) {
  while (record_status) {
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_PORT, (void *)raw_i2s_buffer, sizeof(raw_i2s_buffer), &bytes_read, portMAX_DELAY);

    if (err == ESP_OK && bytes_read > 0) {
      uint32_t samples_read = bytes_read / sizeof(int32_t);

      // Convert 24-bit INMP441 sample (in 32-bit slot) to 16-bit PCM for Edge Impulse
      for (uint32_t x = 0; x < samples_read; x++) {
        sample_buffer[x] = (int16_t)(raw_i2s_buffer[x] >> 14);
      }

      audio_inference_callback(samples_read);
    }
  }
  vTaskDelete(NULL);
}

// Initialize I2S Master RX for INMP441
static bool init_i2s() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = EI_CLASSIFIER_FREQUENCY, // 16000 Hz
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = SAMPLE_BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_I2S_SCK,
    .ws_io_num = PIN_I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PIN_I2S_SD
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[ERR] i2s_driver_install failed: %d\n", err);
    return false;
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("[ERR] i2s_set_pin failed: %d\n", err);
    return false;
  }

  i2s_zero_dma_buffer(I2S_PORT);
  return true;
}

// Allocate inference buffers and start audio capture task
static bool microphone_inference_start(uint32_t n_samples) {
  inference.buffers[0] = (signed short *)ps_malloc(n_samples * sizeof(signed short));
  if (inference.buffers[0] == NULL) {
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));
  }
  if (inference.buffers[0] == NULL) return false;

  inference.buffers[1] = (signed short *)ps_malloc(n_samples * sizeof(signed short));
  if (inference.buffers[1] == NULL) {
    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));
  }
  if (inference.buffers[1] == NULL) {
    free(inference.buffers[0]);
    return false;
  }

  inference.buf_select = 0;
  inference.buf_count = 0;
  inference.n_samples = n_samples;
  inference.buf_ready = 0;

  if (!init_i2s()) {
    return false;
  }

  record_status = true;
  xTaskCreatePinnedToCore(capture_samples_task, "CaptureSamples", 4096, NULL, 10, NULL, 1);
  return true;
}

static volatile uint32_t overrun_count = 0;

// Wait until a slice of audio is ready
static bool microphone_inference_record(void) {
  if (inference.buf_ready == 1) {
    overrun_count++;  // inference slower than audio slice
    inference.buf_ready = 0;
  }

  while (inference.buf_ready == 0) {
    delay(1);
  }

  inference.buf_ready = 0;
  return true;
}

// Audio feed callback required by Edge Impulse
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
  numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
  return 0;
}

void setup() {
  Serial.begin(115200);

  // Initialize LED Pins
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);
  pinMode(PIN_LED_3, OUTPUT);
  pinMode(PIN_LED_48, OUTPUT);
  set_leds(false);

  // Startup LED Welcome Animation (Self-test)
  for (int i = 0; i < 2; i++) {
    set_leds(true);
    delay(150);
    set_leds(false);
    delay(150);
  }

  // Don't block on USB CDC so the board runs standalone (power bank)
  delay(1500);

  Serial.println("\n========================================================");
  Serial.println("  ESP32-S3 SuperMini Wake Word: '33rd Anniversary' ❤️   ");
  Serial.println("========================================================");
  Serial.printf("Chip Model       : %s\n", ESP.getChipModel());
  Serial.printf("PSRAM Available  : %.2f MB\n", ESP.getPsramSize() / (1024.0 * 1024.0));
  Serial.printf("Free PSRAM       : %d bytes\n", ESP.getFreePsram());
  Serial.println("--------------------------------------------------------");
  Serial.println("Microphone (INMP441): SD=GPIO4, WS=GPIO5, SCK=GPIO6");
  Serial.println("LEDs                : GPIO 11, 12, 13 + Onboard LED 48");
  Serial.println("Trigger Word        : 'สามสิบสามปี' (Label: 33_years)");
  Serial.println("Classes             : 33_years, noise, unknown");
  Serial.println("========================================================\n");

  run_classifier_init();

  if (!microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE)) {
    Serial.println("[ERR] Could not start microphone inference pipeline!");
    return;
  }

  Serial.println("[INFO] Continuous Listening started! Speak 'สามสิบสามปี'...\n");
}

void loop() {
  // Turn off LEDs when duration expires
  if (leds_active && millis() >= led_turn_off_time) {
    set_leds(false);
    Serial.println("[LED] LEDs turned OFF. Waiting for next trigger...");
  }

  bool ok = microphone_inference_record();
  if (!ok) return;

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
  signal.get_data = &microphone_audio_signal_get_data;
  ei_impulse_result_t result = {0};

  EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, false);
  if (r != EI_IMPULSE_OK) {
    Serial.printf("[ERR] Classifier error (%d)\n", r);
    return;
  }

  // ---- DEBUG: audio level of this slice ----
  int16_t *slice = inference.buffers[inference.buf_select ^ 1];
  int32_t peak = 0;
  for (uint32_t i = 0; i < EI_CLASSIFIER_SLICE_SIZE; i++) {
    int32_t a = abs(slice[i]);
    if (a > peak) peak = a;
  }

  // Check classifier predictions
  static uint32_t dbg_count = 0;
  float s33 = 0, snoise = 0, sunk = 0;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    const char *label = result.classification[ix].label;
    float score = result.classification[ix].value;
    if (strcmp(label, "33_years") == 0) s33 = score;
    else if (strcmp(label, "noise") == 0) snoise = score;
    else sunk = score;

    if (strcmp(label, "33_years") == 0 && score >= WAKE_WORD_THRESHOLD) {
      Serial.printf(">>> [DETECTED] '33_years' Confidence: %.2f (Threshold: %.2f)\n", score, WAKE_WORD_THRESHOLD);
      trigger_anniversary_lighting();
    }
  }

  // Print every slice so we can see what the model thinks
  Serial.printf("[%5lu] 33=%.2f noise=%.2f unk=%.2f | peak=%5ld | dsp=%dms nn=%dms | overrun=%lu\n",
                (unsigned long)dbg_count++, s33, snoise, sunk, (long)peak,
                result.timing.dsp, result.timing.classification, (unsigned long)overrun_count);
}
