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
  tft.print("Current monitor");

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

  prev.push_back({
      .busVoltage = busVoltage,
      .current = std::floor(current),
      .power = std::floor(power)
    });
  if (prev.size() > 40) {
    prev.pop_front();
  }

  tft.fillRect(0, 72, 240, 63, 0);
  for (uint8_t i {0}; i < prev.size() - 1; ++i) {
    tft.drawLine(6 * i, 135 - prev[i - 1].busVoltage * 8, 6 * (i + 1), 135 - prev[i].busVoltage * 8, 0xf800);
    tft.drawLine(6 * i, 135 - prev[i - 1].current / 9, 6 * (i + 1), 135 - prev[i].current / 9, 0x07e0);
    tft.drawLine(6 * i, 135 - prev[i - 1].power / 80, 6 * (i + 1), 135 - prev[i].power / 80, 0xffe0);
  }

  delay(20);
  
  if (millis() - lastClear > 10000 || millis() < lastClear) {
    tft.fillScreen(0);
    lastClear = millis();
  }
}