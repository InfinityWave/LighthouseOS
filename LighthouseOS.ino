
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
#include <DFRobotDFPlayerMini.h>
//#include <Fonts/FreeSans18pt7b.h>
#include "Adresses.h"  // FRAM Adresses



//DS3231 pins
#define DS3231_INT 2      // interrupt

//DCF77 pins
#define DCF_PIN 3         // Connection pin to DCF 77 device, interrupt
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
#define BTN_C 7           //interrupt
#define BTN_L A1           //interrupt
#define BTN_R A0           //interrupt

//LED pins
#define LED_BTN_C 6        //Button LED
#define LED_BTN_R 6        //Button LED
#define LED_BTN_L 6        //Button LED
#define LED_MAIN 5         //PWM pin
#define ALARM_LIGHT_MAX 255

// stepper pins
#define STEP_IN1 A7
#define STEP_IN2 A6
#define STEP_IN3 A3
#define STEP_IN4 A2

//Parameters
#define COLOR_BKGND ILI9341_BLACK // background color
#define COLOR_TXT ILI9341_WHITE // text color
#define COLOR_BLUE 0x001F
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0


/* Pixels for FontSize
Number "88":
12pt size 1: 24x17 
12pt size 2: 48x34
12pt size 3: 72x51
18pt size 1: 36x25 
18pt size 2: 72x50
Date "11.09.2021":
12pt size 1: 112x17 
12pt size 2: 224x34
18pt size 1: 165x25 
18pt size 2: 296x50
*/
// TFT&Canvas Settings
#define FONTSIZE_BIG_HEIGHT 51
#define FONTSIZE_NORMAL_HEIGHT 34
#define FONTSIZE_SMALL_HEIGHT 17
//#define FONTSIZE_TINY_HEIGHT 17
#define FONT_MARGIN 4
#define CANVAS_CLOCK_WIDTH 76
#define CANVAS_DATE_WIDTH 232
// Margins due to the surrounding wooden box. TODO Edit for final setup
#define TFT_MARGIN_LEFT 5
#define TFT_MARGIN_RIGHT 30
#define TFT_MARGIN_TOP 7 
#define TFT_MARGIN_BOT 25
// Canvas Settings for Left and Right Menu Icons
#define CANVAS_MENULR_WIDTH 45
#define CANVAS_MENULR_HEIGHT 25
//Global Position for Icon Canvas
#define CANVAS_MENULR_ICONOFFSET 12
#define CANVAS_MENUL_X 5 //Left Setter Icon X-POS
#define CANVAS_MENUL_Y 5 //Left Setter Icon Y-POS
#define CANVAS_MENUR_X 235 //Right Setter Icon X-POS
#define CANVAS_MENUR_Y 0 //Left Setter Icon Y-POS
#define CENTER_ICON_X 135 //Center Icon X-POS
#define CENTER_ICON_Y 13  //Center Icon Y-POS
#define CENTER_ICON_SIZE 12 //Center icon Size
#define CENTER_ICON_LW 4  //Linwidth of center icon
//ClockDisplaySettings
#define CLOCKDISPLAY_CLOCK_X 55 //Clock on std. display X-pos
#define CLOCKDISPLAY_CLOCK_Y 60 //Clock on std. display Y-pos
#define CLOCKDISPLAY_DATE_X 35  //Date on std. display X-pos
#define CLOCKDISPLAY_DATE_Y 125 //Date on std. display Y-pos
#define CLOCKDISPLAY_LH_X 0 //LighthouseOS txt Position on std. Display
#define CLOCKDISPLAY_SYNC_X 175 //DCF Sync Status X_pos on std. display
#define CLOCKDISPLAY_TEMP_X 245  //Temp. display x-pos on std. display

//MenuSettings
#define MENULINEWIDTH 3  // Standard Linewidth for all lines
#define MENU_TOPLINE_Y 50 // Position of the Topmost line. All other items are adjusted according to this
#define MENU_ITEMS 4  // Num Items to Display
#define MENU_ITEMS_X 15  // X-Offset for Items
#define MENU_SEL_X 10  //Selector Icon X-Pos
#define MENU_SEL_Y 133 //Selector Icon Y-Pos
#define MENU_SEL_SIZE 5 //Selector Icon Size
//SpecialMenuSettings
#define MAINMENU_ITEMS 6    // number of menu entries
#define MAINMENU_TXT_X 120
//SpecialSubMenuSettings
#define SUBMENU_MAX_ITEMS 8
#define SOUND_ITEMS 4
#define ALARM_ITEMS 4
#define ALARMTIME_ITEMS 5 // Number of modes for alarms 0: Off, 1: Once, 2: Every day, 3: Weekdays, 4: Weekend
#define CLOCK_ITEMS 2		

#define LITE 200  // standard brightness
#define LITE_MIN 10 //max display brightness
#define LITE_MAX 255 //max display brightness
#define VOLUME_MAX 30 //max volume setting

#define DCF_INT 6     // interval of dcf sync in hours
#define DCF_HOUR 0    // hour to start dcf sync
#define DCF_MIN 0     // minute to start dcf sync
#define DCF_LEN 3600    // length in seconds of dcf sync attempt

#define STEP_RPM 1
#define STEP_RPM_FAST 10
#define STEP_N 4096

// initializations
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC/*, TFT_RESET*/);

GFXcanvas1 canvasClock(CANVAS_CLOCK_WIDTH, FONTSIZE_BIG_HEIGHT+FONT_MARGIN+MENULINEWIDTH); // Canvas for Clock Numbers
GFXcanvas1 canvasDate(CANVAS_DATE_WIDTH, FONTSIZE_NORMAL_HEIGHT+FONT_MARGIN+MENULINEWIDTH); // Canvas for Date
//GFXcanvas1 canvasMenuLR(CANVAS_MENULR_WIDTH, CANVAS_MENULR_HEIGHT);
GFXcanvas1 canvasNormalFont(120, FONTSIZE_NORMAL_HEIGHT+3*FONT_MARGIN+MENULINEWIDTH); // Bigger Canvas not supported
GFXcanvas1 canvasSmallFont(160, FONTSIZE_SMALL_HEIGHT+2*FONT_MARGIN+MENULINEWIDTH);



//Variables to store size of display/canvas texts
int16_t  x1, y1;
uint16_t w1, h1;
uint16_t x2, y2;
uint16_t w2, h2;
int16_t cursorX, cursorY, cursorYMenuStart;
uint8_t currentMenuSetter = 0;

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
State* S11 = machine.addState(&stateCreditsMenu);

State* S99 = machine.addState(&stateAlarmActive); // State for active alarm

// Define Clock-HW
////////////////////////////////////////////////////////////
DS3232RTC myRTC(false);
bool rtcavailable;

DCF77 DCF = DCF77(DCF_PIN, digitalPinToInterrupt(DCF_PIN), false);

Adafruit_FRAM_I2C fram = Adafruit_FRAM_I2C();
bool framAvailable;

CheapStepper stepper (STEP_IN1, STEP_IN2, STEP_IN3, STEP_IN4); 


volatile time_t isrTime;
volatile bool isrTimeChange;
bool isrTimeUpdate;
char timeString[3];
char dateString[11];
char outString[100];
bool dcfSync = false;
bool dcfSyncSucc = false;
bool DCFSyncStatus = false;
bool DCFSyncChanged = false;
uint32_t dcfSyncStart = 0;

uint8_t clockHour, clockMinute, clockSecond, clockDay;
float currentTemp = 19.0;
bool tempChanged = false;

// MP3 Player
DFRobotDFPlayerMini myDFPlayer;

// button flags
volatile bool isrButtonC = false;
volatile bool isrButtonL = false;
volatile bool isrButtonR = false;

