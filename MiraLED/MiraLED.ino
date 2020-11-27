#include <TimerOne.h>

#include <PinChangeInterrupt.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptSettings.h>

#include <math.h>

#include <FastLED.h>

//Pin definitions
#define LED_DATA 2
#define POT A4
#define BTN1 7
#define BTN2 12

//program constants
#define BRIGHT_DIVS 5
#define BRIGHT_INC 255/BRIGHT_DIVS
#define NUM_LEDS 46

//interrupt controls
#define DEBOUNCE_MS 150 //minimum time between execution of button interrupts; low-effort debounce
#define POT_DEADBAND_HW 2 //half-width of potentiometer deadband
#define POT_REVERSE 1 //boolean to reverse potentiometer direction

CRGB leds[NUM_LEDS]; 

//Interrupt controls again
volatile unsigned long btn1_last = 0; //Global to hold last time button interrupts triggered; use for debouncing
volatile unsigned long btn2_last = 0;
volatile uint8_t pot_last = 0; //Global to hold last pot reading; used to enforce deadband
volatile uint8_t pot_read; //global to hold state of potentiometer
volatile uint16_t pot_read_raw; //raw potentiometer reading

//definition of states;
typedef enum {
  OFF=0,
  SOLID,//solid color; color controlled by pot
  RAINBOW, //scrolling rainbow; scroll speed controlled by pot 
} STATE;
volatile STATE state = SOLID;
const uint8_t num_states = 3;

volatile boolean update_flag = 0; //flag for default wait case with prev_state = SOLId to return to SOLID and update LEDs
volatile boolean break_flag = 0;  //flag for animated states to exit while loop

//Brightness
volatile uint8_t ind_bright = BRIGHT_DIVS; //brightness index
volatile uint8_t brightness = ind_bright*BRIGHT_INC; //global brightness variable with computation example

volatile boolean test = 0;

void setup()
{
  FastLED.addLeds<WS2812B,LED_DATA,GRB>(leds,NUM_LEDS);
  //FastLED.setCorrection(TypicalLEDStrip);
   
  Serial.begin(9600);
  //configure ports
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(POT, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  //initialize pot_read
  potISR();
  
  //attach interrupts
  attachPCINT(digitalPinToPCINT(BTN1), incrementState, FALLING);
  attachPCINT(digitalPinToPCINT(BTN2), incrementBrightness, FALLING);
  Timer1.initialize(160000); //160000 us period = 160 ms = 0.16 s;
  Timer1.attachInterrupt(potISR);                  

  //saftey first
  delay(1000);  
}

void loop() {
 
     switch(state) {
      case SOLID:
        solidColor();
        break;
      case RAINBOW:
        scrollRainbow();
        break;
      case OFF:
        FastLED.showColor(CRGB::Black, 0);
        while (!break_flag) delay(200);
        break;
      default: //WAIT
        delay(200);
        break;
    }
}

void incrementBrightness() {
  if (millis() - btn2_last > DEBOUNCE_MS) {
    btn2_last = millis();
    
    ind_bright++;
    ind_bright %= (BRIGHT_DIVS + 1);
    brightness = ind_bright*BRIGHT_INC;

    update_flag = 1; //raise update_flag used for static routines (i.e. SOLID)
  }
}

void potISR() {
   uint16_t pot_raw = analogRead(POT); 
   uint8_t pot = POT_REVERSE*255 + (uint8_t)(pot_raw >> 2)*(1 - 2*POT_REVERSE); // pot = analogRead if POT_REVERSE = 0; pot = 255 - analogRead if POT_REVERSE = 1
   if ((pot> (pot_last + POT_DEADBAND_HW)) || (pot < (pot_last - POT_DEADBAND_HW))) {
    pot_read = pot;
    pot_read_raw = pot_raw;
    pot_last = pot;

    update_flag = 1; //raise update_flag used for static routines (i.e. SOLID)

    //test = !test;
    //digitalWrite(LED_BUILTIN, test);
   }
}

void incrementState() {
  if ((millis() - btn1_last) > DEBOUNCE_MS) {
    btn1_last = millis();
    
    break_flag = 1;
    update_flag = 1;
    
    state = state + 1;
    state %= num_states; 

    Serial.println(state);
  }
}

void solidColor() {
  while(!break_flag) {
    if(update_flag) {
      update_flag = 0;
      FastLED.showColor(CHSV(pot_read, 255, brightness));
    }
    else delay(150);
  }
  break_flag = 0;
}

void scrollRainbow() {
  uint8_t start_hue = 0;
  uint16_t a = 60000/256; //maximum rainbow scroll period ms
  uint16_t b = 1000/256; //minimuim rainbow scroll perido ms
  uint8_t n = 1; //potentiometer mapping order
  
  while (!break_flag) {
    fill_rainbow(leds, NUM_LEDS, start_hue, 255/NUM_LEDS);
    FastLED.show(brightness);
    
    uint16_t d = map(pot_read, 0, 255, a, b);
    delay(d);
    start_hue++;
    start_hue %= 256;
  }

  update_flag = 0; //this routine doesn't use the update_flag raised by button presses or pot turns; reset when transitioning out of this routine
  break_flag = 0;
}
