# ESP32 OLED Saat Paneli / ESP32 OLED Clock Panel

ESP32 + SSD1306 OLED üzerinde çalışan çok fonksiyonlu saat paneli. NTP ile gerçek saat, Open-Meteo API'den canlı hava durumu, pomodoro zamanlayıcı, kronometre ve WiFi tarama içerir. Tek BOOT tuşu ile yönetilir.

A multifunctional clock panel running on ESP32 + SSD1306 OLED. Features a real-time NTP clock, live weather from the Open-Meteo API, a Pomodoro timer, a stopwatch and a WiFi scanner — all controlled with a single BOOT button.

<img width="3072" height="4096" alt="IMG_20260919_195826" src="https://github.com/user-attachments/assets/96afec34-30db-480c-9978-0e77a50310f3" />

<img width="3072" height="4096" alt="IMG_20260919_195826" src="https://github.com/user-attachments/assets/c7611b3d-06c4-4ff0-8d2e-6a4321d6d3b7" />


<details open>
<summary>Türkçe</summary>

## Özellikler

- **Saat ekranı** — NTP ile gerçek zaman (Türkiye saati, GMT+3), gün/ay, WiFi sinyal çubuğu, hava durumu satırı ve saniye göstergesi
- **Hava Durumu** — Open-Meteo API'sinden sıcaklık, nem ve durum (Acik/Bulutlu/Yagmur vb.), 10 dakikada bir otomatik yenilenir, anahtar gerektirmez
- **Pomodoro** — 25 dk çalışma / 5 dk mola, ilerleme çubuğu, oturum sayacı, bitince LED uyarısı
- **Kronometre** — başlat/durdur/sıfırla
- **WiFi Tarama** — çevredeki ağları SSID + RSSI ile listele
- **NTP Yenile** — saati elle yeniden senkronlar
- **WiFi Kur** — telefondan veya seri porttan WiFi kurulumu

## Donanım Gereksinimi

- ESP32 dev board (BOOT tuşlu klasik devkit)
- SSD1306 OLED 128x64 (I2C, adres 0x3C)
- Bağlantı: OLED `SDA`/`SCL` → ESP32 I2C pinleri (genelde GPIO21/GPIO22), `VCC`→3.3V, `GND`→GND
- BOOT tuşu GPIO0 (dahili), dahili LED GPIO2 (pomodoro uyarısı)

## Kullanılan Kütüphaneler

Arduino IDE Kütüphane Yöneticisi'nden kur (ESP32 çekirdeği kurulu olmalı):

- `Adafruit GFX`
- `Adafruit SSD1306`

`WiFi`, `DNSServer`, `WebServer`, `Preferences`, `WiFiClientSecure` ESP32 çekirdeğinde hazır gelir.

## Kurulum ve Yükleme

1. `oled_panel.ino` dosyasını Arduino IDE ile aç (klasör adı `.ino` ile aynı olmalı)
2. Kart olarak ESP32 Dev Module seç, doğru COM portu ayarla
3. İstersen üstteki ayarları değiştir:
   - `CITY_NAME`, `CITY_LAT`, `CITY_LON` — hava durumu şehrin
   - `GMT_OFFSET_SEC` — saat dilimi (Türkiye 3*3600)
   - `WORK_SEC` / `BREAK_SEC` — pomodoro süreleri
4. Yükle (Upload)

## İlk Açılış: WiFi Kurulumu

ESP32 sadece 2.4 GHz ağlara bağlanır; 5 GHz ağlar görünmez. Kayıtlı ağ yoksa veya 15 saniyede bağlanamazsa kart kendini **ESP32_Saat** erişim noktası olarak açar.

İki kurulum yöntemi vardır:

### 1. Telefon ile
- Telefonundan `ESP32_Saat` ağına bağlan
- Tarayıcıda `192.168.4.1` adresini aç
- Ağ adını elle yaz (veya taramadan seç), şifreni gir → *Kaydet ve Baglan*
- Kart yeniden başlar ve bağlanır

### 2. Seri Port ile
- ESP32'yi USB ile PC'ye bağla
- Arduino IDE → Seri Monitor aç, 115200 baud seç
- Şu satırı yazıp enterla:
  ```
  AğAdın|Şifren
  ```
  Örnek: `MyWifi|12345678`
- Ayarları silmek için: `SIFIRLA`

Ayarlar kartın kalıcı hafızasında (Preferences/NVS) saklanır; her açılışta hatırlanır.

## Kullanım

| Ekran | Kısa bas (BOOT) | Basılı tut (700ms) |
|---|---|---|
| Saat | Menüyü açar | - |
| Menü | Aşağı kaydır | Seçili öğeye gir |
| Kronometre | Başlat / Durdur | Koşarken: geri; dururken: sıfırla |
| Pomodoro | Başlat / Durdur | Geri dön |
| Hava Durumu | Yenile | Geri dön |
| WiFi Tarama | Kaydır | Geri dön |
| NTP Yenile | Geri dön | Geri dön |
| WiFi Kur | Geri dön | Geri dön (kapat, ağa bağlan) |

> Not: Saat ekranında saati düzeltme yoktur; `NTP Yenile` menüsüyle senkron yaparsın.

