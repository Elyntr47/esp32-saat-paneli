#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_ADDR 0x3C
#define BTN_GPIO 0
#define LED_GPIO 2

const long GMT_OFFSET_SEC = 3 * 3600;

// ---------- hava durumu (Open-Meteo, API anahtari gerekmez) ----------
const char* CITY_NAME = "Istanbul";
const float CITY_LAT = 41.02;
const float CITY_LON = 28.96;
const unsigned long WEATHER_INTERVAL_MS = 10UL * 60UL * 1000UL;

Adafruit_SSD1306 display(128, 64, &Wire, -1);

enum Mode { MODE_CLOCK, MODE_MENU, MODE_STOPWATCH, MODE_POMODORO, MODE_WIFISCAN, MODE_NTP, MODE_WEATHER, MODE_SETUP };
Mode mode = MODE_CLOCK;

const char* dayNames[] = { "Paz", "Pzt", "Sal", "Car", "Per", "Cum", "Cmt" };

const IPAddress apIP(192, 168, 4, 1);
const byte DNS_PORT = 53;
const char* apName = "ESP32_Saat";

DNSServer dnsServer;
WebServer server(80);
Preferences prefs;

String cfgSSID = "";
String cfgPass = "";
bool apRunning = false;

// ---------- button ----------
bool btnDown = false;
bool longFired = false;
unsigned long pressStart = 0;
unsigned long lastRender = 0;

void renderAgain() { lastRender = 0; }

// ---------- stopwatch ----------
unsigned long swBase = 0;
unsigned long swStartMs = 0;
bool swRunning = false;

// ---------- pomodoro ----------
enum PomMode { POM_WORK, POM_BREAK };
PomMode pomoMode = POM_WORK;
bool pomoRunning = false;
unsigned long pomoSec = 25 * 60;
unsigned long pomoLast = 0;
int pomoCycle = 0;
const unsigned long WORK_SEC = 25 * 60;
const unsigned long BREAK_SEC = 5 * 60;
const unsigned long POMO_BLINK_MS = 1500;

bool ledOn = false;
unsigned long blinkUntil = 0;
unsigned long blinkLast = 0;

// ---------- weather ----------
String wDesc = "";
String wShort = "";
int wTemp = 0;
int wHum = -1;
bool wOk = false;
unsigned long wLastMs = 0;

// ---------- wifi scan ----------
#define MAX_NETS 20
String wifiNames[MAX_NETS];
int16_t wifiRssi[MAX_NETS];
int wifiCount = 0;
int scanOffset = 0;

// ---------- menu ----------
const char* menuItems[] = { "Kronometre", "Pomodoro", "Hava Durumu", "WiFi Tarama", "NTP Yenile", "WiFi Kur" };
const int menuCount = 6;
int menuSel = 0;

// ---------- front helpers ----------
void fmtMSS(unsigned long sec, char* buf) {
  sprintf(buf, "%02lu:%02lu", sec / 60, sec % 60);
}

void drawFooter(const char* text) {
  display.setTextSize(1);
  display.setCursor(128 - 6 * strlen(text), 57);
  display.print(text);
}

// ---------- clock ----------
void drawRssi(int x, int y) {
  int bars = 0;
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    if (rssi >= -55) bars = 4;
    else if (rssi >= -65) bars = 3;
    else if (rssi >= -75) bars = 2;
    else if (rssi >= -85) bars = 1;
  }
  if (bars == 0 && WiFi.status() != WL_CONNECTED) {
    display.setTextSize(1);
    display.setCursor(x, y);
    display.print("x");
    return;
  }
  for (int i = 0; i < 4; i++) {
    int h = 2 + i * 2;
    int yy = y + 8 - h;
    if (i < bars) display.fillRect(x + i * 4, yy, 3, h, WHITE);
    else display.drawRect(x + i * 4, yy, 3, h, WHITE);
  }
}

