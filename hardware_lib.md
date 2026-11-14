# Hardware & Library Specs

## Hardware

| Part | Model | Interface | Notes |
|---|---|---|---|
| MCU Board | ESP32-S3 SuperMini | USB-C (Native CDC) | Dual-core Xtensa LX7 @ 240MHz, Flash 4MB, PSRAM QSPI 2MB |
| Digital Mic | INMP441 | I2S Digital | Omnidirectional MEMS microphone, 24-bit I2S output, VDD 3.3V |

## Libraries

| Purpose | Library | Status | Notes |
|---|---|---|---|
| I2S Communication | ESP32 Built-in I2S Driver (`driver/i2s.h`) | Active | ไดรเวอร์มาตรฐานของ ESP-IDF/Arduino สำหรับการรับข้อมูลเสียงแบบ DMA |
| Wake Word / TinyML | TBD (Edge Impulse / ESP-SR / microWakeWord) | Candidate | อยู่ระหว่างการพิจารณาเลือกใช้ใน Phase 2 |

## Datasheet / References

- **ESP32-S3 Series Datasheet:** Xtensa dual-core 32-bit LX7 microprocessor with vector instructions
- **INMP441 Datasheet:** Bottom-port I2S output digital microphone, SNR 61 dBA, Sensitivity -26 dBFS
- **Vault Hardware Reference:** `G:\My Drive\AI Agent Vault\MCU\ESP32\ESP32_S3_SuperMini_Hardware.md`

## Hardware Issues

- **Flash Size & Partition Table (4MB):** บอร์ด SuperMini มี Flash ขนาด 4MB (0x400000) หากใช้ค่าเริ่มต้นของ `esp32-s3-devkitc-1` ซึ่งเป็น 8MB บูตโหลดเดอร์จะฟ้อง `partition 3 invalid ... exceeds flash chip size 0x400000` และวนลูปบูตไม่หยุด ต้องใช้ custom partition table ขนาด 4MB (`partitions_4mb.csv`) และต้อง `pio run -t erase` เพื่อล้างตารางเดิมออก
- บอร์ด ESP32-S3 SuperMini Flash 4MB ส่วนใหญ่ในตลาดใช้ PSRAM ชนิด Quad SPI (QSPI) 2MB การตั้งค่า `board_build.arduino.memory_type` หรือ Arduino IDE ต้องระวังไม่ให้เป็น Octal (OPI) เพราะอาจทำให้บอร์ด Panic ได้
- จำเป็นต้องเปิด `ARDUINO_USB_CDC_ON_BOOT=1` เสมอเพื่อให้ Serial ผ่านพอร์ต USB Type-C ของบอร์ดทำงานได้ถูกต้อง

## Test Notes

- **I2S Test Configuration:**
  - Sample Rate: 16,000 Hz
  - Bit per Sample: 32 bits (ข้อมูลเสียงจริงอยู่ที่ MSB 24 bits)
  - Channel: Mono (Left channel)
  - Buffer Count: 4
  - Buffer Length: 256 samples
