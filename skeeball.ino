// SkeeBall Game
// Peter Vatne, November 2017
// Adam Shrey, November 2017 Sound and fixes added
// Peter Vatne, November 2018 V2 Added attract and countdown modes
// Animation routines from FastLED_Demo by Mark Kriegsman, December 2014

#include "FastLED.h"
FASTLED_USING_NAMESPACE

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

// Global settings
#define SOUND_POST 1
#define DEBUG_SWITCHES 0
#define DEBUG_POTS 0
#define HANDICAP_DIALS 0
#define RACING_LIGHTS 1

// definitions for the LED strips
#define STRIP1_DATA_PIN    8
#define STRIP2_DATA_PIN    9
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define BRIGHTNESS         200
#define FRAMES_PER_SECOND  120
#define MAXMILLIAMPS       2000

// definitions for topology of LED strips
// NUM_LEDS must equal NUM_SCORE_BAR_LEDS + NUM_STAR_LEDS
// if not all leds light, try increasing MAXMILLIAMPS
#define NUM_LEDS           55
#define NUM_SCORE_BAR_LEDS 50
#define NUM_STAR_LEDS       5

// definitions for the scoring switches
#define INNER1_BUMPER_PIN  18
#define OUTER1_BUMPER_PIN  19
#define INNER2_BUMPER_PIN  20
#define OUTER2_BUMPER_PIN  21
#define DEBOUNCE_BUMPER_IN_MS 300UL
void inner1_bumper_ISR();
void outer1_bumper_ISR();
void inner2_bumper_ISR();
void outer2_bumper_ISR();

// definitions for sounds
#define LOW_SCORE_SOUND 0
#define HIGH_SCORE_SOUND 1
#define PLAYER1_SOUND 2
#define PLAYER2_SOUND 3
#define YOU_WIN_SOUND 4
#define NUM_SOUNDS 5
int gCurrentSoundPin = 0;
unsigned long gCurrentSoundTimeout = 0;
int gNextSound = -1;
#define LOW_SCORE_SOUND_PIN 30   //low point noise
#define HIGH_SCORE_SOUND_PIN 31   //high point noise
#define PLAYER1_SOUND_PIN 32   //player 1
#define PLAYER2_SOUND_PIN 33   //player 2
#define YOU_WIN_SOUND_PIN 34   //you win
#define SOUND5_PIN 35
#define SOUND6_PIN 36
#define SOUND7_PIN 37
#define SOUND8_PIN 38
#define SOUND9_PIN 39   //left right ?

// definitions for "GO" buttons
#define GO1_BUTTON_PIN   2
#define GO2_BUTTON_PIN   3
#define DEBOUNCE_GO_BUTTON_IN_MS 300UL
void go1_button_ISR();
void go2_button_ISR();

// Scores are large enough to allow scaling
#define SCORE_INNER 50
#define SCORE_OUTER 20
#define SCORE_COUNTDOWN -10
#define SCORE_MAX 501

#if HANDICAP_DIALS
// definitions for the handicap potentiometers
#define POT1_PIN    A2
#define POT2_PIN    A3
#define DEBOUNCE_POT_IN_MS 300UL
#endif // HANDICAP_DIALS

// the per-player variables
#define PLAYER1      0
#define PLAYER2      1
#define NUM_PLAYERS  2
CRGB gLeds[NUM_PLAYERS][NUM_LEDS];  // one strip of leds per player
int gNumLeds[NUM_PLAYERS];  // the total number of leds actually being lit on a strip
volatile int gScore[NUM_PLAYERS];    // player's score
volatile int gPotVal[NUM_PLAYERS];   // value of "handicap" potentiometer
volatile int gGo[NUM_PLAYERS];       // go button pressed
volatile int gReady[NUM_PLAYERS];    // player ready
volatile int gInner[NUM_PLAYERS];    // inner bumper scored
volatile int gOuter[NUM_PLAYERS];    // outer bumper scored
#if DEBUG_SWITCHES
volatile int gLastInner[NUM_PLAYERS];// shows last bumper pushed regardless of game mode
volatile int gLastOuter[NUM_PLAYERS];// shows last bumper pushed regardless of game mode
#endif
  