// menu entries
const char *mainMenuEntries[MAINMENU_ITEMS] = {"Clock", "Alarm 1", "Alarm 2", "Sound+Tower", "Credits", "Back"};
const char *soundOptions[SOUND_ITEMS] = {"Off", "Bell", "Horn", "Wedding"};
const char *alarmTimeOptions[ALARMTIME_ITEMS] = {"Off", "Once" ,"Every Day", "Mo-Fr", "Sa+So"}; // 0: Off, 1: Once, 2: Every day, 3: Weekdays, 4: Weekend
const char *alarmOptions[ALARM_ITEMS] = {"Off", "Light+Sound", "Sound only", "Light only"};
const char *clockOptions[CLOCK_ITEMS] = {"Off", "Motor"};
const char *dcfOptions[2] = {"DCF77 Off", "DCF77 On"};

    // Read settings from FRAM
uint16_t settingClock; 						// Setting from clockOptions
uint16_t settingVolume = 17;
uint16_t settingLEDBrightness;
uint16_t settingDisplayBrightness;
bool settingDCFEnabled = true;

//menu variables
int8_t selectedMMItem = 0;
uint32_t rtcSyncDelay = 10*60*1000; // 10min
uint32_t rtcSyncTimer = 0;
uint32_t sleepDelay = 60000;
uint32_t sleepTimer = 0;
bool updateScreen = true;
bool updateMenuSelection = false;
bool updateClockSign = false;
bool changeValue = false;

// Alarm variables
uint32_t alarmLightDelay = 1000;	// Time for tower light on (ms)
uint32_t alarmLightTimer = 0;		// Tower light times
bool alarmLightOn = false;			// If tower light is on
time_t isrTime_last = 0;			// Stores time to check if a new alarm check is needed
time_t tmpTime = 0;
tmElements_t tmp_tm;
uint32_t alarmTotalTimer = 0;		// Timer to stop alarm after some time (if noone is at home to press the button)
uint32_t alarmTotalDelay = 120000;	// 2 min
uint8_t alm_soundfile_actual = 0;
uint8_t alm_towermode_actual = 0;

// Stepper and tower variables
bool moveTower = true;
bool stepperActive = false;			// Does the stepper run? Do not command any new movement before the last one is finished
uint16_t towerPosOffset = 0; 		// 0 o'clock light position
uint16_t towerPos_last = 0; 		// To compare and check if a new position has to be commanded


//alarms
struct alarms {
    uint8_t hh;
    uint8_t mm;
    uint8_t towermode;	// What tower should do (all, turn+light, sound, off)
    uint8_t soundfile;	// Sound file to play
    time_t nextAlarm; // Time stamp of next alarm. Will not be stored on FRAM
	uint8_t mode; // 0: Off, 1: Once, 2: Every day, 3: Weekdays, 4: Weekend
} alm1, alm2;

// settings
uint8_t hours_per_day = 24; 

struct menuStruct {
    uint8_t num_items;
    uint8_t selectedItem = 0;
    uint8_t currentVal = 0;
    int16_t item[SUBMENU_MAX_ITEMS];
    //int16_t prev_item[SUBMENU_MAX_ITEMS];
    int8_t dateinfo_starts;
    int8_t timeinfo_starts;
    uint8_t maxVal[SUBMENU_MAX_ITEMS];
    uint8_t minVal[SUBMENU_MAX_ITEMS];
    uint8_t increment[SUBMENU_MAX_ITEMS];
} submenu;

bool weddingModeFinished = false;

