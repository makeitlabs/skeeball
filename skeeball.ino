// SkeeBall Game
// Peter Vatne, November 2017
// Animation routines from FastLED_Demo by Mark Kriegsman, December 2014

#include "FastLED.h"
FASTLED_USING_NAMESPACE

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

// definitions for the LED strips
#define STRIP1_DATA_PIN    8
#define STRIP2_DATA_PIN    9
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define BRIGHTNESS          200
#define FRAMES_PER_SECOND  120
#define MAXMILLIAMPS       2000

// definitions for topology of LED strips
// NUM_LEDS must equal NUM_SCORE_BAR_LEDS + NUM_STAR_LEDS
// if not all leds light, try increasing MAXMILLIAMPS
#define NUM_LEDS    55
#define NUM_SCORE_BAR_LEDS 50
#define NUM_STAR_LEDS 5

// definitions for the scoring switches
#define INNER1_SWITCH_PIN  18
#define OUTER1_SWITCH_PIN  19
#define INNER2_SWITCH_PIN  20
#define OUTER2_SWITCH_PIN  21

int play0 = 0;
int play1 = 0;
int game_over=0;

#define SOUND0 30   //low point noise
#define SOUND1 31   //high point noise
#define SOUND2 32   //player 1
#define SOUND3 33   //player 2
#define SOUND4 34   //you win
#define SOUND5 35
#define SOUND6 36
#define SOUND7 37
#define SOUND8 38
#define SOUND9 39   //left right
void inner1_switch_ISR();
void outer1_switch_ISR();
void inner2_switch_ISR();
void outer2_switch_ISR();

// definitions for "GO" buttons
#define GO1_SWITCH_PIN   2
#define GO2_SWITCH_PIN   3
void go1_switch_ISR();
void go2_switch_ISR();

// Scores are large enough to allow scaling
#define SCORE_INNER 50
#define SCORE_OUTER 10
#define SCORE_MAX 500

// definitions for the handicap potentiometers
#define POT1_PIN    A2
#define POT2_PIN    A3

// the per-player variables
#define PLAYER1      0
#define PLAYER2      1
#define NUM_PLAYERS  2
CRGB gLeds[NUM_PLAYERS][NUM_LEDS];  // one strip of leds per player
int gNumLeds[NUM_PLAYERS];  // the total number of leds actually being lit on a strip
int gScore[NUM_PLAYERS];    // player's score
int gPotVal[NUM_PLAYERS];   // value of "handicap" potentiometer
int gGo1 = 0;               // PLAYER1's go button
int gGo2 = 0;               // PLAYER2's go button
int gGo = 0;                // master go control
int p1_played=0;            // flag if player 1 start sound has been played
int p2_played=0;            // flag if player 2 start sound has been played 

// List of patterns to cycle through.  Each is defined as a separate function below.
 //     typedef void (*SimplePatternList[])(CRGB leds[], int num_leds);
 //     SimplePatternList gPatterns = { rainbow, sinelon, bpm, juggle, confetti, rainbowWithGlitter, winner};

// Note: NUM_PATTERNS does not include the winner pattern
#define NUM_PATTERNS 6

uint8_t gCurrentPatternNumber[NUM_PLAYERS]; // Index number of which pattern is current for each player
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
  
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void rainbow(CRGB leds[], int num_leds) 
{
  // FastLED's built-in rainbow generator
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  if (num_leds == 0) return;
  fill_rainbow(leds, num_leds, gHue, 7);
}

void rainbowWithGlitter(CRGB leds[], int num_leds) 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow(leds, num_leds);
  if (num_leds == 0) return;
  addGlitter(leds, num_leds, 80);
}

void addGlitter(CRGB leds[], int num_leds, fract8 chanceOfGlitter) 
{
  if (random8() < chanceOfGlitter) {
    leds[random16(num_leds)] += CRGB::White;
  }
}

