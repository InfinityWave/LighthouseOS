
#include <Arduino.h>
#include <SPI.h>
#include <StateMachine.h>
#include <TimeLib.h>      //https://github.com/PaulStoffregen/Time.git
#include <DS3232RTC.h>    //https://github.com/H4K5M6/DS3232RTC.git
#include <DCF77.h>        //https://github.com/thijse/Arduino-DCF77.git
#include <Adafruit_FRAM_I2C.h> //from lib manager v2.0
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>// Controller specific library
#include <CheapStepper.h> //https://github.com/H4K5M6/CheapStepper
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include "Adresses.h"

//DS3231 pins
#define DS3231_INT 3      // interrupt

//DCF77 pins
#define DCF_PIN 2         // Connection pin to DCF 77 device, interrupt
#define DCF_EN_PIN 4      // Enable DCF77 module active low

//TFT pins
//use Hardware SPI NANO: SCK=13, MISO=12, MOSI=11 
#define TFT_DC 9
#define TFT_CS 8
#define TFT_LITE 10 //PWM pin
#define TFT_WIDTH 320
#define TFT_HEIGHT 240
//#define TFT_RESET 

//Button pins
#define BTN_C 5           //interrupt
#define BTN_L 7           //interrupt
#define BTN_R 6           //interrupt

//LED pins
#define LED_BTN_C 
#define LED_BTN_R
#define LED_BTN_L
#define LED_MAIN          //PWM pin

// stepper pins
#define STEP_IN1 A0
#define STEP_IN2 A1
#define STEP_IN3 A2
#define STEP_IN4 A3

//Parameters
#define COLOR_BKGND ILI9341_BLACK // background color
#define COLOR_TXT ILI9341_WHITE // text color
#define MMEL 9    // number of menu entries


/* Pixels for FontSize
Number "88":
12pt size 1: 24x17 
12pt size 2: 48x34
18pt size 1: 36x25 
18pt size 2: 72x50
Date "11.09.2021":
12pt size 1: 112x17 
12pt size 2: 224x34
18pt size 1: 165x25 
18pt size 2: 296x50
*/
// TFT&Canvas Settings
#define FONTSIZE_BIG_HEIGHT 50
#define FONTSIZE_NORMAL_HEIGHT 34
#define FONTSIZE_SMALL_HEIGHT 25
#define FONTSIZE_TINY_HEIGHT 17
#define CANVAS_FONT_MARGIN 4
#define CANVAS_CLOCK_WIDTH 76
#define CANVAS_DATE_WIDTH 232
// Margins due to the surrounding box
#define TFT_MARGIN_LEFT 10
#define TFT_MARGIN_LEFT 30
#define TFT_MARGIN_TOP 7 
#define TFT_MARGIN_BOT 20
// Canvas Settings for Left and Right Menu Icons
#define CANVAS_MENULR_WIDTH 45
#define CANVAS_MENULR_HEIGHT 45
#define CANVAS_MENULR_OFFSET_X 5
#define CANVAS_MENULR_OFFSET_Y 7
//Global Position for Icon Canvas
#define CANVAS_MENUL_X 0
#define CANVAS_MENUL_Y 0
#define CANVAS_MENUR_X 230
#define CANVAS_MENUR_Y 0
#define CENTER_ICON_X 160
#define CENTER_ICON_Y 16
#define CENTER_ICON_SIZE 9
#define CENTER_ICON_LW 4
//ClockDisplaySettings
#define CLOCKDISPLAY_CLOCK_X 55
#define CLOCKDISPLAY_CLOCK_Y 38
#define CLOCKDISPLAY_DATE_X 0
#define CLOCKDISPLAY_DATE_Y 120
#define CLOCKDISPLAY_INFO_BOTTOM 50
#define CLOCKDISPLAY_LH_X 0
#define CLOCKDISPLAY_SYNC_X 150
#define CLOCKDISPLAY_TEMP_X 200

//MenuSettings
#define MENULINEWIDTH 3
#define MENU_TOPLINE_Y 55
#define MENU_ITEM1_Y 80



#define LITE 200  // standard brightness

#define DCF_INT 6     // interval of dcf sync in hours
#define DCF_HOUR 0    // hour to start dcf sync
#define DCF_MIN 0     // minute to start dcf sync
#define DCF_LEN 3600    // length in seconds of dcf sync attempt

#define STEP_RPM 1
#define STEP_N 4096


//


// initializations
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC/*, TFT_RESET*/);