// game variables
#define ATTRACT_MODE 0
#define GAME_START_MODE 1
#define GAME_ON_MODE 2
#define GAME_OVER_MODE 3
volatile int gGameMode = ATTRACT_MODE;
#if DEBUG_SWITCHES
CRGB gModeColors[4] = {CRGB::Red, CRGB::Yellow, CRGB::Green, CRGB::Blue}; //debug
#endif

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

#if RACING_LIGHTS
void countdown_pattern(CRGB leds[], int num_leds) {
  static CRGB color_bar[5] = {CRGB::Green, CRGB::Yellow, CRGB::Yellow, CRGB::Yellow, CRGB::Red};
  int num_led_decades = (num_leds + 9) / 10;
  
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  switch (num_led_decades) {
  case 0:
    break;
  case 1:
    fill_solid(&leds[0], 50, CRGB::Green);
    break;
  case 2:
    fill_solid(&leds[10], 10, CRGB::Yellow);
    break;
  case 3:
    fill_solid(&leds[20], 10, CRGB::Yellow);
    break;
  case 4:
    fill_solid(&leds[30], 10, CRGB::Yellow);
    break;
  case 5:
    fill_solid(&leds[0], 50, CRGB::Red);
    break;
  }
  return;

}
#endif // RACING_LIGHTS

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
  // score can never go below 0 or above SCORE_MAX.
  // if it reaches SCORE_MAX, it sticks there.
  if (score == SCORE_MAX) {
    return score;
  }
  score += delta;
  if (score < 0) {
    score = 0;
  }
  if (score >= SCORE_MAX) {
    score = SCORE_MAX;
  }
  return score;
}

void inner1_bumper_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < DEBOUNCE_BUMPER_IN_MS) {
    return;
  }
  last_interrupt_time = interrupt_time;
  
#if DEBUG_SWITCHES
  gLastInner[PLAYER1] = 1; // debug
#endif
  if (gGameMode != GAME_ON_MODE) {
    return;
  }
  gInner[PLAYER1] = 1;
}

void outer1_bumper_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < DEBOUNCE_BUMPER_IN_MS) {
    return;
  }
  last_interrupt_time = interrupt_time;

#if DEBUG_SWITCHES
  gLastOuter[PLAYER1] = 1; // debug
#endif
  if (gGameMode != GAME_ON_MODE) {
    return;
  }
  gOuter[PLAYER1] = 1;
}

void inner2_bumper_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < DEBOUNCE_BUMPER_IN_MS) {
    return;
  }
  last_interrupt_time = interrupt_time;

#if DEBUG_SWITCHES
  gLastInner[PLAYER2] = 1; // debug
#endif
  if (gGameMode != GAME_ON_MODE) {
    return;
  }
  gInner[PLAYER2] = 1;
}

void outer2_bumper_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < DEBOUNCE_BUMPER_IN_MS) {
    return;
  }
  last_interrupt_time = interrupt_time;

#if DEBUG_SWITCHES
  gLastOuter[PLAYER2] = 1; // debug
#endif
  if (gGameMode != GAME_ON_MODE) {
    return;
  }
  gOuter[PLAYER2] = 1;
}

void go1_button_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < DEBOUNCE_GO_BUTTON_IN_MS) {
    return;
  }
  last_interrupt_time = interrupt_time;
  
  if (gReady[PLAYER1]) {
    return;
  }
  gGo[PLAYER1] = 1;//digitalRead(GO1_BUTTON_PIN);
}

void go2_button_ISR()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
 
  // debounce the switch
  if (interrupt_time - last_interrupt_time < DEBOUNCE_GO_BUTTON_IN_MS) {
    return;
  }
  last_interrupt_time = interrupt_time;

  if (gReady[PLAYER2]) {
    return;
  }
  gGo[PLAYER2] = 1;//digitalRead(GO2_BUTTON_PIN);
}

