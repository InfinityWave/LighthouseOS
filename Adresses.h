//defines for FRAM Adresses 
#define FRAM_C_STEP 0x0000 //start address for current step pos 2 byte
#define FRAM_ALARM1 0x0100 //start address for ALARM1 reserve 16 byte
#define FRAM_ALARM2 0x0110 //start address for ALARM2 reserve 16 byte
#define FRAM_CLOCK_SETTINGS 0x0120 //start address for Clock settings reserve 1 byte
#define FRAM_VOLUME 0x0130 //start address for volume settings
#define FRAM_LED_BRIGHTNESS 0x0140 // Setting for LED brightness
#define FRAM_DISPLAY_BRIGHTNESS 0x0150 //Setting for display brightness
#define FRAM_DCF_ENABLE 0x0160 //Setting for display brightness
#define FRAM_TOWER_POS 0x0170 //Save last position of tower 