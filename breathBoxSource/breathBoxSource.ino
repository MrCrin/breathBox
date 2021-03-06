#include <SPI.h>

#include <FastLED.h>        //https://github.com/FastLED/FastLED
#include <FastLED_GFX.h>    //needs a link
#include <LEDMatrix.h>      //https://github.com/Jorgen-VikingGod/LEDMatrix
#include <Wire.h>           //I2C Interfacing
#include <avr/wdt.h>        //Watchdog for MPU hangs
#include <avr/sleep.h>      //For low power sleep mode
#include <MPU6050.h>        //MPU6050 Library

// Pins
#define ledControl          7
#define button              2
#define batteryMonitor      0
#define ldrSensor           15
#define moveInterrupt       3
#define debug               5

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
int masterBrightness = 255;
int breatheEdgeLight = masterBrightness/2;
int currentBrightness = masterBrightness;
int ambientLight = analogRead(ldrSensor);

//Stuff for movement
int tilt = 0;
int pitch, roll;  // variables for accelerometer raw data
volatile bool movement = false;
unsigned long newMovementTime = 0;
const unsigned long movementTimeout = 3000;
int currentMovement = 0;
MPU6050 mpu;

//Sand
int sandX = 3;
int sandY = 3;

//Startup Config
int mode = 0;
uint8_t rotatingHue = 0;

//Button stuff
//unsigned long timePressed = 0;
int buttonState = 0;
volatile int buttonLatch = 0;

//Standby timer stuff
unsigned long waitBeganAt = 0;
unsigned long waitCountdownBegun = 0;
const unsigned long standbyDelay = 10000;
unsigned long workmateTimeSinceReset = 0;
unsigned long workmateDuration = 60000;

// create our matrix based on matrix definition
cLEDMatrix<MATRIX_WIDTH, MATRIX_HEIGHT, MATRIX_TYPE> matrix;
CRGB *leds = matrix[0];

//Battery stats
int battPerc;
int battHue = 0;

int batteryCheck() {
    int value = analogRead(batteryMonitor);
    float voltage = (value * 4.90) / 1023;
    int perc  = map(value, 630, 818, 0, 100);
    // Serial.print("Battery is at ");
    // Serial.print(perc);
    // Serial.println("%");
    // Serial.print("Value is at ");
    // Serial.print(value);
    // Serial.println("");
    // Serial.print("Voltage is at ");
    // Serial.print(voltage);
    // Serial.println("V");
    battPerc = perc;
    if (battPerc > 50) {
        battHue = 86;
    } else if (battPerc > 25) {
        battHue = 43;
    } else if (battPerc > 15) {
        battHue = 12;
    }
}

int ambientCheck(){
    ambientLight = analogRead(ldrSensor);
    if (ambientLight <= 200) //Low ambient light
    {
        masterBrightness = lowBrightness;
        Serial.print("*Master Brightness set to low - ambient light is ");
        Serial.println(ambientLight);
    } else if (ambientLight > 200 && ambientLight <= 450) //Medium ambient light
    {
        masterBrightness = mediumBrightness;
        Serial.print("*Master Brightness set to medium - ambient light is ");
        Serial.println(ambientLight);
    } else { //Bright light
        Serial.print("*Master Brightness set to high - ambient light is ");
        Serial.println(ambientLight);
        masterBrightness = highBrightness;
    }
    FastLED.setBrightness(masterBrightness);
    return(ambientLight);
}

void ISR_buttonChange() {
    if (digitalRead(button)==HIGH)
    {
        buttonState = 1;
    } else {
        buttonState = 0;
    }
}