#if HANDICAP_DIALS
void read_pot1_val()
{
  static long int old_pot_time = 0;
  static int old_pot_val = 0;
  long int pot_time = millis();
  int pot_val = analogRead(POT1_PIN);    // read the value from the sensor
  
  if (((pot_time - old_pot_time) > DEBOUNCE_POT_IN_MS) && (old_pot_val != pot_val)) {
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
  
  if (((pot_time - old_pot_time) > DEBOUNCE_POT_IN_MS) && (old_pot_val != pot_val)) {
    old_pot_time = pot_time;
    old_pot_val = pot_val;
  }
  gPotVal[PLAYER2] = pot_val;
}
#endif // HANDICAP_DIALS

int score_to_number_of_leds(int score)
{
  // only light the star when you reach SCORE_MAX
  if (score == SCORE_MAX) {
    return NUM_SCORE_BAR_LEDS + NUM_STAR_LEDS;
  }
  return NUM_SCORE_BAR_LEDS * score / (SCORE_MAX - 1);
}

int score_to_pattern_number(int score)
{
  // the winner pattern is displayed if you reach SCORE_MAX
  if (score == SCORE_MAX) {
    return NUM_PATTERNS;
  }
  return NUM_PATTERNS * score / SCORE_MAX;
}

int play_sound(int sound)
{
  int pin[NUM_SOUNDS] = {LOW_SCORE_SOUND_PIN, HIGH_SCORE_SOUND_PIN, PLAYER1_SOUND_PIN, PLAYER2_SOUND_PIN, YOU_WIN_SOUND_PIN};
  unsigned long timeout_in_ms[NUM_SOUNDS] = {200, 200, 1000, 1000, 1000};

  if (gCurrentSoundTimeout) {
    clear_sound();
  }  
  gCurrentSoundPin = pin[sound];
  digitalWrite(gCurrentSoundPin, LOW);
  gCurrentSoundTimeout = millis() + timeout_in_ms[sound];
}

int play_2_sounds(int sound1, int sound2)
{
  play_sound(sound1);
  gNextSound = sound2;
}

int timeout_sound()
{
  if (!gCurrentSoundTimeout) {
    return 0;
  }
  if (millis() < gCurrentSoundTimeout) {
    return 0;
  }
  gCurrentSoundTimeout = 0;
  digitalWrite(gCurrentSoundPin, HIGH);
  if (gNextSound >= 0) {
    play_sound(gNextSound);
    gNextSound = -1;
  }
}

int wait_for_sound()
{
  if (gCurrentSoundTimeout) {
    delay(gCurrentSoundTimeout - millis());
    gCurrentSoundTimeout = 0;
  }
  digitalWrite(gCurrentSoundPin, HIGH);
  if (gNextSound >= 0) {
    play_sound(gNextSound);
    wait_for_sound();
  }
}

int clear_sound()
{
  if (!gCurrentSoundTimeout) {
    return 0;
  }
  gCurrentSoundTimeout = 0;
  digitalWrite(gCurrentSoundPin, HIGH);
  gNextSound = -1;
}

int reset_buttons()
{
  int i;
  
  for (i = 0; i < NUM_PLAYERS; i++) {
    gGo[i] = 0;
    gReady[i] = 0;
  }
}

int reset_bumpers()
{
  int i;
  
  for (i = 0; i < NUM_PLAYERS; i++) {
    gInner[i] = 0;
    gOuter[i] = 0;
  }
}

int reset_scores()
{
  int i;
  
  for (i = 0; i < NUM_PLAYERS; i++) {
    gScore[i] = 0;
  }
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
  pinMode(INNER1_BUMPER_PIN, INPUT_PULLUP);
  pinMode(OUTER1_BUMPER_PIN, INPUT_PULLUP);
  pinMode(INNER2_BUMPER_PIN, INPUT_PULLUP);
  pinMode(OUTER2_BUMPER_PIN, INPUT_PULLUP);
  pinMode(GO1_BUTTON_PIN, INPUT_PULLUP);
  pinMode(GO2_BUTTON_PIN, INPUT_PULLUP);

  pinMode(LOW_SCORE_SOUND_PIN, OUTPUT);
  pinMode(HIGH_SCORE_SOUND_PIN, OUTPUT);
  pinMode(PLAYER1_SOUND_PIN, OUTPUT);
  pinMode(PLAYER2_SOUND_PIN, OUTPUT);
  pinMode(YOU_WIN_SOUND_PIN, OUTPUT);
  pinMode(SOUND5_PIN, OUTPUT);
  pinMode(SOUND6_PIN, OUTPUT);
  pinMode(SOUND7_PIN, OUTPUT);
  pinMode(SOUND8_PIN, OUTPUT);
  pinMode(SOUND9_PIN, OUTPUT);

  digitalWrite(LOW_SCORE_SOUND_PIN, HIGH);
  digitalWrite(HIGH_SCORE_SOUND_PIN, HIGH);
  digitalWrite(PLAYER1_SOUND_PIN, HIGH);
  digitalWrite(PLAYER2_SOUND_PIN, HIGH);
  digitalWrite(YOU_WIN_SOUND_PIN, HIGH);
  digitalWrite(SOUND5_PIN, HIGH);
  digitalWrite(SOUND6_PIN, HIGH);
  digitalWrite(SOUND7_PIN, HIGH);
  digitalWrite(SOUND8_PIN, HIGH);
  digitalWrite(SOUND9_PIN, HIGH);

#if SOUND_POST
  // play all the sounds as a form of POST
  play_sound(LOW_SCORE_SOUND);
  wait_for_sound();
  delay(1000);
  play_sound(HIGH_SCORE_SOUND);
  wait_for_sound();
  delay(1000);
  play_sound(PLAYER1_SOUND);
  wait_for_sound();
  play_sound(PLAYER2_SOUND);
  wait_for_sound();
  play_sound(YOU_WIN_SOUND);
  wait_for_sound();
#endif // SOUND_POST
 
  attachInterrupt(digitalPinToInterrupt(INNER1_BUMPER_PIN), inner1_bumper_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(OUTER1_BUMPER_PIN), outer1_bumper_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(INNER2_BUMPER_PIN), inner2_bumper_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(OUTER2_BUMPER_PIN), outer2_bumper_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(GO1_BUTTON_PIN), go1_button_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(GO2_BUTTON_PIN), go2_button_ISR, RISING);
  
  // initialize the scores
  reset_buttons();
  reset_bumpers();
  reset_scores();
  gNumLeds[PLAYER1] = score_to_number_of_leds(gScore[PLAYER1]);
  gNumLeds[PLAYER2] = score_to_number_of_leds(gScore[PLAYER2]);
  gCurrentPatternNumber[PLAYER1] = score_to_pattern_number(gScore[PLAYER1]);
  gCurrentPatternNumber[PLAYER2] = score_to_pattern_number(gScore[PLAYER2]);
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])(CRGB leds[], int num_leds);
SimplePatternList gPatterns = { rainbow, rainbow, rainbow, rainbow, rainbowWithGlitter, rainbowWithGlitter, juggle };

