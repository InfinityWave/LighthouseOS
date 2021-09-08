
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
#define LED_BTN_C 0        //TODO Button LED
#define LED_BTN_R 0        //TODO Button LED
#define LED_BTN_L 0        //TODO Button LED
#define LED_MAIN 0         //TODO PWM pin
#define ALARM_LIGHT_MAX 0  //TODO Set

// stepper pins
#define STEP_IN1 A0
#define STEP_IN2 A1
#define STEP_IN3 A2
#define STEP_IN4 A3

//Parameters
#define COLOR_BKGND ILI9341_BLACK // background color
#define COLOR_TXT ILI9341_WHITE // text color
#define COLOR_BLUE 0x001F
#define COLOR_RED 0xF800



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
// Margins due to the surrounding wooden box. TODO Edit for final setup
#define TFT_MARGIN_LEFT 5
#define TFT_MARGIN_RIGHT 30
#define TFT_MARGIN_TOP 7 
#define TFT_MARGIN_BOT 25
// Canvas Settings for Left and Right Menu Icons
#define CANVAS_MENULR_WIDTH 45
#define CANVAS_MENULR_HEIGHT 38
#define CANVAS_MENULR_ICONOFFSET 20
//Global Position for Icon Canvas
#define CANVAS_MENUL_X 5
#define CANVAS_MENUL_Y 7
#define CANVAS_MENUR_X 235
#define CANVAS_MENUR_Y 7
#define CENTER_ICON_X 135
#define CENTER_ICON_Y 13
#define CENTER_ICON_SIZE 12
#define CENTER_ICON_LW 4
//ClockDisplaySettings
#define CLOCKDISPLAY_CLOCK_X 55
#define CLOCKDISPLAY_CLOCK_Y 55
#define CLOCKDISPLAY_DATE_X 35
#define CLOCKDISPLAY_DATE_Y 125
#define CLOCKDISPLAY_LH_X 0
#define CLOCKDISPLAY_SYNC_X 175
#define CLOCKDISPLAY_TEMP_X 245

//MenuSettings
#define MENULINEWIDTH 3
#define MENU_TOPLINE_Y 43
#define MENU_ITEMS 3
#define MENU_ITEMS_X 35
#define MENU_ITEM1_Y 80
#define MENU_SEL_X 10
#define MENU_SEL_Y 138
#define MENU_SEL_SIZE 5
//SpecialMenuSettings
#define MAINMENU_ITEMS 9    // number of menu entries
#define MAINMENU_TXT_X 120




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
GFXcanvas1 canvasMenuItem(160, FONTSIZE_SMALL_HEIGHT+4*CANVAS_FONT_MARGIN+MENULINEWIDTH); // Bigger Canvas not supported
GFXcanvas1 canvasTinyFont(220, FONTSIZE_TINY_HEIGHT+3*CANVAS_FONT_MARGIN);
GFXcanvas1 canvasMenuLR(CANVAS_MENULR_WIDTH, CANVAS_MENULR_HEIGHT);

//Variables to store size of display/canvas texts
int16_t  x1, y1;
uint16_t w1, h1;
uint16_t x2, y2;
uint16_t w2, h2;
int16_t cursorX, cursorY;



// Define StateMachine
/////////////////////////////////////////////////////////////
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

State* S99 = machine.addState(&stateAlarmActive); // State for active alarm

// Define Clock-HW
////////////////////////////////////////////////////////////
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
const char *mainMenuEntries[MAINMENU_ITEMS] = {"Clock", "Alarm1", "Alarm2", "Sound", "Light", "Brightness", "Homing", "Back", "Credits"};

//menu variables
int8_t selectedMMItem = 0;
int8_t currentMenu = -1;
uint32_t sleepDelay = 60000;
uint32_t sleepTimer = 0;
bool updateScreen = true;
bool updateMenuSelection = false;
bool updateClockSign = false;
bool changeValue = false;

// Alarm variables
uint32_t alarmLightDelay = 1000;
uint32_t alarmLightTimer = 0;
bool alarmLightOn = false;