GFXcanvas1 canvasClock(CANVAS_CLOCK_WIDTH, FONTSIZE_BIG_HEIGHT+CANVAS_FONT_MARGIN+MENULINEWIDTH); // Canvas for Clock Numbers
GFXcanvas1 canvasDate(CANVAS_DATE_WIDTH, FONTSIZE_NORMAL_HEIGHT+CANVAS_FONT_MARGIN+MENULINEWIDTH); // Canvas for Date
GFXcanvas1 canvasMenuItem(300, FONTSIZE_SMALL_HEIGHT+CANVAS_FONT_MARGIN+MENULINEWIDTH);
GFXcanvas1 canvasTinyFont(300, FONTSIZE_TINY_HEIGHT+CANVAS_FONT_MARGIN);
GFXcanvas1 canvasMenuLR(CANVAS_MENULR_WIDTH, CANVAS_MENULR_HEIGHT);

//Variables to store size of display/canvas texts
int16_t  x1, y1;
uint16_t w1, h1;
uint16_t x2, y2;
uint16_t w2, h2;


StateMachine machine = StateMachine();

State* S0 = machine.addState(&stateClockDisplay);
State* S1 = machine.addState(&stateStandby);
State* S2 = machine.addState(&stateMainMenu);
State* S3 = machine.addState(&stateWeddingMode);
State* S4 = machine.addState(&stateClockMenu);
State* S5 = machine.addState(&stateAlarm1Menu);
State* S6 = machine.addState(&stateAlarm2Menu);
State* S7 = machine.addState(&stateSoundMenu);
State* S8 = machine.addState(&stateLighthouseMenu);
State* S9 = machine.addState(&stateBrightnessMenu);
State* S10 = machine.addState(&stateHomingMenu);
State* S11 = machine.addState(&stateCreditsMenu);

DS3232RTC myRTC(false);

DCF77 DCF = DCF77(DCF_PIN, digitalPinToInterrupt(DCF_PIN), false);

Adafruit_FRAM_I2C fram = Adafruit_FRAM_I2C();

CheapStepper stepper (STEP_IN1, STEP_IN2, STEP_IN3, STEP_IN4); 

volatile time_t isrTime;
volatile bool isrTimeChange;
bool isrTimeUpdate;
char timeString[3];
char dateString[11];
char outString[100];
bool dcfSync = false;
bool dcfSyncSucc = false;
bool DCFSyncStatus = true;
bool DCFSyncChanged = false;
uint32_t dcfSyncStart = 0;

uint8_t clockHour, clockMinute, clockSecond, clockDay;
float currentTemp = 20.0;
bool tempChanged = false;


// button flags
volatile bool isrbuttonC = false;
volatile bool isrButtonL = false;
volatile bool isrButtonR = false;

// menu entries
const char *mainMenuEntries[MMEL] = {"Clock", "Alarm1", "Alarm2", "Sound", "Light", "Brightness", "Homing", "Back", "Credits"};

//menu variables
int8_t selectedMMItem = 0;
int8_t currentMenu = -1;
uint32_t sleepDelay = 60000;
uint32_t sleepTimer = 0;
bool updateScreen = true;
bool updateClockSign = false;
bool changeValue = false;

//stepper variables
bool moveTower = true;
bool stepperActive = false;

//display variable
uint8_t brightness = LITE;

//alarms
struct alarms {
    uint8_t hh;
    uint8_t mm;
    uint8_t dd;
    bool act;
    bool light;
    uint8_t soundfile;
} alm1, alm2;

// settings
uint8_t hours_per_day = 24; 

struct menuStruct {
    uint8_t selectedItem = 0;
    uint8_t currentVal = 0;
    int16_t item[8];
    
} submenu;

bool weddingModeFinished = false;

