#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <Adafruit_INA219.h>
#include <SPI.h>
#include <deque>

// Pin numbers
static constexpr uint8_t BUTTON {0};

Adafruit_INA219 ina219;
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
// Display resolution: 240x135


typedef struct {
  float busVoltage;
  float current;
  float power;
  float shuntVoltage;
  float loadVoltage;
} measurement;

typedef struct {
  uint16_t bgColor;
  uint16_t fillColor;
  uint16_t labelColor;
  uint16_t valueColor;
} colors;

enum MODE : uint8_t {
  Current,
  Voltage,
  Power,
  Detailed,
};


static std::deque<measurement> prev;
static uint8_t mode {MODE::Current};
static uint32_t lastClear {0};
static bool latch {false};
static constexpr uint8_t memorySize {80};
static constexpr uint8_t divisionWidth {240 / memorySize};
static constexpr colors currentColors {
  .bgColor = 0x2226,
  .fillColor = 0x3d6e,
  .labelColor = 0x2c4a,
  .valueColor = 0xbff9
};
static constexpr colors voltageColors {
  .bgColor = 0x4144,
  .fillColor = 0xaa87,
  .labelColor = 0x89a5,
  .valueColor = 0xfeb8
};
static constexpr colors powerColors {
  .bgColor = 0x4204,
  .fillColor = 0xad07,
  .labelColor = 0x8bc5,
  .valueColor = 0xff97
};


void setupCurrent() {
  tft.fillScreen(currentColors.bgColor);
}


void setupVoltage() {
  tft.fillScreen(voltageColors.bgColor);
}


void setupPower() {
  tft.fillScreen(powerColors.bgColor);
}


void setupDetailed() {
  tft.fillScreen(0);

  tft.setCursor(0, 0);
  tft.setTextSize(3);
  tft.setTextColor(0xffe0);
  tft.print("Power meter");

}


void drawCurrent(const measurement& max, const measurement& avg) {
  float scale = 135 / max.current;

  for (uint8_t i {0}; i < prev.size() - 1; ++i) {
    bool up = prev[i].current < prev[i + 1].current;
    uint8_t minCoord = 135 - (up? prev[i].current : prev[i + 1].current) * scale;

    tft.fillRect(divisionWidth * i, 0,
      divisionWidth, minCoord,
      currentColors.bgColor);

    if (max.current) {
      tft.fillRect(divisionWidth * i, minCoord,
        divisionWidth, 135, 
        currentColors.fillColor);
      
      tft.fillTriangle(divisionWidth * i, 135 - prev[i].current * scale, 
        divisionWidth * (i + 1), 135 - prev[i + 1].current * scale,
        up? divisionWidth * (i + 1) : divisionWidth * i, minCoord,
        currentColors.fillColor);
    }

    if ((i % 8) == 0) {
      tft.setTextSize(5);

      tft.setCursor(16, 80);
      tft.setTextColor(currentColors.labelColor);
      tft.print("Current");

      tft.setCursor(16, 20);
      tft.setTextColor(currentColors.valueColor);
      if (avg.current < 100) {
        tft.print(avg.current);
      } else {
        tft.print(static_cast<int>(avg.current));
      }
      tft.print("mA ");
    }
  }
}


void drawVoltage(const measurement& max, const measurement& avg) {
  float scale = 135 / max.busVoltage;

  for (uint8_t i {0}; i < prev.size() - 1; ++i) {
    bool up = prev[i].busVoltage < prev[i + 1].busVoltage;
    uint8_t minCoord = 135 - (up? prev[i].busVoltage : prev[i + 1].busVoltage) * scale;

    tft.fillRect(divisionWidth * i, 0,
      divisionWidth, minCoord,
      voltageColors.bgColor);

    if (max.busVoltage) {
      tft.fillRect(divisionWidth * i, minCoord,
        divisionWidth, 135, 
        voltageColors.fillColor);
      
      tft.fillTriangle(divisionWidth * i, 135 - prev[i].busVoltage * scale, 
        divisionWidth * (i + 1), 135 - prev[i + 1].busVoltage * scale,
        up? divisionWidth * (i + 1) : divisionWidth * i, minCoord,
        voltageColors.fillColor);
    }

    if ((i % 8) == 0) {
      tft.setTextSize(5);

      tft.setCursor(16, 80);
      tft.setTextColor(voltageColors.labelColor);
      tft.print("Voltage");

      tft.setCursor(16, 20);
      tft.setTextColor(voltageColors.valueColor);
      tft.print(avg.busVoltage); tft.print("V ");
    }
  }
}