const char* wmoShort(int code, bool day) {
  switch (code) {
    case 0: return "Acik";
    case 1: return "Az Bulutlu";
    case 2: return "Parcali";
    case 3: return "Bulutlu";
    case 45:
    case 48: return "Sisli";
    case 51: case 53: case 55: case 56: case 57: return "Cisili";
    case 61: return "Hafif Yagmur";
    case 63: return "Yagmur";
    case 65: return "Siddetli Yagmur";
    case 66: case 67: return "Buzlu Yagmur";
    case 71: return "Hafif Kar";
    case 73: return "Kar";
    case 75: return "Siddetli Kar";
    case 77: return "Kar Taneli";
    case 80: case 81: case 82: return "Saganak";
    case 85: case 86: return "Kar Saganagi";
    case 95: return "Gok Gurultulu";
    case 96: case 99: return "Dolu Firtina";
    default: return "...";
  }
}

bool jsonNum(const String& p, const char* key, float& out) {
  int i = p.indexOf(key);
  if (i < 0) return false;
  int c = p.indexOf(':', i);
  if (c < 0) return false;
  int s = c + 1;
  while (s < (int)p.length() && p[s] == ' ') s++;
  int e = s;
  while (e < (int)p.length() && p[e] != '"' && p[e] != ',' && p[e] != '}') e++;
  if (e <= s) return false;
  out = atof(p.substring(s, e).c_str());
  return true;
}

bool fetchWeather() {
  WiFiClientSecure client;
  client.setInsecure();
  String url = String("/v1/forecast?latitude=") + String(CITY_LAT, 2) +
               "&longitude=" + String(CITY_LON, 2) +
               "&current=temperature_2m,relative_humidity_2m,weather_code,is_day&timezone=auto";

  if (!client.connect("api.open-meteo.com", 443)) return false;

  client.print(String("GET ") + url + " HTTP/1.1\r\nHost: api.open-meteo.com\r\nConnection: close\r\n\r\n");

  String payload = "";
  unsigned long t0 = millis();
  while (millis() - t0 < 6000) {
    while (client.available()) payload += (char)client.read();
    if (!client.connected()) break;
  }
  client.stop();

  int ci = payload.indexOf("\"current\":");
  if (ci < 0) return false;
  String c = payload.substring(ci);

  float f = 0;
  if (!jsonNum(c, "temperature_2m", f)) return false;
  wTemp = (int)(f + 0.5);
  if (jsonNum(c, "relative_humidity_2m", f)) wHum = (int)(f + 0.5);
  float isDay = 1, code = 0;
  jsonNum(c, "is_day", isDay);
  jsonNum(c, "weather_code", code);
  wDesc = wmoShort((int)code, isDay > 0.5);
  if (wDesc.length() > 13) wDesc = wDesc.substring(0, 13);
  wShort = wDesc;
  if (isDay < 0.5) wShort = "Gece " + wDesc;
  wOk = true;
  return true;
}

void weatherCheck() {
  if (apRunning) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (wLastMs != 0 && millis() - wLastMs < WEATHER_INTERVAL_MS) return;
  wLastMs = millis();
  fetchWeather();
  if (mode == MODE_WEATHER) renderAgain();
}

void renderClock() {
  struct tm t;
  display.clearDisplay();
  display.setTextColor(WHITE);

  if (!getLocalTime(&t, 0) || t.tm_year < 120) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    if (cfgSSID.length() == 0) display.print("WiFi ayarlanmadi!");
    else if (WiFi.status() == WL_CONNECTED) display.print("NTP bekleniyor...");
    else display.print("WiFi baglaniyor...");
    drawRssi(112, 0);
    display.setTextSize(4);
    display.setCursor(4, 14);
    display.print("--:--");
    display.setTextSize(1);
    display.setCursor(0, 57);
    display.print(apRunning ? "AP acik" : "Kur=Menu");
    display.display();
    return;
  }

  char top[20];
  snprintf(top, sizeof(top), "%s %02d.%02d", dayNames[t.tm_wday], t.tm_mday, t.tm_mon + 1);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(top);
  drawRssi(112, 0);

  char tbuf[6];
  snprintf(tbuf, sizeof(tbuf), "%02d:%02d", t.tm_hour, t.tm_min);
  display.setTextSize(4);
  display.setCursor(4, 12);
  display.print(tbuf);

  if (wOk) {
    char wline[24];
    String cty = String(CITY_NAME);
    if (cty.length() > 6) cty = cty.substring(0, 6);
    snprintf(wline, sizeof(wline), "%s %dC %s", cty.c_str(), wTemp, wShort.c_str());
    String ws = wline;
    if (ws.length() > 20) ws = ws.substring(0, 20);
    display.setTextSize(1);
    display.setCursor(0, 46);
    display.print(ws);
  } else {
    display.setTextSize(1);
    display.setCursor(0, 46);
    display.print("Hava verisi yok...");
  }

  char fbuf[16];
  snprintf(fbuf, sizeof(fbuf), "%02d sn", t.tm_sec);
  drawFooter("=Menu");
  display.setTextSize(1);
  display.setCursor(0, 57);
  display.print(fbuf);

  display.display();
}

