#include <Arduino.h>
#include <anniversary_33_inferencing.h>
#include <driver/i2s.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <DFRobotDFPlayerMini.h>

// ==========================================
// Pin Configurations
// ==========================================
// INMP441 I2S Pins
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const int PIN_I2S_SD  = 4;   // Data Out from Mic
static const int PIN_I2S_WS  = 5;   // Word Select / LRCK
static const int PIN_I2S_SCK = 6;   // Clock / BCLK

// Stepper Motor ULN2003 Driver Pins
static const int PIN_STEPPER_IN1 = 9;   // IN1 -> GPIO 9
static const int PIN_STEPPER_IN2 = 10;  // IN2 -> GPIO 10
static const int PIN_STEPPER_IN3 = 11;  // IN3 -> GPIO 11
static const int PIN_STEPPER_IN4 = 12;  // IN4 -> GPIO 12

// Light Output Pins (External light on GPIO 13 + Onboard Red LED on GPIO 48)
static const int PIN_LIGHT_EXT   = 13;
static const int PIN_LIGHT_48    = 48;

// Physical Push Button Pin (GPIO 8 to GND, internal PULLUP)
static const int PIN_BUTTON      = 8;

// DFPlayer Mini Pins (UART1)
static const int PIN_DF_TX       = 1;   // ESP32 TX (GPIO 1) -> DFPlayer RX (via 1k resistor)
static const int PIN_DF_RX       = 2;   // ESP32 RX (GPIO 2) <- DFPlayer TX
static const int PIN_DF_BUSY     = 3;   // DFPlayer Pin 16 (BUSY) -> GPIO 3 (LOW = Playing, HIGH = Done)

// ==========================================
// DFPlayer Mini & Playlist Management
// ==========================================
HardwareSerial dfSerial(1);
DFRobotDFPlayerMini myDFPlayer;
static bool dfplayer_ready = false;
static unsigned long track_start_time = 0;

static const int TOTAL_TRACKS = 7;
static int playlist[TOTAL_TRACKS] = {1, 2, 3, 4, 5, 6, 7};
static int playlist_index = 0;
static int current_seq_track = 1;                      // In RAM: always starts at track 1 on reboot!
static volatile uint8_t play_mode = 0;                 // 0 = Sequential (1..7), 1 = Random/Shuffle

static void shuffle_playlist() {
  for (int i = TOTAL_TRACKS - 1; i > 0; i--) {
    int j = random(i + 1);
    int temp = playlist[i];
    playlist[i] = playlist[j];
    playlist[j] = temp;
  }
  playlist_index = 0;
}

static int get_next_track() {
  if (play_mode == 0) {
    // Sequential: 1 -> 2 -> ... -> 7 -> 1
    int track = current_seq_track;
    current_seq_track++;
    if (current_seq_track > TOTAL_TRACKS) {
      current_seq_track = 1;
    }
    return track;
  } else {
    // Random / Shuffle
    if (playlist_index >= TOTAL_TRACKS) {
      shuffle_playlist();
    }
    return playlist[playlist_index++];
  }
}

// ==========================================
// BLE GATT Service & Characteristics UUIDs
// ==========================================
#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CHAR_SPEED_UUID     "19b10001-e8f2-537e-4f6c-d104768a1214"
#define CHAR_THRESHOLD_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"
#define CHAR_VOLUME_UUID    "19b10003-e8f2-537e-4f6c-d104768a1214"
#define CHAR_STATE_UUID     "19b10004-e8f2-537e-4f6c-d104768a1214"
#define CHAR_PLAYMODE_UUID  "19b10005-e8f2-537e-4f6c-d104768a1214"

// Dynamic Parameters (Persistent via Preferences)
static volatile uint32_t step_delay_ms = 3;            // Speed while spinning: default 3ms (0 = disabled)
static volatile float wake_word_threshold = 0.80f;     // Default 0.80
static volatile uint8_t df_volume = 20;                // Default 20 (0 - 30)

static Preferences preferences;

// BLE Server & Window (5 minutes on boot)
static NimBLEServer* pServer = nullptr;
static NimBLECharacteristic* pCharSpeed = nullptr;
static NimBLECharacteristic* pCharThreshold = nullptr;
static NimBLECharacteristic* pCharVolume = nullptr;
static NimBLECharacteristic* pCharState = nullptr;
static NimBLECharacteristic* pCharPlayMode = nullptr;