void setup(void) {
    //Serial.begin(9600);
   
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
	S0->addTransition(&startAlarm,S99); // check and start alarm
    //S0->addTransition(&reAttachUnhandledInterrupts,S0);   //Must be last item
    // from standby
    S1->addTransition(&wakeup,S0);          // to clock
	S1->addTransition(&startAlarm,S99); // check and start alarm
    // from main menu
    S2->addTransition(&returnToClockDisplay,S0); //After timeout
    S2->addTransition(&closeMainMenu,S0);   // to clock
    S2->addTransition(&changeMainMenuSelection,S2); // stay and change selection
    S2->addTransition(&openClockMenu,S4);
    S2->addTransition(&openAlarm1Menu,S5);
    S2->addTransition(&openAlarm2Menu,S6);
    S2->addTransition(&openSoundMenu,S7);
    S2->addTransition(&openCreditsMenu,S11);
	S2->addTransition(&startAlarm,S99); // check and start alarm
    //S2->addTransition(&anyButtonPressed,S2);   //Must be last item
    // from wedding mode
    S3->addTransition(&exitWeddingMode,S0);
    //Returns from all SubMenus (Timeout and pressed Buttons and alarms)
    //Clock Menu
    S4->addTransition(&returnToClockDisplay,S0); //After timeout
    S4->addTransition(&returnToMainMenu,S2);
    S4->addTransition(&saveReturnToMainMenu,S2);
    S4->addTransition(&changeSubMenuSelection,S4);
    S4->addTransition(&startAlarm,S99); // check and start alarm
    //Alarm1 Menu
    S5->addTransition(&returnToClockDisplay,S0); //After timeout
    S5->addTransition(&returnToMainMenu,S2);
    S5->addTransition(&saveReturnToMainMenu,S2);
    S5->addTransition(&changeSubMenuSelection,S5);
	S5->addTransition(&startAlarm,S99); // check and start alarm    
    //Alarm2 Menu
    S6->addTransition(&returnToClockDisplay,S0); //After timeout
    S6->addTransition(&returnToMainMenu,S2);
    S6->addTransition(&saveReturnToMainMenu,S2);
    S6->addTransition(&changeSubMenuSelection,S6);
	S6->addTransition(&startAlarm,S99); // check and start alarm
    //Sound + Tower Menu
    S7->addTransition(&returnToClockDisplay,S0); //After timeout
    S7->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&saveReturnToMainMenu,S2);
    S7->addTransition(&changeSubMenuSelection,S7);
	S7->addTransition(&startAlarm,S99); // check and start alarm
    //Credits Menu
    S11->addTransition(&returnToClockDisplay,S0); //After timeout
	S11->addTransition(&anyButtonPressed,S2);
	S11->addTransition(&startAlarm,S99); // check and start alarm

	// Stop alarm and return to clock
	S99->addTransition(&stopAlarm,S2);
	S99->addTransition(&reAttachUnhandledInterrupts,S99);
	
    // TFT setup
    delay(500);
    tft.begin();
    tft.setTextColor(COLOR_TXT);
    tft.setRotation(3);
    set_default_font();
    //canvasClock.setFont(&FreeSans18pt7b);
    //canvasClock.setTextSize(2);
    canvasClock.setFont(&FreeSans12pt7b);
    canvasClock.setTextSize(3);
    
    canvasDate.setFont(&FreeSans12pt7b);
    canvasDate.setTextSize(2);
    //canvasMenuLR.setFont(&FreeSans18pt7b);
    //canvasMenuLR.setTextSize(2);
    //canvasMenuLR.setFont(&FreeSans12pt7b);
    //canvasMenuLR.setTextSize(3);
    canvasSmallFont.setFont(&FreeSans12pt7b);
    canvasSmallFont.setTextSize(1);
    //canvasNormalFont.setFont(&FreeSans18pt7b);
    //canvasNormalFont.setTextSize(1);
    canvasNormalFont.setFont(&FreeSans12pt7b);
    canvasNormalFont.setTextSize(2);    
   
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
    rtcavailable = true; 
    updateClock();
    if(isrTime > 0){
        //Serial.println("RTC set system time");
        rtcavailable = true;
    }
    else{
        //Serial.println("Unable to sync with RTC");
        rtcavailable = false;
    }
        
        
    prepareClock();

    // initialize FRAM
    if (fram.begin()){// you can stick the new i2c addr in here, e.g. begin(0x51);
        //Serial.println("Found FRAM");
        framAvailable = true;
    }
    else{
        //Serial.println("FRAM error");
        framAvailable = false;
    }

    // Read settings from FRAM
    settingClock = framread16bit(FRAM_CLOCK_SETTINGS); 
    settingVolume = framread16bit(FRAM_VOLUME);
    settingLEDBrightness = framread16bit(FRAM_LED_BRIGHTNESS);
    settingDisplayBrightness = framread16bit(FRAM_DISPLAY_BRIGHTNESS);
    // Override 0 Display brightness
    if (! settingDisplayBrightness){
        settingDisplayBrightness = LITE_MAX;
    }
    settingDCFEnabled = framread16bit(FRAM_DCF_ENABLE);

    // setup TFT Backlight as off
    pinMode(TFT_LITE, OUTPUT);
    //analogWrite(TFT_LITE, settingDisplayBrightness);
    digitalWrite(TFT_LITE, true);

    // setup tower LED
    pinMode(LED_MAIN, OUTPUT);
    analogWrite(LED_MAIN, 0);		


	// Read stored alarms from FRAM
	///////////////////////////////////
    readAlarms(FRAM_ALARM1, alm1);
    readAlarms(FRAM_ALARM2, alm2);
	
	// Set time value for alarms
	////////////////////////////////////
	// Note: isrTime must be correct!
	recalcAlarm(alm1);
	recalcAlarm(alm2);
	// Init Stepper
	//////////////////////////////////
    stepper.setStep(framread16bit(FRAM_C_STEP));
    
    // Buttons interrupt setup
    pinMode(BTN_C, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
    pinMode(BTN_R, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
    pinMode(BTN_L, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
    update_temperature();
    Serial1.begin(9600); //Hardware Serial for MP3
    myDFPlayer.begin(Serial1);
	clearTFT();
}

void loop()
{
    //tft.setCursor(100,100);
    //tft.println("Holzi und Kete");
	// Time from RTC
    if (isrTimeChange) {
		// Get new time from RTC, sets isrTime
        updateClock();
		// Check if tower has to be moved to new posistion and command movement
		updateTowerLight(isrTime);
    }
    // Trigger new read from RTC
    if (millis() - rtcSyncTimer > rtcSyncDelay){
        rtcSyncTimer = millis();
        isrTimeChange = true;
        update_temperature();
    }
	if (settingDCFEnabled)
    {
        // Time from DCF
        if (!dcfSync && hour(isrTime)%DCF_INT == DCF_HOUR && minute(isrTime) == DCF_MIN) {
            DCF.Start();
            digitalWrite(DCF_EN_PIN, LOW);
            dcfSync = true;
            dcfSyncStart = isrTime;
            //Serial.print("DCFSync start: ");
            //Serial.print(hour(isrTime));
            //Serial.print(":");
            //Serial.println(minute(isrTime));
        }
        if (dcfSync && isrTime >= dcfSyncStart+DCF_LEN) {
            DCF.Stop();
            digitalWrite(DCF_EN_PIN, HIGH);
            dcfSync = false;
            dcfSyncSucc = false;
            DCFSyncStatus = false;
            DCFSyncChanged = true;
            //Serial.println("Sync Failed");
        }
        if (dcfSync && DCF.getTime() != 0) {
            DCF.Stop();
            if (rtcavailable){
                myRTC.set(DCF.getTime());
            }
            updateClock();
            digitalWrite(DCF_EN_PIN, HIGH);
            dcfSync = false;
            dcfSyncSucc = true;
            DCFSyncStatus = true;
            DCFSyncChanged = true;
			recalcAlarm(alm1);
			recalcAlarm(alm2);
            //Serial.print("Sync Success");
            //Serial.print(hour(isrTime));
            //Serial.print(":");
            //Serial.println(minute(isrTime));
        }
    }
	// Run interface
	//////////////////////////
	// StateMachine
    machine.run();
	// Stepper
    if (moveTower) {
        stepper.run();
		// Is the stepper run finished (no stepps left)?
        if (stepperActive && stepper.getStepsLeft() == 0) {
            stepperActive = false;							// Not active any more
            stepper.off();									// TODO: Check if the stepper is switched on in newMoveDegreesCCW
        }
    }
}

void set_default_font(){
    tft.setFont(&FreeSans12pt7b);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TXT);
}

// Draw Menu Functions
///////////////////////////////////////////////////////////
void drawClockDisplayInfo(){
    tempChanged = false;
    DCFSyncChanged = false;
    
    int16_t ypos = TFT_HEIGHT - TFT_MARGIN_BOT - canvasSmallFont.height();
     
    tft.fillRect(0, ypos-MENULINEWIDTH, TFT_WIDTH, MENULINEWIDTH, COLOR_TXT);

    canvasSmallFont.fillScreen(COLOR_BKGND);
    canvasSmallFont.setCursor(0, FONTSIZE_SMALL_HEIGHT+FONT_MARGIN);
    canvasSmallFont.print("LighthouseOS");
    tft.drawBitmap(CLOCKDISPLAY_LH_X+TFT_MARGIN_LEFT, ypos, canvasSmallFont.getBuffer(), canvasSmallFont.width(), canvasSmallFont.height(), COLOR_TXT, COLOR_BKGND);

    if (DCFSyncStatus){
        canvasSmallFont.fillScreen(COLOR_BKGND);
        canvasSmallFont.setCursor(0, FONTSIZE_SMALL_HEIGHT+FONT_MARGIN);
        canvasSmallFont.print("DCF");
        tft.drawBitmap(CLOCKDISPLAY_SYNC_X+TFT_MARGIN_LEFT, ypos, canvasSmallFont.getBuffer(), canvasSmallFont.width(), canvasSmallFont.height(), COLOR_TXT, COLOR_BKGND);
    }
    canvasSmallFont.fillScreen(COLOR_BKGND);
    canvasSmallFont.setCursor(0, FONTSIZE_SMALL_HEIGHT+FONT_MARGIN);
    sprintf(outString, "%d", (int)currentTemp);
    canvasSmallFont.print(outString);
    //canvasSmallFont.print((char)247); //Not supported by non standard Font
    canvasSmallFont.print("C");
    tft.drawBitmap(CLOCKDISPLAY_TEMP_X+TFT_MARGIN_LEFT, ypos, canvasSmallFont.getBuffer(), canvasSmallFont.width(), canvasSmallFont.height(), COLOR_TXT, COLOR_BKGND);
}

void drawSubMenuEntry(const char **menuOptions, uint8_t menuItem)
{
    if (updateScreen || submenu.selectedItem==menuItem){
        drawSubMenuEntryText(menuOptions[submenu.item[menuItem]], menuItem);
    }
    else {
    cursorY = cursorY + canvasSmallFont.height();
    }
}

void drawSubMenuEntryText(const char *menuText, uint8_t menuItem)
{
    updateCanvasText(canvasSmallFont, menuText, submenu.selectedItem==menuItem);
    tft.drawBitmap(cursorX, cursorY, canvasSmallFont.getBuffer(), canvasSmallFont.width(), canvasSmallFont.height(), COLOR_TXT, COLOR_BKGND);
    cursorY = cursorY + canvasSmallFont.height();
}

// Draw menu to set alarms 
void drawAlarmMenu(){
    if (updateScreen) {
        updateMenuSelection = true;
    }
    if (updateMenuSelection){      
        cursorY = cursorYMenuStart;
        cursorX = CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT;
        uint8_t underline = 0;
        // Draw Clock Setter
        if (updateScreen || submenu.selectedItem==0 || submenu.selectedItem==1){
            if (submenu.selectedItem==0 || submenu.selectedItem==1){underline=submenu.selectedItem+1;}
            drawClock(canvasSmallFont, submenu.item[0], submenu.item[1], cursorX, cursorY, underline);      
        }
        cursorY = cursorY + canvasSmallFont.height();
        drawSubMenuEntry(alarmTimeOptions, 2);
        drawSubMenuEntry(alarmOptions, 3);
        drawSubMenuEntry(soundOptions, 4);
        
        /*const char **menuOptions=NULL;
        for (int i=2; i<5; i++)
        {   
            switch (i){
                case 2: menuOptions = alarmTimeOptions; break;
                case 3: menuOptions = alarmOptions; break;
                case 4: menuOptions = soundOptions; break;
            }
            drawSubMenuEntry(menuOptions, i);
            if (updateScreen || submenu.selectedItem==i){
                updateCanvasText(canvasSmallFont, menuOptions[submenu.item[i]], submenu.selectedItem==i);
                tft.drawBitmap(cursorX, cursorY, canvasSmallFont.getBuffer(), canvasSmallFont.width(), canvasSmallFont.height(), COLOR_TXT, COLOR_BKGND);
            }
        }*/
        updateScreen = false;
        updateMenuSelection = false;
    }
}

void drawMenuTop(char *menuName)
{
    // Draw Top Icons
    drawMenuSetter(2+4);
    cursorY = TFT_MARGIN_TOP+MENU_TOPLINE_Y;
    tft.fillRect(0,cursorY,TFT_WIDTH,MENULINEWIDTH,COLOR_TXT);
    cursorY = cursorY + MENULINEWIDTH+FONT_MARGIN+FONTSIZE_SMALL_HEIGHT+2;
    // Write Center Entry
    tft.getTextBounds(menuName, 0, cursorY, &x1, &y1, &w1, &h1);
    cursorX = (uint8_t) ((TFT_WIDTH - TFT_MARGIN_LEFT - TFT_MARGIN_RIGHT - w1) / 2);
    tft.setCursor(cursorX, cursorY);
    tft.println(menuName);      
    tft.fillRect(0,y1+h1+6,TFT_WIDTH,MENULINEWIDTH,COLOR_TXT); 
    cursorYMenuStart = y1+h1+6+MENULINEWIDTH+6;
}
//StateMachine States
////////////////////////////////////////////////////////////
//S0 = main clock display
void stateClockDisplay()
{
    if (updateScreen) {
        //Serial.println("Entering ClockDisplay");
        clearTFT();
        updateScreen = false;
        updateClockSign = true;
        drawClockDisplayInfo();
        drawMenuSetter(1+4);
        tft.fillRect(0,MENU_TOPLINE_Y+TFT_MARGIN_TOP,TFT_WIDTH,MENULINEWIDTH,COLOR_TXT);
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
	  analogWrite(LED_MAIN, settingLEDBrightness);	// Switch tower light
	  digitalWrite(LED_BTN_C, HIGH);			// Switch btn LEDs on
	  digitalWrite(LED_BTN_R, HIGH);			// Switch btn LEDs on
	  digitalWrite(LED_BTN_L, HIGH);			// Switch btn LEDs on
	  alarmLightTimer = millis();				// (Re-)Start timer
    }
	if (millis() - alarmLightTimer > alarmLightDelay){ // Time is up...
		alarmLightOn = false; 						// Turn on light flag
		analogWrite(LED_MAIN, 0);					// Switch tower light
		digitalWrite(LED_BTN_C, LOW);				// Switch btn LEDs off
		digitalWrite(LED_BTN_R, LOW);				// Switch btn LEDs off
		digitalWrite(LED_BTN_L, LOW);				// Switch btn LEDs off
	}
}

// S1 = standby state
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
	// Continue showing the clock
	if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
        
        //analogWrite(TFT_LITE, settingDisplayBrightness);
        digitalWrite(TFT_LITE, true);
        
	}
	// Switch on and off the tower LED
	if (millis() - alarmLightTimer > alarmLightDelay) {
		alarmLightTimer = millis();								// Restart timer
		alarmLightOn = not alarmLightOn;						// Switch state of light
		analogWrite(LED_MAIN, settingLEDBrightness * alarmLightOn);	// Switch tower light
	}
	// Move the tower in full revs
	if (stepperActive == false){								// Only command a new movemoent if nothing is running
		stepperActive	= true;									// Set flag to make sure no other movement is commanded
		stepper.newMoveDegreesCCW(360);							// Command one full rev
	}
}