void setup(void) {
    Serial.begin(9600);
    
    // setup TFT Backlight as off
    pinMode(TFT_LITE, OUTPUT);
    analogWrite(TFT_LITE, 0);

    //stepper setup
    stepper.setRpm(STEP_RPM);

    // DCF setup
    digitalWrite(DCF_EN_PIN, HIGH);
    pinMode(DCF_EN_PIN, OUTPUT);

    // setup state machine
    // from clockDisplay
    S0->addTransition(&toSleep,S1);         // to standby
    S0->addTransition(&openMainMenu,S2);    // to main menu
    S0->addTransition(&changeToWeddingMode,S3); //start wedding mode
    S0->addTransition(&attachUnhandledInterrupts,S0);   //Must be last item
    // from standby
    S1->addTransition(&wakeup,S0);          // to clock
    // from main menu
    S2->addTransition(&closeMainMenu,S0);   // to clock
    S2->addTransition(&toSleep,S1);         // to standby
    S2->addTransition(&changeMenuSelection,S2); // stay and change selection
    S2->addTransition(&openClockMenu,S4);
    S2->addTransition(&openAlarm1Menu,S5);
    S2->addTransition(&openAlarm2Menu,S6);
    S2->addTransition(&openSoundMenu,S7);
    S2->addTransition(&openLighthouseMenu,S8);
    S2->addTransition(&openBrigthnessMenu,S9);
    S2->addTransition(&openHomingMenu,S10);
    S2->addTransition(&openCreditsMenu,S11);
    //S2->addTransition(&attachUnhandledInterrupts,S2);   //Must be last item
    // from wedding mode
    S3->addTransition(&exitWeddingMode,S0);
    //Return from all SubMenus
    S4->addTransition(&returnToMainMenu,S2);
    S4->addTransition(&changeClockMenuSelection,S4);
    S5->addTransition(&returnToMainMenu,S2);
    S6->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&returnToMainMenu,S2);
    S8->addTransition(&returnToMainMenu,S2);
    S9->addTransition(&returnToMainMenu,S2);
    S10->addTransition(&returnToMainMenu,S2);
    S11->addTransition(&returnToMainMenu,S2);
    S11->addTransition(&attachUnhandledInterrupts,S11);
    // TFT setup
    delay(500);
    tft.begin();
    tft.setTextColor(COLOR_TXT);
    tft.setTextSize(2);
    tft.setRotation(3);
    tft.fillScreen(COLOR_BKGND);
    tft.setFont(&FreeSans18pt7b);
