#include <Adafruit_GFX.h>  // Core graphics library
#include <Adafruit_INA219.h>
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <deque>
#include <Fonts/FreeSansBold24pt7b.h>
#include <SPI.h>

// Pin numbers
constexpr static uint8_t BUTTON_PIN {0};

// Definitions
constexpr static uint16_t displayWidth {240};
constexpr static uint16_t displayHeight {135};

Adafruit_INA219 ina219;
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16     canvas(displayWidth, displayHeight);  // Display resolution: displayWidthxdisplayHeight

enum MODE : uint8_t {
	Current,
	Voltage,
	Power,
	Detailed,
};

struct colors {
	uint16_t bgColor;
	uint16_t fillColor;
	uint16_t labelColor;
	uint16_t valueColor;
};

// Configuration options
static uint8_t            mode {MODE::Current};  // Initial mode to start in
constexpr static uint32_t longPressTime {2000};  // Long press time (to switch to detailed mode)
constexpr static uint8_t  memorySize {80};       // Number of samples to keep
constexpr static uint32_t windowTime {5000};     // History window time

constexpr static uint16_t valueBaseline {50};   // Vertical coordinate of the value (0 - top)
constexpr static uint16_t labelBaseline {115};  // Vertical coordinate of the label (0 - top)

constexpr static colors
    currentColors {.bgColor = 0x2226, .fillColor = 0x3d6e, .labelColor = 0x2c4a, .valueColor = 0xbff9};
constexpr static colors
    voltageColors {.bgColor = 0x4144, .fillColor = 0xaa87, .labelColor = 0x89a5, .valueColor = 0xfeb8};
constexpr static colors
    powerColors {.bgColor = 0x4204, .fillColor = 0xad07, .labelColor = 0x8bc5, .valueColor = 0xff97};

// End of configuration options


struct measurement {
	float busVoltage;
	float current;
	float power;
	float shuntVoltage;
	float supplyVoltage;
};

struct textCoords {
	int16_t  x0, y0;
	int16_t  x1, y1;
	uint16_t w, h;
};

static std::deque<measurement> prev;
static uint32_t                startTime {0};
static uint32_t                buttonPressTime {0};
constexpr static uint8_t       divisionWidth {displayWidth / memorySize};
constexpr static uint32_t      stepTime {windowTime / memorySize};

textCoords getTextCoords(Adafruit_GFX& gfx, const char* string, int16_t x, int16_t y) {
	int16_t  x1, y1;
	uint16_t w, h;

	gfx.getTextBounds(string, x, y, &x1, &y1, &w, &h);

	return textCoords {x, y, x1, y1, w, h};
}

void drawCurrent(const measurement& max, const measurement& avg) {
	float scale = displayHeight / max.current;
	canvas.fillScreen(currentColors.bgColor);

	for (uint8_t i {0}; i < prev.size() - 1; ++i) {
		bool    up = prev[i].current < prev[i + 1].current;
		uint8_t minCoord = displayHeight - (up ? prev[i].current : prev[i + 1].current) * scale;

		if (max.current) {
			canvas.fillRect(divisionWidth * i, minCoord, divisionWidth, displayHeight, currentColors.fillColor);

			canvas.fillTriangle(
			    divisionWidth * i,
			    displayHeight - prev[i].current * scale,
			    divisionWidth * (i + 1),
			    displayHeight - prev[i + 1].current * scale,
			    up ? divisionWidth * (i + 1) : divisionWidth * i,
			    minCoord,
			    currentColors.fillColor
			);
		}
	}

	canvas.setFont(&FreeSansBold24pt7b);

	{
		char str[] = "Current";
		canvas.setCursor((displayWidth - getTextCoords(canvas, str, 0, labelBaseline).w) / 2, labelBaseline);
		canvas.setTextColor(currentColors.labelColor);
		canvas.print(str);
	}

	{
		char str[16] = {};

		if (avg.current < 100) {
			snprintf(str, 16, "%4.2fmA", avg.current);
		} else if (avg.current > 1000) {
			snprintf(str, 16, "%dA", static_cast<unsigned>(avg.current / 1000));
		} else {
			snprintf(str, 16, "%dmA", static_cast<int>(avg.current));
		}

		canvas.setCursor((displayWidth - getTextCoords(canvas, str, 0, valueBaseline).w) / 2, valueBaseline);
		canvas.setTextColor(currentColors.valueColor);
		canvas.print(str);
	}
}