//S2 = main menu
void stateMainMenu()
{
    if (updateScreen) {
        updateScreen = false;        
        drawMenuTop("Main Menu");
        updateMenuSelection = true;
        tft.fillCircle(MENU_SEL_X,TFT_MARGIN_TOP+MENU_SEL_Y,MENU_SEL_SIZE,COLOR_TXT);
    }
    if (updateMenuSelection){
        cursorY = cursorYMenuStart;
        for (int i=0; i<MENU_ITEMS; i++){
            //Serial.println(outString);
            canvasSmallFont.fillScreen(COLOR_BKGND);
            canvasSmallFont.setCursor(0,FONT_MARGIN + FONTSIZE_SMALL_HEIGHT);
            canvasSmallFont.println(mainMenuEntries[(selectedMMItem+MAINMENU_ITEMS-1+i)%MAINMENU_ITEMS]);
            tft.drawBitmap(TFT_MARGIN_LEFT+MENU_ITEMS_X, cursorY, canvasSmallFont.getBuffer(), canvasSmallFont.width(), canvasSmallFont.height(), COLOR_TXT, COLOR_BKGND);
            cursorY = cursorY + canvasSmallFont.height();
        updateMenuSelection = false;
        }
    }
	// Switch off tower light (if left on in other states like S11, Credits or S0)
	if (alarmLightOn){ // if light is still on
		alarmLightOn = false; 						// Turn on light flag
		analogWrite(LED_MAIN, 0);					// Switch tower light
	}
}


//S3
void stateWeddingMode()
{
    // TODO
    //Switch Music On
    //Switch Motor On
    //Switch Main Light On
    //Blink auxiliary lights
    //Display something
    
}

//S4
void stateClockMenu()
{
    if (updateScreen) {
        updateMenuSelection = true;
    }
    if (updateMenuSelection){      
        cursorY = cursorYMenuStart;
        cursorX = CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT;
        uint8_t underline = 0;
        // Draw Clock Setter
        if (updateScreen || submenu.selectedItem==0 || submenu.selectedItem==1){
            if (submenu.selectedItem==0 || submenu.selectedItem==1){underline=submenu.selectedItem+1;}
            drawClock(canvasSmallFont, submenu.item[0], submenu.item[1], cursorX, cursorY, underline);      
        }
        // Draw Date Setter
        cursorY = cursorY + canvasSmallFont.height();
        if (updateScreen || submenu.selectedItem==2 || submenu.selectedItem==3 || submenu.selectedItem==4){
            switch (submenu.selectedItem){
                case 2: underline = 1; break;
                case 3: underline = 2; break;
                case 4: underline = 4; break;
                default: underline = 0; break;
            }
            updateCanvasDate(canvasSmallFont, submenu.item[2], submenu.item[3], submenu.item[4], underline);
            tft.drawBitmap(cursorX, cursorY, canvasSmallFont.getBuffer(), canvasSmallFont.width(), canvasSmallFont.height(), COLOR_TXT, COLOR_BKGND);
         }
        cursorY = cursorY + canvasSmallFont.height();
        drawSubMenuEntry(clockOptions, 5);
        drawSubMenuEntry(dcfOptions, 6);

        updateScreen = false;
        updateMenuSelection = false;
    }
}



//S5
void stateAlarm1Menu()
{
    drawAlarmMenu();
}

//S6
void stateAlarm2Menu()
{
    drawAlarmMenu();
}

//S7
void stateSoundMenu(const char *menuOptions)
{
    if (updateScreen) {
        updateMenuSelection = true;
    }
    if (updateMenuSelection){
        cursorY = cursorYMenuStart;
        cursorX = CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT;
        sprintf(outString, "Volume %d%%", (100*submenu.item[0]/VOLUME_MAX));
        drawSubMenuEntryText(outString, 0);
		drawSubMenuEntry(clockOptions, 1);
        sprintf(outString, "LED %d%%", (100*submenu.item[2]/ALARM_LIGHT_MAX));
        drawSubMenuEntryText(outString, 2);
        updateScreen = false;
        updateMenuSelection = false;
    }
    //Set Volume 0 - 30
}