static unsigned long ble_boot_time = 0;
static const unsigned long BLE_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes
static bool ble_active = false;
static bool ble_client_connected = false;

// ==========================================
// Stepper Motor State & Half-Stepping (8-step)
// ==========================================
enum StepperState {
  STEPPER_IDLE,
  STEPPER_RUNNING,
  STEPPER_RETURNING
};

static volatile StepperState stepper_state = STEPPER_IDLE;
static volatile int32_t current_step_pos = 0;          // Position 0 .. 4095
static const int32_t STEPS_PER_REV = 4096;             // 28BYJ-48 8-step half-stepping
static const uint32_t RETURN_STEP_DELAY_MS = 2;        // Speed while returning to 0: 2ms
static volatile bool system_active = false;
static unsigned long wake_word_cooldown_until = 0;

static const uint8_t STEP_SEQ[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

static int8_t step_index = 0;

static void set_stepper_pins(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  digitalWrite(PIN_STEPPER_IN1, a ? HIGH : LOW);
  digitalWrite(PIN_STEPPER_IN2, b ? HIGH : LOW);
  digitalWrite(PIN_STEPPER_IN3, c ? HIGH : LOW);
  digitalWrite(PIN_STEPPER_IN4, d ? HIGH : LOW);
}

static void stepper_stop_coils() {
  set_stepper_pins(0, 0, 0, 0);
}

static void do_one_step(int dir) {
  if (dir > 0) {
    step_index = (step_index + 1) & 7;
  } else {
    step_index = (step_index - 1 + 8) & 7;
  }
  set_stepper_pins(STEP_SEQ[step_index][0],
                   STEP_SEQ[step_index][1],
                   STEP_SEQ[step_index][2],
                   STEP_SEQ[step_index][3]);
}

// Light control
static void set_light(bool state) {
  uint8_t val = state ? HIGH : LOW;
  digitalWrite(PIN_LIGHT_EXT, val);
  digitalWrite(PIN_LIGHT_48, val);
}

// Notify Web BLE client of state change
static void notify_ble_state(bool running) {
  if (pCharState && ble_client_connected) {
    uint8_t val = running ? 1 : 0;
    pCharState->setValue(&val, 1);
    pCharState->notify();
  }
}

// Forward declaration of toggle function
static void toggle_system(bool enable);

// Stepper Motor Background Task running on Core 0
static void stepper_motor_task(void *pvParameters) {
  while (true) {
    if (stepper_state == STEPPER_RUNNING) {
      if (step_delay_ms > 0) {
        do_one_step(1); // Spin forward
        current_step_pos = (current_step_pos + 1) % STEPS_PER_REV;
        vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
      } else {
        // Motor disabled from Web/BLE: keep coils off
        stepper_stop_coils();
        vTaskDelay(pdMS_TO_TICKS(50));
      }
    } else if (stepper_state == STEPPER_RETURNING) {
      if (current_step_pos != 0) {
        // Return to 0 via shortest direction
        int dir = (current_step_pos <= (STEPS_PER_REV / 2)) ? -1 : 1;
        do_one_step(dir);
        current_step_pos = (current_step_pos + dir + STEPS_PER_REV) % STEPS_PER_REV;
        vTaskDelay(pdMS_TO_TICKS(RETURN_STEP_DELAY_MS));
      } else {
        // Reached home position 0!
        stepper_stop_coils();
        stepper_state = STEPPER_IDLE;
        set_light(false);
        notify_ble_state(false);
        Serial.println("\n[SYSTEM] Stepper returned to 0 (Home). Coils de-energized. Light turned OFF.\n");
      }
    } else {
      stepper_stop_coils();
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

static void toggle_system(bool enable) {
  if (enable) {
    system_active = true;
    set_light(true);
    if (step_delay_ms > 0) {
      stepper_state = STEPPER_RUNNING;
    }

    int track = get_next_track();
    if (dfplayer_ready) {
      myDFPlayer.play(track);
      track_start_time = millis();
    }

    notify_ble_state(true);
    Serial.println("\n********************************************************");
    Serial.printf("  >>> [TOGGLE ON] Playing Track #%d (%s) | Light ON & Motor Active! ❤️\n",
                  track, play_mode == 0 ? "Sequential" : "Shuffle");
    Serial.println("********************************************************\n");
  } else {
    system_active = false;
    if (dfplayer_ready) {
      myDFPlayer.stop();
    }
    stepper_state = STEPPER_RETURNING;
    Serial.println("\n********************************************************");
    Serial.println("  >>> [TOGGLE OFF] Music STOPPED | Stepper RETURNING to 0... Light OFF at 0!");
    Serial.println("********************************************************\n");
  }
}

// ==========================================
// BLE Server & Characteristic Callbacks
// ==========================================
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) override {
    ble_client_connected = true;
    Serial.println("\n[BLE] >>> Client Connected! <<<\n");
  }

  void onDisconnect(NimBLEServer* pServer) override {
    ble_client_connected = false;
    Serial.println("\n[BLE] Client Disconnected.");
    if (ble_active) {
      NimBLEDevice::startAdvertising();
    }
  }
};

class SpeedCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
      uint8_t val = (uint8_t)value[0];
      if (val <= 10) {
        step_delay_ms = val;
        preferences.begin("anniv33", false);
        preferences.putUInt("speed", step_delay_ms);
        preferences.end();
        if (step_delay_ms == 0) {
          Serial.println("[BLE] Motor Disabled (Speed = 0, Won't spin)");
        } else {
          Serial.printf("[BLE] Updated Speed Delay: %u ms (Saved to Flash)\n", step_delay_ms);
        }
      }
    }
  }
};

class ThresholdCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
      uint8_t val = (uint8_t)value[0];
      if (val >= 50 && val <= 99) {
        wake_word_threshold = (float)val / 100.0f;
        preferences.begin("anniv33", false);
        preferences.putFloat("threshold", wake_word_threshold);
        preferences.end();
        Serial.printf("[BLE] Updated Threshold: %.2f (%u%%) (Saved to Flash)\n", wake_word_threshold, val);
      }
    }
  }
};

class VolumeCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
      uint8_t val = (uint8_t)value[0];
      if (val <= 30) {
        df_volume = val;
        preferences.begin("anniv33", false);
        preferences.putUChar("volume", df_volume);
        preferences.end();
        if (dfplayer_ready) {
          myDFPlayer.volume(df_volume);
        }
        Serial.printf("[BLE] Updated Volume: %u / 30 (Saved to Flash)\n", df_volume);
      }
    }
  }
};

class StateCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
      uint8_t val = (uint8_t)value[0];
      bool target_run = (val == 1);
      if (target_run != system_active) {
        toggle_system(target_run);
      }
    }
  }
};

class PlayModeCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
      uint8_t val = (uint8_t)value[0];
      play_mode = (val == 1) ? 1 : 0;
      preferences.begin("anniv33", false);
      preferences.putUChar("play_mode", play_mode);
      preferences.end();
      Serial.printf("[BLE] Updated Play Mode: %s (Saved to Flash)\n", play_mode == 0 ? "Sequential (1-7)" : "Random (Shuffle)");
    }
  }
};