void ISR_checkMove() {
    //sleep_disable();
    if (currentMovement == 1) {
        movement = true;
    } else {
        movement = false;
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
            ambientCheck();
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
    Serial.print("Sleeping...");
    while(checkCurrentMovement() == 0 && buttonState == 0 && (millis() - workmateTimeSinceReset < workmateDuration)){
        delay(10);
    }
    Serial.println(" waking up");
    delay(10);
    if ((millis() - workmateTimeSinceReset > workmateDuration)) {
        mode=8;
    } else if (buttonState==1){
        onWake();
        mode=1;
    } else {
        onWake();
        mode=0;
    }
    // sleep_enable();//Enabling sleep mode
    // set_sleep_mode(SLEEP_MODE_PWR_DOWN);//Setting the sleep mode, in our case full sleep
    // sleep_cpu();//activating sleep mode
    // // <<<<<Stops here until interrupt
}

void workmateNotify(){
    int r = 45;
    int d = 1;
    int b = 255;
    int h;
    int c = 0;
    int fadeDelay = 25;
    int preFade = 10;
    ambientCheck();
    currentBrightness = masterBrightness;
    FastLED.setBrightness(currentBrightness);
    FastLED.clear();
    h=random(255);
    while(c<5){
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;}
    for(int x=0; x<8; x++){
        checkCurrentMovement();
    if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;}            
       for (int i = 0; i < b; i+=r)
        {
        matrix.DrawRectangle(x, 0, x, 7, CHSV(h, 50, i));
        FastLED.show();
        FastLED.delay(d);
        }
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
        for (int i = 0; i<64; i++) {
   
            leds[i].fadeToBlackBy(preFade);
        }
    }
    while(leds[63]) {
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
        for (int i = 0; i<64; i++) {
            leds[i].fadeToBlackBy(1);
        }
        FastLED.delay(fadeDelay);
        FastLED.show();
    }
    FastLED.delay(2000);
    h=random(255);
    for(int y=0; y<8; y++){
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
       for (int i = 0; i < b; i+=r)
        {
        matrix.DrawRectangle(0, y, 7, y, CHSV(h, 50, i));
        FastLED.show();
        FastLED.delay(d);
        }
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
        for (int i = 0; i<64; i++) {
   
            leds[i].fadeToBlackBy(preFade);
        }
    }
    while(leds[63]) {
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
        for (int i = 0; i<64; i++) {
   
            leds[i].fadeToBlackBy(1);
        }
        FastLED.delay(fadeDelay);
        FastLED.show();
    }
    FastLED.delay(2000);
    h=random(255);
    for(int x=7; x>=0; x--){
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
       for (int i = 0; i < b; i+=r)
        {
   
        matrix.DrawRectangle(x, 0, x, 7, CHSV(h, 50, i));
        FastLED.show();
        FastLED.delay(d);
        } 
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
        for (int i = 0; i<64; i++) {
   
            leds[i].fadeToBlackBy(preFade);
        }
    }
    while(leds[0]) {
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
        for (int i = 0; i<64; i++) {
   
            leds[i].fadeToBlackBy(1);
        }
        FastLED.delay(fadeDelay);
        FastLED.show();
    }
    FastLED.delay(2000);
    h=random(255);
    for(int y=7; y>=0; y--){
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
       for (int i = 0; i < b; i+=r)
        {
   
        matrix.DrawRectangle(0, y, 7, y, CHSV(h, 50, i));
        FastLED.show();
        FastLED.delay(d);
        } 
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
        for (int i = 0; i<64; i++) {
   
            leds[i].fadeToBlackBy(preFade);
        }
    }
    while(leds[0]) {
        checkCurrentMovement();
        if(currentMovement == 1 || buttonState == 1){mode=0; workmateTimeSinceReset=millis(); goto workmateNotifyBailout;} 
        for (int i = 0; i<64; i++) {
   
            leds[i].fadeToBlackBy(1);
        }
        FastLED.delay(fadeDelay);
        FastLED.show();
    }
    c++;
    FastLED.delay(5000);
}
workmateNotifyBailout:
mode=7;
workmateTimeSinceReset=millis();
for(int i=0; i<255; i++){
for (int i = 0; i<64; i++) {
            leds[i].fadeToBlackBy(1);
        }
        FastLED.delay(5);
    }
    onWake();
}