//S11
void stateCreditsMenu()
{
    if (updateScreen) {
        updateScreen = false;   
        tft.setFont(&FreeSans12pt7b);
        tft.setTextSize(1);
        tft.setCursor(0, cursorYMenuStart+FONTSIZE_SMALL_HEIGHT);
        tft.println("  Frauke");
        tft.println("  Christian");
        //tft.setFont();
        //tft.setTextSize(2);
        //tft.println(" ");
        tft.setFont(&FreeSans12pt7b);
        tft.setTextSize(1);
        tft.println("  Hans");
        tft.println("  Ben");
        set_default_font();
    }
	// Switch on and off the tower LED
	if (millis() - alarmLightTimer > alarmLightDelay) {
		alarmLightTimer = millis();								// Restart timer
		alarmLightOn = not alarmLightOn;						// Switch state of light
		analogWrite(LED_MAIN, settingLEDBrightness * alarmLightOn);	// Switch tower light
	}
	// Move the tower in full revs
	if (stepperActive == false){								// Only command a new movemoent if nothing is running
		stepperActive	= true;									// Set flag to make sure no other movement is commanded
		stepper.newMoveDegreesCCW(360);							// Command one full rev
	}
}

//StateMachine Transitions
///////////////////////////////////////////////
bool toSleep()
{
    if (millis() - sleepTimer >  + sleepDelay) {
        analogWrite(TFT_LITE, 0);
        //digitalWrite(TFT_LITE, 0);
        return true;
    }
    return false;
}

bool wakeup()
{
    if (isrButtonC||isrButtonR||isrButtonL) {
      // Following section uncommented to save pressed buttons after WakeUP
      //isrButtonC = false;
      //isrButtonR = false;
      //isrButtonL = false;
      sleepTimer = millis();
      isrTimeUpdate = true;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
      //analogWrite(TFT_LITE, settingDisplayBrightness);
      digitalWrite(TFT_LITE, true);
      return true;
    }
    return false;
}

bool openMainMenu()
{
    if (isrButtonC) {
      isrButtonC = false;
      sleepTimer = millis();
      clearTFT();
      selectedMMItem = MAINMENU_ITEMS-1;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      return true;
    }
    return false;
}

bool changeMainMenuSelection()
{
    if(isrButtonR){
        isrButtonR = false;
        sleepTimer = millis();
        updateMenuSelection = true;
        selectedMMItem = (selectedMMItem+1)%MAINMENU_ITEMS;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        return true;
    }
    if(isrButtonL){
        isrButtonL = false;
        sleepTimer = millis();
        updateMenuSelection = true;
        selectedMMItem = (selectedMMItem+MAINMENU_ITEMS-1)%MAINMENU_ITEMS;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        return true;
    }
    return false;
}

bool closeMainMenu()
{
    if (isrButtonC) {
      if (selectedMMItem == (MAINMENU_ITEMS-1)) {
          sleepTimer = millis();
          isrButtonC = false;
          clearTFT();
          prepareClock();
          isrTimeChange = true; // to enable clock display
          delay(50);
          attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
          return true;
          }
    }
    return false;
}

// Stop alarm when pressing L-Button
bool stopAlarm()
{
	// Stop-Alam button was pressed OR on timeout
	if ((isrButtonL)||(millis()-alarmTotalTimer > alarmTotalDelay)) {
      isrButtonL = false;		// Reset Btn-flag
      isrTimeUpdate = true;		// ??
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
	  analogWrite(LED_MAIN, 0); 			// Switch off tower light
	  alarmLightOn = false;					// Switch off tower light (flag)
	  digitalWrite(LED_BTN_C, LOW);			// Switch btn LEDs off
	  digitalWrite(LED_BTN_R, LOW);			// Switch btn LEDs off
	  digitalWrite(LED_BTN_L, LOW);			// Switch btn LEDs off
	  myDFPlayer.stop();					// Stop MP3-Player

      return true;
    }
    return false;
}

// Start alarm when time has come
bool startAlarm()
{
	if (checkAlarms()) {
		// Light + Sound OR Light Only
		if ((alm_towermode_actual == 1)||(alm_towermode_actual == 3)){
			digitalWrite(LED_BTN_C, HIGH);			// Switch btn LEDs on
			digitalWrite(LED_BTN_R, HIGH);			// Switch btn LEDs on
			digitalWrite(LED_BTN_L, HIGH);			// Switch btn LEDs on
			alarmTotalTimer = millis();				// Start timer for max alarm time
			alarmLightTimer = millis();				// Start timer for light
			alarmLightOn = true;					// Switch on tower light (flag)
			analogWrite(LED_MAIN, settingLEDBrightness);	// Switch on tower light (real)
		}
		// Light + Sound OR Sound Only
		if ((alm_towermode_actual == 1)||(alm_towermode_actual == 2)){
			myDFPlayer.volume(settingVolume);		// Set sound Volume
			myDFPlayer.loop(alm_soundfile_actual);		// Start MP3-Player (actual sound is selected in checkAlarms)
		}
		return true;
    }
	return false;
}


bool returnToClockDisplay()
{
    if (millis() - sleepTimer > sleepDelay){
        sleepTimer = millis();
        clearTFT();
        prepareClock();
        isrTimeChange = true; // to enable clock display
        return true;
    }   
    return false;
}

