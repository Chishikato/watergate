// ===================== NODE_NETWORK.ino =====================
// Unified Transceiver Node
// - Unique Node IDs
// - Dynamic entry/exit
// - Bidirectional (Send & Receive)
// - Shared Network Pipe

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= USER CONFIGURATION =================
// CHANGE THIS for every board (1, 2, 3...)
#define THIS_NODE_ID 1 

// ================= HARDWARE CONFIG =================
#define RF_CE_PIN 9
#define RF_CS_PIN 10
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ================= NETWORK CONFIG =================
// Common address for all nodes to share
const uint64_t NETWORK_PIPE = 0xE8E8F0F0E1LL; 
const uint8_t RF_CHANNEL = 108;

// ================= DATA STRUCTURES =================
struct NodePayload {
  uint8_t nodeId;       // ID of the sender
  int16_t waterRaw;     // Raw analog reading
  uint8_t waterPct;     // Calculated percent
  int16_t tempC_x10;    // Temp * 10
  uint32_t msgCount;    // Message counter
};

// ================= GLOBALS =================
RF24 radio(RF_CE_PIN, RF_CS_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BMP085 bmp;

// State Variables
bool bmpOK = false;
NodePayload localData;
NodePayload remoteData; // Stores the last received packet
bool hasRemoteData = false;
unsigned long lastRxTime = 0;

// Timers
unsigned long lastSampleTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastDrawTime = 0;

const unsigned long SAMPLE_INTERVAL = 250;
const unsigned long SEND_INTERVAL = 1000; // Broadcast every 1 sec
const unsigned long DRAW_INTERVAL = 100;
const unsigned long REMOTE_TIMEOUT = 5000; // Clear remote data if silent for 5s

// Calibration
const int WATER_RAW_EMPTY = 0;
const int WATER_RAW_FULL = 1023;

// ================= HELPERS =================
uint8_t rawToPercent(int16_t raw) {
  long denom = (long)(WATER_RAW_FULL - WATER_RAW_EMPTY);
  if (denom == 0) return 0;
  long pct = (long)(raw - WATER_RAW_EMPTY) * 100L / denom;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (uint8_t)pct;
}

const char* moistureLabel(uint8_t pct) {
  if (pct <= 25) return "DRY";
  if (pct <= 50) return "SEMI";
  if (pct <= 75) return "MOIST";
  return "WET";
}

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  Wire.begin();

  // --- OLED ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("OLED failed"));
    }
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print(F("Node Starting..."));
  display.display();

  // --- BMP Sensor ---
  bmpOK = bmp.begin();
  
  // --- Radio ---
  radio.begin();
  radio.setChannel(RF_CHANNEL);
  radio.setPALevel(RF24_PA_MAX); // Max power for range
  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(false); // Disable Ack for broadcast mesh
  
  // Open pipes
  radio.openWritingPipe(NETWORK_PIPE); // We send here
  radio.openReadingPipe(1, NETWORK_PIPE); // We listen here
  
  radio.startListening(); // Default state: Listening
  
  // Init Local Data
  localData.nodeId = THIS_NODE_ID;
  localData.msgCount = 0;
}

// ================= MAIN LOOP =================
void loop() {
  unsigned long now = millis();

  // 1. RECEIVE DATA (Always check first)
  if (radio.available()) {
    NodePayload incoming;
    radio.read(&incoming, sizeof(NodePayload));
    
    // Ignore our own packets (echo cancellation)
    if (incoming.nodeId != THIS_NODE_ID) {
      remoteData = incoming;
      hasRemoteData = true;
      lastRxTime = now;
      
      // Debug
      Serial.print(F("RX from Node ")); Serial.println(incoming.nodeId);
    }
  }

  // Check timeout for remote node
  if (hasRemoteData && (now - lastRxTime > REMOTE_TIMEOUT)) {
    hasRemoteData = false;
  }

  // 2. SAMPLE SENSORS
  if (now - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = now;
    
    localData.waterRaw = analogRead(A0);
    localData.waterPct = rawToPercent(localData.waterRaw);
    
    if (bmpOK) {
      localData.tempC_x10 = (int16_t)(bmp.readTemperature() * 10.0f);
    } else {
      localData.tempC_x10 = 0;
    }
  }

  // 3. BROADCAST DATA
  if (now - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = now;
    
    // Prepare to send
    radio.stopListening(); 
    
    // Add small random delay to prevent collisions if nodes sync up
    delay(random(5, 15)); 
    
    localData.msgCount++;
    bool ok = radio.write(&localData, sizeof(NodePayload));
    
    Serial.print(F("TX Node ")); Serial.print(THIS_NODE_ID);
    Serial.println(ok ? F(" OK") : F(" Fail")); // Fail is normal in multicast (no ack)

    radio.startListening(); // Go back to listening
  }

  // 4. DRAW UI
  if (now - lastDrawTime >= DRAW_INTERVAL) {
    lastDrawTime = now;
    drawScreen();
  }
}

// ================= DRAWING =================
void drawScreen() {
  display.clearDisplay();
  
  // --- TOP HALF: LOCAL STATUS ---
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("LOCAL (ID:")); 
  display.print(THIS_NODE_ID); 
  display.print(F(")"));

  display.setCursor(0, 10);
  display.print(moistureLabel(localData.waterPct));
  display.print(F(" "));
  display.print(localData.waterPct);
  display.print(F("% "));
  
  if (bmpOK) {
    display.print(localData.tempC_x10 / 10.0f, 1);
    display.print(F("C"));
  }

  // Draw separator line
  display.drawLine(0, 25, 128, 25, SSD1306_WHITE);

  // --- BOTTOM HALF: REMOTE STATUS ---
  display.setCursor(0, 28);
  if (hasRemoteData) {
    display.print(F("REMOTE (ID:")); 
    display.print(remoteData.nodeId); 
    display.print(F(")"));
    
    display.setTextSize(2); // Make remote data big
    display.setCursor(0, 40);
    display.print(remoteData.waterPct);
    display.print(F("% "));
    
    display.setTextSize(1);
    display.print(moistureLabel(remoteData.waterPct));
    
    // Signal strength indicator (fake based on timeout)
    display.setCursor(100, 28);
    display.print(F("RX"));
  } else {
    display.print(F("Scanning..."));
    display.setCursor(0, 45);
    display.setTextSize(1);
    display.print(F("No signal"));
  }

  display.display();
}