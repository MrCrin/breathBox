#include <FastLED.h>        //https://github.com/FastLED/FastLED
#include <FastLED_GFX.h>    //needs a link
#include <LEDMatrix.h>      //https://github.com/Jorgen-VikingGod/LEDMatrix
#include <Wire.h>

// Pins
#define ledControl          6
#define button              2
#define batteryMonitor      14
#define ldrSensor           15

// Change the next defines to match your matrix type and size
#define COLOR_ORDER         GRB
#define CHIPSET             WS2812B

// initial matrix layout (to get led strip index by x/y)
#define MATRIX_WIDTH        8
#define MATRIX_HEIGHT       8
#define MATRIX_TYPE         HORIZONTAL_MATRIX
#define MATRIX_SIZE         (MATRIX_WIDTH*MATRIX_HEIGHT)
#define NUMPIXELS           MATRIX_SIZE

//Light Level Globals
int lowBrightness = 50;
int mediumBrightness = 150;
int highBrightness = 255;
int masterBrightness = 150;
int breatheEdgeLight = masterBrightness/2;
int currentBrightness = masterBrightness;
int ambientLight = analogRead(ldrSensor);

//Stuff for gyroscope
int tilt = 0;
const int MPU_ADDR = 0x68;
int16_t accelerometer_x, accelerometer_y, accelerometer_z; // variables for accelerometer raw data
int16_t gyro_x, gyro_y, gyro_z; // variables for gyro raw data
int16_t temperature; // variables for temperature data
char tmp_str[7]; // temporary variable used in convert function
char* convert_int16_to_str(int16_t i) { // converts int16 to string. Moreover, resulting strings will have the same length in the debug monitor.
    sprintf(tmp_str, "%6d", i);
    return tmp_str;
}

//Startup Config
int mode = 0;
uint8_t rotatingHue = 0;

//Button stuff
//unsigned long timePressed = 0;
int buttonState = 0;
int buttonLatch = 0;

//Standby timer stuff
unsigned long waitBeganAt = 0;
unsigned long waitCountdownBegun = 0;
const unsigned long standbyDelay = 30000;

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

int ambientCheck(){
    ambientLight = analogRead(ldrSensor);
    if (ambientLight <= 200) //Low ambient light
    {
        masterBrightness = lowBrightness;
        Serial.print("Master Brightness set to low - ambient light is ");
        Serial.println(ambientLight);
    } else if (ambientLight > 200 && ambientLight <= 450) //Medium ambient light
    {
        masterBrightness = mediumBrightness;
        Serial.print("Master Brightness set to medium - ambient light is ");
        Serial.println(ambientLight);
    } else { //Bright light
        Serial.print("Master Brightness set to high - ambient light is ");
        Serial.println(ambientLight);
        masterBrightness = highBrightness;
    }
    return(ambientLight);
}

void buttonChange() {
    if (digitalRead(button)==HIGH)
    {
        //timePressed = millis();
        //waitCountdownBegun = 1;
        buttonState = 1;
    } else {
        //timePressed = 0;
        buttonState = 0;
    }
}

void setTilt() {
    if (accelerometer_x < 1000 && accelerometer_y < -8000) {
        tilt = 1;
    } else if (accelerometer_x < 1000 && accelerometer_y > 8000) {
        tilt = 2;
    } else if (accelerometer_x < -8000 && accelerometer_y < 1000) {
        tilt = 3;
    } else if (accelerometer_x > 8000 && accelerometer_y < 1000) {
        tilt = 4;
    } else {
        tilt = 0;
    }
}

int fadecurrentBrightness(int v){
    if (currentBrightness!=v)
    {
        if (currentBrightness>v) {
            for (int i = masterBrightness; i > v; i--) {
                if (buttonState == 1) {
                    break;
                }
                currentBrightness = i;
                FastLED.setBrightness(currentBrightness);
                FastLED.show();
                FastLED.delay(5);
            }
        } else {
            for (int i = masterBrightness; i < v; i++) {
                if (buttonState == 1) {
                    break;
                }
                currentBrightness = i;
                FastLED.setBrightness(currentBrightness);
                FastLED.show();
                FastLED.delay(5);
            }
        }
        currentBrightness = v;
    }
}

void returnToMasterBrightness(){
    if (currentBrightness!=masterBrightness)
    {
        while(currentBrightness<masterBrightness) { // Draws ≈ 175mA
            if (buttonState == 1) {
                break;
            }
            FastLED.setBrightness(currentBrightness);
            currentBrightness=currentBrightness+50;
            FastLED.show();
            FastLED.delay(20);
        }
        currentBrightness = masterBrightness;
        FastLED.setBrightness(currentBrightness);
    }
}

