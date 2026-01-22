#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <avr/wdt.h>

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

Payload last = {0, 0, 0};
unsigned long lastRecvMs = 0;

unsigned long lastDrawMs = 0;
const unsigned long DRAW_INTERVAL_MS = 350;

bool initOLED(uint8_t addr) {
  if (!display.begin(SSD1306_SWITCHCAPVCC, addr)) return false;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();
  return true;
}

void drawUI(const Payload& p, bool linkOK) {
  display.clearDisplay();

  display.setCursor(0, 0);
  display.print("RECEIVER (Remote)");

  display.setCursor(0, 12);
  display.print("Link: ");
  display.print(linkOK ? "OK" : "NO DATA");

  display.setCursor(0, 24);
  display.print("Water: ");
  display.print(p.waterRaw);

  display.setCursor(0, 36);
  display.print("Temp: ");
  display.print(p.tempC_x10 / 10.0);
  display.print("C");

  display.setCursor(0, 48);
  display.print("Pres: ");
  display.print(p.pressurePa / 100.0);
  display.print("hPa");

  display.display();
}

void setup() {
  Serial.begin(9600);

  wdt_enable(WDTO_2S);

  Wire.begin();
  Wire.setClock(100000);
#if defined(TWCR) && defined(TWEN)
  Wire.setWireTimeout(25000, true);
#endif

  bool oledOK = initOLED(0x3C) || initOLED(0x3D);
  if (!oledOK) {
    Serial.println("Receiver OLED not found");
    while (true) {}
  }

  radio.begin();
  radio.setChannel(RF_CHANNEL);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(true);
  radio.setRetries(5, 15);
  radio.openReadingPipe(1, address);
  radio.startListening();

  Serial.println("Receiver ready");
}

void loop() {
  wdt_reset();

  bool gotNew = false;
  while (radio.available()) {
    Payload p;
    radio.read(&p, sizeof(p));
    last = p;
    lastRecvMs = millis();
    gotNew = true;
  }

  bool linkOK = (millis() - lastRecvMs) < 1500;

  unsigned long now = millis();
  if (gotNew || (now - lastDrawMs) >= DRAW_INTERVAL_MS) {
    drawUI(last, linkOK);
    lastDrawMs = now;
  }

  delay(5);
}