// ---------- menu ----------
void renderMenu() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("MENU");
  display.setCursor(128 - 6 * 6, 0);
  display.print("Tut=Gir");

  for (int i = 0; i < menuCount; i++) {
    int y = 9 + i * 9;
    if (i == menuSel) {
      display.fillRect(0, y, 128, 9, WHITE);
      display.setTextColor(BLACK, WHITE);
      display.setCursor(10, y + 1);
      display.print(menuItems[i]);
    } else {
      display.setTextColor(WHITE);
      display.setCursor(10, y + 1);
      display.print(menuItems[i]);
    }
  }
  display.display();
}

// ---------- stopwatch ----------
unsigned long swElapsed() {
  if (swRunning) return swBase + (millis() - swStartMs);
  return swBase;
}

void renderStopwatch() {
  unsigned long ms = swElapsed();
  unsigned long sec = ms / 1000;
  char buf[8];
  fmtMSS(sec, buf);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("KRONOMETRE");
  display.setCursor(128 - 6 * 7, 0);
  display.print(swRunning ? "CALISIYOR" : "DURAKLADI");

  display.setTextSize(4);
  display.setCursor(4, 14);
  display.print(buf);

  char sub[12];
  snprintf(sub, sizeof(sub), ".%lu", (ms / 100) % 10);
  display.setTextSize(2);
  display.setCursor(4, 48);
  display.print(sub);
  display.setTextSize(1);
  display.setCursor(16, 53);
  display.print("sn");

  drawFooter(swRunning ? "Kisa=Dur  Tut=Geri" : "Kisa=Bas  Tut=Reset");
  display.display();
}

// ---------- pomodoro ----------
void startPomodoroRun() {
  pomoRunning = true;
  pomoLast = millis();
}

void finishPomodoro() {
  pomoRunning = false;
  blinkUntil = millis() + POMO_BLINK_MS;
  blinkLast = millis();
  ledOn = false;
  if (pomoMode == POM_WORK) {
    pomoCycle++;
    pomoMode = POM_BREAK;
    pomoSec = BREAK_SEC;
  } else {
    pomoMode = POM_WORK;
    pomoSec = WORK_SEC;
  }
}

void tickPomodoro() {
  if (!pomoRunning) return;
  if (millis() - pomoLast >= 1000) {
    pomoLast = millis();
    if (pomoSec > 0) pomoSec--;
    if (pomoSec == 0) finishPomodoro();
  }
}

void updateBlink() {
  if (millis() < blinkUntil) {
    if (millis() - blinkLast >= 150) {
      blinkLast = millis();
      ledOn = !ledOn;
      digitalWrite(LED_GPIO, ledOn);
    }
  } else {
    digitalWrite(LED_GPIO, LOW);
  }
}

void renderPomodoro() {
  char tbuf[8];
  fmtMSS(pomoSec, tbuf);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(pomoMode == POM_WORK ? "POMODORO" : "MOLA");
  char cyc[12];
  snprintf(cyc, sizeof(cyc), "Oturum:%d", pomoCycle);
  display.setCursor(128 - 6 * strlen(cyc), 0);
  display.print(cyc);

  display.setTextSize(4);
  display.setCursor(4, 12);
  display.print(tbuf);

  unsigned long total = (pomoMode == POM_WORK) ? WORK_SEC : BREAK_SEC;
  unsigned long done = total - pomoSec;
  display.drawRect(0, 48, 128, 6, WHITE);
  display.fillRect(0, 48, (long)128 * done / total, 6, WHITE);

  display.setTextSize(1);
  display.setCursor(128 - 6 * 11, 57);
  display.print(pomoRunning ? "CALISIYOR" : "DURAKLADI");
  drawFooter("Ksa=Bas/Devam");
  display.display();
}

