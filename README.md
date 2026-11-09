# Deteksi Kebakaran Dini v2 — ESP32 + DHT22 + Gas Sensor + LDR + MQTT + Telegram (Wokwi)

## Cara pakai
1. Wokwi: https://wokwi.com/projects/new/esp32, salin `sketch.ino`, `diagram.json`, `libraries.txt`.
2. Ganti `TOPIC_BASE` di `sketch.ino` (dan topik di flow) supaya unik di broker publik.
3. Node-RED: import `nodered_flow.json` (butuh `node-red-dashboard` dan `node-red-contrib-telegrambot`). Dashboard: http://127.0.0.1:1880/ui
4. Token Telegram dan chat ID jangan masuk Wokwi publik atau screenshot.

## Tabel pin
| Komponen | Pin komponen | GPIO / pin ESP32 |
|---|---|---|
| DHT22 | VCC / SDA / GND | 3V3 / 15 / GND |
| Gas sensor (MQ-2) | VCC / AOUT / GND | 3V3 / 35 (ADC1) / GND |
| LDR (photoresistor) | VCC / AO / GND | 3V3 / 34 (ADC1) / GND |
| Tombol RESET | 1.l / 2.l | 4 (INPUT_PULLUP) / GND |
| LED merah / hijau | A | 26 / 27 (C ke GND) |
| Buzzer | 2 (+) / 1 (-) | 25 / GND |

## Logika
Tiga kondisi: suhu > 50 (off <= 45), asap > 60% (off <= 40%), api (cahaya LDR) > 70% (off <= 50%).
- normal: tidak ada yang aktif
- waspada: 1 sensor aktif, LED merah berkedip
- bahaya: >= 2 sensor aktif selama 3 sampel berturut-turut, latching, buzzer, alert MQTT
- reset: tombol RESET atau command `reset`; alert maksimal 1x per 60 detik

## Kalibrasi (wajib, sekali)
Serial Monitor mencetak `gasRaw` dan `ldrRaw`. Catat nilainya saat kondisi gelap/bersih dan terang/gas maksimum,
lalu isi `LDR_RAW_DARK`, `LDR_RAW_BRIGHT`, `GAS_RAW_CLEAN`, `GAS_RAW_MAX` di `sketch.ino`.

## Tabel pengujian
| No | Skenario | Input | Respons diharapkan | Hasil aktual | Sesuai? |
|---|---|---|---|---|---|
| 1 | Normal | suhu 25, gas rendah, LDR gelap | normal, LED hijau | | |
| 2 | Hanya suhu | suhu 60 | waspada | | |
| 3 | Hanya asap | gas tinggi | waspada | | |
| 4 | Hanya api | LDR terang | waspada | | |
| 5 | Suhu + asap | keduanya tinggi >3 dtk | bahaya, buzzer, alert | | |
| 6 | Asap + api | keduanya tinggi >3 dtk | bahaya | | |
| 7 | Lonjakan singkat | 1 sensor <3 dtk | tidak bahaya (alarm palsu) | | |
| 8 | Reset | tombol / dashboard | alarm mati | | |
| 9 | MQTT putus lalu pulih | putus jaringan | alarm lokal jalan, reconnect | | |