## Proje Yapısı / Repo İçeriği

```
esp32-saat-paneli/
├── oled_panel.ino   # tek dosya, tüm kod / single file, all the code
└── README.md
```

## Sorun Giderme

| Sorun | Çözüm |
|---|---|
| Saat çekmiyor / WiFi yok | Bağlı değilken otomatik kurulum AP'si açılmalı; açılmıyorsa `WiFi Kur` menüsüne gir |
| Taramada ağ görünmüyor | 2.4 GHz ağ olduğundan emin ol; telefon hotspot ile dene |
| Hava durumu `...` gösteriyor | İnternet bağlantısını kontrol et; şehir koordinatlarını doğrula |
| Ekranda `--:--` | NTP beklemede; WiFi bağlandıktan sonra birkaç saniye bekleyin |
| Menüden çıkılamıyor | 700ms'den uzun bas; LED yanıp sönene kadar tut |

> Not: Bu sistem sadece İstanbul sınırları içerisinde denenmiştir. Farklı ülkelerde ve şehirlerde sorunlar olabilir.

</details>

<details>
<summary>English / İngilizce</summary>

## Features

- **Clock screen** — real-time NTP time (GMT+3, Turkey), day/date, WiFi signal bars, weather line and seconds display
- **Weather** — temperature, humidity and condition (Clear/Cloudy/Rain etc.) from the Open-Meteo API, refreshed automatically every 10 minutes, no API key required
- **Pomodoro** — 25 min work / 5 min break, progress bar, session counter, LED blink when finished
- **Stopwatch** — start/pause/reset
- **WiFi Scan** — list nearby networks with SSID + RSSI
- **Resync NTP** — manually resync the clock
- **WiFi Setup** — configure WiFi from your phone or over serial

## Hardware Requirements

- ESP32 dev board (classic devkit with BOOT button)
- SSD1306 OLED 128x64 (I2C, address 0x3C)
- Wiring: OLED `SDA`/`SCL` → ESP32 I2C pins (usually GPIO21/GPIO22), `VCC`→3.3V, `GND`→GND
- BOOT button on GPIO0 (built-in), onboard LED on GPIO2 (pomodoro alert)

## Required Libraries

Install from Arduino IDE Library Manager (ESP32 core must be installed):

- `Adafruit GFX`
- `Adafruit SSD1306`

`WiFi`, `DNSServer`, `WebServer`, `Preferences`, `WiFiClientSecure` come built-in with the ESP32 core.

## Setup and Upload

1. Open `oled_panel.ino` in the Arduino IDE (the folder name must match the `.ino` file)
2. Select **ESP32 Dev Module** as the board and choose the correct COM port
3. Optionally edit the settings at the top of the file:
   - `CITY_NAME`, `CITY_LAT`, `CITY_LON` — your weather city
   - `GMT_OFFSET_SEC` — timezone (Turkey: 3*3600)
   - `WORK_SEC` / `BREAK_SEC` — Pomodoro durations
4. Upload

## First Boot: WiFi Setup

The ESP32 only connects to 2.4 GHz networks; 5 GHz networks are not visible. If no network is saved, or it can't connect within 15 seconds, the board turns itself into an access point named **ESP32_Saat**.

There are two setup methods:

### 1. Via phone
- Connect your phone to the `ESP32_Saat` network
- Open `192.168.4.1` in a browser
- Type the network name manually (or pick it from the scan), enter your password → *Save & Connect*
- The board restarts and connects

### 2. Via serial port
- Connect the ESP32 to your PC over USB
- Open Arduino IDE → Serial Monitor, select 115200 baud
- Type the following line and press enter:
  ```
  NetworkName|Password
  ```
  Example: `MyWifi|12345678`
- To clear the settings: type `SIFIRLA`

Settings are stored in the board's persistent memory (Preferences/NVS) and remembered on every boot.

## Usage

| Screen | Short press (BOOT) | Long press (700ms) |
|---|---|---|
| Clock | Opens menu | - |
| Menu | Scroll down | Enter selected item |
| Stopwatch | Start / Pause | Running: back; paused: reset |
| Pomodoro | Start / Pause | Back |
| Weather | Refresh | Back |
| WiFi Scan | Scroll | Back |
| Resync NTP | Back | Back |
| WiFi Setup | Back | Back (close, reconnect) |

> Note: There is no manual time correction on the clock screen; use the `Resync NTP` item to sync.

## Repository Contents

```
esp32-saat-paneli/
├── oled_panel.ino   # single file, all the code
└── README.md
```

## Troubleshooting

| Problem | Solution |
|---|---|
| Clock not syncing / no WiFi | The setup AP should auto-start when disconnected; if not, open the `WiFi Setup` menu |
| No networks in scan | Make sure it's a 2.4 GHz network; try a phone hotspot |
| Weather shows `...` | Check the internet connection; verify the city coordinates |
| Screen shows `--:--` | NTP still waiting; give it a few seconds after WiFi connects |
| Can't leave the menu | Press and hold longer than 700ms; keep holding until the LED blinks |

> Note: This system has only been tested within Istanbul. Other countries and cities may cause issues.

</details>