void loop()
{
  // timeout the sounds
  timeout_sound();
  
  // only process bumpers in game mode
  if (gGameMode != GAME_ON_MODE) {
    reset_bumpers();
  }
  // attract mode pretends to hit bumpers, and lets the players alternate winning
  if (gGameMode == ATTRACT_MODE) {
    static int winner = PLAYER1;
    static int odd = 0;
      
    if (gScore[PLAYER1] < SCORE_MAX && gScore[PLAYER2] < SCORE_MAX) {
      EVERY_N_MILLISECONDS(500) {
        switch (winner) {
        case PLAYER1:
          gOuter[PLAYER1] = 1;
          if (odd) {
            gOuter[PLAYER2] = 1;
          }
          break;
        case PLAYER2:
          if (odd) {
            gOuter[PLAYER1] = 1;
          }
          gOuter[PLAYER2] = 1;
          break;
        }
        odd = !odd;
      }
    } else {
      static unsigned long attract_mode_timeout = 0;
      if (!attract_mode_timeout) {
        attract_mode_timeout = millis() + 7000UL;
      }
      if (millis() >= attract_mode_timeout) {
        reset_scores();
        winner = !winner;
        attract_mode_timeout = 0;
      }
    }
  }
  // adjust scores based on bumper hits
  if (gOuter[PLAYER1] || gOuter[PLAYER2]) {
    if (gOuter[PLAYER1]) {
      gScore[PLAYER1] = add_to_score(gScore[PLAYER1], SCORE_OUTER);
      gOuter[PLAYER1] = 0;
    }
    if (gOuter[PLAYER2]) {
      gScore[PLAYER2] = add_to_score(gScore[PLAYER2], SCORE_OUTER);
      gOuter[PLAYER2] = 0;
    }
    if (gGameMode != ATTRACT_MODE) {
      play_sound(LOW_SCORE_SOUND);
    }
  }
  if (gInner[PLAYER1] || gInner[PLAYER2]) {
    if (gInner[PLAYER1]) {
      gScore[PLAYER1] = add_to_score(gScore[PLAYER1], SCORE_INNER);
      gInner[PLAYER1] = 0;
    }
    if (gInner[PLAYER2]) {
      gScore[PLAYER2] = add_to_score(gScore[PLAYER2], SCORE_INNER);      
      gInner[PLAYER2] = 0;
    }
    if (gGameMode != ATTRACT_MODE) {
      play_sound(HIGH_SCORE_SOUND);
    }
  }

  // Call the current pattern function once, updating the 'gLeds' arrays
  gPatterns[gCurrentPatternNumber[PLAYER1]](gLeds[PLAYER1], gNumLeds[PLAYER1]);
  gPatterns[gCurrentPatternNumber[PLAYER2]](gLeds[PLAYER2], gNumLeds[PLAYER2]);
  
#if RACING_LIGHTS
  // fancy countdown in game start mode
  if (gGameMode == GAME_START_MODE) {
    countdown_pattern(gLeds[PLAYER1], gNumLeds[PLAYER1]);
    countdown_pattern(gLeds[PLAYER2], gNumLeds[PLAYER2]);
  }
#endif // RACING_LIGHTS
  
#if DEBUG_SWITCHES
  // Set bottom led to state of game
  gLeds[PLAYER1][0] = gModeColors[gGameMode];
  gLeds[PLAYER2][0] = gModeColors[gGameMode];
  
  // Set next led to state of gReady
  gLeds[PLAYER1][1] = gReady[PLAYER1] ? CRGB::Green : CRGB::Red;
  gLeds[PLAYER2][1] = gReady[PLAYER2] ? CRGB::Green : CRGB::Red;
  
  // Set next led to state of outer bumper
  gLeds[PLAYER1][2] = gLastOuter[PLAYER1] ? CRGB::Green : CRGB::Red;
  gLeds[PLAYER2][2] = gLastOuter[PLAYER2] ? CRGB::Green : CRGB::Red;
  
  // Set next led to state of inner bumper
  gLeds[PLAYER1][3] = gLastInner[PLAYER1] ? CRGB::Green : CRGB::Red;
  gLeds[PLAYER2][3] = gLastInner[PLAYER2] ? CRGB::Green : CRGB::Red;
  
  gLeds[PLAYER1][4] = CRGB::Blue;
  gLeds[PLAYER2][4] = CRGB::Blue;
  
  {
    static unsigned long clear_timeout = 0;
    
    if (!clear_timeout) {
      clear_timeout = millis() + 500UL;
    }
    if (millis() >= clear_timeout) {
      gLastOuter[PLAYER1] = 0;
      gLastOuter[PLAYER2] = 0;
      gLastInner[PLAYER1] = 0;
      gLastInner[PLAYER2] = 0;
      clear_timeout = 0;
    }
  }
#endif // DEBUG_SWITCHES
#if DEBUG_POTS
  fill_solid(gLeds[PLAYER1], 5, CRGB::Red);
  fill_solid(gLeds[PLAYER2], 5, CRGB::Red);
  fill_solid(gLeds[PLAYER1], gPotVal[PLAYER1] * 5 / 1024, CRGB::Green);
  fill_solid(gLeds[PLAYER2], gPotVal[PLAYER2] * 5 / 1024, CRGB::Green);
#endif

  // send the 'gLeds' arrays out to the actual LED strips
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) {
    gHue = gHue + 1;
  } // slowly cycle the "base color" through the rainbow

