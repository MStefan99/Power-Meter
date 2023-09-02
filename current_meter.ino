#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <Adafruit_INA219.h>
#include <SPI.h>
#include <deque>

Adafruit_INA219 ina219;
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

typedef struct {
  float busVoltage;
  float current;
  float power;
} measurement;

std::deque<measurement> prev;
uint32_t lastClear {0};
constexpr uint8_t memorySize {80};
constexpr uint8_t divisionWidth {240 / memorySize};


void setup() {
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
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
  float shuntVoltage = 0;
  float busVoltage = 0;
  float current = 0;
  float loadVoltage = 0;
  float power = 0;

  shuntVoltage = ina219.getShuntVoltage_mV();
  busVoltage = ina219.getBusVoltage_V();
  current = ina219.getCurrent_mA();
  power = ina219.getPower_mW();
  loadVoltage = busVoltage + (shuntVoltage / 1000);  
  
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.setTextColor(0xffe0);
  tft.print("Power meter");

  tft.setCursor(0, 24);
  tft.setTextSize(1);
  tft.setTextColor(0xf800, 0);
  tft.print("Bus Voltage:   "); tft.print(busVoltage); tft.println(" V    ");
  tft.setTextColor(0x07e0, 0);
  tft.print("Current:       "); tft.print(current); tft.println(" mA    ");
  tft.setTextColor(0xffe0, 0);
  tft.print("Power:         "); tft.print(power); tft.println(" mW    ");
  tft.setTextColor(0xffff, 0);
  tft.print("Shunt Voltage: "); tft.print(shuntVoltage); tft.println(" mV    ");
  tft.print("Load Voltage:  "); tft.print(loadVoltage); tft.println(" V    ");

  if (busVoltage < 0.1) {busVoltage = 0;}
  if (current < 1.0) {current = 0;}
  if (power < 1.0) {power = 0;}

  prev.push_back({
    .busVoltage = busVoltage,
    .current = current,
    .power = power
  });
  if (prev.size() > memorySize + 1) {
    prev.pop_front();
  }

  float maxBusVoltage = 0;
  float maxCurrent = 0;
  float maxPower = 0;

  for (const auto& p : prev) {
    if (p.busVoltage > maxBusVoltage) {maxBusVoltage = p.busVoltage;}
    if (p.current > maxCurrent) {maxCurrent = p.current;}
    if (p.power > maxPower) {maxPower = p.power;}
  }

  float busVoltageScale = 70 / maxBusVoltage;
  float currentScale = 70 / maxCurrent;
  float powerScale = 70 / maxPower;

  for (uint8_t i {0}; i < prev.size() - 1; ++i) {
    tft.fillRect(divisionWidth * i, 64, divisionWidth, 71, 0);
    tft.drawLine(divisionWidth * i, 135 - prev[i].busVoltage * busVoltageScale, divisionWidth * (i + 1), 135 - prev[i + 1].busVoltage * busVoltageScale, 0xf800);
    tft.drawLine(divisionWidth * i, 135 - prev[i].current * currentScale, divisionWidth * (i + 1), 135 - prev[i + 1].current * currentScale, 0x07e0);
    tft.drawLine(divisionWidth * i, 135 - prev[i].power * powerScale, divisionWidth * (i + 1), 135 - prev[i + 1].power * powerScale, 0xffe0);
  }
 
  delay(5);
  
  if (millis() - lastClear > 60000 || millis() < lastClear) {
    tft.fillScreen(0);
    lastClear = millis();
  }
}