void breatheIn(int hue, int duration) { //Duration is in seconds
    FastLED.clear();
    matrix.DrawRectangle(0, 0, 7, 7, CHSV(hue, 255, breatheEdgeLight));
    returnToMasterBrightness();
    for (float i = 10; i < 255; i++) {
        if (buttonState == 1) {
            break;
        }
        matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, 255, i));
        FastLED.delay((duration * 1000) / 255);
        FastLED.show();
    }
}

void breatheOut(int hue, int duration) { //Duration is in seconds
    FastLED.clear();
    matrix.DrawRectangle(0, 0, 7, 7, CHSV(hue, 255, breatheEdgeLight));
    returnToMasterBrightness();
    for (float i = 255; i > 10; i--) {
        if (buttonState == 1) {
            break;
        }
        matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, 255, i));
        FastLED.delay((duration * 1000) / 255);
        FastLED.show();
    }
}

void holdIn(int hue, int duration) { //Duration is in seconds
    FastLED.clear();
    matrix.DrawRectangle(0, 0, 7, 7, CHSV(hue, 255, breatheEdgeLight));
    returnToMasterBrightness();
    for (float j = 0; j < duration; j++) {
        for (float i = 255; i > 190; i--) {
            if (buttonState == 1) {
                break;
            }
            matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, i, i));
            FastLED.show();
            FastLED.delay(4);
        }
        for (float i = 190; i < 255; i++) {
            if (buttonState == 1) {
                break;
            }
            matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, i, i));
            FastLED.show();
            FastLED.delay(4);
        }
    }
}

void holdOut(int hue, int duration) { //Duration is in seconds
    FastLED.clear();
    matrix.DrawRectangle(0, 0, 7, 7, CHSV(hue, 255, breatheEdgeLight));
    returnToMasterBrightness();
    for (float j = 0; j < duration; j++) {
        for (float i = 0; i < 65; i++) {
            if (buttonState == 1) {
                break;
            }
            matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, 255, i));
            FastLED.show();
            FastLED.delay(4);
        }
        for (float i = 65; i > 1; i--) {
            if (buttonState == 1) {
                break;
            }
            matrix.DrawFilledRectangle(1, 1, 6, 6, CHSV(hue, 255, i));
            FastLED.show();
            FastLED.delay(4);
        }
    }
}

void standby(){ //Draws ≈ 18mA draw at peak
    fadecurrentBrightness(0);
    FastLED.clear();
    FastLED.delay(10);
}

void transitionToStartWait(){
    while(currentBrightness>0) { // Draws ≈ 175mA
        if (buttonState == 1) {
            break;
        }
        FastLED.setBrightness(currentBrightness);
        currentBrightness=currentBrightness-(masterBrightness/5);
        FastLED.show();
        FastLED.delay(20);
    }
    FastLED.clear();
    currentBrightness = 0;
    FastLED.setBrightness(currentBrightness);
    FastLED.show();
    while(currentBrightness<masterBrightness) { // Draws ≈ 175mA
        if (buttonState == 1) {
            break;
        }
        FastLED.setBrightness(currentBrightness);
        matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(255, 0, 100));
        currentBrightness=currentBrightness+(masterBrightness/5);
        FastLED.show();
        FastLED.delay(20);
    }
    currentBrightness = masterBrightness;
    FastLED.setBrightness(currentBrightness);
}

void startWait() { //Draws ≈ 90mA draw at peak
    if  (waitCountdownBegun == 0) {
        waitBeganAt = millis();
        waitCountdownBegun = 1;
    }
    if ((millis() - waitBeganAt < standbyDelay)) {
        Serial.println("Waiting...");
        FastLED.clear();
        int x = random(2, 6);
        int y = random(2, 6);
        int c = random(255);
        if((x==3&&y==3)||(x==3&&y==4)||(x==4&&y==3)||(x==4&&y==4)) {
            //Do nothing - get another set of coordinates
        } else {
            for (float i = 0; i < 255; i++) {
                if (buttonState == 1) {
                    break;
                }
                matrix.DrawPixel(x, y, CHSV(c, 255, i));
                matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(255, 0, 100));
                FastLED.delay(10);
                FastLED.show();
            }
            for (float i = 255; i > 0; i--) {
                if (buttonState == 1) {
                    break;
                }
                matrix.DrawPixel(x, y, CHSV(c, 255, i));
                matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(255, 0, 100));
                FastLED.delay(10);
                FastLED.show();
            }
        }
    } else {
        Serial.println("Switching to standby");
        waitCountdownBegun = 0;
        mode = 6;
    }
}

