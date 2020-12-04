#include <FastLED.h>        //https://github.com/FastLED/FastLED
#include <FastLED_GFX.h>
#include <LEDMatrix.h>      //https://github.com/Jorgen-VikingGod/LEDMatrix

// Change the next defines to match your matrix type and size
#define ledControl          6
#define button              10
#define batteryMonitor      14

#define COLOR_ORDER         GRB
#define CHIPSET             WS2812B

// initial matrix layout (to get led strip index by x/y)
#define MATRIX_WIDTH        8
#define MATRIX_HEIGHT       8
#define MATRIX_TYPE         HORIZONTAL_MATRIX
#define MATRIX_SIZE         (MATRIX_WIDTH*MATRIX_HEIGHT)
#define NUMPIXELS           MATRIX_SIZE

//Light Level Globals
#define breatheEdgeLight 120

//Startup Config
int mode = 0;
uint8_t rotatingHue = 0;

// create our matrix based on matrix definition
cLEDMatrix<MATRIX_WIDTH, MATRIX_HEIGHT, MATRIX_TYPE> matrix;
//CRGB leds[64];

//Battery stats
int battPerc;
int battHue = 0;

int batteryCheck() {
    int value = analogRead(batteryMonitor);
    float voltage = (value * 4.8) / 1023;
    int perc  = map(value, 700, 1023, 0, 100);
    Serial.print("Battery is at ");
    Serial.print(perc);
    Serial.println("%");
    Serial.print("Value is at ");
    Serial.print(value);
    Serial.println("");
    Serial.print("Voltage is at ");
    Serial.print(voltage);
    Serial.println("V");
    battPerc = perc;
}

void buttonPress() {
    Serial.println("Button pressed");
    if (mode < 4) {
        mode++;
    } else {
        mode = 1;
    }
    FastLED.delay(200);
    batteryCheck();
}

void breatheIn(int hue, int duration) {
    matrix.DrawRectangle(0, 0, 7, 7, CHSV(hue, 255, breatheEdgeLight));
    for (float i = 10; i < 255; i++) {
        if (digitalRead(button) == HIGH) {
            break;
        }
        matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, 255, i));
        FastLED.delay((duration * 1000) / 255);
        FastLED.show();
    }
}

void breatheOut(int hue, int duration) {
    matrix.DrawRectangle(0, 0, 7, 7, CHSV(hue, 255, breatheEdgeLight));
    for (float i = 255; i > 10; i--) {
        if (digitalRead(button) == HIGH) {
            break;
        }
        matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, 255, i));
        FastLED.delay((duration * 1000) / 255);
        FastLED.show();
    }
}

void holdIn(int hue, int duration) {
    matrix.DrawRectangle(0, 0, 7, 7, CHSV(hue, 255, breatheEdgeLight));
    for (float j = 0; j < duration; j++) {
        for (float i = 255; i > 190; i--) {
            if (digitalRead(button) == HIGH) {
                break;
            }
            matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, i, i));
            FastLED.show();
            FastLED.delay(4);
        }
        for (float i = 190; i < 255; i++) {
            if (digitalRead(button) == HIGH) {
                break;
            }
            matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, i, i));
            FastLED.show();
            FastLED.delay(4);
        }
    }
}

void holdOut(int hue, int duration) {
    FastLED.clear();
    matrix.DrawRectangle(0, 0, 7, 7, CHSV(hue, 255, breatheEdgeLight));
    for (float j = 0; j < duration; j++) {
        for (float i = 0; i < 65; i++) {
            if (digitalRead(button) == HIGH) {
                break;
            }
            matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, 255, i));
            FastLED.show();
            FastLED.delay(4);
        }
        for (float i = 65; i > 1; i--) {
            if (digitalRead(button) == HIGH) {
                break;
            }
            matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, 255, i));
            FastLED.show();
            FastLED.delay(4);
        }
    }
}

void startWait() {
    FastLED.clear();
    int x = random(2, 5);
    int y = random(2, 5);
    int c = random(255);
    if((x==3&&y==3)||(x==3&&y==4)||(x==4&&y==3)||(x==4&&y==4)) {
        //Do nothing - get another set of coordinates
    } else {
        for (float i = 0; i < 255; i++) {
            if (digitalRead(button) == HIGH) {
                break;
            }
            matrix.DrawPixel(x, y, CHSV(c, 255, i));
            matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(255, 0, 100));
            FastLED.delay(1);
            FastLED.show();
        }
        for (float i = 255; i > 0; i--) {
            if (digitalRead(button) == HIGH) {
                break;
            }
            matrix.DrawPixel(x, y, CHSV(c, 255, i));
            matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(255, 0, 100));
            FastLED.delay(1);
            FastLED.show();
        }
    }
}

void box(int hue) {
    Serial.println("Box sequence started");
    breatheIn(hue, 4);
    holdIn(hue, 4);
    breatheOut(hue, 4);
    holdOut(hue, 4);
}

void triangle(int hue) {
    Serial.println("Triangle sequence started");
    breatheIn(hue, 4);
    holdIn(hue, 4);
    breatheOut(hue, 4);
}

void relax(int hue) {
    Serial.println("Relax sequence started");
    breatheIn(hue, 4);
    holdIn(hue, 7);
    breatheOut(hue, 8);
}

void ujjayiPranayama(int hue) {
    Serial.println("Ujjayi sequence started");
    breatheIn(hue, 4);
    holdIn(hue, 7);
    breatheOut(hue, 8);
}

void setup()
{
    batteryCheck();
    Serial.begin(9600);
    Serial.println("Serial monitor started");
    // initial LEDs
    FastLED.addLeds<CHIPSET, ledControl, COLOR_ORDER>(matrix[0], matrix.Size()).setCorrection(TypicalSMD5050);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(255);
    FastLED.clear(true);
    pinMode(ledControl, OUTPUT);
    pinMode(button, INPUT);
    pinMode(batteryMonitor, INPUT);

    if (battPerc > 50) {
        battHue = 86;
    } else if (battPerc > 25) {
        battHue = 43;
    } else if (battPerc > 15) {
        battHue = 12;
    }
    for (int i = 1; i < 8; i++) {
        matrix.DrawLine(0, 0, i, i, CHSV(battHue, 255, 255));
        FastLED.delay(30);
        FastLED.show();
    }
    for (int i = 1; i < 8; i++) {
        matrix.DrawLine(0, 0, i, i, CHSV(battHue, 255, 0));
        FastLED.delay(30);
        FastLED.show();
    }
    FastLED.delay(200);
}

void loop()
{
    if (battPerc > 10) {
        if (digitalRead(button) == HIGH) {
            buttonPress();
        } else {
            switch (mode) {
            case 0:
                startWait();
                break;
            case 1:
                box(120);
                break;
            case 2:
                triangle(200);
                break;
            case 3:
                relax(50);
                break;
            case 4:
                ujjayiPranayama(15);
                break;
            }
        }
    } else {
        FastLED.clear();
        matrix.DrawLine(7, 7, 7, 7, CHSV(0, 255, 50));
        FastLED.show();
        FastLED.delay(1000);
        FastLED.clear();
        FastLED.delay(1000);
    }
}