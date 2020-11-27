/*TODO:
 * debug buzz in headphones heard while arduino plugged by usb and powered by 5V
 * *  Try again without 5V power connected
 * Switch from NeoPixel.h to FastLED.h 
 * Reimplement button interrupts once button jumped to DI
 * Make my own code! (The fun shit)
 */

#define FASTLED_DEBUG 

#include <FastLED.h>
#ifdef __AVR__
#include <avr/power.h>
#endif 

#define LED_DATA 4
#define POT_BRIGHTNESS A0

#define NUM_LEDS 60
#define BTN_PIN A6
#define MAX_MODE 7
#define BTN_SAMPLE_PERIOD_MS 20


#define MSGEQ_OUT A1
#define STROBE 2
#define RESET 3

//MINIMUM DELAY FOR BUTTON TO TRIGGER NEXT ISR
#define ISR_DELAY 400
  CRGB leds[NUM_LEDS];  //declare FastLED array object


uint16_t time_since_isr=0;
boolean breakMode=false;
uint8_t mode=1;
int spectrumValue[8];
uint8_t mapValue[8];
uint32_t audioBuffer[NUM_LEDS];
int filter = 0;
uint16_t brightness = 255;

bool countUp=true;
uint32_t lastColor=0;


void setup() {
  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
#if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif

  FastLED.addLeds<NEOPIXEL, LED_DATA>(leds,NUM_LEDS);
  
  Serial.begin(9600);
  Serial.println("START");
  
  pinMode(MSGEQ_OUT, INPUT);
  pinMode(STROBE, OUTPUT);
  pinMode(RESET, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  
  digitalWrite(RESET, LOW);
  digitalWrite(STROBE, HIGH);

  pinMode(POT_BRIGHTNESS, INPUT);

}

void loop() {
  breakMode=false;
  switch(mode){
    case 0:
      Serial.println("Raindow");
      rainbowFade2White();
      break;
   case 1:                        // CHANGE IF NOT USING MSGEQ7
      Serial.println("Music");
      musicAnalyzer();
      break;
   case 2:
      Serial.println("Red");
      plainColor(255,0,0);
      break;
    case 3:
      Serial.println("Green");
      plainColor(0,255,0);
      break;
    case 4:
      Serial.println("Blue");
      plainColor(0,0,255);
      break;
   case 5:
      Serial.println("White");
      plainColor(255,255,255);
      break;
   case 6:
      Serial.println("ColorRoom");
      colorRoom();
      break;
   default:
      Serial.println("DEFAULT");
      FastLED.showColor(CRGB::Black); //All leds off
      break;
  }
  changeMode();
}

/* ISR for Button to switch modes */
void changeMode(void) {
   uint16_t btn = analogRead(BTN_PIN);
  if (!btn) {
    mode=(mode+1)%MAX_MODE;
    breakMode=true;
    delay(1000);
  }
}

/* Checks if Potentiometer value has changed, sets new Brightness and return true */
boolean checkBrightness(){
  //WS2812 takes value between 0-255
  uint16_t bright = map(constrain(analogRead(POT_BRIGHTNESS),0,1023), 0, 1023, 1, 255);
  if (abs(bright-brightness)> 5){
    brightness = bright;
    FastLED.setBrightness(brightness);
    Serial.println("SET");
    return true;
  }
  return false;
}

//Animation showing fading rainBow color along the strip
void rainbowFade2White() {
  static uint8_t initHue = 0;
  static uint8_t delHue = 2;
  
  fill_rainbow((&leds[0]), NUM_LEDS, initHue, delHue);
  initHue += delHue;
  
  checkBrightness();
  changeMode();
  if(breakMode) return;
  delay(500);
}

// Use Brightness Potentiometer to change Color
void colorRoom(){
    //leds.setBrightness(255); //not needed due to setHSV() value parameter
    int colorNew = map(constrain(analogRead(POT_BRIGHTNESS),0,1023), 0, 1023, 0, 255);
    if (abs(colorNew-lastColor)>5){
      lastColor=colorNew;
      FastLED.showColor(CRGB.setHue(colorNew));
      leds.show();
    delay(10);
  }
}

void plainWhite(){
  FastLED.showColor(CRGB::White);
  while(!breakMode){
    changeMode();
  }
}

//Shows plain color along the strip
void plainColor(uint8_t red,uint8_t green,uint8_t blue){
  FastLED.showColor(CRGB(red,green,blue));
  leds.show();
  while(!breakMode){
    changeMode();
    if(checkBrightness())
      leds.show();
  }
}

void musicAnalyzer(){
  while(true){
    changeMode();
    checkBrightness();
    if(breakMode)return;
    
    //Reset MSGEQ
    digitalWrite(RESET, HIGH);
    digitalWrite(RESET, LOW);
    //Read all 8 Audio Bands
    for (int i = 0; i < 8; i++){
      digitalWrite(STROBE, LOW);
      delayMicroseconds(30);
      spectrumValue[i] = analogRead(MSGEQ_OUT);
      //Serial.print("spectrumValue =");
      //Serial.println(spectrumValue[i]); 
      spectrumValue[i] = constrain(spectrumValue[i], filter, 1023);
      digitalWrite(STROBE, HIGH);
      //Skip band 3 (seems to be random for my MSGEQ7, but feel free to check yours
//      if (i == 3)
//        continue;
      mapValue[i] = map(spectrumValue[i], filter, 1023, 0, 255);
      if (mapValue[i] < 50)
        mapValue[i] = 0;
    }
    //Shift LED values forward
    for (int k = NUM_LEDS - 1; k > 0; k--) {
      audioBuffer[k] = audioBuffer[k - 1];
    }
    //Uncomment / Comment above to Shift LED values backwards
    //for (int k = 0; k < NUM_LEDS-1; k++) {
    //  audioBuffer[k] = audioBuffer[k + 1];
    //}
    
    //Load new Audio value to first LED
    //Uses band 0,2,4 (Bass(red), Middle(green), High(blue) Frequency Band)
    //Lowest 8Bit: Blue , Middle Green , Highest Red
    //Use audioBuffer[NUM_LEDS-1] when using LED shift backwards!
    audioBuffer[0] = mapValue[5]; //RED
    audioBuffer[0] = audioBuffer[0] << 16;
    audioBuffer[0] |= ((mapValue[2] /2) << 8);  //GREEN
    audioBuffer[0] |= (mapValue[4]/4);         //BLUE
    
//   if (NUM_LEDS%2 == 1) {
//      audioBuffer[NUM_LEDS/2] = audioBuffer[0];
//    }
//    for (int i = 0; i < NUM_LEDS / 2; i++) {
//      audioBuffer[NUM_LEDS/2 + i + NUM_LEDS%2] = audioBuffer[i];
//    }
//    for (int i = 0; i < NUM_LEDS / 2; i++){
//     audioBuffer[NUM_LEDS / 2 - 1 - i] = audioBuffer[NUM_LEDS/2 + i + NUM_LEDS%2];
//    }
    
    //Send new LED values to WS2812
    for ( int i = 0; i < NUM_LEDS; i++){
      leds[i] = audiobuffer[i];
    }
    leds.show();
    delay(13);
  }
}
   