//////////////////////
// Begin Section for SubMenus
//
// Return without saving changes
bool openClockMenu()
{
    if (isrButtonC  && (selectedMMItem == 0)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openAlarm1Menu()
{
    if (isrButtonC  && (selectedMMItem == 1)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openAlarm2Menu()
{
    if (isrButtonC  && (selectedMMItem == 2)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openSoundMenu()
{
    if (isrButtonC  && (selectedMMItem == 3)){
        openSuBMenu();
        return true;   
    }
    return false;
}

// Credits (with tower light on)
bool openCreditsMenu()
{
    if (isrButtonC  && (selectedMMItem == 4)){
        openSuBMenu();
		alarmLightTimer = millis();				// Start timer for light
		alarmLightOn = true;					// Switch on tower light (flag)
		analogWrite(LED_MAIN, settingLEDBrightness);	// Switch on tower light (real)
        return true;   
    }
    return false;
}

void updateSubMenuCenterIcon()
{
    if (submenu.selectedItem==(submenu.num_items+1)){
        drawMenuSetter(16);
    }
    if (submenu.selectedItem==submenu.num_items){
        drawMenuSetter(8);
    }
    if (submenu.selectedItem<submenu.num_items){
        drawMenuSetter(2+4);
    }
}

bool returnToMainMenu()
{   
    //Return to main menu without saving
    if ((isrButtonL || isrButtonR)  && (submenu.selectedItem == (submenu.num_items+1))){
        sleepTimer = millis();
        clearTFT();
        reAttachUnhandledInterrupts();
        //Serial.println("Return to MM");
        return true;   
    }
    return false;
}

// Save changes to FRAM and apply to system + return
bool saveReturnToMainMenu()
{   
    //Save and return to main menu, with center button and last item in menu
    if ((isrButtonL || isrButtonR)  && (submenu.selectedItem == (submenu.num_items))){
        sleepTimer = millis();
        switch (selectedMMItem){
            case 0:
                //ClockMenu
                tmp_tm.Hour = submenu.item[0];
                tmp_tm.Minute = submenu.item[1];
                tmp_tm.Second = 0;
                tmp_tm.Day = submenu.item[2];
                tmp_tm.Month = submenu.item[3];
                tmp_tm.Year = submenu.item[4]-1970;
	            tmpTime = makeTime(tmp_tm);
                settingClock = submenu.item[5];               
                framwrite16bit(FRAM_CLOCK_SETTINGS, settingClock);
                settingDCFEnabled = (bool)submenu.item[6];               
                framwrite16bit(FRAM_DCF_ENABLE, settingDCFEnabled);
                isrTime = tmpTime;
                if (rtcavailable){
                    myRTC.set(tmpTime);
                }
                DCFSyncStatus = false;
                DCFSyncChanged = true;
                updateClock();
				recalcAlarm(alm1);
				recalcAlarm(alm2);
                break;
            case 1:
                //Alarm 1 menu
				alm1.hh = submenu.item[0];
				alm1.mm = submenu.item[1];
				alm1.mode = submenu.item[2];
                alm1.towermode = submenu.item[3];
                alm1.soundfile = submenu.item[4];
                // Write alarm to FRAM
                writeAlarms(FRAM_ALARM1, alm1);
                // Update Alarms for next call
                recalcAlarm(alm1);
                break;
            case 2:
                //Alarm 2 menu
				alm2.hh = submenu.item[0];
				alm2.mm = submenu.item[1];
				alm2.mode = submenu.item[2];
                alm2.towermode = submenu.item[3];
                alm2.soundfile = submenu.item[4];
                // Write alarms to FRAM
                writeAlarms(FRAM_ALARM2, alm2);
                // Update Alarms for next call
                recalcAlarm(alm2);
                break;            
            case 3:
                //Sound Menu
                settingVolume = submenu.item[0];
                framwrite16bit(FRAM_VOLUME, settingVolume);
				settingClock = submenu.item[1];
				framwrite16bit(FRAM_CLOCK_SETTINGS, settingClock);
                settingLEDBrightness = submenu.item[1];
				framwrite16bit(FRAM_LED_BRIGHTNESS, settingLEDBrightness);
            case 8:
                //Credits
                submenu.num_items = 0;
                break;


        }
        clearTFT();
        reAttachUnhandledInterrupts();
        //Serial.println("Save and Return to MM");
        return true;   
    }
    return false;
}

void openSuBMenu()
{
    sleepTimer = millis();
    isrButtonC = false;
    // Init
    submenu.dateinfo_starts = -1;  // define whether there is a day to check and its pos in submenu
    submenu.timeinfo_starts = -1;  // define whether thers is time to check an position in submenu
    for (int i=0; i<SUBMENU_MAX_ITEMS; i++){
        submenu.maxVal[i] = 0;
        submenu.minVal[i] = 0;
        submenu.increment[i] = 1;
    }
    switch (selectedMMItem){
        case 0:
            //ClockMenu
            submenu.num_items = 7;
            submenu.dateinfo_starts = 0;
            submenu.item[0] = hour(isrTime);
            submenu.item[1] = minute(isrTime);
            submenu.item[2] = day(isrTime);
            submenu.item[3] = month(isrTime);
            submenu.item[4] = year(isrTime);
            submenu.item[5] = settingClock;
            submenu.maxVal[5]=CLOCK_ITEMS-1;
            submenu.item[6] = (int16_t)settingDCFEnabled;
            submenu.maxVal[6]=1;
            break;
		case 1:
            //Alarm1
            submenu.num_items = 5; 
            submenu.timeinfo_starts = 0; // Only time, no date
            submenu.item[0] = alm1.hh;
            submenu.item[1] = alm1.mm;
            submenu.item[2] = alm1.mode;
            submenu.maxVal[2] = ALARMTIME_ITEMS-1;
            submenu.item[3] = alm1.towermode;
			submenu.maxVal[3] = ALARM_ITEMS-1;
            submenu.item[4] = alm1.soundfile;
			submenu.maxVal[4] = SOUND_ITEMS-1;
            break;
		case 2:
            //Alarm2
            submenu.num_items = 5; 
            submenu.timeinfo_starts = 0; // Only time, no date
            submenu.item[0] = alm2.hh;
            submenu.item[1] = alm2.mm;
            submenu.item[2] = alm2.mode;
            submenu.maxVal[2] = ALARMTIME_ITEMS-1;
            submenu.item[3] = alm2.towermode;
			submenu.maxVal[3] = ALARM_ITEMS-1;
            submenu.item[4] = alm2.soundfile;
			submenu.maxVal[4] = SOUND_ITEMS-1;
            break;
		case 3:
            //Sound
            submenu.num_items = 3; 
            submenu.item[0] = settingVolume;
			submenu.maxVal[0] = VOLUME_MAX;
            submenu.item[1] = settingClock;
			submenu.maxVal[1] = CLOCK_ITEMS-1;
            submenu.item[2] = settingLEDBrightness;
			submenu.maxVal[2] = ALARM_LIGHT_MAX;
            submenu.increment[2]=25;

            break;

        case 4:
            //Credits
            submenu.num_items = 0;
    }
    for (int i=0; i <submenu.num_items; i++){
        checkSubMenuItemValidity(i);
    }
    // Begin with selection of last item, which should be save and exit
    submenu.selectedItem = submenu.num_items+1;
    clearTFT();
    drawMenuTop(mainMenuEntries[selectedMMItem]);
    updateSubMenuCenterIcon();
    delay(50);
    attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
}

bool changeSubMenuSelection()
{
    if (isrButtonC){
        isrButtonC = false;
        sleepTimer = millis();
        submenu.selectedItem = (submenu.selectedItem + 1) % (submenu.num_items+2);  
        updateScreen = true;
        updateSubMenuCenterIcon();
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;
    }
    if (isrButtonL || isrButtonR) {
        sleepTimer = millis();
        // Handle normal increment of currently selected value
        updateMenuSelection = true;
        if (isrButtonL){
            isrButtonL = false;
            submenu.item[submenu.selectedItem] = submenu.item[submenu.selectedItem]-submenu.increment[submenu.selectedItem];
            delay(50);
            attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        }
        else {
            isrButtonR = false;
            submenu.item[submenu.selectedItem] = submenu.item[submenu.selectedItem]+submenu.increment[submenu.selectedItem];
            delay(50);
            attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        }
        checkSubMenuItemValidity(submenu.selectedItem);
            return true;
        }
    return false;
}

void checkSubMenuItemValidity(uint8_t selectedItem)
{
    // Correct date if enabled for this submenu
    if (submenu.dateinfo_starts >=0){
        // Check if selected item is a date
        if ((submenu.dateinfo_starts <= selectedItem) && (submenu.dateinfo_starts+5 > selectedItem)){
            correct_date(submenu.item + submenu.dateinfo_starts);
            return;
        }
    }
    // Correct time if enabled for this submenu
    if (submenu.timeinfo_starts >=0){
        // Check if selected item is a time
        if ((submenu.timeinfo_starts <= selectedItem) && (submenu.timeinfo_starts+2 > selectedItem)){
            correct_time(submenu.item + submenu.timeinfo_starts);
            return;
        }
    }     
    // Check Max Value
    if (submenu.item[selectedItem] > submenu.maxVal[selectedItem]){
        submenu.item[selectedItem] = submenu.minVal[selectedItem];
    }
    // Check Min Value
    if (submenu.item[selectedItem] < submenu.minVal[selectedItem]){
        submenu.item[selectedItem] = submenu.maxVal[selectedItem];
    }
    return;

}

// Helper functions and wedding mode
///////////////////////////////
void correct_date(int16_t *date)
{
    // date must be an array with 1st entry hour, than minute, day, month, year

    // Check if leap year
    int leapyear = 0;
    if (!(date[4] % 4)){
        leapyear = 1;
    }
    if (date[4] < 1970){date[4]=1970;}
    // Check month overflow
    if (date[3] < 1 ){date[3] = 12;}
    if (date[3] > 12){date[3] = 1;}
    // check correct number of days for month
    // Begin with month with 31 days
    uint8_t daysPerMonth;
    if (date[3]==1 || date[3]==3 || date[3]==5 || date[3]==7 || date[3]==8 || date[3]==10 || date[3]==12){
        daysPerMonth = 31;
    }
    // Month with 30 days
    if (date[3]==4 || date[3]==6 || date[3]==9 || date[3]==11){
        daysPerMonth = 30;
    }
    // February
    if (date[3]==2){
        daysPerMonth = 28 + leapyear;
    }
        if(date[2] < 1){date[2] = daysPerMonth;}
        if (date[2] > daysPerMonth){date[2] = 1;}

    // Correct hour over and underflow
    correct_time(date);
}

void correct_time(int16_t *dtime)
{
    // Check the time for over and underflows
    // date must be an array with 1st entry hour, than minute
    if (dtime[0] < 0){dtime[0] = hours_per_day-1;}
    if (dtime[0] > hours_per_day-1){dtime[0] = 0;}
    if (dtime[1] < 0){dtime[1] = 59;}
    if (dtime[1] > 59){dtime[1] = 0;}
}

bool changeToWeddingMode()
{
    if(isrButtonL){
        isrButtonL = false;    
        delay(100);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        //Serial.println("Entering WeddingMode");
        stepper.setRpm(STEP_RPM_FAST);
        moveTower = true;
        stepperActive = true;
        stepper.newMove(false, 36000);
        //Serial.println("Start Player");
        analogWrite(LED_MAIN, settingLEDBrightness);
        analogWrite(LED_BTN_C, 255);
        myDFPlayer.volume(settingVolume);
        myDFPlayer.loop(1);
        return true;
    }
}

bool exitWeddingMode()
{
    bool button_pressed = anyButtonPressed();
    if(button_pressed){
        //Serial.println("Exit WeddingMode");
        stepper.setRpm(STEP_RPM);
        analogWrite(LED_BTN_C, 0);
        myDFPlayer.stop();
        moveTower = false;
        stepperActive = false;
        //stepper.newMoveDegreesCCW(360);
        analogWrite(LED_MAIN, 0);
    }
   return button_pressed;
}

bool anyButtonPressed()
{
    bool buttonPressed = false;
    if(isrButtonR){
        isrButtonR = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        buttonPressed = true;
    }
    if(isrButtonL){
        isrButtonL = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        buttonPressed = true;
    }
    if(isrButtonC){
        isrButtonC = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        buttonPressed = true;
    }
    if (buttonPressed){
        sleepTimer = millis();
        clearTFT();
    }
    return buttonPressed;
}

bool reAttachUnhandledInterrupts()
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
    if(isrButtonC){
        isrButtonC = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
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
    if (rtcavailable){
        isrTime = myRTC.get();
        myRTC.alarm(ALARM_2);
    }
    update_temperature();
}

void updateCanvasText(GFXcanvas1& cCanvas, char *text, bool underline)
{
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH);
    cCanvas.println(text);
    if (underline)
    {
        cCanvas.getTextBounds(text, 0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH, &x1, &y1, &w1, &h1);
        cCanvas.getTextBounds("8", 0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1, y2+h2+1, w1, MENULINEWIDTH, COLOR_TXT);
    }
}

void updateCanvasClock(GFXcanvas1& cCanvas, uint8_t cDigit, bool underline)
{
    sprintf(timeString,"%02d",cDigit);
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH);
    cCanvas.println(timeString);
    if (underline)
    {
        cCanvas.getTextBounds(timeString, 0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH, &x1, &y1, &w1, &h1);
        cCanvas.fillRect(x1, y1+h1+1, w1, MENULINEWIDTH, COLOR_TXT);
    }
}

void updateCanvasDate(GFXcanvas1& cCanvas, uint8_t cDay, uint8_t cMonth, uint16_t cYear, uint8_t underline)
{
    uint8_t cursorPos = cCanvas.height()-FONT_MARGIN-MENULINEWIDTH;
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

void drawClock(GFXcanvas1& cCanvas, uint8_t cHour, uint8_t cMinute, int16_t xPos, int16_t yPos, uint8_t underline)
{     
    uint8_t cursorPos = cCanvas.height()-FONT_MARGIN-MENULINEWIDTH;
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cursorPos);
    sprintf(dateString,"%02d:%02d", cHour, cMinute);
    cCanvas.println(dateString);
    cCanvas.getTextBounds(dateString, 0, cursorPos, &x1, &y1, &w1, &h1);   
    sprintf(outString, "Underline Setting %d", underline);
    //Serial.println(outString);
    if (underline == 0){}   
    if (underline & 1){
        // Underline hour
        //Serial.println("Underline Hour");
        sprintf(dateString,"%02d", cHour);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1, y1+h2+1, w2, MENULINEWIDTH, COLOR_TXT); 
    } 
    if (underline & 2){
        //underline day
        //Serial.println("Underline Minute");
        sprintf(dateString,"%02d", cMinute);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1+w1-w2, y1+h2+1, w2, MENULINEWIDTH, COLOR_TXT);
    }
    if (underline & 4){
        //underline all
        cCanvas.fillRect(x1, y1+h1+1, w1, MENULINEWIDTH, COLOR_TXT);
    }
    tft.drawBitmap(xPos, yPos, cCanvas.getBuffer(), cCanvas.width(), cCanvas.height(), COLOR_TXT, COLOR_BKGND); 
}

void drawMenuSetter(uint8_t isOn)
{
    if (isOn != currentMenuSetter){
        currentMenuSetter = isOn;
        // Blank top part of display
        tft.fillRect(0, 0, TFT_WIDTH, TFT_MARGIN_TOP+MENU_TOPLINE_Y, COLOR_BKGND);
        if (isOn & 1){
            // Left Right Icons for Clock Display
            //TODO Replace with Hearts
            tft.fillCircle(CANVAS_MENUL_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, COLOR_RED);
            //tft.fillCircle(CANVAS_MENUL_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE-CENTER_ICON_LW, COLOR_BKGND);
            tft.fillCircle(CANVAS_MENUR_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, COLOR_BLUE);
            //tft.fillCircle(CANVAS_MENUR_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE-CENTER_ICON_LW, COLOR_BKGND);
        }
        if (isOn & 2){
            // Left Right Icons for Menus
            tft.setFont(&FreeSans12pt7b);
            tft.setTextSize(3);
            tft.setCursor(TFT_MARGIN_LEFT+CANVAS_MENUL_X, TFT_MARGIN_TOP+CANVAS_MENUL_Y+CANVAS_MENULR_HEIGHT);
            tft.println('-');
            tft.setCursor(TFT_MARGIN_LEFT+CANVAS_MENUR_X, TFT_MARGIN_TOP+CANVAS_MENUR_Y+CANVAS_MENULR_HEIGHT);
            tft.println('+');
            set_default_font();
        }
        if (isOn & 4){
            tft.fillCircle(CENTER_ICON_X+TFT_MARGIN_LEFT, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, COLOR_TXT);
            tft.fillCircle(CENTER_ICON_X+TFT_MARGIN_LEFT, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE-CENTER_ICON_LW, COLOR_BKGND);
        }
        // ToDo Ensure old Text is overwritten/blanked
        if ((isOn & 8) || (isOn & 16)){
                uint16_t tcolor; 
            if (isOn & 8){
                // Center Icon Save
                sprintf(outString, "Save");
                tcolor = COLOR_GREEN;
            }
            else {
                // Center Icon Cancel
                sprintf(outString, "Return");
                tcolor = COLOR_RED;
            }
            canvasSmallFont.getTextBounds(outString, 0, canvasSmallFont.height()-FONT_MARGIN-MENULINEWIDTH, &x1, &y1, &w1, &h1);
            cursorX = CENTER_ICON_X+TFT_MARGIN_LEFT - (uint8_t)((x1+w1)/2) - CANVAS_MENUL_X - CANVAS_MENULR_WIDTH;
            canvasSmallFont.fillScreen(COLOR_BKGND);
            canvasSmallFont.setCursor(cursorX, canvasSmallFont.height()-FONT_MARGIN-MENULINEWIDTH);
            canvasSmallFont.println(outString);        
            canvasSmallFont.fillRect(x1+cursorX,canvasSmallFont.height()-FONT_MARGIN,w1,MENULINEWIDTH,tcolor);
            cursorY = CENTER_ICON_Y+TFT_MARGIN_TOP - (uint8_t)(canvasSmallFont.height()/2);
            tft.drawBitmap(TFT_MARGIN_LEFT+CANVAS_MENUL_X+CANVAS_MENULR_WIDTH, cursorY, canvasSmallFont.getBuffer(), canvasSmallFont.width(), canvasSmallFont.height(), tcolor, COLOR_BKGND);
            //Draw Circle Icons left and right
            tft.fillCircle(CANVAS_MENUL_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, tcolor);
            tft.fillCircle(CANVAS_MENUR_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, tcolor);
        }
    }   
}

void drawClockItems(GFXcanvas1& cCanvas, int16_t xPos, int16_t yPos, uint8_t clock_item)
{
    switch (clock_item){
        case 0: 
            xPos = xPos; // Hour
            break;
        case 1:
            cCanvas.getTextBounds("88", 0, cCanvas.height(), &x1, &y1, &w1, &h1);
            xPos = xPos + x1 + w1 + FONT_MARGIN; // :-Sign
            break;
        case 2: 
            cCanvas.getTextBounds("88", 0, cCanvas.height(), &x1, &y1, &w1, &h1);
            cCanvas.getTextBounds(":", 0, cCanvas.height(), &x2, &y2, &w2, &h2);
            xPos = xPos  + x1 + w1 + x2 + w2 + 3*FONT_MARGIN;    // Minute
            break;
    }
    tft.drawBitmap(xPos, yPos, cCanvas.getBuffer(), cCanvas.width(), cCanvas.height(), COLOR_TXT, COLOR_BKGND);
}

void clockDisplay(time_t t)
{

    if (updateClockSign){
        updateClockSign = false;
        canvasClock.fillScreen(COLOR_BKGND);
        canvasClock.setCursor(0, canvasClock.height()-FONT_MARGIN-MENULINEWIDTH);
        canvasClock.println(":");
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_CLOCK_Y+TFT_MARGIN_TOP, 1);
       
    }
    if (clockHour != hour(t)){
        clockHour = hour(t);
        updateCanvasClock(canvasClock, clockHour, false);
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_CLOCK_Y+TFT_MARGIN_TOP, 0);
    }
    if (clockMinute != minute(t)){
        //sprintf(outString, ("Moving stepper due to Minute: %02d, %02d", clockMinute, minute(t)));
        //Serial.println(outString);
        clockMinute = minute(t);
        updateCanvasClock(canvasClock, clockMinute, false);
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_CLOCK_Y+TFT_MARGIN_TOP, 2);
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

void drawNotImplementedMessage(){
    tft.setFont();
    tft.setCursor(0, cursorYMenuStart);
    tft.setTextSize(2);
    tft.println("Hey Thomas, stellt sich raus du bist nicht einzige mit einem Deadline-Problem. Der Unterschied ist nur, wir koennen es nicht loesen! https://github.com/InfinityWave/LighthouseOS/ :-)");
    set_default_font();
}

// read temperature from RTC:
void update_temperature(){
    // Read temperature from RTC and update if necessary
    if (rtcavailable){
        float newtemp = myRTC.temperature()/4; 
        if (! (newtemp==currentTemp)){
            currentTemp = newtemp;
            tempChanged = false;
        }
    }
}

// Move tower light
void updateTowerLight(time_t t){
	uint16_t towerPos;
	uint16_t towerWay;
	towerPos = towerPosOffset + ( 15 * (uint16_t)hour(t)) + ((uint16_t)minute(t) / 15) * 3; 	// 360°/24h plus 3° per 1/4 h. Will change every 15min
	// Command new position if needed ...
	if ((moveTower)&&(stepperActive==false)&&(towerPos_last!=towerPos)) {		// Set point changed AND nothing is running		
		if (towerPos > towerPos_last){											// make sure the resulting way is positive
			towerWay = towerPos-towerPos_last;
		}else{
			towerWay = towerPos+360-towerPos_last;
		}
		if (settingClock > 0){					// Move only when selected in settings
			stepper.newMoveDegreesCCW(towerWay);									// Command movemnt (relative! not absolute)
			towerPos_last = towerPos;												// Store changed pos value
			stepperActive = true;					// Set flag to make sure no other movement is commanded
		}
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
    isrButtonC = true;
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
// Note: Saving changes in the alarms and recalculation the next alarm is done in saveReturnToMainMenu

// Check if an alarm must be triggered
bool checkAlarms () {	
	// Only check once per min
	if (minute(isrTime)!= minute(isrTime_last)){
		isrTime_last = isrTime;								// Store time to see when the next minute comes
		return (checkAlarm(alm1) || checkAlarm(alm2));		// Check both alarms
	}
	return false;
}

// General alarm check and reset for next day
bool checkAlarm (struct alarms &alm){
	// Weekday:
	// 1: Son, 2: Mon, 3: Tue, 4: Wed, 5: Thr, 6: Fr, 7: Sa
	if (alm.mode > 0 && alm.nextAlarm <= isrTime){ // Alarm is triggered
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
		alm_soundfile_actual = alm.soundfile;
		alm_towermode_actual = alm.towermode;
		return true;
	}
	return false;
}

// Recalculate nextAlarm, e.g. after reading from FRAM or setting a new alarm in the menu
void recalcAlarm(struct alarms &alm){
	
	// 1st: Set alarm for today
	tmElements_t myElements = {0, alm.mm, alm.hh, weekday(isrTime), day(isrTime), month(isrTime), year(isrTime)-1970 };
	alm.nextAlarm = makeTime(myElements);
	// 2nd: Check Alarm is in the past or not allowed today
	if (alm.mode<=2){ 						// Not related to weekdays
		if (alm.nextAlarm < isrTime){		// If alarm is in the past...
			alm.nextAlarm += 60*60*24;		// ... add 24h
		}
	}else{									// Alarm is related to weekdays
		// Weekday:
		// 1: Son, 2: Mon, 3: Tue, 4: Wed, 5: Thr, 6: Fr, 7: Sa
		switch (weekday(isrTime)){ 			// Check today:
			case 0: 						// Sunday
				if (alm.mode==3){			// Next weekday is Monday => add 24h
					alm.nextAlarm += 60*60*24;
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // if in the past, next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 6; // Add 6 days
					}
				}
			break;
			case 1: 						// Monday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past ... 
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 5; // Add 5 days
					}
				}
			break;
			case 2: 						// Tuesday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past ... 
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 4; // Add 4 days
					}
				}
			break;
			case 3: 						// Wednesday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past ... 
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 3; // Add 3 days
					}
				}
			break;
			case 4: 						// Thurday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past ... 
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 2; // Add 2 days
					}
				}
			break;
			case 5: 						// Friday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past, next alarm is on Mo 
						alm.nextAlarm += 60*60*24 * 3; // Add 3 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}
			break;
			case 6: 						// Saturday
				if (alm.mode==3){			// Next weekday is Monday => add 2 days
					alm.nextAlarm += 60*60*24 *2;
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // if in the past, next alarm is on So...
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}
			break;
		}
	}

}
//FRAM functions
//////////////////////////////////////////////////////