// Stepper and tower variables
bool moveTower = true;
bool stepperActive = false;
uint16_t towerPosOffset = 0; // 0 o'clock light position (homing)

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
    time_t nextAlarm; // Time stamp of next alarm
	uint8_t mode; // 0: Off, 1: Once, 2: Every day, 3: Weekdays, 4: Weekend
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
    analogWrite(TFT_LITE, brightness);

    // setup tower LED
    pinMode(LED_MAIN, OUTPUT);
    analogWrite(LED_MAIN, 0);		

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
	//S0->addTransition(&startAlarm,S99); // check and start alarm
    S0->addTransition(&attachUnhandledInterrupts,S0);   //Must be last item
    // from standby
    S1->addTransition(&wakeup,S0);          // to clock
	//S1->addTransition(&startAlarm,S99); // check and start alarm
    // from main menu
    S2->addTransition(&returnToClockDisplay,S0); //After timeout
    S2->addTransition(&closeMainMenu,S0);   // to clock
    S2->addTransition(&changeMenuSelection,S2); // stay and change selection
    S2->addTransition(&openClockMenu,S4);
    S2->addTransition(&openAlarm1Menu,S5);
    S2->addTransition(&openAlarm2Menu,S6);
    S2->addTransition(&openSoundMenu,S7);
    S2->addTransition(&openLighthouseMenu,S8);
    S2->addTransition(&openBrigthnessMenu,S9);
    S2->addTransition(&openHomingMenu,S10);
    S2->addTransition(&openCreditsMenu,S11);
	//S2->addTransition(&startAlarm,S99); // check and start alarm
    //S2->addTransition(&attachUnhandledInterrupts,S2);   //Must be last item
    // from wedding mode
    S3->addTransition(&exitWeddingMode,S0);
    //Return from all SubMenus
    S4->addTransition(&returnToClockDisplay,S0); //After timeout
    S4->addTransition(&returnToMainMenu,S2);
    S4->addTransition(&changeClockMenuSelection,S4);
    S5->addTransition(&returnToClockDisplay,S0); //After timeout
    S5->addTransition(&returnToMainMenu,S2);
    S6->addTransition(&returnToClockDisplay,S0); //After timeout
    S6->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&returnToClockDisplay,S0); //After timeout
    S7->addTransition(&returnToMainMenu,S2);
    S8->addTransition(&returnToClockDisplay,S0); //After timeout
    S8->addTransition(&returnToMainMenu,S2);
    S9->addTransition(&returnToClockDisplay,S0); //After timeout
    S9->addTransition(&returnToMainMenu,S2);
    S10->addTransition(&returnToClockDisplay,S0); //After timeout
    S10->addTransition(&returnToMainMenu,S2);
    S11->addTransition(&returnToClockDisplay,S0); //After timeout
	S4->addTransition(&startAlarm,S99); // check and start alarm
    S5->addTransition(&returnToMainMenu,S2);
	//S5->addTransition(&startAlarm,S99); // check and start alarm
    S6->addTransition(&returnToMainMenu,S2);
	//S6->addTransition(&startAlarm,S99); // check and start alarm
    S7->addTransition(&returnToMainMenu,S2);
	//S7->addTransition(&startAlarm,S99); // check and start alarm
    S8->addTransition(&returnToMainMenu,S2);
	//S8->addTransition(&startAlarm,S99); // check and start alarm
    S9->addTransition(&returnToMainMenu,S2);
	//S9->addTransition(&startAlarm,S99); // check and start alarm
    S10->addTransition(&returnToMainMenu,S2);
	//S10->addTransition(&startAlarm,S99); // check and start alarm
    S11->addTransition(&returnToMainMenu,S2);
	//S11->addTransition(&startAlarm,S99); // check and start alarm
    S11->addTransition(&attachUnhandledInterrupts,S11);

	// Stop alarm and return to clock
	S99->addTransition(&stopAlarm,S2);
	S99->addTransition(&attachUnhandledInterrupts,S99);
	
    // TFT setup
    delay(500);
    tft.begin();
    tft.setTextColor(COLOR_TXT);
    tft.setTextSize(2);
    tft.setRotation(3);
    tft.fillScreen(COLOR_BKGND);
    tft.setFont(&FreeSans12pt7b);
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
    //myRTC.set(1631107034);
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
	// Time from RTC
    if (isrTimeChange) {
        updateClock();
    }
	// Time from DCF
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
    sprintf(outString, "RTC Time: %d", myRTC.get());
	Serial.println(outString);
	// Run interface
	//////////////////////////
	// StateMachine
    machine.run();
	// Show time via tower light
	//updateTowerLight(isrTime);
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
////////////////////////////////////////////////////////////
//S0 = main clock display
void stateClockDisplay()
{
    if (updateScreen) {
        updateScreen = false;
        updateClockSign = true;
        drawClockDisplayInfo();
        drawMenuSetter(false);
        tft.drawRect(0,MENU_TOPLINE_Y+TFT_MARGIN_TOP,TFT_WIDTH,MENULINEWIDTH,COLOR_TXT);
    }
    if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
    }
    if (DCFSyncChanged || tempChanged){
        drawClockDisplayInfo();
    }
	if (isrButtonR) {							// Light button was pressed
      isrButtonR = false;						// Reset Btn-flag
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
	  alarmLightOn = true; 						// Turn on light flag
	  analogWrite(LED_MAIN, ALARM_LIGHT_MAX);	// Switch tower light
	  digitalWrite(LED_BTN_C, HIGH);			// Switch btn LEDs on
	  digitalWrite(LED_BTN_R, HIGH);			// Switch btn LEDs on
	  digitalWrite(LED_BTN_L, HIGH);			// Switch btn LEDs on
	  alarmLightTimer = millis();				// (Re-)Start timer
    }
	if (millis() > alarmLightTimer + alarmLightDelay){ // Time is up...
		alarmLightOn = false; 						// Turn on light flag
		analogWrite(LED_MAIN, 0);					// Switch tower light
		digitalWrite(LED_BTN_C, LOW);				// Switch btn LEDs off
		digitalWrite(LED_BTN_R, LOW);				// Switch btn LEDs off
		digitalWrite(LED_BTN_L, LOW);				// Switch btn LEDs off
	}
}