// ---------- wifi scan (device) ----------
void startScan() {
  WiFi.mode(WIFI_STA);
  if (WiFi.status() != WL_CONNECTED) WiFi.disconnect();
  WiFi.scanDelete();
  wifiCount = WiFi.scanNetworks();
  for (int i = 0; i < wifiCount && i < MAX_NETS; i++) {
    wifiNames[i] = WiFi.SSID(i);
    wifiRssi[i] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  scanOffset = 0;
}

void renderScan() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WIFI:");
  display.print(wifiCount);
  drawFooter("Tut=Geri");

  int visible = 5;
  int maxOff = wifiCount > visible ? wifiCount - visible : 0;
  if (scanOffset > maxOff) scanOffset = maxOff;

  for (int i = 0; i < visible; i++) {
    int idx = scanOffset + i;
    int y = 11 + i * 11;
    if (idx >= wifiCount) break;
    char line[32];
    String n = wifiNames[idx];
    if (n.length() > 13) n = n.substring(0, 13);
    snprintf(line, sizeof(line), "%02d %s", idx, n.c_str());
    display.setTextSize(1);
    display.setCursor(0, y);
    display.print(line);
    char r[8];
    snprintf(r, sizeof(r), "%d", wifiRssi[idx]);
    display.setCursor(128 - 6 * strlen(r), y);
    display.print(r);
  }
  display.display();
}

// ---------- ntp ----------
void syncTime() {
  configTime(GMT_OFFSET_SEC, 0, "tr.pool.ntp.org", "pool.ntp.org");
}

void renderNtp() {
  struct tm t;
  bool ok = getLocalTime(&t, 0);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("NTP YENILEME");
  display.setTextSize(2);
  display.setCursor(34, 22);
  display.print(ok ? "OK" : "...");
  if (ok) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    display.setTextSize(1);
    display.setCursor(128 - 6 * strlen(buf), 38);
    display.print(buf);
  }
  drawFooter("Tus=Geri");
  display.display();
}

// ---------- weather page ----------
void renderWeather() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(CITY_NAME);
  display.setCursor(128 - 6 * 7, 0);
  display.print(wOk ? "HAVA" : "YOK");

  display.setTextSize(4);
  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%dC", wTemp);
  display.setCursor(4, 10);
  display.print(wOk ? tmp : "--");

  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print(wDesc);

  if (wHum >= 0) {
    char hline[16];
    snprintf(hline, sizeof(hline), "Nem: %d%%", wHum);
    display.setCursor(0, 52);
    display.print(hline);
  }

  drawFooter("Ksa=Yenile");
  display.display();
}

// ---------- setup portal ----------
void loadConfig() {
  prefs.begin("saat", false);
  cfgSSID = prefs.getString("wssid", "");
  cfgPass = prefs.getString("wpass", "");
  prefs.end();
}

void saveConfig() {
  prefs.begin("saat", false);
  prefs.putString("wssid", cfgSSID);
  prefs.putString("wpass", cfgPass);
  prefs.end();
}

void clearConfig() {
  prefs.begin("saat", false);
  prefs.remove("wssid");
  prefs.remove("wpass");
  prefs.end();
}

