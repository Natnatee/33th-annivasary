# 33th-anniversary (Interactive Anniversary Gift)

## Overview

โปรเจกต์กล่องของขวัญฉลองครบรอบ 33 ปี (33rd Anniversary Interactive Gift) ทำงานบน **ESP32-S3 SuperMini** (Flash 4MB, PSRAM QSPI 2MB) ผสานระบบ AI ตรวจจับคำสั่งเสียงภาษาไทยคำว่า **"สามสิบสามปี"** (Edge Impulse EON Continuous Inference), เครื่องเล่นเพลง **DFPlayer Mini** พร้อมลำโพง 4Ω 3W, มอเตอร์หมุนแท่นโชว์โมเดล 28BYJ-48 (ULN2003 Driver), เอาต์พุตไฟ 100mA, ปุ่มกดจริงบนกล่อง (GPIO 8), และระบบจูนค่าผ่านหน้าเว็บด้วย **Web Bluetooth (Web BLE)** โฮสต์บน Vercel

---

## Production Links

- **Live Web BLE Controller (Vercel):** [https://web-liard-nine-63.vercel.app/](https://web-liard-nine-63.vercel.app/)
- **Obsidian Vault Runbook:** [ESP32_S3_EdgeImpulse_WakeWord_Runbook.md](file:///G:/My%20Drive/AI%20Agent%20Vault/MCU/ESP32/ESP32_S3_EdgeImpulse_WakeWord_Runbook.md)

---

## 4-Part System Architecture

1. **Quiet Polite Startup (เอาระบบ Self-Test ออก):**
   - เสียบปลั๊กปุ๊บ ➡️ ไฟกระพริบเบาๆ ทักทาย 1 ครั้ง (200ms) แล้ว **อยู่นิ่งๆ สแตนด์บายทันที** (ไม่เปิดเพลง/มอเตอร์ไม่หมุนรบกวน)
2. **Dual Control (สั่งด้วยเสียง + ปุ่มกดจริง GPIO 8):**
   - 🗣️ พูดคำว่า **"สามสิบสามปี"** หรือ 🔘 **กดปุ่มสั้น 1 ครั้ง:**
     - ถ้าเครื่องปิดอยู่ (OFF) ➡️ **เปิดทำงาน (เล่นเพลง + มอเตอร์หมุน + ไฟติด)**
     - ถ้าเครื่องกำลังเล่นอยู่ (ON) ➡️ **ข้ามไปเพลงถัดไป (Next Song)**
   - 🔘 **กดปุ่มแช่ 1.5 วินาที (Long Press):** ➡️ **สั่งปิดทันที (เพลงหยุด + มอเตอร์กลับ 0° + ไฟดับ)**
3. **Music Playback Modes (ปรับได้ผ่านหน้าเว็บ):**
   - 🔁 **โหมดปกติ (Sequential - Default):** เล่นเรียง `1 ➡️ 2 ➡️ 3 ➡️ ... ➡️ 7` จนครบแล้ววนใหม่ (เปิดเครื่องใหม่เริ่มที่เพลง 1 เสมอ จำใน RAM)
   - 🔀 **โหมดสุ่ม (Random / Shuffle):** สุ่มเพลงไม่ซ้ำจนครบ 7 เพลงแล้วเริ่มสุ่มชุดใหม่
   - **Auto-Stop เมื่อเพลงจบ:** ตรวจจับผ่านขา `BUSY` (GPIO 3) เมื่อเพลงจบจะสั่งมอเตอร์หมุนกลับ 0° และไฟดับอัตโนมัติ
4. **28BYJ-48 Stepper Motor + ULN2003 (Completed & Verified):**
   - ควบคุมแบบ 8-step half-stepping (4,096 steps/rev) หมุนนุ่มนวล
   - รันเป็น Background Task บน **Core 0** (FreeRTOS)
   - เมื่อสั่งปิด มอเตอร์จะคำนวณทางที่สั้นที่สุดแล้วหมุนกลับมาที่จุด 0° ก่อนหยุดจ่ายกระแสเข้าขดลวด (`0 mA`)
   - รองรับโหมด "ไม่หมุน" (Speed = 0) จากหน้าเว็บ
5. **Light Output (Completed & Verified for Toggle):**
   - ขา GPIO 13 + Onboard Red LED 48
   - สว่างค้างขณะทำงาน และจะดับลง**หลังจากที่มอเตอร์หมุนกลับถึงจุด 0° เรียบร้อยแล้วเท่านั้น**
6. **Web Bluetooth Interface (Elderly-Friendly):**
   - หน้าเว็บ [https://web-liard-nine-63.vercel.app/](https://web-liard-nine-63.vercel.app/)
   - ปรับความเร็วมอเตอร์ (⏸️ ไม่หมุน / ระดับ 1 - 5)
   - โหมดเล่นเพลง (🔁 เล่นเรียง 1-7 vs 🔀 สุ่มเพลง)
   - ปรับความไวไมค์ (ระดับ 1 - 5)
   - ปรับความดังเพลง (🔇 ปิดเสียง / ระดับ 1 - 5)
   - สวิตช์ปุ่มเปิด/ปิดขนาดใหญ่

---

## Pin Map

| Function | Pin | Device | Direction | Notes |
|---|---|---|---|---|
| I2S SD | GPIO 4 | INMP441 (SD) | Input | เสียงดิจิทัลจากไมค์ I2S |
| I2S WS | GPIO 5 | INMP441 (WS) | Output | LRCK Word Select |
| I2S SCK | GPIO 6 | INMP441 (SCK) | Output | BCLK Bit Clock |
| Channel | GND | INMP441 (L/R) | Input | ต่อลง GND เลือก Left Channel |
| Mic Power | 3.3V | INMP441 (VDD) | Power | **ห้ามต่อ 5V เด็ดขาด** |
| Stepper IN1 | GPIO 9 | ULN2003 (IN1) | Output | Half-step coil drive |
| Stepper IN2 | GPIO 10 | ULN2003 (IN2) | Output | Half-step coil drive |
| Stepper IN3 | GPIO 11 | ULN2003 (IN3) | Output | Half-step coil drive |
| Stepper IN4 | GPIO 12 | ULN2003 (IN4) | Output | Half-step coil drive |
| Push Button | GPIO 8 | Push Button to GND | Input | **Pullup ในชิป** (สั้น: เปิด/ข้าม, แช่ 1.5วิ: ปิด) |
| DFPlayer TX | GPIO 1 | DFPlayer Mini (RX) | Output | **ผ่านตัวต้านทาน 1kΩ** |
| DFPlayer RX | GPIO 2 | DFPlayer Mini (TX) | Input | ต่อตรง |
| DFPlayer BUSY| GPIO 3 | DFPlayer Mini (Pin 16)| Input | LOW = กำลังเล่น, HIGH = เพลงจบ |
| Light Out | GPIO 13 | External Light Drive | Output | ไดรฟ์ไฟ 100mA (ผ่านทรานซิสเตอร์/MOSFET) |
| Onboard LED | GPIO 48 | Onboard Red LED | Output | สว่างพร้อมไฟภายนอก |
| Common GND | GND | Shared GND | Power | กราวด์ร่วม ESP32, DFPlayer, ULN2003, Mic, Button |

---

## Build & Upload Instructions

```bash
# คอมไพล์และอัปโหลดลงบอร์ด
pio run -t upload

# มอนิเตอร์ดู Log
pio device monitor -b 115200
```
