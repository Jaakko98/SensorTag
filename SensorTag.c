/*
Jaakko Jyrkkä 2544098
Timo Malo 2463991
-------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>


/* Board Header files */
#include "Board.h"

#include "wireless/comm_lib.h"
#include "sensors/bmp280.h"
#include "sensors/mpu9250.h"
#include "buzzer.h"

/* Task */
#define STACKSIZE 2048

void playMusic(uint16_t taajuus, uint8_t desisec);

Char commTaskStack[STACKSIZE];
Char taskStack[STACKSIZE];
Char displayStack[STACKSIZE];
Char buzzerStack[STACKSIZE];

/* Display */
Display_Handle hDisplay;

static PIN_Handle buttonHandle;
static PIN_State buttonState;

static PIN_Handle hMpuPin; //MPU-pinni määritelmä
static PIN_State MpuPinState;
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static PIN_Handle hButtonShut;
static PIN_State bStateShut;

static PIN_Handle ledHandle;
static PIN_State ledState;

static PIN_Handle buzzHandle;

// MPU9250 I2C CONFIG
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};
//globaalit muuttujat
uint8_t nappi_painettu = 0;
uint8_t liike = 0;
char payload[16];
enum state { MENU=1, READ_SENSOR, MUSIC }; // tilakoneen tilat
enum state TILA = MENU;

//Painonappi
PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE 
};

PIN_Config buttonShut[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE
};
PIN_Config buttonWake[] = {
   Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
   PIN_TERMINATE
};

// Ledipinni
PIN_Config ledConfig[] = {
   Board_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX, 
   PIN_TERMINATE
};

void playMusic(uint16_t taajuus, uint8_t ms)
{
    buzzerOpen(buzzHandle);
    buzzerSetFrequency(taajuus);
    Task_sleep(ms * 10000 / Clock_tickPeriod);
    buzzerClose();
}