String optionHtml() {
  WiFi.scanDelete();
  int n = WiFi.scanNetworks();
  String html = "";
  for (int i = 0; i < n; i++) {
    html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  if (n == 0) html += "<option value=''>(tarama sonucu yok)</option>";
  WiFi.scanDelete();
  return html;
}

String pageHtml(bool saved) {
  String h = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'></head><body style='font-family:sans-serif'><h2 style='text-align:center'>ESP32 Saat - WiFi Kurulumu</h2>";
  h += "<div style='max-width:400px;margin:0 auto'>";
  if (saved) h += "<p style='color:green'>Kaydedildi! Yeniden basliyor...</p>";
  h += "<form method='POST' action='/save'>";
  h += "<p><label>Ag adi:</label><br><input type='text' name='ssidMan' style='width:100%' placeholder='Ornek: TurkTelekom_2.4G' required></p>";
  h += "<p><label>veya taramadan sec:</label><br><select name='ssid' style='width:100%'>" + optionHtml() + "</select></p>";
  h += "<p><label>Sifre:</label><br><input type='password' name='pass' style='width:100%'></p>";
  h += "<p><button type='submit' style='width:100%;padding:10px'>Kaydet ve Baglan</button></p>";
  h += "</form>";
  h += "<p style='text-align:center'><a href='/reset'>Ayarlari sifirla</a></p>";
  h += "<p style='font-size:13px;color:#666'>Not: ESP32 sadece 2.4 GHz aglara baglanir. 5 GHz aglar gorunmez.</p>";
  h += "</div></body></html>";
  return h;
}

void handleRoot() {
  server.send(200, "text/html", String("<!doctype html><html><head><meta charset='utf-8'></head><body><h2>WiFi kurulum sayfasi</h2><p><a href='/setup'>Devam</a></p></body></html>"));
}

void handleSetup() {
  server.send(200, "text/html", pageHtml(false));
}

void handleSave() {
  String ss = server.arg("ssidMan");
  if (ss.length() == 0) ss = server.arg("ssid");
  if (ss.length() > 0) {
    cfgSSID = ss;
    cfgPass = server.arg("pass");
    saveConfig();
  }
  server.send(200, "text/html", "<html><body><h2>Kaydedildi, yeniden basliyor...</h2></body></html>");
  delay(600);
  ESP.restart();
}

void handleReset() {
  clearConfig();
  server.send(200, "text/html", "<html><body><h2>Ayarlar silindi. Yeniden basliyor...</h2></body></html>");
  delay(600);
  ESP.restart();
}

void handleAny() {
  handleRoot();
}

void connectSta() {
  WiFi.mode(WIFI_STA);
  if (cfgSSID.length() > 0) {
    WiFi.begin(cfgSSID.c_str(), cfgPass.c_str());
    syncTime();
  }
}

void startSetupAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName);
  dnsServer.start(DNS_PORT, "*", apIP);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, handleSetup);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset", HTTP_GET, handleReset);
  server.onNotFound(handleAny);
  server.begin();
  apRunning = true;
  mode = MODE_SETUP;
  renderAgain();
}

void stopSetupAP() {
  if (!apRunning) return;
  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  apRunning = false;
  connectSta();
}

void renderSetup() {
  String s = cfgSSID.length() == 0 ? String("Yeni kurulum") : String("Baglanamiyor: ") + cfgSSID;
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WIFI KURULUMU");
  display.setCursor(0, 12);
  display.print("AP: ");
  display.print(apName);
  display.setCursor(0, 24);
  display.print("IP: 192.168.4.1");
  display.setCursor(0, 36);
  display.print("Telefonla bu IP ac");
  display.setCursor(0, 46);
  if (s.length() > 21) s = s.substring(0, 21);
  display.print(s);
  drawFooter("Tut=Geri");
  display.display();
}

// ---------- seri portla wifi kurulumu ----------
void handleSerialCmd() {
  static String buf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf.trim();
      if (buf.length() > 0) {
        if (buf == "SIFIRLA") {
          clearConfig();
          cfgSSID = "";
          cfgPass = "";
          Serial.println("[OK] Ayarlar silindi");
          startSetupAP();
        } else {
          int sep = buf.indexOf('|');
          if (sep > 0 && sep < (int)buf.length() - 1) {
            cfgSSID = buf.substring(0, sep);
            cfgPass = buf.substring(sep + 1);
            saveConfig();
            Serial.println("[OK] Kaydedildi, baglaniliyor: " + cfgSSID);
            if (apRunning) stopSetupAP();
            else connectSta();
          } else {
            Serial.println("[HATA] Format: AgAdi|Sifre  (orn: MyWifi|12345678)");
          }
        }
      }
      buf = "";
    } else {
      buf += c;
    }
  }
}

// ---------- actions ----------
void enterItem(int idx) {
  switch (idx) {
    case 0:
      swBase = 0;
      swRunning = false;
      mode = MODE_STOPWATCH;
      break;
    case 1:
      pomoRunning = false;
      pomoMode = POM_WORK;
      pomoSec = WORK_SEC;
      mode = MODE_POMODORO;
      break;
    case 2:
      mode = MODE_WEATHER;
      fetchWeather();
      break;
    case 3:
      startScan();
      mode = MODE_WIFISCAN;
      break;
    case 4:
      syncTime();
      mode = MODE_NTP;
      break;
    case 5:
      startSetupAP();
      break;
  }
  renderAgain();
}

