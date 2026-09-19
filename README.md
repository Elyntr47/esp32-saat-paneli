# esp32-saat-paneli

# ESP32 OLED Saat Paneli

ESP32 + SSD1306 OLED üzerinde çalışan çok fonksiyonlu saat paneli. NTP ile gerçek saat, Open-Meteo API'den canlı hava durumu, pomodoro zamanlayıcı, kronometre ve WiFi tarama içerir. Tek BOOT tuşu ile yönetilir.

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

<img width="3072" height="4096" alt="IMG_20260919_195826" src="https://github.com/user-attachments/assets/c083c858-c71d-4436-84b3-e38dbf81499e" />

<img width="3072" height="4096" alt="IMG_20260919_195838" src="https://github.com/user-attachments/assets/4a575e9e-ab1a-4a37-aa4c-8cc7b87e87d4" />

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

## Proje Yapısı

```
oled_panel.ino   # tek dosya, tüm kod
```

## Sorun Giderme

| Sorun | Çözüm |
|---|---|
| Saat çekmiyor / WiFi yok | Bağlı değilken otomatik kurulum AP'si açılmalı; açılmıyorsa `WiFi Kur` menüsüne gir |
| Taramada ağ görünmüyor | 2.4 GHz ağ olduğundan emin ol; telefon hotspot ile dene |
| Hava durumu `...` gösteriyor | İnternet bağlantısını kontrol et; şehir koordinatlarını doğrula |
| Ekranda `--:--` | NTP beklemede; WiFi bağlandıktan sonra birkaç saniye bekleyin |
| Menüden çıkılamıyor | 700ms'den uzun bas; LED yanıp sönene kadar tut |# esp32-saat-paneli


Not: Bu sistem sadece İstanbul sınırları içerisinde denenmiştir. Farklı ülkelerde ve şehirlerde sorunlar olabilir.