void drawVoltage(const measurement& max, const measurement& avg) {
	float scale = displayHeight / max.busVoltage;
	canvas.fillScreen(voltageColors.bgColor);

	for (uint8_t i {0}; i < prev.size() - 1; ++i) {
		bool    up = prev[i].busVoltage < prev[i + 1].busVoltage;
		uint8_t minCoord = displayHeight - (up ? prev[i].busVoltage : prev[i + 1].busVoltage) * scale;

		if (max.busVoltage) {
			canvas.fillRect(divisionWidth * i, minCoord, divisionWidth, displayHeight, voltageColors.fillColor);

			canvas.fillTriangle(
			    divisionWidth * i,
			    displayHeight - prev[i].busVoltage * scale,
			    divisionWidth * (i + 1),
			    displayHeight - prev[i + 1].busVoltage * scale,
			    up ? divisionWidth * (i + 1) : divisionWidth * i,
			    minCoord,
			    voltageColors.fillColor
			);
		}
	}

	canvas.setFont(&FreeSansBold24pt7b);

	{
		char str[] = "Voltage";
		canvas.setCursor((displayWidth - getTextCoords(canvas, str, 0, labelBaseline).w) / 2, labelBaseline);
		canvas.setTextColor(voltageColors.labelColor);
		canvas.print(str);
	}

	{
		char str[16] = {};

		if (avg.busVoltage < 0.1f) {
			snprintf(str, 16, "%4.2fmV", avg.busVoltage * 1000);
		} else if (avg.busVoltage < 1) {
			snprintf(str, 16, "%dmV", static_cast<unsigned>(avg.busVoltage * 1000));
		} else {
			snprintf(str, 16, "%5.2V", avg.busVoltage);
		}

		canvas.setCursor((displayWidth - getTextCoords(canvas, str, 0, valueBaseline).w) / 2, valueBaseline);
		canvas.setTextColor(voltageColors.valueColor);
		canvas.print(str);
	}
}

void drawPower(const measurement& max, const measurement& avg) {
	float scale = displayHeight / max.power;
	canvas.fillScreen(powerColors.bgColor);

	for (uint8_t i {0}; i < prev.size() - 1; ++i) {
		bool    up = prev[i].power < prev[i + 1].power;
		uint8_t minCoord = displayHeight - (up ? prev[i].power : prev[i + 1].power) * scale;

		if (max.power) {
			canvas.fillRect(divisionWidth * i, minCoord, divisionWidth, displayHeight, powerColors.fillColor);

			canvas.fillTriangle(
			    divisionWidth * i,
			    displayHeight - prev[i].power * scale,
			    divisionWidth * (i + 1),
			    displayHeight - prev[i + 1].power * scale,
			    up ? divisionWidth * (i + 1) : divisionWidth * i,
			    minCoord,
			    powerColors.fillColor
			);
		}
	}

	canvas.setFont(&FreeSansBold24pt7b);

	{
		char str[] = "Power";
		canvas.setCursor((displayWidth - getTextCoords(canvas, str, 0, labelBaseline).w) / 2, labelBaseline);
		canvas.setTextColor(powerColors.labelColor);
		canvas.print(str);
	}

	{
		char str[16] = {};

		if (avg.power < 100) {
			snprintf(str, 16, "%4.2fmW", avg.power);
		} else if (avg.power > 1000) {
			snprintf(str, 16, "%dW", static_cast<unsigned>(avg.power / 1000));
		} else {
			snprintf(str, 16, "%dmW", static_cast<int>(avg.power));
		}

		canvas.setCursor((displayWidth - getTextCoords(canvas, str, 0, valueBaseline).w) / 2, valueBaseline);
		canvas.setTextColor(powerColors.valueColor);
		canvas.print(str);
	}
}

void drawDetailed(const measurement& max, const measurement& avg) {
	auto last {prev.back()};

	canvas.setFont();
	canvas.fillScreen(0);

	canvas.setCursor(0, 0);
	canvas.setTextSize(3);
	canvas.setTextColor(0xffe0);
	canvas.print("Power meter");

	canvas.setTextSize(1);
	canvas.setTextColor(0xf800, 0);
	canvas.print("Bus:     ");
	canvas.print(last.busVoltage);
	canvas.print(" V. Avg: ");
	canvas.print(avg.busVoltage);
	canvas.println(" V    ");
	canvas.setTextColor(0x07e0, 0);
	canvas.print("Current: ");
	canvas.print(last.current);
	canvas.print(" mA. Avg: ");
	canvas.print(avg.current);
	canvas.println(" mA    ");
	canvas.setTextColor(0xffe0, 0);
	canvas.print("Power:   ");
	canvas.print(last.power);
	canvas.print(" mW. Avg: ");
	canvas.print(avg.power);
	canvas.println(" mW    ");
	canvas.setTextColor(0xffff, 0);
	canvas.print("Shunt:   ");
	canvas.print(last.shuntVoltage);
	canvas.print(" mV. Avg: ");
	canvas.print(avg.shuntVoltage);
	canvas.println(" mV    ");
	canvas.print("Load:    ");
	canvas.print(last.supplyVoltage);
	canvas.print(" V. Avg: ");
	canvas.print(avg.supplyVoltage);
	canvas.println(" V    ");

	float busVoltageScale = 70 / max.busVoltage;
	float currentScale = 70 / max.current;
	float powerScale = 70 / max.power;

	for (uint8_t i {0}; i < prev.size() - 1; ++i) {
		canvas.fillRect(divisionWidth * i, 64, divisionWidth, 71, 0);
		canvas.drawLine(
		    divisionWidth * i,
		    displayHeight - prev[i].busVoltage * busVoltageScale,
		    divisionWidth * (i + 1),
		    displayHeight - prev[i + 1].busVoltage * busVoltageScale,
		    0xf800
		);
		canvas.drawLine(
		    divisionWidth * i,
		    displayHeight - prev[i].current * currentScale,
		    divisionWidth * (i + 1),
		    displayHeight - prev[i + 1].current * currentScale,
		    0x07e0
		);
		canvas.drawLine(
		    divisionWidth * i,
		    displayHeight - prev[i].power * powerScale,
		    divisionWidth * (i + 1),
		    displayHeight - prev[i + 1].power * powerScale,
		    0xffe0
		);
	}
}