void onShort() {
  if (mode == MODE_MENU) {
    menuSel = (menuSel + 1) % menuCount;
  } else if (mode == MODE_CLOCK) {
    mode = MODE_MENU;
  } else if (mode == MODE_STOPWATCH) {
    if (swRunning) {
      swBase = swElapsed();
      swRunning = false;
    } else {
      swStartMs = millis();
      swRunning = true;
    }
  } else if (mode == MODE_POMODORO) {
    if (pomoRunning) {
      pomoRunning = false;
    } else {
      if (millis() >= blinkUntil) startPomodoroRun();
    }
  } else if (mode == MODE_WIFISCAN) {
    scanOffset++;
  } else if (mode == MODE_NTP) {
    mode = MODE_MENU;
  } else if (mode == MODE_WEATHER) {
    fetchWeather();
  } else if (mode == MODE_SETUP) {
    mode = MODE_MENU;
  }
  renderAgain();
}

void onLong() {
  if (mode == MODE_MENU) {
    enterItem(menuSel);
  } else if (mode == MODE_STOPWATCH) {
    if (swRunning) {
      mode = MODE_MENU;
    } else {
      swBase = 0;
    }
  } else if (mode == MODE_POMODORO) {
    mode = MODE_MENU;
  } else if (mode == MODE_WIFISCAN) {
    mode = MODE_MENU;
  } else if (mode == MODE_NTP) {
    mode = MODE_MENU;
  } else if (mode == MODE_WEATHER) {
    mode = MODE_MENU;
  } else if (mode == MODE_SETUP) {
    stopSetupAP();
    mode = MODE_MENU;
  }
  renderAgain();
}

// ---------- misc ----------
void readButton() {
  bool raw = digitalRead(BTN_GPIO) == LOW;
  if (!btnDown && raw) {
    btnDown = true;
    pressStart = millis();
    longFired = false;
  } else if (btnDown && !raw) {
    bool firedLong = longFired;
    btnDown = false;
    if (!firedLong) onShort();
  } else if (btnDown && raw && !longFired && millis() - pressStart >= 700) {
    longFired = true;
    onLong();
  }
}

void wifiCheck() {
  static unsigned long lastTry = 0;
  static unsigned long connectFailStart = 0;
  static time_t lastSync = 0;

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastTry > 10000) {
      lastTry = millis();
      WiFi.reconnect();
    }
    if (!apRunning && cfgSSID.length() > 0) {
      if (connectFailStart == 0) connectFailStart = millis();
      else if (millis() - connectFailStart > 15000) {
        connectFailStart = 0;
        startSetupAP();
      }
    }
    return;
  }
  connectFailStart = 0;
  time_t now = time(nullptr);
  if (lastSync == 0 || now - lastSync > 3600) {
    lastSync = now;
    syncTime();
  }
}

void render() {
  unsigned long now = millis();
  if (now - lastRender < 100) return;
  lastRender = now;
  switch (mode) {
    case MODE_CLOCK: renderClock(); break;
    case MODE_MENU: renderMenu(); break;
    case MODE_STOPWATCH: renderStopwatch(); break;
    case MODE_POMODORO: renderPomodoro(); break;
    case MODE_WIFISCAN: renderScan(); break;
    case MODE_NTP: renderNtp(); break;
    case MODE_WEATHER: renderWeather(); break;
    case MODE_SETUP: renderSetup(); break;
  }
}

void setup() {
  pinMode(BTN_GPIO, INPUT_PULLUP);
  pinMode(LED_GPIO, OUTPUT);
  Serial.begin(115200);

  loadConfig();

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.display();

  if (cfgSSID.length() > 0) {
    connectSta();
    Serial.println("Baglaniliyor: " + cfgSSID);
  } else {
    startSetupAP();
    Serial.println();
    Serial.println("=== WiFi kurulumu ===");
    Serial.println("1. Telefonunla ESP32_Saat agina baglan, 192.168.4.1 ac");
    Serial.println("2. veya buraya  AgAdi|Sifre  yaz, orn: MyWifi|12345678");
    Serial.println();
  }
}

void loop() {
  readButton();
  handleSerialCmd();
  updateBlink();
  tickPomodoro();
  wifiCheck();
  weatherCheck();
  if (apRunning) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
  render();
  delay(10);
}