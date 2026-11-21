# 33th-anniversary (Interactive Anniversary Gift)

## Overview

โปรเจกต์กล่องของขวัญฉลองครบรอบ 33 ปี (33rd Anniversary Interactive Gift) ทำงานบน **ESP32-S3 SuperMini** (Flash 4MB, PSRAM QSPI 2MB) ผสานระบบ AI ตรวจจับคำสั่งเสียงภาษาไทยคำว่า **"สามสิบสามปี"** (Edge Impulse EON Continuous Inference), เครื่องเล่นเพลง **DFPlayer Mini** พร้อมลำโพง 4Ω 3W, มอเตอร์หมุนแท่นโชว์โมเดล 28BYJ-48 (ULN2003 Driver), เอาต์พุตไฟ 100mA, และระบบจูนค่าผ่านหน้าเว็บด้วย **Web Bluetooth (Web BLE)** โฮสต์บน Vercel

---

## Production Links

- **Live Web BLE Controller (Vercel):** [https://web-liard-nine-63.vercel.app/](https://web-liard-nine-63.vercel.app/)
- **Obsidian Vault Runbook:** [ESP32_S3_EdgeImpulse_WakeWord_Runbook.md](file:///G:/My%20Drive/AI%20Agent%20Vault/MCU/ESP32/ESP32_S3_EdgeImpulse_WakeWord_Runbook.md)

---

## 4-Part System Architecture

1. **Power-On Self-Test (POST - 10 วินาทีตอนเสียบปลั๊ก):**
   - เสียบปลั๊กปุ๊บ ➡️ เล่นเพลงที่ 1 + ไฟติด + มอเตอร์หมุน นาน **10 วินาที**
   - ครบ 10 วินาที ➡️ หยุดเพลง + มอเตอร์หมุนกลับจุด 0° (Home) + ไฟดับ เพื่อเช็คว่าทุกส่วนทำงานปกติ
2. **Wake Word Detection (Completed & Verified):**
   - ตรวจจับคำว่า **"สามสิบสามปี"** (`33_years`) ทำหน้าที่เป็นสวิตช์ Toggle ON/OFF
   - ป้องกันการ Detect เบิ้ลด้วย Cooldown 2.5 วินาที
   - ค่าความไว (Threshold): ค่าเริ่มต้น `0.80` (ปรับแต่งได้ผ่าน Web BLE `0.50` - `0.99`)
3. **DFPlayer Mini Music Player (Integrated):**
   - เล่นเพลงเมื่อสั่งเปิดระบบ และสุ่มเพลงไปเรื่อยๆ จนครบ 7 เพลงแล้ววนสุ่มชุดใหม่ (Shuffle)
   - ถ้าพูดสั่งงานขณะเพลงกำลังเล่นอยู่ จะสุ่มข้ามไปเพลงถัดไป (Next Song)
   - **Auto-Stop เมื่อเพลงจบ:** ตรวจจับผ่านขา `BUSY` (GPIO 3) เมื่อเพลงจบจะสั่งมอเตอร์หมุนกลับ 0° และไฟดับอัตโนมัติ
   - ปรับระดับเสียง (Volume 0 - 30) ผ่าน Web BLE และบันทึกลง Flash
4. **28BYJ-48 Stepper Motor + ULN2003 (Completed & Verified):**
   - ควบคุมแบบ 8-step half-stepping (4,096 steps/rev) หมุนนุ่มนวล
   - รันเป็น Background Task บน **Core 0** (FreeRTOS) ไม่สะดุดกับ I2S Audio บน Core 1
   - **ระบบหมุนกลับตำแหน่งเดิม (Return Home to 0°):** เมื่อสั่งปิด มอเตอร์จะคำนวณทางที่สั้นที่สุดแล้วหมุนกลับมาที่จุด 0° ก่อนหยุดจ่ายกระแสเข้าขดลวด (`0 mA`) เพื่อไม่ให้มอเตอร์ร้อน
   - รองรับโหมด "ไม่หมุน" (Speed = 0) จากหน้าเว็บ
5. **Light Output (Completed & Verified for Toggle):**
   - ขา GPIO 13 + Onboard Red LED 48
   - สว่างค้างขณะทำงาน และจะดับลง**หลังจากที่มอเตอร์หมุนกลับถึงจุด 0° เรียบร้อยแล้วเท่านั้น**
6. **Web Bluetooth Interface (Completed & Verified):**
   - หน้าเว็บ [https://web-liard-nine-63.vercel.app/](https://web-liard-nine-63.vercel.app/) ทำงานผ่าน Google Chrome / Edge บน Android/PC/Mac
   - **5-Minute Config Window on Boot:** หลังเสียบปลั๊ก บอร์ดจะปล่อย BLE `33rd-Anniversary` นาน 5 นาที หากไม่มีใครเชื่อมต่อจะปิดตัวเองเพื่อประหยัดไฟและตัดคลื่นรบกวนไมค์ 100%
   - ดีไซน์ระดับ 1-5 ตัวโต สบายตา สำหรับผู้สูงอายุ

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
| DFPlayer TX | GPIO 1 | DFPlayer Mini (RX) | Output | **ผ่านตัวต้านทาน 1kΩ** |
| DFPlayer RX | GPIO 2 | DFPlayer Mini (TX) | Input | ต่อตรง |
| DFPlayer BUSY| GPIO 3 | DFPlayer Mini (Pin 16)| Input | LOW = กำลังเล่น, HIGH = เพลงจบ |
| Light Out | GPIO 13 | External Light Drive | Output | ไดรฟ์ไฟ 100mA (ผ่านทรานซิสเตอร์/MOSFET) |
| Onboard LED | GPIO 48 | Onboard Red LED | Output | สว่างพร้อมไฟภายนอก |
| Common GND | GND | Shared GND | Power | กราวด์ร่วม ESP32, DFPlayer, ULN2003, Mic |

---

## Build & Upload Instructions

```bash
# คอมไพล์และอัปโหลดลงบอร์ด
pio run -t upload

# มอนิเตอร์ดู Log
pio device monitor -b 115200
```