void onWake(){
    int r = 25;
    int d = 5;
    int b = 250;
    ambientCheck();
    batteryCheck();
    currentBrightness = masterBrightness;
    FastLED.setBrightness(currentBrightness);
    FastLED.clear();
    for (int i = 0; i < b; i+=r)
    {
        matrix.DrawRectangle(3, 3, 4, 4, CHSV(battHue, 100, i));
        FastLED.show();
        FastLED.delay(d);
    }
    for (int i = 0; i < b-25; i+=r)
    {
        matrix.DrawRectangle(2, 2, 5, 5, CHSV(battHue, 75, i));
        FastLED.show();
        FastLED.delay(d);
    }
    for (int i = 0; i < b-150; i+=r)
    {
        matrix.DrawRectangle(1, 1, 6, 6, CHSV(battHue, 50, i));
        FastLED.show();
        FastLED.delay(d);
    }
    for (int i = 0; i < b-200; i+=r)
    {
        matrix.DrawRectangle(0, 0, 7, 7, CHSV(battHue, 0, i));
        FastLED.show();
        FastLED.delay(d);
    }
    while(leds[28]) {
        for (int i = 0; i<64; i++) {
            leds[i].fadeToBlackBy(5);

        }
        FastLED.delay(4);
        FastLED.show();
    }
    FastLED.clear();
    FastLED.show();
    currentBrightness=0;
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
    ambientCheck();
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

void gentleAmbient(){
    int x = random(2, 6);
    int y = random(2, 6);
    int c = random(255);
    matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(255, 0, 255));
    while(currentBrightness<masterBrightness) {
        FastLED.setBrightness(currentBrightness);
        matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(255, 0, 255));
        currentBrightness=currentBrightness+(masterBrightness/10);
        FastLED.show();
        FastLED.delay(20);
    }
    if((x==3&&y==3)||(x==3&&y==4)||(x==4&&y==3)||(x==4&&y==4)) {
        //Do nothing - get another set of coordinates
    } else {
        for (float i = 0; i < 255; i++) {
            if (buttonState == 1) {
                break;
            }
            FastLED.clear();
            matrix.DrawPixel(x, y, CHSV(c, 255, i));
            matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(255, 0, 255-i));
            FastLED.delay(10);
            FastLED.show();
        }
        for (float i = 255; i > 0; i--) {
            if (buttonState == 1) {
                break;
            }
            FastLED.clear();
            matrix.DrawPixel(x, y, CHSV(c, 255, i));
            matrix.DrawFilledRectangle(3, 3, 4, 4, CHSV(255, 0, 255-i));
            FastLED.delay(10);
            FastLED.show();
        }
    }
}

