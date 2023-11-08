#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <Adafruit_INA219.h>
#include <SPI.h>
#include <deque>

Adafruit_INA219 ina219;
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);


static constexpr uint8_t BUTTON {0};

typedef struct {
  float busVoltage;
  float current;
  float power;
  float shuntVoltage;
  float loadVoltage;
} measurement;

enum MODE : uint8_t {
  Current,
  Voltage,
  Power,
  Detailed,
};


std::deque<measurement> prev;
uint8_t mode {MODE::Detailed};
uint32_t lastClear {0};
bool latch {false};
constexpr uint8_t memorySize {80};
constexpr uint8_t divisionWidth {240 / memorySize};


void setupCurrent() {
  tft.fillScreen(0xffcc);
  tft.setCursor(0, 0);
  tft.setTextSize(3);
  tft.setTextColor(0xa500);
  tft.print("Current");
}


void setupVoltage() {
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.setTextColor(0xf800, 0);
  tft.print("Voltage");
}


void setupPower() {
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.setTextColor(0xffe0, 0);
  tft.print("Power");
}


void setupDetailed() {}


void drawCurrent(const measurement& max, const measurement& avg) {}


void drawVoltage(const measurement& max, const measurement& avg) {}


void drawPower(const measurement& max, const measurement& avg) {}


void drawDetailed(const measurement& max, const measurement& avg) {
  auto last {prev.back()};

  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.setTextColor(0xffe0);
  tft.print("Power meter");

  tft.setCursor(0, 24);
  tft.setTextSize(1);
  tft.setTextColor(0xf800, 0);
  tft.print("Bus:     "); tft.print(last.busVoltage); tft.print(" V. Avg: "); tft.print(avg.busVoltage); tft.println(" V    ");
  tft.setTextColor(0x07e0, 0);
  tft.print("Current: "); tft.print(last.current); tft.print(" mA. Avg: "); tft.print(avg.current); tft.println(" mA    ");
  tft.setTextColor(0xffe0, 0);
  tft.print("Power:   "); tft.print(last.power); tft.print(" mW. Avg: "); tft.print(avg.power); tft.println(" mW    ");
  tft.setTextColor(0xffff, 0);
  tft.print("Shunt:   "); tft.print(avg.shuntVoltage); tft.println(" mV    ");
  tft.print("Load:    "); tft.print(avg.loadVoltage); tft.println(" V    ");


  float busVoltageScale = 70 / max.busVoltage;
  float currentScale = 70 / max.current;
  float powerScale = 70 / max.power;

  for (uint8_t i {0}; i < prev.size() - 1; ++i) {
    tft.fillRect(divisionWidth * i, 64, divisionWidth, 71, 0);
    tft.drawLine(divisionWidth * i, 135 - prev[i].busVoltage * busVoltageScale, divisionWidth * (i + 1), 135 - prev[i + 1].busVoltage * busVoltageScale, 0xf800);
    tft.drawLine(divisionWidth * i, 135 - prev[i].current * currentScale, divisionWidth * (i + 1), 135 - prev[i + 1].current * currentScale, 0x07e0);
    tft.drawLine(divisionWidth * i, 135 - prev[i].power * powerScale, divisionWidth * (i + 1), 135 - prev[i + 1].power * powerScale, 0xffe0);
  }
}


void (*setupFunctions[])() = {setupCurrent, setupVoltage, setupPower, setupDetailed};
void (*drawFunctions[])(const measurement& max, const measurement& avg) = {drawCurrent, drawVoltage, drawPower, drawDetailed};

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  pinMode(BUTTON, INPUT_PULLUP);
  delay(10);
  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(0);
  tft.setTextWrap(true);

  if (!ina219.begin()) {
    tft.setCursor(0, 24);
    tft.setTextColor(0xf800);
    tft.setTextSize(2);
    tft.print("INA219 not connected");

    while (1) { 
      delay(1); 
    }
  }

  tft.setTextSize(1);
}

void loop() {
  if (!digitalRead(BUTTON) && !latch) {
    latch = true;
    ++mode;
    if (mode > 3) {
      mode = 0;
    }
    tft.fillScreen(0);
    setupCurrent(); // Should be setupFunctions[mode]() but doesn't work yet
  } else if (digitalRead(BUTTON) && latch) {
    latch = false;
  }

  measurement curr {
    .busVoltage = ina219.getBusVoltage_V(),
    .current = ina219.getCurrent_mA(),
    .power = ina219.getPower_mW(),
    .shuntVoltage = ina219.getShuntVoltage_mV()
  };
  curr.loadVoltage = curr.busVoltage + (curr.shuntVoltage / 1000);
  
  if (curr.busVoltage < 0.1) {curr.busVoltage = 0;}
  if (curr.current < 1.0) {curr.current = 0;}
  if (curr.power < 1.0) {curr.power = 0;}

  prev.push_back(curr);
  if (prev.size() > memorySize + 1) {
    prev.pop_front();
  }
  
  measurement max;
  measurement avg;

  for (uint8_t i {0}; i < prev.size(); ++i) {
    if (prev[i].busVoltage > max.busVoltage) {max.busVoltage = prev[i].busVoltage;}
    if (prev[i].current > max.current) {max.current = prev[i].current;}
    if (prev[i].power > max.power) {max.power = prev[i].power;}

    avg.busVoltage += (prev[i].busVoltage - avg.busVoltage) / (i + 1);
    avg.current += (prev[i].current - avg.current) / (i + 1);
    avg.power += (prev[i].power - avg.power) / (i + 1);
  }

  drawFunctions[mode](max, avg);
 
  delay(5);
  
  if (millis() - lastClear > 60000 || millis() < lastClear) {
    tft.fillScreen(0);
    lastClear = millis();
  }
}
