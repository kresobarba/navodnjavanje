/*
PROTOTIP SUSTAVA PAMETNOG NAVODNJAVANJA
Klijent
*/

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
//#include <Adafruit_Sensor.h>
//#include <DHT.h>
#include <TFT_eSPI.h>

// DHT senzor
#define DHTPIN 4  
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//DHT dht(DHTPIN, DHTTYPE);

#define PUMP_PIN 5

//MAC Addresa primatelja
uint8_t broadcastAddress[] = {0x44, 0x17, 0x93, 0x88, 0xdd, 0xd8};

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

struct_message sentData, receivedData;
int timeToNext=0;

unsigned long previousMillis = 0;     // Zadnje očitanje
const long interval = 2*1000;        // Interval očitanja
unsigned int readingId = 0;

// Ekran na uređaju
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

// Međuspremnici za pomoć kod rada sa stringovima
char buf[32];
char buffer[128];

// SSID servera, trenutno hardkodirano
constexpr char WIFI_SSID[] = "ESP_88DDD9"; //"ESP_897DDC"; "ESP_897DDD";

int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
              return WiFi.channel(i);
          }
      }
  }
  return 0;
}

float readDHTTemperature() {
  float t = (esp_random()%1000)/10.0; //dht.readTemperature();
  if (isnan(t)) {    
    Serial.println("Greška kod čitanja temperature!");
    return 0;
  }
  else {
    Serial.println(t);
    return t;
  }
}

float readDHTHumidity() {
  float h = (esp_random()%1000)/10.0; //dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Greška kod čitanja temperature!");
    return 0;
  }
  else {
    Serial.println(h);
    return h;
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nZadnji paket, Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Dostavljen" : "Neuspjelo");
}

// Callback funkcija koja se poziva kada se primi paket s podacima od drugog uređaja
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  // Kopira MAC pošiljatelja u string
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  memcpy(&receivedData, incomingData, sizeof(receivedData));
}
 
void updateScreen() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextSize(2);
  WiFi.macAddress().toCharArray(buf,20);
  canvas.drawString(String(buf), 16, 4);
  canvas.drawString("Temp:  " + String(sentData.temperature)+" C", 0, 26);
  canvas.drawString("Vlaga: " + String(sentData.soilMoisture)+" %", 0, 42);
  canvas.drawString("ID: " + String(sentData.readingId), 0, 58);
  canvas.setTextSize(1);
  canvas.drawString("Pumpa:    " + String(receivedData.pumpState), 0, 80);
  canvas.drawString("Interval: " + String(receivedData.interval), 0, 90);
  canvas.drawString("Trajanje: " + String(receivedData.duration), 100, 90);
  canvas.drawString("Zadnje:   " + String(receivedData.lastActive), 0, 100);
  canvas.drawString("Sljedece: " + String(receivedData.timeToNext), 100, 100);
  canvas.drawString("ID: " + String(receivedData.readingId), 0, 110);

  canvas.pushSprite(0, 0);
}

void setup() {
  Serial.begin(115200);

  // Ekran
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_DARKCYAN);
  canvas.setColorDepth(8);
  canvas.createSprite(TFT_HEIGHT, TFT_WIDTH);
  canvas.fillSprite(TFT_BLACK);
  canvas.pushSprite(0, 0);
  canvas.setTextSize(3);
  canvas.drawString("SENZOR", 0, 70);
  canvas.setTextSize(2);
  canvas.drawString("Tražim server...", 0, 100);
  canvas.pushSprite(0, 0);



  WiFi.mode(WIFI_STA);

  int32_t channel = getWiFiChannel(WIFI_SSID);

  WiFi.printDiag(Serial); 
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  WiFi.printDiag(Serial); 

  //Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Greška kod pokretanja ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  
 
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.encrypt = false;
  
        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Neuspjelo dodavanje ESP-NOW uređaja");
    return;
  }
  Serial.println("Dodan ESP-NOW uređaj");
}

void StartPump() {
  digitalWrite(PUMP_PIN, HIGH);
}

void StopPump() {
  digitalWrite(PUMP_PIN, LOW);
}


 
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    
    previousMillis = currentMillis;
    
    sentData.uid = WiFi.macAddress()[5];
    sentData.temperature = readDHTTemperature();
    sentData.soilMoisture = readDHTHumidity();
    sentData.readingId = readingId++;
     
    
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &sentData, sizeof(sentData));
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
  }

  if (receivedData.pumpState == 1) {
    StartPump();
  }
  else {
    StopPump();
  }
  updateScreen();
  delay(1000);
}
