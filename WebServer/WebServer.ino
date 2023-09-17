/*
PROTOTIP SUSTAVA PAMETNOG NAVODNJAVANJA
Server
*/
// Vanjske biblioteke
#include <esp_now.h>
#include <WiFi.h>
#include <ESPAsyncWebSrv.h>
#include <Arduino_JSON.h>
#include <TFT_eSPI.h>
#include <sqlite3.h>
#include "SPIFFS.h"
#include <ESPmDNS.h>           // Built-in

// Zamijeniti sa svojim podacima za pristup WiFi mreži
const char* ssid = "test";
const char* password = "test";

// Struktura koja se šalje preko ESP-NOW protokola
// Moraju biti iste na svim uređajima !
typedef struct struct_message {
  int uid;                 // ID uređaja

  float temperature;        // Temperatura
  float soilMoisture;       // Vlažnost tla

  int interval;             // Interval paljenja prskalice
  int duration;             // Trajanje navodnjavanja
  int pumpState;            // Stanje prskalice
  int lastActive;           // Vrijeme posljednjeg navodnjavanja
  int timeToNext;           // Vrijeme do sljedećeg navodnjavanja
  
  time_t timestamp;         // Vrijeme

  unsigned int readingId;   // Broj poruke
} struct_message;

struct_message incomingReadings;

JSONVar jsonData;
AsyncWebServer server(80);
AsyncEventSource events("/events");

// Baza podataka
sqlite3 *db1;
int rc;
sqlite3_stmt *res; 
int rec_count = 0;
const char *tail; 

// Ekran na uređaju
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// Međuspremnici za pomoć kod rada sa stringovima
char buf[32];
char buffer[128];

// IP adresa uređaja
IPAddress Actual_IP;
IPAddress PageIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress ip;

// Initialise mDNS service and add hostname and URL
bool StartMDNSservice(const char* Name) {
  esp_err_t err = mdns_init();             
  if (err) {
    printf("MDNS Init failed: %d\n", err); // Return if error detected
    return false;
  }
  mdns_hostname_set(Name);                 // Set hostname
  return true;
}

// Callback funkcija koja se poziva kada se primi paket s podacima od drugog uređaja
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  // Kopira MAC pošiljatelja u string
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  
  // Pohranjuje podatke u JSON varijablu
  jsonData["id"] = macStr; //incomingReadings.id;
  jsonData["temperature"] = incomingReadings.temperature;
  jsonData["humidity"] = incomingReadings.soilMoisture;
  jsonData["readingId"] = String(incomingReadings.readingId);
  String jsonString = JSON.stringify(jsonData);

  // Šalje podatke na web stranicu, gdje se prikazuju
  events.send(jsonString.c_str(), "new_readings", millis());
  
  // Ispis podataka za debugiranje
  Serial.printf("Board ID %u: %u bytes\n", incomingReadings.uid, len);
  Serial.printf("t value: %4.2f \n", incomingReadings.temperature);
  Serial.printf("h value: %4.2f \n", incomingReadings.soilMoisture);
  Serial.printf("readingID value: %d \n", incomingReadings.readingId);
  Serial.println();
}

// Login forma
void handleLogin(AsyncWebServerRequest *request) {
  Serial.println("Login initiated");
  String username = "";
  String password = "";
  if (request!=null)
  { 
    if (request->getParam("username")!=null)
      username = request->getParam("username")->value();
    if (request->getParam("password")!=null)  
      password = request->getParam("password")->value();
  }
  Serial.println("Username: " + username+ " Password: " + password);
  Serial.println("Checking credentials...");
  String query = "SELECT * FROM users WHERE uname='" + username + "' AND passwd='" + password + "';";
  Serial.println(query);

  //rc = sqlite3_exec(db1, query.c_str(), NULL, NULL, NULL);
  rc = sqlite3_prepare_v2(db1, query.c_str(), 1000, &res, &tail);

  if (rc != SQLITE_OK) {
    Serial.println("Error executing SQLite query");
    Serial.println(sqlite3_errmsg(db1));
  } 
  else {
    Serial.println("Query executed successfully");
  }

  if (sqlite3_step(res) == SQLITE_ROW) {
    rec_count = sqlite3_column_int(res, 0);
    Serial.println("count: " + String(rec_count));
    ////request->session().set("logged_in", true);
    ////request->redirect("/protected");
    String message = "Login ok";
    request->send(200, "text/plain", message);
  } else {
    String message = "Neispravni podaci";
    request->send(200, "text/plain", message);
  }

  Serial.println(rc);
  sqlite3_finalize(res);
  
}


