// RECEIVER
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

RF24 radio(9, 10);
const byte address[6] = "D4O21";
const uint8_t RF_CHANNEL = 108;

struct Payload {
  int16_t waterRaw;
  int16_t tempC_x10;
  int32_t pressurePa;
};

Payload lastRecv = {0, 0, 0};
unsigned long lastRecvMs = 0;
unsigned long lastDrawMs = 0;
const unsigned long DRAW_INTERVAL_MS = 350;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Fail"));
  }

  radio.begin();
  radio.setChannel(RF_CHANNEL);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS); // Must match Sender
  radio.setAutoAck(true);
  radio.openReadingPipe(1, address);
  radio.startListening();
}

void loop() {
  if (radio.available()) {
    radio.read(&lastRecv, sizeof(Payload));
    lastRecvMs = millis();
  }

  unsigned long now = millis();
  if (now - lastDrawMs >= DRAW_INTERVAL_MS) {
    bool linkOK = (now - lastRecvMs < 2000); // 2-second timeout
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextColor(SSD1306_WHITE);
    display.println(F("RECEIVER (Remote)"));
    display.print(F("Link: ")); display.println(linkOK ? F("OK") : F("LOST"));
    
    if(linkOK) {
      display.print(F("H2O:  ")); display.println(lastRecv.waterRaw);
      display.print(F("Temp: ")); display.print(lastRecv.tempC_x10/10.0); display.println(F("C"));
    }
    display.display();
    lastDrawMs = now;
  }
}
