# 33th-annivasary: ESP32-S3 SuperMini + INMP441 Wake Word Sensor

โปรเจคสร้างเซนเซอร์ตรวจจับเสียง (Wake Word / Voice Sensor) ด้วยบอร์ดไมโครคอนโทรลเลอร์ขนาดเล็ก ESP32-S3 SuperMini และโมดูลไมค์ I2S INMP441

## Features

- สื่อสารสัญญาณเสียงผ่าน I2S ดิจิทัล (ความถี่แซมปลิง 16 kHz Mono)
- รองรับการเปิดใช้งานและตรวจสอบฮาร์ดแวร์ PSRAM (QSPI 2MB) บน ESP32-S3
- พิมพ์ข้อมูลสถานะ Memory Heap, PSRAM และระดับความดังของเสียง (RMS / Peak) ออกทาง Serial Monitor
- เตรียมความพร้อมเพื่อขยายไปสู่โมเดล TinyML Wake Word Detection

## Hardware

- **MCU:** ESP32-S3 SuperMini (Flash 4MB, PSRAM QSPI 2MB, Dual-core 240MHz)
- **Microphone:** INMP441 I2S Omnidirectional Digital Microphone Module

## Pin Map

### Pinout ASCII Diagram

```text
               ESP32-S3 SuperMini (Top View)
                     +---------------+
                     |    [USB-C]    |
       (Red LED) 48 -| 1          18 |- BAT
                 TX -| 2          17 |- 5V
                 RX -| 3          16 |- GND
             GPIO 1 -| 4          15 |- 3V3
             GPIO 2 -| 5          14 |- GPIO 13
             GPIO 3 -| 6          13 |- GPIO 12
        [SD] GPIO 4 -| 7          12 |- GPIO 11
        [WS] GPIO 5 -| 8          11 |- GPIO 10 (WS2812 RGB: 48)
       [SCK] GPIO 6 -| 9          10 |- GPIO 9
             GPIO 7 -| [Pad]    [Pad]|- GPIO 8
                     +---------------+

                         INMP441
                       +---------+
                       | (MIC O) |
                 SCK --| 1     6 |-- VDD (3.3V)
                  WS --| 2     5 |-- GND
                 L/R --| 3     4 |-- SD (Data)
                       +---------+
```

### ตารางการเชื่อมต่อขา (Wiring Connection)

| ขา อุปกรณ์ | ขา ESP32-S3 SuperMini | หน้าที่ |
|---|---|---|
| **INMP441 VDD** | **3.3V** | จ่ายไฟเลี้ยง 3.3V (ห้ามต่อ 5V) |
| **INMP441 GND** | **GND** | กราวด์ร่วม |
| **INMP441 SD** | **GPIO 4** | I2S Serial Data |
| **INMP441 WS** | **GPIO 5** | I2S Word Select (LRCK) |
| **INMP441 SCK** | **GPIO 6** | I2S Serial Clock (BCLK) |
| **INMP441 L/R** | **GND** | เลือกลำโพง/ช่องซ้าย (Left Channel) |
| **LED 1 (+R)** | **GPIO 11** | ไฟติดเมื่อตรวจพบคำว่า "MARVIN" |
| **LED 2 (+R)** | **GPIO 12** | ไฟติดเมื่อตรวจพบคำว่า "MARVIN" |
| **LED 3 (+R)** | **GPIO 13** | ไฟติดเมื่อตรวจพบคำว่า "MARVIN" |
| **LED ขาลบ (-)** | **GND** | ต่อผ่านตัวต้านทานลง GND ร่วม |

## Wiring Notes

- **ไฟเลี้ยง:** ห้ามต่อขา VDD ของ INMP441 เข้ากับขา 5V เด็ดขาด เนื่องจากไมค์รองรับแรงดันสูงสุด 3.3V
- **สายสัญญาณ I2S:** ควรเดินสายให้สั้นเพื่อลดสัญญาณรบกวนความถี่สูงบนสัญญาณ BCLK/LRCK
- **ขา L/R:** ต้องต่อลง GND เพื่อให้ไมค์ส่งข้อมูลในแชนเนลซ้าย

## Firmware Status

- **Status:** Prototype / Testing Phase (Phase 1)
- ฟังก์ชันปัจจุบัน: ตรวจสอบชิป, วัดขนาด Flash & PSRAM, เริ่มต้น I2S Driver และอ่านข้อมูลเสียงคำนวณ RMS ออก Serial Monitor

## Getting Started

### 1. คอมไพล์โปรเจค

```bash
pio run
```

### 2. อัปโหลดลงบอร์ด

```bash
pio run -t upload
```

### 3. เปิด Serial Monitor ดูผลลัพธ์

```bash
pio device monitor -b 115200
```

## Safety

- ตรวจสอบให้แน่ใจว่าสายไฟ 3.3V และ GND ไม่ลัดวงจร
- ระวังความร้อนบนตัวบอร์ด SuperMini ระหว่างใช้งานต่อเนื่อง

## Roadmap

- [x] ตรวจสอบฮาร์ดแวร์ ESP32-S3 SuperMini & PSRAM (2MB QSPI)
- [x] ทดสอบการอ่านค่าเสียง I2S จาก INMP441
- [x] ติดตั้งและคอมไพล์โมเดล TinyML Wake Word ("MARVIN")
- [x] ควบคุมไฟ LED 3 หลอด (GPIO 11, 12, 13) ตอบสนองเมื่อตรวจพบคำสั่งเสียง
- [x] พัฒนาโมเดลคำสั่งภาษาไทย "สามสิบสามปี" (`33_years`) พร้อมคลาส `unknown` ป้องกันการลั่น

## License

MIT License
