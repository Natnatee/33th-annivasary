#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

// I2S Pin Definitions for INMP441
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const int PIN_I2S_SD = 4;   // Data Out from Mic
static const int PIN_I2S_WS = 5;   // Word Select / LRCK
static const int PIN_I2S_SCK = 6;  // Clock / BCLK

// Audio Buffer Configuration
static const int SAMPLE_RATE = 16000;
static const int BUFFER_SIZE = 256;
static int32_t raw_samples[BUFFER_SIZE];

// Initialize I2S driver for INMP441
static bool init_i2s_microphone() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_SIZE,
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
    Serial.printf("[ERR] Failed to install I2S driver: %d\n", err);
    return false;
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("[ERR] Failed to set I2S pins: %d\n", err);
    return false;
  }

  i2s_zero_dma_buffer(I2S_PORT);
  return true;
}

// Print hardware & memory information
static void print_hardware_info() {
  Serial.println("\n========================================");
  Serial.println("   ESP32-S3 SuperMini Hardware Check    ");
  Serial.println("========================================");

  Serial.printf("Chip Model       : %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision    : %d\n", ESP.getChipRevision());
  Serial.printf("CPU Cores        : %d\n", ESP.getChipCores());
  Serial.printf("CPU Frequency    : %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Chip Size  : %.2f MB\n", ESP.getFlashChipSize() / (1024.0 * 1024.0));
  Serial.printf("Free Internal Heap: %d bytes\n", ESP.getFreeHeap());

  Serial.println("\n--- PSRAM Status ---");
  if (psramFound()) {
    Serial.println("PSRAM Detection  : SUCCESS");
    Serial.printf("Total PSRAM Size : %.2f MB (%d bytes)\n",
                  ESP.getPsramSize() / (1024.0 * 1024.0), ESP.getPsramSize());
    Serial.printf("Free PSRAM       : %d bytes\n", ESP.getFreePsram());
  } else {
    Serial.println("PSRAM Detection  : NOT FOUND / DISABLED");
    Serial.println("Note: Check platformio.ini memory_type (qio_qspi)");
  }
  Serial.println("========================================\n");
}

void setup() {
  Serial.begin(115200);

  // Wait for USB CDC connection on SuperMini
  while (!Serial) {
    delay(10);
  }
  delay(1500);

  print_hardware_info();

  Serial.println("[INFO] Initializing INMP441 I2S microphone...");
  if (init_i2s_microphone()) {
    Serial.println("[INFO] I2S Driver initialized successfully!");
    Serial.println("[INFO] Streaming audio volume levels. Speak or tap the mic to test...\n");
  } else {
    Serial.println("[ERR] Microphone initialization failed!");
  }
}

void loop() {
  size_t bytes_read = 0;
  esp_err_t result = i2s_read(I2S_PORT, &raw_samples, sizeof(raw_samples), &bytes_read, portMAX_DELAY);

  if (result == ESP_OK && bytes_read > 0) {
    int samples_count = bytes_read / sizeof(int32_t);
    double sum_squares = 0.0;
    int32_t peak_value = 0;

    for (int i = 0; i < samples_count; i++) {
      // INMP441 outputs 24-bit data MSB aligned in 32-bit container
      int32_t sample = raw_samples[i] >> 14; // Normalize to roughly 16-bit signed range
      int32_t abs_sample = abs(sample);
      if (abs_sample > peak_value) {
        peak_value = abs_sample;
      }
      sum_squares += (double)sample * sample;
    }

    double rms = sqrt(sum_squares / samples_count);

    // Rate-limit display to ~10 times per second
    static unsigned long last_print_time = 0;
    unsigned long now = millis();
    if (now - last_print_time >= 100) {
      last_print_time = now;

      // Draw a simple ASCII VU meter bar
      int bar_length = (int)(rms / 250.0);
      if (bar_length > 30) bar_length = 30;

      char vu_bar[32];
      for (int i = 0; i < bar_length; i++) {
        vu_bar[i] = '#';
      }
      vu_bar[bar_length] = '\0';

      Serial.printf("RMS: %6.1f | Peak: %5d | [%-30s]\n", rms, peak_value, vu_bar);
    }
  }
}