//    canvas.setFont(&FreeSans18pt7b);
//    canvas.setTextSize(2);
    canvasClock.setFont(&FreeSans18pt7b);
    canvasClock.setTextSize(2);
    canvasDate.setFont(&FreeSans12pt7b);
    canvasDate.setTextSize(2);
    canvasMenuLR.setFont(&FreeSans18pt7b);
    canvasMenuLR.setTextSize(2);
    canvasTinyFont.setFont(&FreeSans12pt7b);
    canvasTinyFont.setTextSize(1);
    canvasMenuItem.setFont(&FreeSans18pt7b);
    canvasMenuItem.setTextSize(1);
    

    brightness = LITE; //set brightness
    
    // RTC startup
    pinMode(DS3231_INT, INPUT);
    myRTC.begin();
    myRTC.squareWave(SQWAVE_NONE);
    myRTC.setAlarm(ALM2_EVERY_MINUTE,0,0,0,0);
    myRTC.alarmInterrupt(ALARM_1, false);
    myRTC.alarmInterrupt(ALARM_2, true);
    attachInterrupt(digitalPinToInterrupt(DS3231_INT), isrChangeTime, FALLING);
    isrTimeChange = true;
    //myRTC.set(1577836800);
    updateClock();
    if(isrTime > 0)
        Serial.println("RTC has set the system time");
    else
        Serial.println("Unable to sync with the RTC");
        
    prepareClock();

    // initialize FRAM
    if (fram.begin())// you can stick the new i2c addr in here, e.g. begin(0x51);
        Serial.println("Found I2C FRAM");
    else
        Serial.println("I2C FRAM not identified");

    // space for variable inits
    readAlarms(ALARM1, alm1);
    readAlarms(ALARM2, alm2);
    stepper.setStep(read16bit(C_STEP));
    
    /*tft.setFont(&FreeSans12pt7b);
    /*for (int i = 0; i < 100; i = i+11){
        sprintf(timeString, "%02d", i);
        tft.getTextBounds("Sonntag", 20, 140, &x, &y, &w, &h);
        Serial.print("Sonntag");
        Serial.print(" ");
        Serial.print(x);
        Serial.print(" ");
        Serial.print(y);
        Serial.print(" ");
        Serial.print(w);
        Serial.print(" ");
        Serial.println(h);
    /*}*/

   /* tft.drawBitmap(121, 138, canvasClock.getBuffer(), 76, 54, ILI9341_WHITE, ILI9341_RED);
    tft.drawBitmap(211, 138, canvasClock.getBuffer(), 76, 54, ILI9341_WHITE, ILI9341_RED);
    /*tft.fillRect(107,138,1,40,ILI9341_WHITE);
    tft.fillRect(108,138,1,40,ILI9341_RED);
    tft.fillRect(109,138,1,40,ILI9341_WHITE);
    tft.fillRect(110,138,1,40,ILI9341_RED);
    tft.fillRect(111,138,1,40,ILI9341_WHITE);
    tft.fillRect(116,138,1,40,ILI9341_WHITE);
    tft.fillRect(117,138,1,40,ILI9341_RED);
    tft.fillRect(118,138,1,40,ILI9341_WHITE);
    tft.fillRect(119,138,1,40,ILI9341_RED);
    tft.fillRect(120,138,1,40,ILI9341_BLACK);*/
    // Buttons interrupt setup
    pinMode(BTN_C, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
    pinMode(BTN_R, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
    pinMode(BTN_L, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
}

void loop()
{
    if (isrTimeChange) {
        updateClock();
    }
    if (checkAlarms()) {
        // refine this
        Serial.println("ALAAAARM!!!");
    }
    if (!dcfSync && hour(isrTime)%DCF_INT == DCF_HOUR && minute(isrTime) == DCF_MIN) {
        DCF.Start();
        digitalWrite(DCF_EN_PIN, LOW);
        dcfSync = true;
        dcfSyncStart = isrTime;
        Serial.print("DCFSync start: ");
        Serial.print(hour(isrTime));
        Serial.print(":");
        Serial.println(minute(isrTime));
    }
    if (dcfSync && isrTime >= dcfSyncStart+DCF_LEN) {
        DCF.Stop();
        digitalWrite(DCF_EN_PIN, HIGH);
        dcfSync = false;
        dcfSyncSucc = false;
        DCFSyncStatus = true; //ToDo
        DCFSyncChanged = true;
        Serial.println("Sync Failed");
    }
    if (dcfSync && DCF.getTime() != 0) {
        DCF.Stop();
        myRTC.set(DCF.getTime());
        digitalWrite(DCF_EN_PIN, HIGH);
        dcfSync = false;
        dcfSyncSucc = true;
        DCFSyncStatus = true;
        DCFSyncChanged = true;
        Serial.print("Sync Success");
        Serial.print(hour(isrTime));
        Serial.print(":");
        Serial.println(minute(isrTime));
    }
    machine.run();
    stepper.run();
    if (moveTower) {
        stepper.run();
        if (stepperActive && stepper.getStepsLeft() == 0) {
            stepperActive = false;
            stepper.off();
        }
    }
}


void correct_date(int16_t *date){
    // Check the date and correct based on calendar
    // Date must be an array with 1st entry hour, than minute, day, month, year
    
    // Check if leap year
    int leapyear = 0;
    if (!(date[4] % 4)){
        leapyear = 1;
    }
    // Check month overflow
    date[3] = date[3] % 12;
    // check correct number of days for month
    // Begin with month with 31 days
    if (date[3]==0 || date[3]==2 || date[3]==4 || date[3]==6 || date[3]==7 || date[3]==9 || date[3]==11){
        while (date[2] < 0){
            date[2] = 31 + date[2]; 
        }
        date[2] = date[2] % 31; 
    }
    // Month with 30 days
    if (date[3]==3 || date[3]==5 || date[3]==8 || date[3]==10){
        while (date[2] < 0){
            date[2] = 30 + date[2]; 
        }
        date[2] = date[2] % 30;
    }
    // February
    if (date[3]==1){
        while (date[2] < 0){
            date[2] = 28 + leapyear + date[2]; 
        }
        date[2] = date[2] % (28 + leapyear);
    }
    // Correct hour over and underflow
    while (date[0] < 0){
        date[0] = date[0] + hours_per_day; 
    }
    date[0] = date[0] % hours_per_day;
    // Correct minute over and underflow
    while (date[1] < 0){
        date[1] = date[1] + 60; 
    }
    date[1] = date[1] % 60;
}

//StateMachine States
//S0
// main clock display
void stateClockDisplay()
{
    if (updateScreen) {
        analogWrite(TFT_LITE, brightness);
        updateScreen = false;
        updateClockSign = true;
        drawClockDisplayInfo();
    }
    if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
    }
    if (DCFSyncChanged || tempChanged){
        drawClockDisplayInfo();
    }
}

void drawClockDisplayInfo(){
    tempChanged = false;
    DCFSyncChanged = false;
    
    int16_t ypos = TFT_HEIGHT - CLOCKDISPLAY_INFO_BOTTOM - canvasTinyFont.height();
    
    canvasTinyFont.fillScreen(COLOR_BKGND);
    tft.fillRect(0, ypos- MENULINEWIDTH, TFT_WIDTH, MENULINEWIDTH, COLOR_TXT);

    canvasTinyFont.setCursor(CLOCKDISPLAY_LH_X, FONTSIZE_TINY_HEIGHT+MENULINEWIDTH+CANVAS_FONT_MARGIN);
    canvasTinyFont.print("LighthouseOS");
    
    if (DCFSyncStatus){
        canvasTinyFont.setCursor(CLOCKDISPLAY_SYNC_X, FONTSIZE_TINY_HEIGHT+MENULINEWIDTH+CANVAS_FONT_MARGIN);
        canvasTinyFont.print("DCF");
    }
    //sprintf(outString, "%d", (int)currentTemp);
    canvasTinyFont.setCursor(CLOCKDISPLAY_TEMP_X, FONTSIZE_TINY_HEIGHT+MENULINEWIDTH+CANVAS_FONT_MARGIN);
    sprintf(outString, "%d", (int)21.3);
    canvasTinyFont.print(outString);
    //canvasTinyFont.print((char)247);
    canvasTinyFont.print("C");
    tft.drawBitmap(0, ypos, canvasTinyFont.getBuffer(), canvasTinyFont.width(), canvasTinyFont.height(), COLOR_TXT, COLOR_BKGND);
}

// standby state
void stateStandby()
{
    //nothing yet maybe send arduino to sleep
    if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
    }
}