void buzzerFxn(UArg arg0, UArg arg1)
{

    while(1)
    {
        if (TILA == MUSIC)
        {
            int i = 0;
            for (i; i < 2; i++)
            {
                playMusic(587, 5);
                playMusic(659, 50);
                playMusic(587, 5);
                playMusic(659, 40);
                playMusic(587, 5);
                playMusic(659, 25);
                playMusic(440, 15);
                playMusic(494, 15);
                playMusic(587, 15);
                playMusic(659, 15);
                playMusic(440, 15);
                playMusic(494, 15);
                playMusic(587, 15);
                playMusic(659, 15);
            }
            i = 0;
            playMusic(0, 2);
            for (i; i < 17; i++)
            {
                playMusic(659, 15);
                playMusic(0, 0);
            }
            i = 0;
            for (i; i < 8; i++)
            {
                playMusic(587, 15);
                playMusic(0, 1);
            }
            playMusic(784, 15);
            playMusic(587, 45);
            playMusic(659, 75);
            TILA = MENU;
        }
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

// Näyttötaski/MENU
Void displayFxn(UArg arg0, UArg arg1) {

   // Alustetaan näyttö nyt taskissa
   Display_Params params;
   Display_Params_init(&params);
   params.lineClearMode = DISPLAY_CLEAR_BOTH;

   // Näyttö käyttöön ohjelmassa
   Display_Handle hDisplay = Display_open(Display_Type_LCD, &params);

    while(1)
    {
        if(TILA == MENU && nappi_painettu == 0)
        {
            Display_print0(hDisplay, 1, 7, "siirry ^");
            Display_print0(hDisplay, 4, 0, "<Tunnista liike>");
            Display_print0(hDisplay, 6, 1, "Soita Paranoid");
            Display_print0(hDisplay, 8, 1, "Sammuta laite");
            Display_print0(hDisplay, 10, 6, "valitse v");
           
        }
        else if(TILA == MENU && nappi_painettu == 1)
        {
            Display_print0(hDisplay, 4, 1, "Tunnista liike");
            Display_print0(hDisplay, 6, 0, "<Soita Paranoid>");
            Display_print0(hDisplay, 8, 1, "Sammuta laite");
        }   
        else if(TILA == MENU && nappi_painettu == 2)
        {
            Display_print0(hDisplay, 4, 1, "Tunnista liike");
            Display_print0(hDisplay, 6, 1, "Soita Paranoid");
            Display_print0(hDisplay, 8, 0, "<Sammuta laite>");
        }
        
        else if(TILA == READ_SENSOR)
        {
            
            if(hDisplay)
            {
                Display_clear(hDisplay);
                Display_print0(hDisplay, 5, 2, "Tee Liike");
                while(TILA == READ_SENSOR)
                {
                    if(liike == 1)
                    {
                        char sendload[16];
                        sprintf(sendload, "%s", "High Five!");
                        Send6LoWPAN(IEEE80154_SERVER_ADDR, sendload, strlen(sendload));
                        StartReceive6LoWPAN();
                        Display_print0(hDisplay, 5, 2, sendload);
                        playMusic(1174, 10);
                        playMusic(1760, 40);
                        Task_sleep(3000000/Clock_tickPeriod);
                        liike = 0;
                        Display_clear(hDisplay);
                        Display_print0(hDisplay, 5, 2, payload);
                        Task_sleep(3000000/Clock_tickPeriod);
                    }

                    if(liike == 2)
                    {
                        char sendload[16];
                        sprintf(sendload, "%s", "Wave!");
                        Send6LoWPAN(IEEE80154_SERVER_ADDR, sendload, strlen(sendload));
                        StartReceive6LoWPAN();
                        Display_print0(hDisplay, 5, 2, sendload);
                        playMusic(523, 10);
                        playMusic(587, 10);
                        playMusic(659, 10);
                        playMusic(698, 10);
                        playMusic(783, 10);
                        playMusic(880, 10);
                        playMusic(988, 10);
                        playMusic(1046, 60);
                        Task_sleep(3000000/Clock_tickPeriod);
                        liike = 0;
                        Display_clear(hDisplay);
                        Display_print0(hDisplay, 5, 2, payload);
                        Task_sleep(3000000/Clock_tickPeriod);
                    }
                    if (liike == 3)
                    {
                        liike = 0;
                        playMusic(440, 20);
                        playMusic(311, 40);
                        Display_clear(hDisplay);
                    }
                    Task_sleep(100000/Clock_tickPeriod);
                    
                }
                
            }
        }
        
        Task_sleep(100000/Clock_tickPeriod); 
    }
} 

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

    PIN_setOutputValue( ledHandle, Board_LED1, !PIN_getOutputValue( Board_LED1 ) );
    nappi_painettu += 1;
    nappi_painettu = nappi_painettu % 3; //menu rakenteen käyttöön tarkoitettu muuttuja
}

//valintanäppäin
Void buttonShutFxn(PIN_Handle handle, PIN_Id pinId) {

    if (TILA == MUSIC)
    {
        TILA = MENU;
    }
    
    else if (TILA == MENU && nappi_painettu == 0)
    {
        TILA = READ_SENSOR;
    }
    
    else if (TILA == MENU && nappi_painettu == 1)
    {
        TILA = MUSIC;
    }
    
    else if (TILA == MENU && nappi_painettu == 2)
    {
        PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
        // Näyttö pois päältä
        Display_clear(hDisplay);
        Display_close(hDisplay);
        Task_sleep(100000 / Clock_tickPeriod);
        
        // Itse taikamenot
        PIN_close(hButtonShut);
        PINCC26XX_setWakeup(buttonWake);
        Power_shutdown(NULL,0);
    }
}

// SENSOR TASK
Void sensorFxn(UArg arg0, UArg arg1) {

    
    // *
    //
    // USE TWO DIFFERENT I2C INTERFACES
    //
    // *
    I2C_Handle i2c; // INTERFACE FOR OTHER SENSORS
    I2C_Params i2cParams;
    I2C_Handle i2cMPU; // INTERFACE FOR MPU9250 SENSOR
    I2C_Params i2cMPUParams;
    
    float ax, ay, az, gx, gy, gz;
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    
    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;
    
    // *
    //
    // MPU OPEN I2C
    //
    // *
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }
    
    // *
    //
    // MPU POWER ON
    //
    // *
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);
    
    // WAIT 100MS FOR THE SENSOR TO POWER UP
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();
    
    // *
    //
    // MPU9250 SETUP AND CALIBRATION
    //
    // *
    System_printf("MPU9250: Setup and calibration...\n");
    System_flush();
    
    mpu9250_setup(&i2cMPU);
    
    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();
    
    // *
    //
    // MPU CLOSE I2C
    //
    // *
    I2C_close(i2cMPU);
    
    while(1)
    {
        if(TILA == READ_SENSOR)
        {

            // *
            //
            // MPU OPEN I2C
            //
            // *
            i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
            if (i2cMPU == NULL) 
            {
                System_abort("Error Initializing I2CMPU\n");
            }
            
            int t;
            uint8_t vitonen_vaihe = 0;
            uint8_t heilutus_vaihe = 0;
            
            for (t = 0; t < 120; t++)
            {
                
                // *
                // MPU ASK DATA
            	//
                //    Accelerometer values: ax,ay,az
             	//    Gyroscope values: gx,gy,gz
            	//
                // *
            	mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
            	
            	//algoritmi
            	//ylävitonen
            	if(gx < -200)
            	{
            	    vitonen_vaihe = 1;
            	}
            	if(ay > 1 && vitonen_vaihe == 1)
            	{
            	    liike = 1;
            	    break;
            	}
            	//käden heilutus
            	if(heilutus_vaihe == 5)
            	{
            	    liike = 2;
            	    break;
            	}
            	if(ax > 0.5 && heilutus_vaihe % 2 == 0)
            	{
            	    heilutus_vaihe += 1;
            	}
            	if(ax < -0.5 && heilutus_vaihe % 2 == 1)
            	{
            	    heilutus_vaihe += 1;
            	}
                Task_sleep(50000 / Clock_tickPeriod);
            }
            if(liike == 0) //ei liikettä
            {
                liike = 3;
            }
            Task_sleep(500000 / Clock_tickPeriod);
            // *
            //
            // MPU CLOSE I2C
            //
            // *
            I2C_close(i2cMPU);
            	    
            TILA = MENU;
        }
        Task_sleep(100000/Clock_tickPeriod);
    }
}