static void init_ble() {
  Serial.println("[BLE] Initializing NimBLE Server: '33rd-Anniversary'...");
  NimBLEDevice::init("33rd-Anniversary");
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  // Speed Characteristic
  pCharSpeed = pService->createCharacteristic(
    CHAR_SPEED_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
  );
  uint8_t sVal = (uint8_t)step_delay_ms;
  pCharSpeed->setValue(&sVal, 1);
  pCharSpeed->setCallbacks(new SpeedCallbacks());

  // Threshold Characteristic
  pCharThreshold = pService->createCharacteristic(
    CHAR_THRESHOLD_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
  );
  uint8_t tVal = (uint8_t)(wake_word_threshold * 100);
  pCharThreshold->setValue(&tVal, 1);
  pCharThreshold->setCallbacks(new ThresholdCallbacks());

  // Volume Characteristic
  pCharVolume = pService->createCharacteristic(
    CHAR_VOLUME_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
  );
  uint8_t vVal = (uint8_t)df_volume;
  pCharVolume->setValue(&vVal, 1);
  pCharVolume->setCallbacks(new VolumeCallbacks());

  // State Characteristic
  pCharState = pService->createCharacteristic(
    CHAR_STATE_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
  );
  uint8_t stVal = system_active ? 1 : 0;
  pCharState->setValue(&stVal, 1);
  pCharState->setCallbacks(new StateCallbacks());

  // Play Mode Characteristic
  pCharPlayMode = pService->createCharacteristic(
    CHAR_PLAYMODE_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
  );
  uint8_t pmVal = play_mode;
  pCharPlayMode->setValue(&pmVal, 1);
  pCharPlayMode->setCallbacks(new PlayModeCallbacks());

  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  ble_active = true;
  ble_boot_time = millis();
  Serial.println("[BLE] Advertising started! Available for 5 minutes after boot.\n");
}

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
    overrun_count = overrun_count + 1;
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

  // Initialize Stepper Pins
  pinMode(PIN_STEPPER_IN1, OUTPUT);
  pinMode(PIN_STEPPER_IN2, OUTPUT);
  pinMode(PIN_STEPPER_IN3, OUTPUT);
  pinMode(PIN_STEPPER_IN4, OUTPUT);
  stepper_stop_coils();

  // Initialize Light Pins
  pinMode(PIN_LIGHT_EXT, OUTPUT);
  pinMode(PIN_LIGHT_48, OUTPUT);
  set_light(false);

  // Initialize Physical Push Button Pin (GPIO 8, active LOW)
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Startup Welcome Light Blink (200ms) - Quiet & Polite
  set_light(true);
  delay(200);
  set_light(false);

  // Don't block on USB CDC so the board runs standalone (power bank)
  delay(1200);

  // Seed random generator
  randomSeed(micros());
  shuffle_playlist();
  current_seq_track = 1; // Always starts at track 1 on boot

  // Load saved preferences from Flash
  preferences.begin("anniv33", true); // Read-only
  step_delay_ms = preferences.getUInt("speed", 3);
  wake_word_threshold = preferences.getFloat("threshold", 0.80f);
  df_volume = preferences.getUChar("volume", 20);
  play_mode = preferences.getUChar("play_mode", 0); // 0 = Sequential, 1 = Random
  preferences.end();

  // Initialize DFPlayer Mini (UART1 on GPIO 2 RX, GPIO 1 TX)
  dfSerial.begin(9600, SERIAL_8N1, PIN_DF_RX, PIN_DF_TX);
  pinMode(PIN_DF_BUSY, INPUT_PULLUP);

  Serial.println("\n[DFPlayer] Initializing...");
  // Use begin(stream, isACK = false, doReset = false) for instant, reliable init
  if (myDFPlayer.begin(dfSerial, false, false)) {
    dfplayer_ready = true;
    delay(100);
    myDFPlayer.volume(df_volume);
    Serial.printf("[DFPlayer] Ready! Volume: %u / 30\n", df_volume);
  } else {
    dfplayer_ready = true;
    delay(100);
    myDFPlayer.volume(df_volume);
    Serial.println("[DFPlayer] Fallback ready!");
  }

  Serial.println("\n========================================================");
  Serial.println("  ESP32-S3 SuperMini Wake Word: '33rd Anniversary' ❤️   ");
  Serial.println("========================================================");
  Serial.printf("Chip Model       : %s\n", ESP.getChipModel());
  Serial.printf("PSRAM Available  : %.2f MB\n", ESP.getPsramSize() / (1024.0 * 1024.0));
  Serial.printf("Free PSRAM       : %d bytes\n", ESP.getFreePsram());
  Serial.println("--------------------------------------------------------");
  Serial.printf("Motor Speed Delay: %u ms (Loaded from Flash)\n", step_delay_ms);
  Serial.printf("Wake Threshold   : %.2f (Loaded from Flash)\n", wake_word_threshold);
  Serial.printf("DFPlayer Volume  : %u / 30 (Loaded from Flash)\n", df_volume);
  Serial.printf("Music Play Mode  : %s (Loaded from Flash)\n", play_mode == 0 ? "Sequential (1-7)" : "Random (Shuffle)");
  Serial.println("--------------------------------------------------------");
  Serial.println("Microphone (INMP441): SD=GPIO4, WS=GPIO5, SCK=GPIO6");
  Serial.println("Stepper (ULN2003)   : IN1=GPIO9, IN2=GPIO10, IN3=GPIO11, IN4=GPIO12");
  Serial.println("Push Button (GND)   : GPIO 8 (Short: Trigger/Next | Long 1.5s: Stop)");
  Serial.println("DFPlayer Mini (UART): TX=GPIO1 (to RX via 1k), RX=GPIO2 (from TX), BUSY=GPIO3");
  Serial.println("Light Output        : GPIO 13 + Onboard Red LED 48");
  Serial.println("Trigger Word        : 'สามสิบสามปี' (Label: 33_years)");
  Serial.println("Web BLE Service     : Active for 5 minutes on boot");
  Serial.println("========================================================\n");

  // Launch Stepper Task on Core 0 (smooth, independent rotation)
  xTaskCreatePinnedToCore(stepper_motor_task, "StepperTask", 2048, NULL, 5, NULL, 0);

  // Initialize BLE Server (Runs on Core 0 background)
  init_ble();

  run_classifier_init();

  if (!microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE)) {
    Serial.println("[ERR] Could not start microphone inference pipeline!");
    return;
  }

  Serial.println("[INFO] Standby ready! Speak 'สามสิบสามปี' or press Button to start...\n");
}

