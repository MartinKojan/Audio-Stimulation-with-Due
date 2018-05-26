
// Define
//-----------------------------------------------------------------------------
#define inputPinPort PIOC   // Register for INPUT pins configuration
#define inputPinMask 0x1FE  // Bit mask for corresponding bits on register

#define outputPinPort PIOC  // Register for OUTPUT pins configuration
#define outputPinMask 0xFF000 // Bit mask for corresponding bits on register


// Imports
//-----------------------------------------------------------------------------
#include <Arduino.h>
#include <DueTimer.h> // for the timers
#include <SPI.h>  // for the serial communication
#include <SD.h>   // for the communication with SD Card
#include <AutoAnalogAudio.h> // for the audio playback
#include "myWAV.h" // for WAV parsing

// Variables
//------------------------------------------------------------------------------
unsigned long timings1, timings2, timings3; // for debug only
volatile int out = 0;
int i;
int val;
long long int outVal;
int audioFlag = 0; // initialize variable with dummy value

//------------------------------------------------------------------------------

void SDCardInfo(void);
void timerInit(void);

//------------------------------------------------------------------------------
// Function to easily output zeros on the output pins
void outZeros(){

        Timer4.start();

        while(1) {
                if(out == 1) {
                        (outputPinPort->PIO_CODR) = 0xFF000;
                        break;
                }
        }

#if defined (AUDIO_DEBUG)
        timings3 = micros();
#endif

        Timer4.stop();
        Timer4.resetIRQ();

}



//------------------------------------------------------------------------------
// Handler for when timer countdown ends
void timerHandler(){
        out = 1;
}



//------------------------------------------------------------------------------
// Handle the input information and decide if only marker or also audio output
void markerHandler(void){

        Timer3.start(); // as the input enable sets, start timer

#if defined (AUDIO_DEBUG)
        timings1 = micros(); // remember timing for audio debug
#endif

        outVal = val << 12; // prepare output variable for marker
        audioFlag = 2; // prepare variable for audio handler
        if(val & 0x40) // if the 2nd most significant bit is set, it is an audio command
                audioHandler();
        switch(audioFlag) {
        case 0:
                while(1) {
                        if(out == 1) {
                                DACC_Handler();
                                dacc_disable_interrupt(DACC, DACC_IER_TXRDY);
                                (outputPinPort->PIO_SODR) = outVal;
                                break;
                        }
                }
                break;

        case 1:
                while(1) {
                        if (out == 1) {
                                DACC_Handler();
                                dacc_enable_interrupt(DACC, DACC_IER_ENDTX);
                                (outputPinPort->PIO_SODR) = outVal;
                                break;
                        }
                }
                break;

        case 2:
                while(1) {
                        if (out == 1) {
                                DACC_Handler();
                                dacc_disable_interrupt(DACC, DACC_IER_TXRDY);
                                (outputPinPort->PIO_SODR) = outVal;
                                break;
                        }
                }
                break;

        default:
                break;
        }

#if defined (AUDIO_DEBUG)
        timings2 = micros();
#endif
        out = 0;
}


//------------------------------------------------------------------------------
// Handler to decide what to do with audio playback

void audioHandler(){

        audioFlag = 1; // add a flag to identify audio marker

        switch(val & 0x7F) {
        /*
           Analyzing the 6 least significant bits of the input, which are data.
           The 2 most significant bits of the input byte are control bits.
           To add another song to this library, create another entry as follows
           ----------------------------------------

           case 0x**:
                playAudio("AudioFileName.wav")
                break;

           ------------------------------------------
                Replace ** by the hexadecimal number that represents
                the 7 bits word that will play the song
         */
        case 0x00: // word is XX00 0000

                break;

        case 0x01: // word is XX00 0001
                playAudio("Roland.wav");
                break;

        case 0x02: // word is XX00 0010
                playAudio("M16b24kS.wav");
                break;

        case 0x03: // word is XX00 0011
                playAudio("Geminini.wav");
                break;

        case 0x04: // word is XX00 0100
                audioFlag = 0;
                break;

        default:
                playAudio("Roland.wav");
                break;

        }
}

//------------------------------------------------------------------------------
// Arduino setuup function, it is run once when Arduino starts.
void setup() {

        // Set up pins for output or input...
        // Pullup means input will be held HIGH if disconnected

        // PLEASE, NOTICE LEAST AND MOST SIGNIFICANT BITS


        pinMode(33, INPUT_PULLUP); // least significant bit, pin 33
        pinMode(34, INPUT_PULLUP);
        pinMode(35, INPUT_PULLUP);
        pinMode(36, INPUT_PULLUP);
        pinMode(37, INPUT_PULLUP);
        pinMode(38, INPUT_PULLUP);
        pinMode(39, INPUT_PULLUP);
        pinMode(40, INPUT_PULLUP); // most significant bit, pin 40


        pinMode(44, OUTPUT); // most significant bit, pin 44
        pinMode(45, OUTPUT);
        pinMode(46, OUTPUT);
        pinMode(47, OUTPUT);
        pinMode(48, OUTPUT);
        pinMode(49, OUTPUT);
        pinMode(50, OUTPUT);
        pinMode(51, OUTPUT); // least significant bit, pin 51

        // Initialize Serial communication for debug
        Serial.begin(115200);

        // Initialize SD Card
        while(!Serial);
        Serial.print("Initializing SD card...");
        if (!SD.begin(F_CPU/2,4)) {
                Serial.println("initialization failed!");
                while (1);
        }
        Serial.println("initialization done.");

        // Start AAAudio with only the DAC
        aaAudio.begin(0, 1);
        aaAudio.autoAdjust = 1;  // Disable automatic timer adjustment

        // Configure Timers
        Timer3.setPeriod(10000);
        Timer3.attachInterrupt(timerHandler);

        Timer4.setPeriod(500);
        Timer4.attachInterrupt(timerHandler);

        // Prepare Timers by reseting them
        Timer3.start();
        Timer4.start();

        Timer3.stop();
        Timer4.stop();

        Timer3.resetIRQ();
        Timer4.resetIRQ();

        // Select audio channels for AAAudio, 2 for stereo
        channelSelection = 2;

        Serial.println("Ready to play");

}

// Arduino loop function, will be run over and over again
void loop() {
        // Read input pins value
        val = ((inputPinPort->PIO_PDSR) & inputPinMask) >> 1;

        // Wait for ENABLE bit to reset
        if( (val & 0x80) == 0) {
                delay(100);  // Wait for signal debounce
                out = 0; // prepare variable for later usage

                // Wait for a new command when ENABLE bit is set
                while(1) {
                        val = ((inputPinPort->PIO_PDSR) & inputPinMask) >> 1;
                        if( (val & 0x80) > 0)
                                break;
                }

                // Handle the input command on ENABLE bit set
                markerHandler();

                // Output a bunch of zeros to prevent marker repetition
                outZeros();

#if defined (AUDIO_DEBUG)

                // Print timing difference for debug
                Serial.println(timings2 - timings1);
                Serial.println(timings3 - timings1);
#endif
        }

}