void startWait() { //Draws ≈ 90mA draw at peak
    if  (waitCountdownBegun == 0) {
        waitBeganAt = millis();
        waitCountdownBegun = 1;
    }
    if ((millis() - waitBeganAt < standbyDelay)) {
        checkCurrentMovement();
        if (currentMovement==1) {
            waitBeganAt = millis();
        }
        Serial.println("Gentle Ambient Mode Running...");
        gentleAmbient();
    } else if (currentMovement!=1) {
        Serial.println("Switching to standby");
        waitCountdownBegun = 0;
        mode = 7;
    } else {
        waitBeganAt = millis();
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

void sensory(int duration){ //Duration is in seconds
    long d = duration*1000;
    int sensoryCentreGlow = 0;
    int i = 0;
    if  (waitCountdownBegun == 0) {
        waitBeganAt = millis();
        waitCountdownBegun = 1;
        Serial.println("Sensory sequence started");
    }
    FastLED.clear();
    returnToMasterBrightness();
    while (buttonState != 1 && (millis() - waitBeganAt) < d) {
        if (sensoryCentreGlow<masterBrightness) {
            sensoryCentreGlow = sensoryCentreGlow+5;
        }
        matrix.DrawRectangle(0, 0, 7, 7, CHSV(i, 255, breatheEdgeLight));
        matrix.DrawRectangle(3, 3, 4, 4, CHSV(i, 255, sensoryCentreGlow));
        FastLED.delay(10);
        FastLED.show();
        i++;
    }
    mode = 0;
    transitionToStartWait();
    Serial.println("Sensory mode complete");
    waitCountdownBegun = 0;
}

void setTilt() {
    //Read normalized values
    Vector normAccel = mpu.readNormalizeAccel();
    // Calculate Pitch & Roll
    pitch = -(atan2(normAccel.XAxis, sqrt(normAccel.YAxis*normAccel.YAxis + normAccel.ZAxis*normAccel.ZAxis))*180.0)/M_PI;
    roll = -(atan2(normAccel.YAxis, normAccel.ZAxis)*180.0)/M_PI;
    int flat = 5;
    int boundary = 15;
    if (roll > boundary && pitch < flat && pitch > flat* -1) {
        tilt = 1; //Forward
    } else if (pitch > boundary && roll > boundary) {
        tilt = 2; //Forward Right
    } else if (pitch > boundary && roll < flat && roll > flat* -1) {
        tilt = 3; //Right
    } else if (pitch > boundary && roll < boundary* -1) {
        tilt = 4; //Backward Right
    } else if (roll < boundary* -1 && pitch < flat && pitch > flat* -1) {
        tilt = 5; //Backward
    } else if (pitch < boundary* -1 && roll < boundary* -1) {
        tilt = 6; //Backward Left
    } else if (pitch < boundary* -1 && roll < flat && roll > flat* -1) {
        tilt = 7; //Left
    } else if (pitch < boundary* -1 && roll > boundary) {
        tilt = 8; //Forward Left
    } else if (pitch > flat* -1 && pitch < flat && roll > flat* -1 && roll < flat ) {
        tilt = 0; //Flat
    }
}

void sand(){
    while(mode==5 && buttonState == 0){
    setTilt();
    switch(tilt) {
    case 1:
        //Serial.println("Forward");
        if(sandY>0) {sandY--;}
        if(sandX>3) {sandX--;}
        if(sandX<3) {sandX++;}
        break;
    case 2:
        //Serial.println("Forward Right");
        if(sandY>0) {sandY--;}
        if(sandX<6) {sandX++;}
        break;
    case 3:
        //Serial.println("Right");
        if(sandX<6) {sandX++;}
        if(sandY>3) {sandY--;}
        if(sandY<3) {sandY++;}
        break;
    case 4:
        //Serial.println("Backward Right");
        if(sandY<6) {sandY++;}
        if(sandX<6) {sandX++;}
        break;
    case 5:
        //Serial.println("Backward");
        if(sandY<6) {sandY++;}
        if(sandX>3) {sandX--;}
        if(sandX<3) {sandX++;}
        break;
    case 6:
        //Serial.println("Backward Left");
        if(sandY<6) {sandY++;}
        if(sandX>0) {sandX--;}
        break;
    case 7:
        //Serial.println("Left");
        if(sandX>0) {sandX--;}
        if(sandY>3) {sandY--;}
        if(sandY<3) {sandY++;}
        break;
    case 8:
        //Serial.println("Forward Left");
        if(sandY>0) {sandY--;}
        if(sandX>0) {sandX--;}
        break;
    case 0:
        //Serial.println("Flat");
        if(sandY>3) {sandY--;}
        if(sandY<3) {sandY++;}
        if(sandX>3) {sandX--;}
        if(sandX<3) {sandX++;}
        break;
    }

    matrix.DrawFilledRectangle(sandX, sandY, sandX+1, sandY+1, CHSV(255, 0, 255));
    matrix.DrawRectangle(sandX-1, sandY-1, sandX+2, sandY+2, CHSV(255, 0, 100));
    for (int i = 0; i<64; i++) {
        leds[i].fadeToBlackBy(15);
    }
    FastLED.show();
    FastLED.delay(30);
}
}

int checkCurrentMovement(){
    Wire.beginTransmission(0x68); // Begins a transmission to the I2C slave (GY-521 board)
    Wire.requestFrom(0x68, 1, true);
    Wire.write(0x61);
    int bit = Wire.read(); // set to zero (wakes up the MPU-6050)
    if (bit==0)
    {
        currentMovement = 1;
    } else {
        currentMovement = 0;
    }
    Wire.endTransmission(false);
    return(currentMovement);
}

void setup()
{
    //IO intial
    pinMode(ledControl, OUTPUT);
    pinMode(debug, INPUT);
    pinMode(button, INPUT);
    attachInterrupt(digitalPinToInterrupt(button), ISR_buttonChange, CHANGE);
    pinMode(moveInterrupt, INPUT);
    attachInterrupt(digitalPinToInterrupt(moveInterrupt), ISR_checkMove, RISING);

    //Only start Serial if debug jumper is pulled high
    // if (digitalRead(debug)==HIGH)
    // {
    //     //Serial initial
    //     Serial.begin(115200);
    //     Serial.println("Serial monitor started");
    // }

    Serial.begin(115200);
    Serial.println("Serial monitor started");

    // LED initial
    FastLED.addLeds<CHIPSET, ledControl, COLOR_ORDER>(matrix[0], matrix.Size()).setCorrection(TypicalSMD5050);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(masterBrightness);
    FastLED.clear(true);
    //Battery monitor initialisation
    pinMode(batteryMonitor, INPUT);
    pinMode(ldrSensor, INPUT);
    batteryCheck();
    FastLED.delay(50);

    //Runs now and each time the device wakes up from sleep
    onWake();

    //Start workmate timer
    workmateTimeSinceReset = millis();

    //Gyro initial
    Wire.begin();
    Serial.println("Initialize MPU6050");
    while(!mpu.begin(MPU6050_SCALE_2000DPS, MPU6050_RANGE_16G))
    {
        Serial.println("Could not find a valid MPU6050 sensor, check wiring!");
        delay(500);
    }
    mpu.setAccelPowerOnDelay(MPU6050_DELAY_3MS);
    mpu.setRange(MPU6050_RANGE_2G);
    mpu.setMotionDetectionThreshold(2);
    mpu.setMotionDetectionDuration(40);
    mpu.setIntMotionEnabled(true); // Interrupts sent when motion detected
    mpu.setIntZeroMotionEnabled(true);
    mpu.setDHPFMode(MPU6050_DHPF_0_63HZ);
    mpu.setZeroMotionDetectionThreshold(40);
    mpu.setZeroMotionDetectionDuration(50);

    //Battery feedback
    // if (battPerc > 50) {
    //     battHue = 86;
    // } else if (battPerc > 25) {
    //     battHue = 43;
    // } else if (battPerc > 15) {
    //     battHue = 12;
    // }
    // for (int i = 1; i < 8; i++) {
    //     matrix.DrawLine(0, 0, i, i, CHSV(battHue, 255, 255));
    //     FastLED.delay(30);
    //     FastLED.show();
    // }
    // for (int i = 1; i < 8; i++) {
    //     matrix.DrawLine(0, 0, i, i, CHSV(battHue, 255, 0));
    //     FastLED.delay(30);
    //     FastLED.show();
    // }
}

void reboot() {
    wdt_disable();
    wdt_enable(WDTO_15MS);
    while (1) {}
}

void loop() {
    //Movement stuff
    int n = Wire.endTransmission(false); // hold the I2C-bus
    if (n!=0) //Check return of endTransmission
    {
        Serial.print("endTransmission error: ");
        Serial.println(n);
        if (n==4)
        {
            Serial.println("Resetting");
            delay(1000);
            reboot(); //call reset
        }
    }
    checkCurrentMovement();

    //Main mode selection logic with shut off for low battery
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
                if (mode==7)
                {
                    Serial.println("Woken up from standby by buttonpress");
                    mode = 0;
                }
                else if (mode < 7) {
                    mode++;
                    Serial.println("Moved forward one mode");
                } else {
                    Serial.println("Moving back to start of modes");
                    mode = 1;
                }
                buttonLatch=0;
            }
            // if (mode==7 && buttonLatch==0 && movement==true)
            // {
            //     Serial.println(" ");
            //     Serial.println("Woken up from standby by movement");
            //     mode = 0;
            // }
            switch (mode) {
            case 0:
                Serial.println("*Main loop switch mode: 0");
                startWait();
                break;
            case 1:
                Serial.println("*Main loop switch mode: 1");
                box(120, 20);
                workmateTimeSinceReset = millis();
                break;
            case 2:
                Serial.println("*Main loop switch mode: 2");
                triangle(200, 20);
                workmateTimeSinceReset = millis();
                break;
            case 3:
                Serial.println("*Main loop switch mode: 3");
                relax(50, 20);
                workmateTimeSinceReset = millis();
                break;
            case 4:
                Serial.println("*Main loop switch mode: 4");
                ujjayiPranayama(15, 20);
                workmateTimeSinceReset = millis();
                break;
            case 5:
                Serial.println("*Main loop switch mode: 5");
                sand();
                workmateTimeSinceReset = millis();
                break;
            case 6:
                Serial.println("*Main loop switch mode: 6");
                sensory(180);
                break;
            case 7:
                Serial.println("*Main loop switch mode: 7");
                standby();
                break;
            case 8:
                Serial.println("*Main loop switch mode: 8");
                workmateNotify();
                break;
            }
        }
    } else {
        for (int i = 0; i < 3; ++i)
        {
            FastLED.clear();
            matrix.DrawLine(7, 7, 7, 7, CHSV(0, 255, 50));
            FastLED.show();
            FastLED.delay(1000);
            FastLED.clear();
            FastLED.delay(1000);
        }
        // sleep_enable();
        // set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        // sleep_cpu();
    }
}