void drawClockDisplayInfo(){
    tempChanged = false;
    DCFSyncChanged = false;
    
    int16_t ypos = TFT_HEIGHT - TFT_MARGIN_BOT - canvasTinyFont.height();
    
    
    tft.fillRect(0, ypos-MENULINEWIDTH, TFT_WIDTH, MENULINEWIDTH, COLOR_TXT);

    canvasTinyFont.fillScreen(COLOR_BKGND);
    canvasTinyFont.setCursor(0, FONTSIZE_TINY_HEIGHT+CANVAS_FONT_MARGIN);
    canvasTinyFont.print("LighthouseOS");
    tft.drawBitmap(CLOCKDISPLAY_LH_X+TFT_MARGIN_LEFT, ypos, canvasTinyFont.getBuffer(), canvasTinyFont.width(), canvasTinyFont.height(), COLOR_TXT, COLOR_BKGND);

    if (DCFSyncStatus){
        canvasTinyFont.fillScreen(COLOR_BKGND);
        canvasTinyFont.setCursor(0, FONTSIZE_TINY_HEIGHT+CANVAS_FONT_MARGIN);
        canvasTinyFont.print("DCF");
        tft.drawBitmap(CLOCKDISPLAY_SYNC_X+TFT_MARGIN_LEFT, ypos, canvasTinyFont.getBuffer(), canvasTinyFont.width(), canvasTinyFont.height(), COLOR_TXT, COLOR_BKGND);
    }
    canvasTinyFont.fillScreen(COLOR_BKGND);
    canvasTinyFont.setCursor(0, FONTSIZE_TINY_HEIGHT+CANVAS_FONT_MARGIN);
    sprintf(outString, "%d", (int)currentTemp);
    canvasTinyFont.print(outString);
    //canvasTinyFont.print((char)247); //Not supported by non standard Font
    canvasTinyFont.print("C");
    tft.drawBitmap(CLOCKDISPLAY_TEMP_X+TFT_MARGIN_LEFT, ypos, canvasTinyFont.getBuffer(), canvasTinyFont.width(), canvasTinyFont.height(), COLOR_TXT, COLOR_BKGND);
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

// S99 = Active alarm
void stateAlarmActive()
{
	if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
        analogWrite(TFT_LITE, brightness);
	}
	if (millis() > alarmLightTimer + alarmLightDelay) {
		alarmLightTimer = millis();								// Restart timer
		alarmLightOn = not alarmLightOn;						// Switch state of light
		analogWrite(LED_MAIN, ALARM_LIGHT_MAX * alarmLightOn);	// Switch tower light
	}
		/////////////////////////////////////
		// TODO
		// Make sure MP3 is running
		/////////////////////////////////////
}