void confetti(CRGB leds[], int num_leds) 
{
  // random colored speckles that blink in and fade smoothly
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  if (num_leds == 0) return;
  fadeToBlackBy(leds, num_leds, 10);
  int pos = random16(num_leds);
  leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

void sinelon(CRGB leds[], int num_leds)
{
  // a colored dot sweeping back and forth, with fading trails
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  if (num_leds == 0) return;
  fadeToBlackBy(leds, num_leds, 20);
  int pos = beatsin16(13, 0, num_leds-1);
  leds[pos] += CHSV(gHue, 255, 192);
}

void bpm(CRGB leds[], int num_leds)
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  if (num_leds == 0) return;
  for (int i = 0; i < num_leds; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}


void juggle(CRGB leds[], int num_leds) {
  // eight colored dots, weaving in and out of sync with each other
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  if (num_leds == 0) return;
  fadeToBlackBy(leds, num_leds, 20);
  byte dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i+7, 0, num_leds-1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void winner(CRGB leds[], int num_leds) {
  // same as bpm but with a faster beat and one direction only
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 124;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  if (num_leds == 0) return;
  for (int i = 0; i < num_leds; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat+50);
  }
}

int add_to_score(int score, int delta)
{
  // only starts workiing when go buttons are pressed.
  // score can never go below 0 or above SCORE_MAX.
  // if it reaches SCORE_MAX, it sticks there, and
  // sets gGo to 0 to prevent other player from scoring.
  if (!gGo) {
    return score;
  }
  if (score == SCORE_MAX) {
    return score;
  }
  score += delta;
  if (score < 0) {
    score = 0;
  }
  if (score >= SCORE_MAX) {
    score = SCORE_MAX;
    gGo = 0;
  }
  return score;
}

void inner1_switch_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < 150) {
    return;
  }
  last_interrupt_time = interrupt_time;
 if(gGo)
  play1=1; 
  gScore[PLAYER1] = add_to_score(gScore[PLAYER1], SCORE_INNER);
}

void outer1_switch_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < 150) {
    return;
  }
  last_interrupt_time = interrupt_time;
  if(gGo)
    play0=1;
  gScore[PLAYER1] = add_to_score(gScore[PLAYER1], SCORE_OUTER);
}

void inner2_switch_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < 150) {
    return;
  }
  last_interrupt_time = interrupt_time;
  if(gGo)
    play1=1;
  gScore[PLAYER2] = add_to_score(gScore[PLAYER2], SCORE_INNER);
}

void outer2_switch_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < 150) {
    return;
  }
  last_interrupt_time = interrupt_time;
  if(gGo)
    play0=1;
    
  gScore[PLAYER2] = add_to_score(gScore[PLAYER2], SCORE_OUTER);
 
}

void go1_switch_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < 150) {
    return;
  }
  last_interrupt_time = interrupt_time;
  
  gGo1 = 1;//digitalRead(GO1_SWITCH_PIN);
}

void go2_switch_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < 150) {
    return;
  }
  last_interrupt_time = interrupt_time;
  gGo2 = 1;//digitalRead(GO2_SWITCH_PIN);
}

void read_pot1_val()
{
  static long int old_pot_time = 0;
  static int old_pot_val = 0;
  long int pot_time = millis();
  int pot_val = analogRead(POT1_PIN);    // read the value from the sensor
  
  if (((pot_time - old_pot_time) > 150) && (old_pot_val != pot_val)) {
    old_pot_time = pot_time;
    old_pot_val = pot_val;
  }
  gPotVal[PLAYER1] = pot_val;
}

void read_pot2_val()
{
  static long int old_pot_time = 0;
  static int old_pot_val = 0;
  long int pot_time = millis();
  int pot_val = analogRead(POT2_PIN);    // read the value from the sensor
  
  if (((pot_time - old_pot_time) > 150) && (old_pot_val != pot_val)) {
    old_pot_time = pot_time;
    old_pot_val = pot_val;
  }
  gPotVal[PLAYER2] = pot_val;
}