//S1
// main menu
void stateMainMenu()
{
    if (updateScreen) {
        updateScreen = false;
        // Draw Top Icons
        drawMenuSetter(true);
        tft.setCursor(100,40);
        tft.setFont(&FreeSans12pt7b);
        tft.println("Menu");
        tft.drawRect(0,55,TFT_WIDTH,MENULINEWIDTH,COLOR_TXT);
        tft.fillCircle(15,145,7,COLOR_TXT);
        tft.setCursor(40,100);
        tft.println(mainMenuEntries[(selectedMMItem+MMEL-1)%MMEL]);
        tft.setCursor(40,160);
        tft.println(mainMenuEntries[selectedMMItem]);
        tft.setCursor(40,220);
        tft.println(mainMenuEntries[(selectedMMItem+1)%MMEL]);
        analogWrite(TFT_LITE, brightness);
    }
}

//S3
void stateWeddingMode()
{
    //Switch Music On
    //Switch Motor On
    //Switch Main Light On
    //Blink auxiliary lights
    //Display something
    weddingModeFinished = true;
}

//S4
void stateClockMenu()
{
    if (updateScreen) {
        updateScreen = false;
        tft.setCursor(100,40);
        //tft.setFont(&FreeSans12pt7b);
        tft.println("Clock");
        tft.drawFastHLine(0,50,TFT_WIDTH,COLOR_TXT);
        tft.fillCircle(15,145,7,COLOR_TXT);
        tft.setCursor(40,100);
        tft.println(mainMenuEntries[(selectedMMItem+MMEL-1)%MMEL]);
        tft.setCursor(40,160);
        tft.println(mainMenuEntries[selectedMMItem]);
        tft.setCursor(40,220);
        tft.println(mainMenuEntries[(selectedMMItem+1)%MMEL]);
        analogWrite(TFT_LITE, brightness);
    }
    //Manually set time
    //Set Light Interval
    //Set Motor Interval
    //clockDisplay(isrTime);
}

//S5
void stateAlarm1Menu()
{
    if (updateScreen) {
        updateScreen = false;
    }
    //Set Alarm1
    //Select Alarm Sound
    //Select if lights should be on
}

//S6
void stateAlarm2Menu()
{
    if (updateScreen) {
        updateScreen = false;
    }
    //Set Alarm2
    //Select Alarm Sound
    //Select if lights should be on
}

//S7
void stateSoundMenu()
{
    if (updateScreen) {
        updateScreen = false;
    }
    //Set Volume 0 - 30

}

//S8
void stateLighthouseMenu()
{
    //Lighthouse Settings for normal clock mode
    //Motor Settings
    //Main Light Settings
}

//S9
void stateBrightnessMenu()
{
    //Brightness Setting for Display
    //MainLED?
}

//S10
void stateHomingMenu()
{
    //Motor Homing Procedure
}