//S1
// main menu
void stateMainMenu()
{
    if (updateScreen) {
        updateScreen = false;
        // Draw Top Icons
        drawMenuSetter(true);
        cursorY = TFT_MARGIN_TOP+MENU_TOPLINE_Y;
        tft.drawRect(0,cursorY,TFT_WIDTH,MENULINEWIDTH,COLOR_TXT);
        cursorY = cursorY + MENULINEWIDTH+CANVAS_FONT_MARGIN+FONTSIZE_NORMAL_HEIGHT;
        // Write Center Entry
        tft.getTextBounds("MainMenu", 0, cursorY, &x1, &y1, &w1, &h1);
        cursorX = (uint8_t) ((TFT_WIDTH - TFT_MARGIN_LEFT - TFT_MARGIN_RIGHT - w1) / 2);
        tft.setCursor(cursorX, cursorY);
        tft.println("MainMenu");       
        
        
        updateMenuSelection = true;
        tft.fillCircle(MENU_SEL_X,TFT_MARGIN_TOP+MENU_SEL_Y,MENU_SEL_SIZE,COLOR_TXT);
    }
    if (updateMenuSelection){
        cursorY = TFT_MARGIN_TOP+MENU_TOPLINE_Y+MENULINEWIDTH+CANVAS_FONT_MARGIN+FONTSIZE_NORMAL_HEIGHT;
        for (int i=0; i<MENU_ITEMS; i++){
            canvasMenuItem.fillScreen(COLOR_BKGND);
            canvasMenuItem.setCursor(0,CANVAS_FONT_MARGIN + FONTSIZE_SMALL_HEIGHT);
            canvasMenuItem.println(mainMenuEntries[(selectedMMItem+MAINMENU_ITEMS-1+i)%MAINMENU_ITEMS]);
            tft.drawBitmap(TFT_MARGIN_LEFT+MENU_ITEMS_X, cursorY, canvasMenuItem.getBuffer(), canvasMenuItem.width(), canvasMenuItem.height(), COLOR_TXT, COLOR_BKGND);
            cursorY = cursorY + canvasMenuItem.height();
        }
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
        tft.println(mainMenuEntries[(selectedMMItem+MAINMENU_ITEMS-1)%MAINMENU_ITEMS]);
        tft.setCursor(40,160);
        tft.println(mainMenuEntries[selectedMMItem]);
        tft.setCursor(40,220);
        tft.println(mainMenuEntries[(selectedMMItem+1)%MAINMENU_ITEMS]);
        
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

//StateMachine Transitions
///////////////////////////////////////////////
bool openMainMenu()
{
    if (isrbuttonC) {
      isrbuttonC = false;
      sleepTimer = millis();
      clearTFT();
      currentMenu = MAINMENU_ITEMS;
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
      analogWrite(TFT_LITE, brightness);
      return true;
    }
    return false;
}

// Stop alarm when pressing L-Button
bool stopAlarm()
{
	if (isrButtonL) {			// Stop-Alam button was pressed
      isrButtonL = false;		// Reset Btn-flag
      currentMenu = -1;			// No menu will be active
      isrTimeUpdate = true;		// ??
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
	  analogWrite(LED_MAIN, 0); // Switch off tower light
	  digitalWrite(LED_BTN_C, LOW);			// Switch btn LEDs off
	  digitalWrite(LED_BTN_R, LOW);			// Switch btn LEDs off
	  digitalWrite(LED_BTN_L, LOW);			// Switch btn LEDs off
	  /////////////////////////////////////
	  // TODO
	  // Stop MP3-Player
	  /////////////////////////////////////
      return true;
    }
    return false;
}

// Start alamr when time has come
bool startAlarm()
{
	if (checkAlarms()) {
		digitalWrite(LED_BTN_C, HIGH);			// Switch btn LEDs on
		digitalWrite(LED_BTN_R, HIGH);			// Switch btn LEDs on
		digitalWrite(LED_BTN_L, HIGH);			// Switch btn LEDs on
		/////////////////////////////////////
		// TODO
		// Start MP3-Player
		/////////////////////////////////////
		return true;
    }
	return false;
}

bool changeMenuSelection()
{
    int noItems = 0;
    switch (currentMenu) {
      case MAINMENU_ITEMS: noItems = MAINMENU_ITEMS;
    }
    if(isrButtonR){
        isrButtonR = false;
        updateMenuSelection = true;
        selectedMMItem = (selectedMMItem+1)%noItems;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        return true;
    }
    if(isrButtonL){
        isrButtonL = false;
        updateMenuSelection = true;
        selectedMMItem = (selectedMMItem+noItems-1)%noItems;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        return true;
    }
    return false;
}

bool returnToClockDisplay()
{

     if (millis() > sleepTimer + sleepDelay){
        sleepTimer = millis();
        clearTFT();
        prepareClock();
        currentMenu = -1;
        isrTimeChange = true; // to enable clock display
        return true;
    }   
    return false;
}

bool returnToMainMenu()
{   
    if (isrbuttonC  && (submenu.selectedItem == 0)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = MAINMENU_ITEMS;
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

// Clock and Display stuff
////////////////////////////////////////////////////////
void clearTFT()
{
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
        cCanvas.getTextBounds(timeString, 0, cCanvas.height()-CANVAS_FONT_MARGIN-MENULINEWIDTH, &x1, &y1, &w1, &h1);
        cCanvas.fillRect(x1, y1+h1+1, w1, MENULINEWIDTH, COLOR_TXT);
    }

}

void updateCanvasDate(GFXcanvas1& cCanvas, uint8_t cDay, uint8_t cMonth, uint16_t cYear, uint8_t underline)
{
    uint8_t cursorPos = cCanvas.height()-CANVAS_FONT_MARGIN-MENULINEWIDTH;
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cursorPos);
    sprintf(dateString,"%02d.%02d.%04d", cDay, cMonth, cYear);
    cCanvas.println(dateString);
    cCanvas.getTextBounds(dateString, 0, cursorPos, &x1, &y1, &w1, &h1);   
    if (underline == 0){
        return;
    }   
    if (underline & 1){
        // Underline day
        sprintf(dateString,"%02d", cDay);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1, y1+h2+1, w2, MENULINEWIDTH, COLOR_TXT); 
    }
    if (underline & 2){
        // Underline month
        int16_t w3;
        sprintf(dateString,"%02d", cMonth);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w3, &h2);
        sprintf(dateString,"%02d.%04d", cMonth, cYear);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1+w1-w2, y1+h2+1, w3, MENULINEWIDTH, COLOR_TXT);
    }      
    if (underline & 4){
        //underline year
        sprintf(dateString,"%04d", cYear);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1+w1-w2, y1+h2+1, w2, MENULINEWIDTH, COLOR_TXT);
    }
    if (underline & 8){
        //underline all
        cCanvas.fillRect(x1, y1+h1+1, w1, MENULINEWIDTH, COLOR_TXT);
    }
}