#if HANDICAP_DIALS
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
#endif

  gNumLeds[PLAYER1] = score_to_number_of_leds(gScore[PLAYER1]);
  gNumLeds[PLAYER2] = score_to_number_of_leds(gScore[PLAYER2]);

  gCurrentPatternNumber[PLAYER1] = score_to_pattern_number(gScore[PLAYER1]);
  gCurrentPatternNumber[PLAYER2] = score_to_pattern_number(gScore[PLAYER2]);
  
  // if one of the players wins, change the game mode
  if (gScore[PLAYER1] >= SCORE_MAX || gScore[PLAYER2] >= SCORE_MAX) {
    switch (gGameMode) {
    case ATTRACT_MODE:
      // attract mode takes care of itself
      break;
    case GAME_START_MODE:
      // start mode cannot add points
      break;
    case GAME_ON_MODE:
      play_2_sounds(gScore[PLAYER1] >= SCORE_MAX? PLAYER1_SOUND : PLAYER2_SOUND, YOU_WIN_SOUND);
      gGameMode = GAME_OVER_MODE;
      break;
    case GAME_OVER_MODE:
      // wait 10 seconds before restarting the game
      {
        static unsigned long game_over_timeout = 0;
        
        if (!game_over_timeout) {
          game_over_timeout = millis() + 10000UL;
        }
        if (millis() >= game_over_timeout) {
          gGameMode = ATTRACT_MODE;
          reset_buttons();
          reset_bumpers();
          reset_scores();
          game_over_timeout = 0;
        }
      }
      break;
    }
  }
  
  // go only when both Go buttons pressed.  Never stop after that.
  if (gGameMode == ATTRACT_MODE || gGameMode == GAME_START_MODE) {
    if (gGo[PLAYER1] && !gReady[PLAYER1]) {
      play_sound(PLAYER1_SOUND);
      gGameMode = GAME_START_MODE;
      gScore[PLAYER1] = SCORE_MAX - 1;
      if (!gReady[PLAYER2]) {
        gScore[PLAYER2] = 0;
      }
      gGo[PLAYER1] = 0;
      gReady[PLAYER1] = 1;
    }
    if (gGo[PLAYER2] && !gReady[PLAYER2]) {
      play_sound(PLAYER2_SOUND);
      gGameMode = GAME_START_MODE;
      gScore[PLAYER2] = SCORE_MAX - 1;
      if (!gReady[PLAYER1]) {
        gScore[PLAYER1] = 0;
      }
      gGo[PLAYER2] = 0;
      gReady[PLAYER2] = 1;
    }
    // if both players are ready, count down the score bars before starting
    if (gReady[PLAYER1] && gReady[PLAYER2]) {
      static unsigned long game_start_timeout = 0;
      
      EVERY_N_MILLISECONDS(50) {
        gScore[PLAYER1] = add_to_score(gScore[PLAYER1], SCORE_COUNTDOWN);
        gScore[PLAYER2] = add_to_score(gScore[PLAYER2], SCORE_COUNTDOWN);
      }
      if (gScore[PLAYER1] == 0 || gScore[PLAYER2] == 0) {
        game_start_timeout = millis();
      }
      if (!game_start_timeout) {
        game_start_timeout = millis() + 5000UL;
      }
      if (millis() >= game_start_timeout) {
        gGameMode = GAME_ON_MODE;
        reset_buttons();
        reset_bumpers();
        reset_scores();
        game_start_timeout = 0;
      }
    }
  }
}