void (*drawFunctions[])(const measurement& max, const measurement& avg) =
    {drawCurrent, drawVoltage, drawPower, drawDetailed};

void Button_Handler() {
	if (!digitalRead(BUTTON_PIN)) {
		buttonPressTime = millis();
	} else if (digitalRead(BUTTON_PIN)) {
		if (mode < 3 || millis() - buttonPressTime < longPressTime) {
			++mode;
			if (mode > 2) {
				mode = 0;
			}
		}
	}
}

void setup() {
	Serial.begin(115200);

	pinMode(TFT_BACKLITE, OUTPUT);
	digitalWrite(TFT_BACKLITE, HIGH);
	pinMode(TFT_I2C_POWER, OUTPUT);
	digitalWrite(TFT_I2C_POWER, HIGH);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	pinMode(LED_BUILTIN, OUTPUT);

	attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), Button_Handler, CHANGE);

	tft.init(displayHeight, displayWidth);
	tft.setRotation(3);
	tft.fillScreen(0);
	tft.setTextWrap(true);

	if (!ina219.begin()) {
		canvas.setFont(&FreeSansBold24pt7b);
		canvas.setCursor(16, labelBaseline);
		tft.setTextColor(0xf800);
		tft.setTextSize(2);
		tft.print("INA219 not connected");

		digitalWrite(LED_BUILTIN, HIGH);

		while (1) {
			delay(1);
		}
	}

	tft.setTextSize(1);
}

void loop() {
	if (!digitalRead(BUTTON_PIN) && millis() - buttonPressTime > longPressTime) {
		mode = 3;
	}

	measurement curr {
	  .busVoltage = ina219.getBusVoltage_V(),
	  .current = ina219.getCurrent_mA(),
	  .shuntVoltage = ina219.getShuntVoltage_mV()
	};

	for (unsigned i {0}; millis() - startTime < stepTime; ++i) {
		curr.busVoltage += (ina219.getBusVoltage_V() - curr.busVoltage) / (i + 1);
		curr.current += (ina219.getCurrent_mA() - curr.current) / (i + 1);
		curr.shuntVoltage += (ina219.getShuntVoltage_mV() - curr.shuntVoltage) / (i + 1);
	};
	startTime = millis();

	curr.supplyVoltage = curr.busVoltage + (curr.shuntVoltage / 1000.0f);
	curr.power = curr.busVoltage * curr.current * 1000.0f;

	if (curr.busVoltage < 0.05f) {
		curr.busVoltage = 0;
	}
	if (curr.current < 0.5f) {
		curr.current = 0;
	}
	if (curr.power < 0.5f) {
		curr.power = 0;
	}
	if (curr.shuntVoltage < 0.02f) {
		curr.shuntVoltage = 0;
	}
	if (curr.supplyVoltage < 0.05f) {
		curr.supplyVoltage = 0;
	}

	char str[128];
	snprintf(
	    str,
	    128,
	    "Time: %lu, Current: %5.2f, Voltage: %6.3f, Power: %6.3f, Shunt voltage: %6.3f, Load voltage: %6.3f\n",
	    millis(),
	    curr.current,
	    curr.busVoltage,
	    curr.power,
	    curr.shuntVoltage,
	    curr.supplyVoltage
	);
	Serial.print(str);

	prev.push_back(curr);
	if (prev.size() > memorySize + 1) {
		prev.pop_front();
	}

	measurement max {};
	measurement avg {};

	for (uint8_t i {0}; i < prev.size(); ++i) {
		if (prev[i].busVoltage > max.busVoltage) {
			max.busVoltage = prev[i].busVoltage;
		}
		if (prev[i].current > max.current) {
			max.current = prev[i].current;
		}
		if (prev[i].power > max.power) {
			max.power = prev[i].power;
		}
		if (prev[i].shuntVoltage > max.shuntVoltage) {
			max.shuntVoltage = prev[i].shuntVoltage;
		}
		if (prev[i].supplyVoltage > max.supplyVoltage) {
			max.supplyVoltage = prev[i].supplyVoltage;
		}

		avg.busVoltage += (prev[i].busVoltage - avg.busVoltage) / (i + 1);
		avg.current += (prev[i].current - avg.current) / (i + 1);
		avg.power += (prev[i].power - avg.power) / (i + 1);
		avg.shuntVoltage += (prev[i].shuntVoltage - avg.shuntVoltage) / (i + 1);
		avg.supplyVoltage += (prev[i].supplyVoltage - avg.supplyVoltage) / (i + 1);
	}

	drawFunctions[mode](max, avg);
	tft.drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height());
}
