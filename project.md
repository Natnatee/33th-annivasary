# 33th-anniversary (Interactive Anniversary Gift)

## Overview

โปรเจกต์กล่องของขวัญฉลองครบรอบ 33 ปี (33rd Anniversary Interactive Gift) ทำงานบน **ESP32-S3 SuperMini** (Flash 4MB, PSRAM QSPI 2MB) ผสานระบบ AI ตรวจจับคำสั่งเสียงภาษาไทยคำว่า **"สามสิบสามปี"** (Edge Impulse EON Continuous Inference), มอเตอร์หมุนแท่นโชว์โมเดล 28BYJ-48 (ULN2003 Driver), เอาต์พุตไฟ 100mA, และระบบจูนค่าผ่านหน้าเว็บด้วย **Web Bluetooth (Web BLE)** โฮสต์บน Vercel

---

## Production Links

- **Live Web BLE Controller (Vercel):** [https://web-liard-nine-63.vercel.app/](https://web-liard-nine-63.vercel.app/)
- **Obsidian Vault Runbook:** [ESP32_S3_EdgeImpulse_WakeWord_Runbook.md](file:///G:/My%20Drive/AI%20Agent%20Vault/MCU/ESP32/ESP32_S3_EdgeImpulse_WakeWord_Runbook.md)

---

## 4-Part System Architecture

1. **Wake Word Detection (Completed & Verified):**
   - ตรวจจับคำว่า **"สามสิบสามปี"** (`33_years`) ทำหน้าที่เป็นสวิตช์ Toggle ON/OFF
   - ป้องกันการ Detect เบิ้ลด้วย Cooldown 2.5 วินาที
   - ค่าความไว (Threshold): ค่าเริ่มต้น `0.80` (ปรับแต่งได้ผ่าน Web BLE `0.50` - `0.99`)
2. **DFPlayer Mini (Music Player - Next Step):**
   - เล่นเพลงเมื่อสั่งเปิดระบบ และหยุดเพลงเมื่อสั่งปิด
   - ปรับระดับเสียง (Volume 0 - 30) ผ่าน Web BLE และบันทึกลง Flash
3. **28BYJ-48 Stepper Motor + ULN2003 (Completed & Verified):**
   - ควบคุมแบบ 8-step half-stepping (4,096 steps/rev) หมุนนุ่มนวล
   - รันเป็น Background Task บน **Core 0** (FreeRTOS) ไม่สะดุดกับ I2S Audio บน Core 1
   - **ระบบหมุนกลับตำแหน่งเดิม (Return Home to 0°):** เมื่อสั่งปิด มอเตอร์จะคำนวณทางที่สั้นที่สุดแล้วหมุนกลับมาที่จุด 0° ก่อนหยุดจ่ายกระแสเข้าขดลวด (`0 mA`) เพื่อไม่ให้มอเตอร์ร้อน
4. **Light Output (Completed & Verified for Toggle):**
   - ขา GPIO 13 + Onboard Red LED 48
   - สว่างค้างขณะทำงาน และจะดับลง**หลังจากที่มอเตอร์หมุนกลับถึงจุด 0° เรียบร้อยแล้วเท่านั้น**
5. **Web Bluetooth Interface (Completed & Verified):**
   - หน้าเว็บ [https://web-liard-nine-63.vercel.app/](https://web-liard-nine-63.vercel.app/) ทำงานผ่าน Google Chrome / Edge บน Android/PC/Mac
   - **5-Minute Config Window on Boot:** หลังเสียบปลั๊ก บอร์ดจะปล่อย BLE `33rd-Anniversary` นาน 5 นาที หากไม่มีใครเชื่อมต่อจะปิดตัวเองเพื่อประหยัดไฟและตัดคลื่นรบกวนไมค์ 100%
   - ปรับสปีดมอเตอร์, ปรับความไวไมค์, ปรับความดังเพลง และปุ่ม Manual Trigger กดสั่งงานแทนเสียงพูด
   - ค่าทั้งหมดเซฟลง Flash อัตโนมัติด้วย `Preferences.h` (`anniv33`)

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
| Light Out | GPIO 13 | External Light Drive | Output | ไดรฟ์ไฟ 100mA (ผ่านทรานซิสเตอร์/MOSFET) |
| Onboard LED | GPIO 48 | Onboard Red LED | Output | สว่างพร้อมไฟภายนอก |
| Common GND | GND | Shared GND | Power | กราวด์ร่วม ESP32, ULN2003, Mic |

---

## BLE GATT Profile

- **Device Name:** `33rd-Anniversary`
- **Service UUID:** `19b10000-e8f2-537e-4f6c-d104768a1214`
- **Speed UUID:** `19b10001-e8f2-537e-4f6c-d104768a1214` (delay ms: 1 - 10)
- **Threshold UUID:** `19b10002-e8f2-537e-4f6c-d104768a1214` (percentage: 50 - 99 -> 0.50 - 0.99)
- **Volume UUID:** `19b10003-e8f2-537e-4f6c-d104768a1214` (volume: 0 - 30)
- **State UUID:** `19b10004-e8f2-537e-4f6c-d104768a1214` (0: OFF, 1: ON + Notify)

---

## Build & Deploy Instructions

### ESP32-S3 Firmware (PlatformIO)
```bash
# คอมไพล์
pio run

# อัปโหลดลงบอร์ด
pio run -t upload

# มอนิเตอร์ดู Log
pio device monitor -b 115200
```

### Web Frontend (Vite + React)
```bash
cd web
pnpm install
pnpm dev      # รันในเครื่องที่ http://localhost:5173
pnpm build    # บิลด์สำหรับ Production (dist/)
```
- บน **Vercel**: ตั้งค่า Root Directory = `web` และปล่อยให้ตรวจจับ Vite Framework Preset อัตโนมัติ (ไม่ต้องสร้าง `vercel.json` ครอบ)