void loop() {
  // Check 5-minute timeout for BLE Advertising (Auto-off if no client connected)
  if (ble_active && !ble_client_connected) {
    if (millis() - ble_boot_time > BLE_TIMEOUT_MS) {
      Serial.println("\n[BLE] 5-minute config window expired without connection.");
      Serial.println("[BLE] Shutting down BLE radio to save power & prevent RF interference.\n");
      NimBLEDevice::deinit(true);
      ble_active = false;
    }
  }

  // ----------------------------------------------------
  // Physical Push Button on GPIO 8 (Active LOW)
  // Short Click (< 1.5s) : Toggle ON or Next Track
  // Long Press  (>= 1.5s): Force STOP & Return to 0°
  // ----------------------------------------------------
  static bool last_btn_state = HIGH;
  static unsigned long btn_press_time = 0;
  static bool long_press_handled = false;

  bool current_btn_state = digitalRead(PIN_BUTTON);
  if (last_btn_state == HIGH && current_btn_state == LOW) {
    // Just pressed
    btn_press_time = millis();
    long_press_handled = false;
  } else if (current_btn_state == LOW) {
    // Being held down
    if (!long_press_handled && (millis() - btn_press_time >= 1500)) {
      long_press_handled = true;
      Serial.println("\n>>> [BUTTON] Long press (1.5s) -> Force STOP!");
      if (system_active) {
        toggle_system(false);
      }
    }
  } else if (last_btn_state == LOW && current_btn_state == HIGH) {
    // Released
    if (!long_press_handled && (millis() - btn_press_time >= 50)) {
      Serial.println("\n>>> [BUTTON] Short press -> Trigger / Next Track!");
      if (!system_active) {
        toggle_system(true);
      } else {
        int track = get_next_track();
        if (dfplayer_ready) {
          myDFPlayer.play(track);
          track_start_time = millis();
        }
        Serial.printf("\n>>> [NEXT TRACK] Button clicked -> Playing Track #%d (%s)\n",
                      track, play_mode == 0 ? "Sequential" : "Shuffle");
      }
    }
  }
  last_btn_state = current_btn_state;

  // Auto-stop when song finishes playing
  if (system_active && dfplayer_ready) {
    // BUSY pin: LOW while playing, HIGH when idle/finished
    if (digitalRead(PIN_DF_BUSY) == HIGH && (millis() - track_start_time > 2000)) {
      Serial.println("\n[SYSTEM] Song finished! Auto-stopping and returning to 0°...");
      toggle_system(false);
    }
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

  // Audio level of this slice
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

    if (strcmp(label, "33_years") == 0 && score >= wake_word_threshold) {
      unsigned long now = millis();
      if (now >= wake_word_cooldown_until) {
        wake_word_cooldown_until = now + 2500; // 2.5s cooldown

        if (!system_active) {
          // System was OFF -> Turn ON
          toggle_system(true);
        } else {
          // Already playing -> Skip to next track!
          int track = get_next_track();
          if (dfplayer_ready) {
            myDFPlayer.play(track);
            track_start_time = millis();
          }
          Serial.printf("\n>>> [NEXT TRACK] '33_years' Detected -> Skipping to Track #%d (%s)\n",
                        track, play_mode == 0 ? "Sequential" : "Shuffle");
        }
      }
    }
  }

  // Print every slice for serial debugging
  Serial.printf("[%5lu] 33=%.2f noise=%.2f unk=%.2f | peak=%5ld | dsp=%dms nn=%dms | overrun=%lu | Sys=%s (MotorPos=%ld) | BLE=%s\n",
                (unsigned long)dbg_count++, s33, snoise, sunk, (long)peak,
                result.timing.dsp, result.timing.classification, (unsigned long)overrun_count,
                system_active ? "ON" : (stepper_state == STEPPER_RETURNING ? "RETURNING" : "OFF"),
                (long)current_step_pos,
                ble_active ? (ble_client_connected ? "Connected" : "Adv 5m") : "OFF");
}
