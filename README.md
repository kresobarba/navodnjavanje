# Pametno Navodnjavanje
## 🌱💧🌳
## Zahtjevi

Potreban je Arduino 1.8.13 ili noviji, a potrebne su i sljedeće biblioteke iz Arduino IDE Library Managera:

ESP32AsyncWebServer
ESPnow
ArduinoJson
sqlite3
TFT_eSPI (konfiguriran za TTGO T-Display)

## Upotreba

Podesiti WiFi i ESPnow podatke u kodu. (WiFi SSID i lozinka, ESPnow MAC adresa i lozinka), WebServer.ino
Kompajlirati za ESP32 Dev Module. Odabrati veliku particiju za ESP32 (SPIFFS Large 16MB)

Upload datoteka u data/ folderu na ESP32 SPIFFS Korištenjem ESP32 Sketch Data Upload alata. [arduino-esp32fs-plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin)

Spojiti se na [navodnjavanje.local](http://navodnjavanje.local) ili na IP na ekranu preko web preglednika za pristup upravljačkom sučelju.

## Offline Demo

Offline demonstracija web sučelja je dostupna ovdje: [Početna](WebServer/data/index.html)