void writeAlarms (uint16_t address, struct alarms alm)
{
    framwrite8bit(address, alm.hh);
    framwrite8bit(address+1, alm.mm);
    framwrite8bit(address+2, alm.towermode);
    framwrite8bit(address+3, alm.soundfile);
	framwrite8bit(address+4, alm.mode);
}


void readAlarms (uint16_t address, struct alarms &alm)
{
	// Note: nextAlarm is not sored. Must be calculated after reading the alarm
    alm.hh = framread8bit(address);
    alm.mm = framread8bit(address+1);
    alm.towermode = framread8bit(address+2);
    alm.soundfile = framread8bit(address+3);
	alm.mode 	= framread8bit(address+4);
}


void framwrite8bit (uint16_t address, uint8_t data)
{
    if (framAvailable){ 
        fram.write(address, data);
    }
}

uint8_t framread8bit (uint16_t address){
    uint8_t data = 0;
    if (framAvailable){
        fram.read(address);
    }
    return data;
}

void framwrite16bit (uint16_t address, uint16_t data)
{
    if (framAvailable){ 
        fram.write(address, (uint8_t*)&data, 2);
    }
}

uint16_t framread16bit (uint16_t address)
{
    uint16_t data = 0;
    if (framAvailable){
        fram.read(address, (uint8_t*)&data, 2);
    }
    return data;
}


void framwrite32bit (uint16_t address, uint32_t data)
{
    fram.write(address, (uint8_t*)&data, 4);
}

uint32_t framread32bit (uint16_t address)
{
    uint32_t data = 0;
    if (framAvailable){
        fram.read(address, (uint8_t*)&data, 4);
    }
    return data;
}