int score_to_number_of_leds(int score)
{
  // only light the star when you reach SCORE_MAX
  if (score == SCORE_MAX) {
    return NUM_SCORE_BAR_LEDS + NUM_STAR_LEDS;
  }
  return NUM_SCORE_BAR_LEDS * score / SCORE_MAX;
}

int score_to_pattern_number(int score)
{
  // the winner pattern is displayed if you reach SCORE_MAX
  if (score == SCORE_MAX) {
    return NUM_PATTERNS;
  }
  return NUM_PATTERNS * score / SCORE_MAX;
}


void setup()
{
  delay(500); // 3 second delay for recovery
  
  // tell FastLED about the LED strip configurations
  FastLED.addLeds<LED_TYPE,STRIP1_DATA_PIN,COLOR_ORDER>(gLeds[PLAYER1], NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE,STRIP2_DATA_PIN,COLOR_ORDER>(gLeds[PLAYER2], NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  // set power maximum
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAXMILLIAMPS);

  // initialize the leds to black
  FastLED.clear();
 // initialize the switches
  pinMode(INNER1_SWITCH_PIN, INPUT_PULLUP);
  pinMode(OUTER1_SWITCH_PIN, INPUT_PULLUP);
  pinMode(INNER2_SWITCH_PIN, INPUT_PULLUP);
  pinMode(OUTER2_SWITCH_PIN, INPUT_PULLUP);
  pinMode(GO1_SWITCH_PIN, INPUT_PULLUP);
  pinMode(GO2_SWITCH_PIN, INPUT_PULLUP);

pinMode(SOUND0, OUTPUT);
pinMode(SOUND1, OUTPUT);
pinMode(SOUND2, OUTPUT);
pinMode(SOUND3, OUTPUT);
pinMode(SOUND4, OUTPUT);
pinMode(SOUND5, OUTPUT);
pinMode(SOUND6, OUTPUT);
pinMode(SOUND7, OUTPUT);
pinMode(SOUND8, OUTPUT);
pinMode(SOUND9, OUTPUT);

digitalWrite(SOUND0, HIGH);
digitalWrite(SOUND1, HIGH);
digitalWrite(SOUND2, HIGH);
digitalWrite(SOUND3, HIGH);
digitalWrite(SOUND4, HIGH);
digitalWrite(SOUND5, HIGH);
digitalWrite(SOUND6, HIGH);
digitalWrite(SOUND7, HIGH);
digitalWrite(SOUND8, HIGH);
digitalWrite(SOUND9, HIGH);

digitalWrite(SOUND0, LOW);
delay(500);
digitalWrite(SOUND0, HIGH);
digitalWrite(SOUND1, LOW);
delay(500);
digitalWrite(SOUND1, HIGH);
digitalWrite(SOUND2, LOW);
delay(500);
digitalWrite(SOUND2, HIGH);
delay(1000);
digitalWrite(SOUND3, LOW);
delay(500);
digitalWrite(SOUND3, HIGH);
delay(1000);
digitalWrite(SOUND4, LOW);
delay(1000);
digitalWrite(SOUND4, HIGH);
  attachInterrupt(digitalPinToInterrupt(INNER1_SWITCH_PIN), inner1_switch_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(OUTER1_SWITCH_PIN), outer1_switch_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(INNER2_SWITCH_PIN), inner2_switch_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(OUTER2_SWITCH_PIN), outer2_switch_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(GO1_SWITCH_PIN), go1_switch_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(GO2_SWITCH_PIN), go2_switch_ISR, RISING);
  
#if 0
  Serial.begin(9600);
#endif

  // initialize the scores
  gScore[PLAYER1] = 0;
  gScore[PLAYER2] = 0;
  gNumLeds[PLAYER1] = score_to_number_of_leds(gScore[PLAYER1]);
  gNumLeds[PLAYER2] = score_to_number_of_leds(gScore[PLAYER2]);
  gCurrentPatternNumber[PLAYER1] = score_to_pattern_number(gScore[PLAYER1]);
  gCurrentPatternNumber[PLAYER2] = score_to_pattern_number(gScore[PLAYER2]);
}


// List of patterns to cycle through.  Each is defined as a separate function below.
      typedef void (*SimplePatternList[])(CRGB leds[], int num_leds);
      SimplePatternList gPatterns = { rainbow, sinelon, bpm, juggle, confetti, rainbowWithGlitter, winner};

void loop()
{


  if(play0)
  {
    digitalWrite(SOUND0, LOW);
    delay(200);
    play0=0;
    digitalWrite(SOUND0, HIGH);
  }
  if(play1)
  {
        digitalWrite(SOUND1, LOW);
    delay(200);
    play1=0;
    digitalWrite(SOUND1, HIGH);
  }
  // Call the current pattern function once, updating the 'gLeds' arrays
  gPatterns[gCurrentPatternNumber[PLAYER1]](gLeds[PLAYER1], gNumLeds[PLAYER1]);
  gPatterns[gCurrentPatternNumber[PLAYER2]](gLeds[PLAYER2], gNumLeds[PLAYER2]);

  // send the 'gLeds' arrays out to the actual LED strips
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
/*
  // reduce score every second based on current value of potentiometer
  // minimum reduction is 0, maximum reduction is 20 points per second
  read_pot1_val();
  read_pot2_val();
  EVERY_N_SECONDS( 1 ) {
    int delta1 = 20 * gPotVal[PLAYER1] / 1024;
    int delta2 = 20 * gPotVal[PLAYER2] / 1024;
    gScore[PLAYER1] = add_to_score(gScore[PLAYER1], -delta1);
    gScore[PLAYER2] = add_to_score(gScore[PLAYER2], -delta2);
  }
  */
  gNumLeds[PLAYER1] = score_to_number_of_leds(gScore[PLAYER1]);
  gNumLeds[PLAYER2] = score_to_number_of_leds(gScore[PLAYER2]);

  gCurrentPatternNumber[PLAYER1] = score_to_pattern_number(gScore[PLAYER1]);
  gCurrentPatternNumber[PLAYER2] = score_to_pattern_number(gScore[PLAYER2]);
  
#if 0
  EVERY_N_SECONDS( 1 ) {
    Serial.println(gScore[PLAYER1]);
    Serial.println(gScore[PLAYER2]);
    Serial.print("\n");
  }
#endif
  if(game_over)
  {
    game_over=0;
    delay(500);
    digitalWrite(SOUND4, LOW);
    delay(250);
    digitalWrite(SOUND4, HIGH);
    delay(5000);
    gScore[PLAYER1] = 0;
  gScore[PLAYER2] = 0;
  gGo1 =0;
  gGo2 = 0;
  gGo=0;
  p1_played=0;
  p2_played=0;
  gNumLeds[PLAYER1] = score_to_number_of_leds(gScore[PLAYER1]);
  gNumLeds[PLAYER2] = score_to_number_of_leds(gScore[PLAYER2]);
  gCurrentPatternNumber[PLAYER1] = score_to_pattern_number(gScore[PLAYER1]);
  gCurrentPatternNumber[PLAYER2] = score_to_pattern_number(gScore[PLAYER2]);
  }
  if(gScore[PLAYER1]>=SCORE_MAX || gScore[PLAYER2] >= SCORE_MAX)
  {
    game_over=1;
  }
  // go only when both Go buttons pressed.  Never stop after that.
  if (!gGo) {
    gGo = gGo1 && gGo2;
    if(gGo1&&!p1_played)
    {
      digitalWrite(SOUND2, LOW);
      delay(200);
      digitalWrite(SOUND2, HIGH);
      p1_played=1;
    }
        if(gGo2&&!p2_played)
    {
      digitalWrite(SOUND3, LOW);
      delay(200);
      digitalWrite(SOUND3, HIGH);
      p2_played=1;
    }
  }
}
  
  