void updateCanvasMenuSetter(int signtype)
{
    canvasMenuLR.fillScreen(COLOR_BKGND);
    canvasMenuLR.setCursor(0, CANVAS_MENULR_HEIGHT);
    
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
        tft.drawBitmap(TFT_MARGIN_LEFT+CANVAS_MENUL_X, CANVAS_MENUL_Y+TFT_MARGIN_TOP, canvasMenuLR.getBuffer(), canvasMenuLR.width(), canvasMenuLR.height(), COLOR_TXT, COLOR_BKGND);
        updateCanvasMenuSetter(1);
        tft.drawBitmap(TFT_MARGIN_LEFT+CANVAS_MENUR_X, CANVAS_MENUR_Y+TFT_MARGIN_TOP, canvasMenuLR.getBuffer(), canvasMenuLR.width(), canvasMenuLR.height(), COLOR_TXT, COLOR_BKGND);
    }
    else{
        //TODO Replace with Hearts
        tft.fillCircle(CANVAS_MENUL_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, COLOR_RED);
        //tft.fillCircle(CANVAS_MENUL_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE-CENTER_ICON_LW, COLOR_BKGND);
        tft.fillCircle(CANVAS_MENUR_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, COLOR_BLUE);
        //tft.fillCircle(CANVAS_MENUR_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE-CENTER_ICON_LW, COLOR_BKGND);
    }
    tft.fillCircle(CENTER_ICON_X+TFT_MARGIN_LEFT, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, COLOR_TXT);
    tft.fillCircle(CENTER_ICON_X+TFT_MARGIN_LEFT, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE-CENTER_ICON_LW, COLOR_BKGND);
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
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_CLOCK_Y+TFT_MARGIN_TOP, 1);
       
    }
    if (clockHour != hour(t)){
        clockHour = hour(t);
        updateCanvasClock(canvasClock, clockHour, false);
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_CLOCK_Y+TFT_MARGIN_TOP, 0);
    }
    if (clockMinute != minute(t)){
        sprintf(outString, ("Moving stepper due to Minute: %02d, %02d", clockMinute, minute(t)));
        Serial.println(outString);
        clockMinute = minute(t);
        updateCanvasClock(canvasClock, clockMinute, false);
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_CLOCK_Y+TFT_MARGIN_TOP, 2);
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
        updateCanvasDate(canvasDate, clockDay, month(isrTime), year(isrTime), 0);
        tft.drawBitmap(CLOCKDISPLAY_DATE_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_DATE_Y+TFT_MARGIN_TOP, canvasDate.getBuffer(), canvasDate.width(), canvasDate.height(), COLOR_TXT, COLOR_BKGND);
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