// Otvara SQLite bazu podataka
int openDb(const char *filename, sqlite3 **db) {
  int rc = sqlite3_open(filename, db);
  if (rc) {
      Serial.printf("Ne mogu otvoriti bazu: %s\n", sqlite3_errmsg(*db));
      return rc;
  } else {
      Serial.printf("Baza otvorena\n");
  }
  return rc;
}


void setup() {
  // Serijalna komunikacija sa računalom kroz USB
  Serial.begin(115200);

  // Ekran
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_DARKCYAN);
  canvas.setColorDepth(8);
  canvas.createSprite(TFT_HEIGHT, TFT_WIDTH);
  canvas.fillSprite(TFT_BLACK);
  canvas.pushSprite(0, 0);
  canvas.setTextSize(3);
  canvas.drawString("NAVODNJAVATOR", 0, 70);
  canvas.setTextSize(2);
  canvas.drawString("Tražim WiFi mrežu...", 0, 100);
  canvas.pushSprite(0, 0);

  // Postavljanje uređaja na WiFi mrežu
  // Station i Soft Access Point istovremeno
  WiFi.mode(WIFI_AP_STA);
  
  // Traži WiFi mrežu
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
    if (!StartMDNSservice("navodnjavanje")) {
    Serial.println("Error starting mDNS Service...");;
  }
  Serial.println("Wi-Fi connected");
  //Ispis podataka o WiFi mreži
  Actual_IP = WiFi.localIP();
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
  
  // Ispis IP adrese na ekran
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextSize(4);
  canvas.drawString("SPOJEN", 30, 70);
  canvas.setTextSize(2);
  Actual_IP.toString().toCharArray(buf,20);
  sprintf(buffer, "%s", buf);
  canvas.drawString(buffer, 0, 0);
  sprintf(buffer, "RSSI: %d dBm", WiFi.RSSI());
  canvas.drawString(buffer, 0, 30);
  canvas.pushSprite(0, 0);

  WiFi.macAddress().toCharArray(buf,20);
  sprintf(buffer, "%s", buf);
  canvas.drawString(buffer, 0, 104);

  // Pokreni ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Ne mogu pokrenuti ESP-NOW");
    return;
  }

  // Konfiguracija Web servera
  // Povezivanje povratnih funkcija za primanje i slanje podataka
  
  // Kad se preko ESP-Now primi paket s podacima, poziva se funkcija OnDataRecv
  esp_now_register_recv_cb(OnDataRecv);

  // Web server, početna stranica
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send_P(200, "text/html", index_html);
    request->send(SPIFFS, "/index.html", String(), false, null);
  });

    // Route to load style.css file
  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/styles.css", "text/css");
  });

  server.on("/testindex", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index_old.html", String(), false, null);
  });

  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, null);
  });

  server.on("/uredaji.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/uredaji.html", String(), false, null);
  });

  server.on("/login.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/login.html", String(), false, null);
  });

  // dodatno
  server.on("/css/all.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/css/all.min.css", String(), false, null);
  });

  server.on("/css/sb-admin-2.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/css/sb-admin-2.min.css", String(), false, null);
  });

  server.on("/css/fa-solid-900.woff2", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/css/fa-solid-900.woff2", String(), false, null);
  });


  // Handle form submissions at the /login URL
  server.on("/login", HTTP_POST, handleLogin);

  // Web server, spajanje novog klijenta
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Klijent spojen! Zadnji ID: %u\n", client->lastId());
    }
    // Event za reconnect
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

//### EKSPERIMENTALNO, SQLITE3 i SPIFFS Web Server

// Pokreni SPIFFS datotečni sustav 
if(!SPIFFS.begin(true)){
  Serial.println("Greška kod pokretanja SPIFFS");
  return;
}
sqlite3_initialize();
// Otvori SQLite3 bazu podataka
openDb("/spiffs/sqlite.db", &db1);

//### EKSPERIMENTALNO

  server.begin();
}
 
void loop() {

  // Šalje se paket s podacima svakih 5 sekundi kao 'ping'
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000;
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    events.send("ping",NULL,millis());
    lastEventTime = millis();
  }


}