void drawPower(const measurement& max, const measurement& avg) {
  float scale = 135 / max.power;

  for (uint8_t i {0}; i < prev.size() - 1; ++i) {
    bool up = prev[i].power < prev[i + 1].power;
    uint8_t minCoord = 135 - (up? prev[i].power : prev[i + 1].power) * scale;

    tft.fillRect(divisionWidth * i, 0,
      divisionWidth, minCoord,
      powerColors.bgColor);

    if (max.power) {
      tft.fillRect(divisionWidth * i, minCoord,
        divisionWidth, 135, 
        powerColors.fillColor);
      
      tft.fillTriangle(divisionWidth * i, 135 - prev[i].power * scale, 
        divisionWidth * (i + 1), 135 - prev[i + 1].power * scale,
        up? divisionWidth * (i + 1) : divisionWidth * i, minCoord,
        powerColors.fillColor);
    }

    if ((i % 8) == 0) {
      tft.setTextSize(5);

      tft.setCursor(16, 80);
      tft.setTextColor(powerColors.labelColor);
      tft.print("Power");

      tft.setCursor(16, 20);
      tft.setTextColor(powerColors.valueColor);
      if (avg.power < 100) {
        tft.print(avg.power);
      } else {
        tft.print(static_cast<int>(avg.power));
      }
      tft.print("mW ");
    }
  }
}


void drawDetailed(const measurement& max, const measurement& avg) {
  auto last {prev.back()};

  tft.setCursor(0, 24);
  tft.setTextSize(1);
  tft.setTextColor(0xf800, 0);
  tft.print("Bus:     "); tft.print(last.busVoltage); tft.print(" V. Avg: "); tft.print(avg.busVoltage); tft.println(" V    ");
  tft.setTextColor(0x07e0, 0);
  tft.print("Current: "); tft.print(last.current); tft.print(" mA. Avg: "); tft.print(avg.current); tft.println(" mA    ");
  tft.setTextColor(0xffe0, 0);
  tft.print("Power:   "); tft.print(last.power); tft.print(" mW. Avg: "); tft.print(avg.power); tft.println(" mW    ");
  tft.setTextColor(0xffff, 0);
  tft.print("Shunt:   "); tft.print(last.shuntVoltage); tft.print(" mV. Avg: "); tft.print(avg.shuntVoltage); tft.println(" mV    ");
  tft.print("Load:    "); tft.print(last.loadVoltage); tft.print(" V. Avg: "); tft.print(avg.loadVoltage); tft.println(" V    ");


  float busVoltageScale = 70 / max.busVoltage;
  float currentScale = 70 / max.current;
  float powerScale = 70 / max.power;

  for (uint8_t i {0}; i < prev.size() - 1; ++i) {
    tft.fillRect(divisionWidth * i, 64, divisionWidth, 71, 0);
    tft.drawLine(divisionWidth * i, 135 - prev[i].busVoltage * busVoltageScale,
      divisionWidth * (i + 1), 135 - prev[i + 1].busVoltage * busVoltageScale,
      0xf800);
    tft.drawLine(divisionWidth * i, 135 - prev[i].current * currentScale,
      divisionWidth * (i + 1), 135 - prev[i + 1].current * currentScale,
      0x07e0);
    tft.drawLine(divisionWidth * i, 135 - prev[i].power * powerScale,
      divisionWidth * (i + 1), 135 - prev[i + 1].power * powerScale,
      0xffe0);
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
  setupFunctions[mode]();
}

void loop() {
  if (!digitalRead(BUTTON) && !latch) {
    latch = true;
    ++mode;
    if (mode > 3) {
      mode = 0;
    }
    setupFunctions[mode]();
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
  if (curr.current < 0.5) {curr.current = 0;}
  if (curr.power < 0.5) {curr.power = 0;}
  if (curr.shuntVoltage < 0.02) {curr.shuntVoltage = 0;}
  if (curr.loadVoltage < 0.1) {curr.loadVoltage = 0;}
  Serial.println(curr.loadVoltage);

  prev.push_back(curr);
  if (prev.size() > memorySize + 1) {
    prev.pop_front();
  }
  
  measurement max {};
  measurement avg {};

  for (uint8_t i {0}; i < prev.size(); ++i) {
    if (prev[i].busVoltage > max.busVoltage) {max.busVoltage = prev[i].busVoltage;}
    if (prev[i].current > max.current) {max.current = prev[i].current;}
    if (prev[i].power > max.power) {max.power = prev[i].power;}
    if (prev[i].shuntVoltage > max.shuntVoltage) {max.shuntVoltage = prev[i].shuntVoltage;}
    if (prev[i].loadVoltage > max.loadVoltage) {max.loadVoltage = prev[i].loadVoltage;}

    avg.busVoltage += (prev[i].busVoltage - avg.busVoltage) / (i + 1);
    avg.current += (prev[i].current - avg.current) / (i + 1);
    avg.power += (prev[i].power - avg.power) / (i + 1);
    avg.shuntVoltage += (prev[i].shuntVoltage - avg.shuntVoltage) / (i + 1);
    avg.loadVoltage += (prev[i].loadVoltage - avg.loadVoltage) / (i + 1);
  }

  drawFunctions[mode](max, avg);
 
  delay(5);
  
  if (millis() - lastClear > 60000 || millis() < lastClear) {
    setupFunctions[mode]();
    lastClear = millis();
  }
}