//S11
void stateCreditsMenu()
{
    //Display Credits on Lighthouse OS
    //Turn on Main Lights and Motor
}

bool openMainMenu()
{
    if (isrbuttonC) {
      isrbuttonC = false;
      sleepTimer = millis();
      clearTFT();
      currentMenu = MMEL;
      selectedMMItem = 0;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      return true;
    }
    return false;
}

bool toSleep()
{
    if (millis() > sleepTimer + sleepDelay) {
        analogWrite(TFT_LITE, 0);
        currentMenu = -1;
        return true;
    }
    return false;
}

bool wakeup()
{
    if (isrbuttonC||isrButtonR||isrButtonL) {
      isrbuttonC = false;
      isrButtonR = false;
      isrButtonL = false;
      sleepTimer = millis();
      currentMenu = -1;
      isrTimeUpdate = true;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
      return true;
    }
    return false;
}

bool changeMenuSelection()
{
    int noItems = 0;
    switch (currentMenu) {
      case MMEL: noItems = MMEL;
    }
    if(isrButtonR){
        isrButtonR = false;
        clearTFT();
        selectedMMItem = (selectedMMItem+1)%noItems;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        return true;
    }
    if(isrButtonL){
        isrButtonL = false;
        clearTFT();
        selectedMMItem = (selectedMMItem+noItems-1)%noItems;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        return true;
    }
    return false;
}

