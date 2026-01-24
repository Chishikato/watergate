// ===================== RECEIVER.ino =====================
// UI: Bar graph + % (no raw water shown on OLED)
// Shows Link OK/LOST + last packet age + temp

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- Radio ----------
RF24 radio(9, 10);
const byte address[6] = "D4O21";
const uint8_t RF_CHANNEL = 108;

// ---------- Payload (MUST match sender) ----------
struct Payload {
  int16_t waterRaw;     // debug only (NOT displayed)
  uint8_t waterPct;     // 0..100 (display)
  int16_t tempC_x10;    // temperature * 10
};

Payload lastRecv = {0, 0, 0};
unsigned long lastRecvMs = 0;
unsigned long lastDrawMs = 0;

const unsigned long DRAW_INTERVAL_MS = 250;
const unsigned long LINK_TIMEOUT_MS  = 2000;

uint32_t rxCount = 0;
uint8_t waterPctSmooth = 0;

static void drawBar(int x, int y, int w, int h, uint8_t pct) {
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  int innerW = w - 2;
  int fillW = (innerW * pct) / 100;
  display.fillRect(x + 1, y + 1, fillW, h - 2, SSD1306_WHITE);
}

static void drawUI(bool linkOK, uint8_t pct, float tempC, uint16_t ageMs) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("RECEIVER"));
  display.setCursor(66, 0);
  display.print(F("Link:"));
  display.print(linkOK ? F("OK") : F("LOST"));

  // Big percent
  display.setTextSize(2);
  display.setCursor(0, 14);
  if (linkOK) {
    display.print(pct);
    display.print(F("%"));
  } else {
    display.print(F("--%"));
  }

  // Bar graph
  drawBar(0, 38, 128, 12, linkOK ? pct : 0);

  // Footer: temp + age
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("Temp: "));
  if (linkOK) {
    display.print(tempC, 1);
    display.print(F("C"));
  } else {
    display.print(F("N/A"));
  }

  display.setCursor(74, 54);
  display.print(F("Age:"));
  display.print(ageMs);
  display.print(F("ms"));

  display.display();
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Fail"));
  }
  display.clearDisplay();
  display.display();

  radio.begin();
  radio.setChannel(RF_CHANNEL);
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(true);
  radio.openReadingPipe(1, address);
  radio.startListening();
}

void loop() {
  // Receive
  if (radio.available()) {
    radio.read(&lastRecv, sizeof(Payload));
    lastRecvMs = millis();
    rxCount++;

    // smooth percent
    waterPctSmooth = (uint8_t)((waterPctSmooth * 7 + lastRecv.waterPct * 3) / 10);

    // Serial debug (raw stays here)
    Serial.print(F("RX#")); Serial.print(rxCount);
    Serial.print(F(" waterRaw=")); Serial.print(lastRecv.waterRaw);
    Serial.print(F(" waterPct=")); Serial.print(lastRecv.waterPct);
    Serial.print(F(" tempC=")); Serial.println(lastRecv.tempC_x10 / 10.0f);
  }

  // Draw
  unsigned long now = millis();
  if (now - lastDrawMs >= DRAW_INTERVAL_MS) {
    lastDrawMs = now;

    bool linkOK = (now - lastRecvMs) < LINK_TIMEOUT_MS;
    uint16_t ageMs = (lastRecvMs == 0) ? 9999 : (uint16_t)min((unsigned long)9999, now - lastRecvMs);
    float tempC = lastRecv.tempC_x10 / 10.0f;

    drawUI(linkOK, waterPctSmooth, tempC, ageMs);
  }
}