// Move tower light
void updateTowerLight(time_t t){
	uint16_t towerPos;
	if (moveTower) {
		towerPos = ( 15 * (uint16_t)hour(t)) + ((uint16_t)minute(t) / 15) * 3; // 360°/24h plus 3° per 1/4 h
		stepper.newMoveToDegreeCCW(towerPos + towerPosOffset);
		stepperActive = true;
        Serial.println("Switching Tower Light");
		Serial.println(towerPos);
		Serial.println(stepper.getStep());
		Serial.println(stepper.getStepsLeft());
	}
	
}
// Interrupt functions
//////////////////////////////////////////////////////

void isrChangeTime()
{
    isrTimeChange = true;
}

// Button interrupt flags
///////////////////////////////////////////////////////

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

// Alarm functions
//////////////////////////////////////////////////////

		/////////////////////////////////////
		// TODO
		// Write almX.nextAlarm when an alarm is set in the menu
		/////////////////////////////////////

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

// Check if an alarm must be triggered
bool checkAlarms () 
{
	return (checkAlarm(alm1) || checkAlarm(alm2));
}

// General alarm check and reset for next day
bool checkAlarm (struct alarms &alm){
	// Weekday:
	// 1: Son, 2: Mon, 3: Tue, 4: Wed, 5: Thr, 6: Fr, 7: Sa
	if (alm.mode > 0 && alm.nextAlarm >= isrTime){ // Alarm is triggered
		switch (alm.mode){
			case 0: return false; break;				// Off: Not possible
			case 1: alm.mode = 0; break; 				// Once: Switch off
			case 2: alm.nextAlarm += 60*60*24; break;	// Every day: Add 24h
			case 3: if (weekday(isrTime)<5) {			// Weekdays: So..Thr: Add 24h
						alm.nextAlarm += 60*60*24;
					}else if (weekday(isrTime)==6) {		// Weekdays: Fr: Add 3 x 24h
						alm.nextAlarm += 60*60*24*3;
					}else if (weekday(isrTime)==7) {		// Weekdays: Fr: Add 2 x 24h
						alm.nextAlarm += 60*60*24*2;
					}
					break;
			case 4: if (weekday(isrTime)==0) {				// Weekend: So: Add 6 x 24h
						alm.nextAlarm += 60*60*24;
					}else if (weekday(isrTime)==7) {		// Weekend: Sa: Add 24h
						alm.nextAlarm += 60*60*24;
					}
					break;
		}
		return true;
	}
}
//FRAM functions
//////////////////////////////////////////////////////

// TODO : Alarm format changed
void writeAlarms (uint16_t address, struct alarms alm)
{
    fram.write(address, alm.hh);
    fram.write(address+1, alm.mm);
    fram.write(address+2, alm.dd);
    fram.write(address+3, alm.act);
    fram.write(address+4, alm.light);
    fram.write(address+5, alm.soundfile);
}

// TODO : Alarm format changed
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