bool returnToMainMenu()
{
    if (isrbuttonC  && (submenu.selectedItem == 0)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = MMEL;
        clearTFT();
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openClockMenu()
{
    if (isrbuttonC  && (selectedMMItem == 0)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        submenu.selectedItem = 0;
        submenu.item[0] = hour(isrTime);
        submenu.item[1] = minute(isrTime);
        submenu.item[2] = day(isrTime);
        submenu.item[3] = month(isrTime);
        submenu.item[4] = year(isrTime);
        clearTFT();
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool changeClockMenuSelection()
{
    if (isrbuttonC){
        isrbuttonC = false;
        submenu.selectedItem = (submenu.selectedItem + 1) % 6;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;
    }
    if (isrButtonL || isrButtonR) {
        if (isrButtonL){
            isrButtonL = false;
            submenu.item[submenu.selectedItem] --;
            delay(50);
            attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        }
        else {
            isrButtonR = false;
            submenu.item[submenu.selectedItem] ++; 
            delay(50);
            attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        }
    }
    // Correct date after input
    correct_date(submenu.item);
}


bool openAlarm1Menu()
{
    if (isrbuttonC  && (selectedMMItem == 1)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        submenu.selectedItem = 0;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openAlarm2Menu()
{
    if (isrbuttonC  && (selectedMMItem == 2)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        submenu.selectedItem = 0;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openSoundMenu()
{
    if (isrbuttonC  && (selectedMMItem == 3)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        submenu.selectedItem = 0;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openLighthouseMenu()
{
    if (isrbuttonC  && (selectedMMItem == 4)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        submenu.selectedItem = 0;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openBrigthnessMenu()
{
    if (isrbuttonC  && (selectedMMItem == 5)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        submenu.selectedItem = 0;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openHomingMenu()
{
    if (isrbuttonC  && (selectedMMItem == 6)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        submenu.selectedItem = 0;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool closeMainMenu()
{
    if (isrbuttonC) {
      if (selectedMMItem == 7) {
          sleepTimer = millis();
          isrbuttonC = false;
          clearTFT();
          prepareClock();
          currentMenu = -1;
          isrTimeChange = true; // to enable clock display
          delay(50);
          attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
          return true;
          }
    }
    return false;
}

bool openCreditsMenu()
{
    if (isrbuttonC  && (selectedMMItem == 8)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool changeToWeddingMode()
{
    return false;
}

bool exitWeddingMode()
{
    if(weddingModeFinished){
        weddingModeFinished = false;
        return true;
    }
    return false;
}

bool attachUnhandledInterrupts()
{
    if(isrButtonR){
        isrButtonR = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
    }
    if(isrButtonL){
        isrButtonL = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
    }
    if(isrButtonR){
        isrButtonR = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
    }
    return true;
}

void clearTFT()
{
    analogWrite(TFT_LITE, 0);
    tft.fillScreen(COLOR_BKGND);
    updateScreen = true;
}

// reset internal clock variables
void prepareClock()
{
    clockHour = 255;
    clockMinute = 255;
    clockSecond = 255;
    clockDay = 255;
}

void updateClock()
{
    isrTimeChange = false;
    isrTimeUpdate = true;
    isrTime = myRTC.get();
    myRTC.alarm(ALARM_2);
}

void updateCanvasClock(GFXcanvas1& cCanvas, uint8_t cDigit, bool underline)
{
    sprintf(timeString,"%02d",cDigit);
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cCanvas.height()-CANVAS_FONT_MARGIN-MENULINEWIDTH);
    cCanvas.println(timeString);
    if (underline)
    {
        int16_t  x, y;
        uint16_t w, h;
        cCanvas.getTextBounds(timeString, 0, cCanvas.height()-CANVAS_FONT_MARGIN-MENULINEWIDTH, &x, &y, &w, &h);
        cCanvas.fillRect(x, y+h+1, w, MENULINEWIDTH, COLOR_TXT);
    }

}

void updateCanvasDate(GFXcanvas1& cCanvas, uint8_t cDay, uint8_t cMonth, uint16_t cYear, int8_t underline)
{
    uint8_t cursorPos = cCanvas.height()-CANVAS_FONT_MARGIN-MENULINEWIDTH;
    sprintf(dateString,"%02d.%02d.%04d", cDay, cMonth, cYear);
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cursorPos);
    cCanvas.println(dateString);
    if (true)
    {
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x1, &y1, &w1, &h1);
        sprintf(dateString,"%04d", cYear);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1+w1-w2, y1+h2+1, w2, MENULINEWIDTH, COLOR_TXT);
    }
}

void updateCanvasMenuSetter(int signtype)
{
    canvasMenuLR.fillScreen(COLOR_BKGND);
    canvasMenuLR.setCursor(CANVAS_MENULR_OFFSET_X, CANVAS_MENULR_HEIGHT-CANVAS_MENULR_OFFSET_Y);
    
    switch (signtype)
    {
        case 0: canvasMenuLR.println('-');
        case 1: canvasMenuLR.println('+');
    }
}

void drawMenuSetter(bool isOn)
{
    if (isOn){
        updateCanvasMenuSetter(0);
        tft.drawBitmap(TFT_MARGIN_LEFT+CANVAS_MENUL_X, CANVAS_MENUL_Y, canvasMenuLR.getBuffer(), canvasMenuLR.width(), canvasMenuLR.height(), COLOR_TXT, COLOR_BKGND);
        updateCanvasMenuSetter(1);
        tft.drawBitmap(TFT_MARGIN_LEFT+CANVAS_MENUR_X, CANVAS_MENUR_Y, canvasMenuLR.getBuffer(), canvasMenuLR.width(), canvasMenuLR.height(), COLOR_TXT, COLOR_BKGND);
    }
    else{

    }
    tft.fillCircle(CENTER_ICON_X, CENTER_ICON_Y, CENTER_ICON_Y, COLOR_TXT);
    tft.fillCircle(CENTER_ICON_X, CENTER_ICON_Y, CENTER_ICON_Y-CENTER_ICON_LW, COLOR_BKGND);
}


void drawClockItems(GFXcanvas1& cCanvas, int16_t xPos, int16_t yPos, uint8_t clock_item)
{
    switch (clock_item){
        case 0: 
            xPos = xPos; // Hour
            break;
        case 1:
            xPos = xPos + cCanvas.width() + CANVAS_FONT_MARGIN; // :-Sign
            break;
        case 2: 
            cCanvas.getTextBounds(":", 0, cCanvas.height(), &x1, &y1, &w1, &h1);
            xPos = xPos  + cCanvas.width() + x1 + w1 + 3 * CANVAS_FONT_MARGIN;    // Minute
            break;
    }
    tft.drawBitmap(xPos, yPos, cCanvas.getBuffer(), cCanvas.width(), cCanvas.height(), COLOR_TXT, COLOR_BKGND);
}

void clockDisplay(time_t t)
{

    if (updateClockSign){
        updateClockSign = false;
        canvasClock.fillScreen(COLOR_BKGND);
        canvasClock.setCursor(0, canvasClock.height()-CANVAS_FONT_MARGIN-MENULINEWIDTH);
        canvasClock.println(":");
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X, CLOCKDISPLAY_CLOCK_Y, 1);
       
    }
    if (clockHour != hour(t)){
        clockHour = hour(t);
        updateCanvasClock(canvasClock, clockHour, true);
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X, CLOCKDISPLAY_CLOCK_Y, 0);
    }
    if (clockMinute != minute(t)){
        clockMinute = minute(t);
        updateCanvasClock(canvasClock, clockMinute, false);
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X, CLOCKDISPLAY_CLOCK_Y, 2);
        if (moveTower) {
            stepper.newMoveToDegree(true, ((uint16_t)clockMinute)*30);
            stepperActive = true;
            Serial.println(((uint16_t)clockMinute)*30);
            Serial.println(stepper.getStep());
            Serial.println(stepper.getStepsLeft());
        }
    }
    /*if (clockSecond != second(isrTime)){
        clockSecond = second(isrTime);
        sprintf(timeString,"%02d",clockSecond);
        canvasClock.fillScreen(ILI9341_BLACK);
        canvasClock.setCursor(0, 50);
        canvasClock.println(timeString);
        tft.drawBitmap(211, 38, canvasClock.getBuffer(), 76, 54, ILI9341_WHITE, ILI9341_RED);
    }*/
    if (clockDay != day(t)){
        clockDay = day(t);
        updateCanvasDate(canvasDate, clockDay, month(isrTime), year(isrTime), -1);
        tft.drawBitmap(TFT_MARGIN_LEFT+CLOCKDISPLAY_DATE_X, CLOCKDISPLAY_DATE_Y, canvasDate.getBuffer(), canvasDate.width(), canvasDate.height(), COLOR_TXT, COLOR_BKGND);
    }
}


// read temperature from RTC:
void update_temperature(){
    // Read temperature from RTC and update if necessary
    float newtemp = 22.1; // ToDo replace with actual reading
    if (! (newtemp==currentTemp)){
        currentTemp = newtemp;
        tempChanged = false;
    }
}

// interrupt functions

void isrChangeTime()
{
    isrTimeChange = true;
}

// button interrupt flags

void buttonC()
{
    detachInterrupt(digitalPinToInterrupt(BTN_C));
    isrbuttonC = true;
}

void buttonR()
{
    detachInterrupt(digitalPinToInterrupt(BTN_R));
    isrButtonR = true;
}

void buttonL()
{
    detachInterrupt(digitalPinToInterrupt(BTN_L));
    isrButtonL = true;
}

// alarm functions

void setAlarmMenu (uint16_t address)
{
    // make alarm menu screen
    // hh:dd
    // Mo Di Mi Do Fr Sa So
    // w/ some kind of selection for weekday
    // Light y/n
    // Sound
}

/*bool setAlarms ()
{
    
    if (isrbuttonC) {
      isrbuttonC = false;
      sleepTimer = millis();
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      return true;
    }
    if (changeValue) {
        switch case
    } else {
        
    }
    return false;
}*/

bool checkAlarms () 
{
    if (alm1.act && alm1.hh == hour(isrTime) && alm1.mm == minute(isrTime)) {
        if (alm1.dd & 0b00000001 << (weekday(isrTime)-1)) {
            return true;
        }
        if (alm1.dd & 0b10000000) {
            alm1.dd = 0;
            writeAlarms (ALARM1, alm1);
            return true; //??
        }
    }
    if (alm2.act && alm2.hh == hour(isrTime) && alm2.mm == minute(isrTime)) {
        if (alm2.dd & 0b00000001 << (weekday(isrTime)-1)) {
            return true;
        }
    }
    return false;
}

//FRAM functions

void writeAlarms (uint16_t address, struct alarms alm)
{
    fram.write(address, alm.hh);
    fram.write(address+1, alm.mm);
    fram.write(address+2, alm.dd);
    fram.write(address+3, alm.act);
    fram.write(address+4, alm.light);
    fram.write(address+5, alm.soundfile);
}

void readAlarms (uint16_t address, struct alarms &alm)
{
    alm.hh = fram.read(address);
    alm.mm = fram.read(address+1);
    alm.dd = fram.read(address+2);
    alm.act = fram.read(address+3);
    alm.light = fram.read(address+4);
    alm.soundfile = fram.read(address+5);
}

void write16bit (uint16_t address, uint16_t data)
{
    fram.write(address, (uint8_t*)&data, 2);
}

uint16_t read16bit (uint16_t address)
{
    uint16_t data;
    fram.read(address, (uint8_t*)&data, 2);
    return data;
}


void write32bit (uint16_t address, uint32_t data)
{
    fram.write(address, (uint8_t*)&data, 4);
}

uint32_t read32bit (uint16_t address)
{
    uint32_t data;
    fram.read(address, (uint8_t*)&data, 4);
    return data;
}
