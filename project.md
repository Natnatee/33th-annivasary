# 33th-annivasary

## Overview

โปรเจคสร้างเซนเซอร์ตรวจจับเสียง (Wake Word / Voice Detection) โดยใช้บอร์ดไมโครคอนโทรลเลอร์ ESP32-S3 SuperMini ร่วมกับเซนเซอร์ไมโครโฟนดิจิทัล INMP441 (I2S MEMS Microphone)

## Goals

1. **Phase 1 (ปัจจุบัน):** ตรวจสอบฮาร์ดแวร์ บอร์ด ESP32-S3 SuperMini, ตรวจสอบการเปิดใช้งาน PSRAM (QSPI 2MB) และทดสอบอ่านค่าสัญญาณเสียงผ่าน I2S แสดงค่า RMS/Peak บน Serial Monitor
2. **Phase 2 (ถัดไป):** พัฒนาระบบ Wake Word Detection (ผ่าน Edge Impulse หรือ ESP-SR / microWakeWord) ตามขนาดหน่วยความจำที่รองรับ

## Hardware

- **MCU Board:** ESP32-S3 SuperMini (Flash 4MB, PSRAM QSPI 2MB, Dual-core 240MHz)
- **Sensor:** INMP441 Omnidirectional MEMS Microphone Module with I2S digital output

## Pin Map

| Function | Pin | Device | Direction | Notes |
|---|---|---|---|---|
| I2S SD (Serial Data) | GPIO 4 | INMP441 (SD) | Input | ข้อมูลเสียงดิจิทัลจากไมค์ |
| I2S WS (Word Select) | GPIO 5 | INMP441 (WS) | Output | LRCK เลือกรอบเฟรมเสียง |
| I2S SCK (Serial Clock) | GPIO 6 | INMP441 (SCK) | Output | BCLK สัญญาณนาฬิกา I2S |
| Channel Select | GND | INMP441 (L/R) | Input (Hardwire) | ต่อลง GND เลือก Left Channel |
| Power VDD | 3.3V | INMP441 (VDD) | Power | ใช้แรงดัน 3.3V เท่านั้น ห้ามต่อ 5V |
| Ground | GND | INMP441 (GND) | Power | กราวด์ร่วม |

## Electrical / Safety Notes

- **ไฟเลี้ยง INMP441:** รองรับแรงดัน 3.3V (1.8V - 3.3V) ห้ามจ่ายไฟ 5V เด็ดขาด อาจทำให้โมดูลเสียหายทันที
- **USB CDC On Boot:** บอร์ด SuperMini อาศัย Native USB ของ ESP32-S3 ต้องเปิด `ARDUINO_USB_CDC_ON_BOOT=1` เสมอเพื่อให้ Serial Monitor ทำงานได้
- **PSRAM Mode:** บอร์ด ESP32-S3 SuperMini Flash 4MB ส่วนใหญ่มาพร้อม PSRAM 2MB ชนิด QSPI (Quad SPI) ห้ามตั้งเป็น Octal (OPI) หากฮาร์ดแวร์ไม่รองรับ

## Firmware Architecture

- **Framework:** Arduino under PlatformIO
- **Audio Driver:** ESP32 I2S Standard Driver (`driver/i2s.h` หรือ `driver/i2s_std.h`)
- **Sampling Rate:** 16,000 Hz, 32-bit (แปลงเป็น 16-bit PCM Mono)
- **Signal Processing:** คำนวณ Root Mean Square (RMS) และ Peak Amplitude เพื่อวัดระดับความดังเสียง

## Libraries

- **Arduino Core for ESP32** (Built-in I2S driver)

## Build & Upload

```bash
# คอมไพล์โปรเจค
pio run

# อัปโหลดเฟิร์มแวร์ลงบอร์ด
pio run -t upload

# เปิด Serial Monitor ตรวจสอบค่า (Baud: 115200)
pio device monitor -b 115200
```

## Current State

- สถานะ: **Phase 1 - Hardware Verification & I2S Audio Testing**
- สภาพแวดล้อม: PlatformIO ถูกสร้างพร้อมคอนฟิกบอร์ดและโครงร่างโค้ดอ่าน I2S

## Open Questions

- *Resolved:* ยืนยัน Pinout และตำแหน่ง Onboard LED เรียบร้อย (Red LED / WS2812 RGB อยู่ที่ GPIO 48)
- เลือกขากำหนดสำหรับต่อ LED 3 หลอดภายนอก (เสนอ GPIO 11, 12, 13 ฝั่งขวา หรือ GPIO 1, 2, 3 ฝั่งซ้าย)
- กำหนดคำ Wake Word ที่ต้องการใช้ (คำไทย หรือคำสำเร็จรูปสากล)

## Issue Log

- **Bootloop from Partition Table Overflow:** บอร์ดเกิด Bootloop ซ้ำ ๆ ที่ Bootloader ด้วย error `partition 3 invalid - offset 0x340000 size 0x330000 exceeds flash chip size 0x400000` เกิดจาก Default partition table ของบอร์ด esp32-s3-devkitc-1 ถูกตั้งไว้สำหรับ 8MB Flash ซึ่งเกินขนาด 4MB Flash ของบอร์ด SuperMini
  - **วิธีแก้:** สร้าง [partitions_4mb.csv](file:///c:/PROJECT/33th-annivasary/partitions_4mb.csv) และเพิ่ม `board_build.partitions = partitions_4mb.csv` ใน `platformio.ini` แล้วรัน `pio run -t erase` ก่อน `pio run -t upload`