/* Communication Task */
Void commTaskFxn(UArg arg0, UArg arg1) {

    uint16_t senderAddr;

    // Radio to receive mode
	int32_t result = StartReceive6LoWPAN();
	if(result != true) {
		System_abort("Wireless receive mode failed");
	}
	

    while (1) {

        // If true, we have a message
    	if (GetRXFlag() == true) {
    		// Tyhjennetään puskuri (ettei sinne jäänyt edellisen viestin jämiä)
           memset(payload,0,16);
           // Luetaan viesti puskuriin payload
           Receive6LoWPAN(&senderAddr, payload, 16);
           // Tulostetaan vastaanotettu viesti konsoli-ikkunaan
        }

    	// Absolutely NO Task_sleep in this task!!
    }
}


Int main(void) {

    // Task variables
    Task_Handle displayTask;
    Task_Params displayTaskParams;
    Task_Handle sensorTask;
    Task_Params sensorTaskParams;
	Task_Handle commTask;
	Task_Params commTaskParams;
	Task_Handle buzzerTask;
	Task_Params buzzerTaskParams;

    // Initialize board
    Board_initGeneral();
    Board_initI2C();
    
   //Otetaan pinnit käyttöön ohjelmassa
   buttonHandle = PIN_open(&buttonState, buttonConfig);
   if(!buttonHandle) {
      System_abort("Error initializing button pins\n");
   }
   ledHandle = PIN_open(&ledState, ledConfig);
   if(!ledHandle) {
      System_abort("Error initializing LED pins\n");
   }
   
       //Valintanäppäimen määrittely
    hButtonShut = PIN_open(&bStateShut, buttonShut);
    if(!hButtonShut) {
        System_abort("Error initializing button shut pins\n");
   }
   if (PIN_registerIntCb(hButtonShut, &buttonShutFxn) != 0) {
      System_abort("Error registering button callback function");
   }

    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
      System_abort("Error registering button callback function"); 
    }
    // *
    //
    // OPEN MPU POWER PIN
    //
    // *
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
    	System_abort("Pin open failed!");
    }
    /* SensorTask */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &taskStack;
    sensorTaskParams.priority = 2;
    sensorTask = Task_create(sensorFxn, &sensorTaskParams, NULL);
    if (sensorTask == NULL) {
    	System_abort("Task create failed!");
    }
    /* DisplayTask */
    Task_Params_init(&displayTaskParams);
    displayTaskParams.stackSize = STACKSIZE;
    displayTaskParams.stack = &displayStack;
    displayTaskParams.priority = 2;
    displayTask = Task_create(displayFxn, &displayTaskParams, NULL);
    if (displayTask == NULL) {
    	System_abort("Task create failed!");
    }
    
    /* Communication Task */
    Init6LoWPAN(); // This function call before use!

    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commTaskStack;
    commTaskParams.priority=1;
    commTask = Task_create(commTaskFxn, &commTaskParams, NULL);
    if (commTask == NULL) {
    	System_abort("Task create failed!");
    }
    
    //BuzzerTask
    Task_Params_init(&buzzerTaskParams);
    buzzerTaskParams.stackSize = STACKSIZE;
    buzzerTaskParams.stack = &buzzerStack;
    buzzerTaskParams.priority = 2;
    buzzerTask = Task_create(buzzerFxn, &buzzerTaskParams, NULL);
    if (buzzerTask == NULL) {
    	System_abort("Task create failed!");
    }

    /* Start BIOS */
    BIOS_start();

    return (0);
}