void box(int hue, int cycles) { // Draws ≈ 310mA at peak
    Serial.println("Box sequence started");
    int cyclesComplete = 0;
    for (int i = 0; i <= cycles; i++) {
        if (buttonState == 1) {
            break;
        }
        Serial.print(">> Starting breath cycle ");
        Serial.println(cyclesComplete+1);
        breatheIn(hue, 4);
        holdIn(hue, 4);
        breatheOut(hue, 4);
        holdOut(hue, 4);
        cyclesComplete = i;
    }
    if (cyclesComplete==cycles)
    {
        mode = 0;
        transitionToStartWait();
        Serial.println("Box sequence complete");
    }
}

void triangle(int hue, int cycles) { // Draws ≈ 340mA at peak
    Serial.println("Triangle sequence started");
    int cyclesComplete = 0;
    for (int i = 0; i <= cycles; i++) {
        if (buttonState == 1) {
            break;
        }
        Serial.print(">> Starting breath cycle ");
        Serial.println(cyclesComplete+1);
        breatheIn(hue, 4);
        holdIn(hue, 4);
        breatheOut(hue, 4);
        cyclesComplete = i;
    }
    if (cyclesComplete==cycles)
    {
        mode = 0;
        transitionToStartWait();
        Serial.println("Triangle sequence complete");
    }
}

void relax(int hue, int cycles) { // Draws ≈ 340mA at peak
    Serial.println("Relax sequence started");
    int cyclesComplete = 0;
    for (int i = 0; i <= cycles; i++) {
        if (buttonState == 1) {
            break;
        }
        Serial.print(">> Starting breath cycle ");
        Serial.println(cyclesComplete+1);
        breatheIn(hue, 4);
        holdIn(hue, 7);
        breatheOut(hue, 8);
        cyclesComplete = i;
    }
    if (cyclesComplete==cycles)
    {
        mode = 0;
        transitionToStartWait();
        Serial.println("Relax sequence complete");
    }
}

void ujjayiPranayama(int hue, int cycles) { // Draws ≈ 330mA at peak
    Serial.println("Ujjayi sequence started");
    int cyclesComplete = 0;
    for (int i = 0; i <= cycles; i++) {
        if (buttonState == 1) {
            break;
        }
        Serial.print(">> Starting breath cycle ");
        Serial.println(cyclesComplete+1);
        breatheIn(hue, 4);
        breatheOut(hue, 4);
        cyclesComplete = i;
    }
    if (cyclesComplete==cycles)
    {
        mode = 0;
        transitionToStartWait();
        Serial.println("Ujjayi sequence complete");
    }
}

void sensory(int duration){ //Duration is in seconds // Draws ≈ 96mA at peak
    long d = duration*1000;
    if  (waitCountdownBegun == 0) {
        waitBeganAt = millis();
        waitCountdownBegun = 1;
        Serial.println("Sensory sequence started");
    }
    if ((millis() - waitBeganAt) < d) {
        FastLED.clear();
        returnToMasterBrightness();
        for (float i = 0; i < 255; i++) {
            if (buttonState == 1) {
                break;
            }
            matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(i, 255, 255));
            FastLED.delay(20);
            FastLED.show();
        }
    } else {
        mode = 0;
        transitionToStartWait();
        Serial.println("Sensory mode complete");
        waitCountdownBegun = 0;
    }
}

void setup()
{
    //Serial initial
    Serial.println("Hello world!");
    Serial.begin(9600);
    Serial.println("Serial monitor started");
    // LED initial
    FastLED.addLeds<CHIPSET, ledControl, COLOR_ORDER>(matrix[0], matrix.Size()).setCorrection(TypicalSMD5050);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(masterBrightness);
    FastLED.clear(true);
    //IO intial
    pinMode(ledControl, OUTPUT);
    pinMode(button, INPUT);
    attachInterrupt(digitalPinToInterrupt(button), buttonChange, CHANGE);
    //Battery monitor initialisation
    pinMode(batteryMonitor, INPUT);
    pinMode(ldrSensor, INPUT);
    batteryCheck();
    FastLED.delay(50);
    ambientCheck();
    //Gyro initial
    // Wire.begin();
    // Wire.beginTransmission(MPU_ADDR); // Begins a transmission to the I2C slave (GY-521 board)
    // Wire.write(0x6B); // PWR_MGMT_1 register
    // Wire.write(0); // set to zero (wakes up the MPU-6050)
    // Wire.endTransmission(true);
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
}

void loop() {
    // //Gyro test stuff
    // Wire.beginTransmission(MPU_ADDR);
    // Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H) [MPU-6000 and MPU-6050 Register Map and Descriptions Revision 4.2, p.40]
    // Wire.endTransmission(false); // the parameter indicates that the Arduino will send a restart. As a result, the connection is kept active.
    // Wire.requestFrom(MPU_ADDR, 7*2, true); // request a total of 7*2=14 registers
    // setTilt();
    // // "Wire.read()<<8 | Wire.read();" means two registers are read and stored in the same variable
    // accelerometer_x = Wire.read()<<8 | Wire.read(); // reading registers: 0x3B (ACCEL_XOUT_H) and 0x3C (ACCEL_XOUT_L)
    // accelerometer_y = Wire.read()<<8 | Wire.read(); // reading registers: 0x3D (ACCEL_YOUT_H) and 0x3E (ACCEL_YOUT_L)
    // accelerometer_z = Wire.read()<<8 | Wire.read(); // reading registers: 0x3F (ACCEL_ZOUT_H) and 0x40 (ACCEL_ZOUT_L)
    // temperature = Wire.read()<<8 | Wire.read(); // reading registers: 0x41 (TEMP_OUT_H) and 0x42 (TEMP_OUT_L)
    // gyro_x = Wire.read()<<8 | Wire.read(); // reading registers: 0x43 (GYRO_XOUT_H) and 0x44 (GYRO_XOUT_L)
    // gyro_y = Wire.read()<<8 | Wire.read(); // reading registers: 0x45 (GYRO_YOUT_H) and 0x46 (GYRO_YOUT_L)
    // gyro_z = Wire.read()<<8 | Wire.read(); // reading registers: 0x47 (GYRO_ZOUT_H) and 0x48 (GYRO_ZOUT_L)
    ambientCheck();//Check and adjust masterBrightness
    if (battPerc > 10) {
        if (buttonState==1) {
            buttonLatch=1;
            while(currentBrightness>0) { // Draws ≈ 175mA
                if (buttonState == 0) {
                    break;
                }
                FastLED.setBrightness(currentBrightness);
                FastLED.show();
                currentBrightness=currentBrightness-(masterBrightness/5);
                FastLED.delay(20);
            }
            FastLED.clear();
            currentBrightness = 0;
            FastLED.setBrightness(currentBrightness);
            FastLED.show();
            while(currentBrightness<masterBrightness) { // Draws ≈ 175mA
                if (buttonState == 0) {
                    break;
                }
                FastLED.setBrightness(currentBrightness);
                matrix.DrawRectangle(0, 0, 7, 7, CHSV(255, 0, 100));
                currentBrightness=currentBrightness+(masterBrightness/5);
                FastLED.show();
                FastLED.delay(20);
            }
            currentBrightness = masterBrightness;
            FastLED.setBrightness(currentBrightness);
            while(buttonState==1) { // Draws ≈ 175mA
                matrix.DrawRectangle(0, 0, 7, 7, CHSV(255, 0, 100));
                FastLED.show();
                FastLED.delay(20);
            }
            currentBrightness = masterBrightness;
            FastLED.setBrightness(currentBrightness);
        } else {
            if (buttonLatch==1) {
                waitCountdownBegun = 0;
                FastLED.setBrightness(currentBrightness);
                if (mode==6)
                {
                    Serial.println("Woken up from standby");
                    mode = 0;
                }
                else if (mode < 5) {
                    mode++;
                    Serial.println("Moved forward one mode");
                } else {
                    mode = 1;
                    Serial.println("Moving back to start of modes");
                }
                buttonLatch=0;
            }
            switch (mode) {
            case 0:
                startWait();
                break;
            case 1:
                box(120, 4);
                break;
            case 2:
                triangle(200, 4);
                break;
            case 3:
                relax(50, 4);
                break;
            case 4:
                ujjayiPranayama(15, 4);
                break;
            case 5:
                sensory(1800);
                break;
            case 6:
                standby();
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
    // switch(tilt) {
    // case 1:
    //     Serial.println("Left");
    //     matrix.DrawLine(7, 7, 7, 7, CHSV(0, 255, 50));
    //     FastLED.show();
    //     break;
    // case 2:
    //     Serial.println("Right");
    //     FastLED.clear();
    //     break;
    // case 3:
    //     Serial.println("Forward");
    //     FastLED.clear();
    //     break;
    // case 4:
    //     Serial.println("Backward");
    //     FastLED.clear();
    //     break;
    // case 0:
    //     Serial.println("Flat");
    //     FastLED.clear();
    //     break;
    // }
}
