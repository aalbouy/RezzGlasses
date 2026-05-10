#include <FastLED.h>

// ------------------------------------------------------
// USER SETTINGS
// ------------------------------------------------------
#define DATA_PIN 6
#define NUM_EYES 2
#define NUM_RINGS 3
#define BRIGHTNESS 5 // [0; 255]

#define RED_HUE 0
#define GREEN_HUE 100
#define BLUE_HUE 230
#define PURPLE_HUE 270

#define RANDOM_PICK_INTERVAL_SECONDS 30
#define RANDOM_PICK_INTERVAL_MSECONDS RANDOM_PICK_INTERVAL_SECONDS*1000

// Order: 24-LED ring, 16-LED ring, 8-LED ring
int ringSize[NUM_RINGS] = {24, 16, 8};

// width of each chaser
int chaseWidth[NUM_RINGS] = {5, 3, 6};

// ------------------------------------------------------
// INTERNAL CALCULATIONS
// ------------------------------------------------------
int ledsPerEye = ringSize[0] + ringSize[1] + ringSize[2];
int totalLEDs  = ledsPerEye * NUM_EYES;

CRGB leds[100];  // Holds up to 96 LEDs, extra for safety

// Only ONE set of positions — both eyes use them
int pos[NUM_RINGS] = {0};

// Currently used direction
int usedDirection[NUM_RINGS] = {+1, +1, +1};

// Currently used hue
int currentHue = GREEN_HUE;

// Last time the color and orientation has been picked
unsigned long lastChange = millis();

// Boolean if the eyes are opposed
bool opposeEyes = false;

// ------------------------------------------------------
// LED index mapping
// ------------------------------------------------------
int idx(int eye, int ring, int pixel) {
    int base = eye * ledsPerEye;

    int offset = 0;
    for (int r = 0; r < ring; r++)
        offset += ringSize[r];

    return base + offset + pixel;
}
// ------------------------------------------------------
// Pattern: Ring Cascade
// Lights up outer → mid → inner, then fades out in reverse
// ------------------------------------------------------

// State for the cascade pattern
int cascadePhase = 0;       // 0=outer on, 1=mid on, 2=inner on
unsigned long cascadeLastStep = 0;
#define CASCADE_STEP_MS 300  // time between each ring lighting up

void patternCascade(int hue) {
    unsigned long now = millis();

    // Advance phase on timer
    if (now - cascadeLastStep > CASCADE_STEP_MS) {
        cascadeLastStep = now;
        cascadePhase = (cascadePhase + 1) % 3;
        // Phases:
        // 0 → outer lights up
        // 1 → mid lights up
        // 2 → inner lights up
    }

    // Determine which rings are on based on phase
    bool ringOn[NUM_RINGS] = {false, false, false};

    if (cascadePhase == 0) ringOn[0] = true;  // outer
    if (cascadePhase == 1) ringOn[1] = true;  // mid
    if (cascadePhase == 2) ringOn[2] = true;  // inner

    // Draw
    for (int eye = 0; eye < NUM_EYES; eye++) {
        for (int r = 0; r < NUM_RINGS; r++) {
            for (int i = 0; i < ringSize[r]; i++) {
                if (ringOn[r])
                    leds[idx(eye, r, i)] = CHSV(hue, 255, 255);
            }
        }
    }
}


// ------------------------------------------------------
// Animation for both eyes (mirrored)
// ------------------------------------------------------
void animateMirroredEyes(int hue) {

    // Update rotation states (only once per ring)
    for (int r = 0; r < NUM_RINGS; r++) {
        int len = ringSize[r];
        pos[r] = (pos[r] + 1) % len;
    }

    // For each eye and each ring
    for (int eye = 0; eye < NUM_EYES; eye++) {
        for (int r = 0; r < NUM_RINGS; r++) {

            int len   = ringSize[r];
            int width = chaseWidth[r];

            int dir = usedDirection[r];
            if (opposeEyes && (eye == 0))
                dir = -dir;

            // Compute current head position for this eye/ring
            int head = (dir > 0) ? pos[r] : (len - pos[r]) % len;

            // Light LEDs
            for (int i = 0; i < len; i++) {
                int globalIndex = idx(eye, r, i);
                int delta = (i - head + len) % len;

                if (delta < width)
                    leds[globalIndex] = CHSV(hue, 255, 255);  // red
                if ((((delta + len/2)%len) < width) && (r < 2))
                    leds[globalIndex] = CHSV(hue, 255, 255);  // red
            }
        }
    }
}
// ------------------------------------------------------
// Pattern: Rainbow Chaser
// Same as animateMirroredEyes but with per-pixel rainbow hue
// ------------------------------------------------------

int rainbowPos[NUM_RINGS] = {0};

void patternRainbow(int hue) {

    // Update rotation states
    for (int r = 0; r < NUM_RINGS; r++) {
        int len = ringSize[r];
        rainbowPos[r] = (rainbowPos[r] + 1) % len;
    }

    for (int eye = 0; eye < NUM_EYES; eye++) {
        for (int r = 0; r < NUM_RINGS; r++) {

            int len   = ringSize[r];
            int width = chaseWidth[r];

            int dir = usedDirection[r];
            if (opposeEyes && (eye == 0))
                dir = -dir;

            int head = (dir > 0) ? rainbowPos[r] : (len - rainbowPos[r]) % len;

            for (int i = 0; i < len; i++) {
                int globalIndex = idx(eye, r, i);
                int delta = (i - head + len) % len;

                // Hue spread across the full ring
                uint8_t pixelHue = (i * 256 / len);

                if (delta < width)
                    leds[globalIndex] = CHSV(pixelHue, 255, 255);
                if ((((delta + len/2) % len) < width) && (r < 2))
                    leds[globalIndex] = CHSV(pixelHue, 255, 255);
            }
        }
    }
}

// Pattern function signature - To 
typedef void (*PatternFunc)(int hue);
PatternFunc patterns[] = { animateMirroredEyes, patternCascade, patternRainbow };
int numPatterns = sizeof(patterns) / sizeof(patterns[0]);
int currentPattern = 0;

// ------------------------------------------------------
// Random pick direction and color
// ------------------------------------------------------
void pickSettings() {
    currentHue = random(359);
    // Only changing middle ring
    usedDirection[1] =  random(2) * 2 - 1; // -1 or +1
    currentPattern = random(numPatterns);
}


// ------------------------------------------------------
// SETUP
// ------------------------------------------------------
void setup() {
    randomSeed(analogRead(0));
    pickSettings();
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, totalLEDs);
    FastLED.setBrightness(BRIGHTNESS);
}


// ------------------------------------------------------
// LOOP
// ------------------------------------------------------
void loop() {
    FastLED.clear();
    patterns[currentPattern](currentHue);
    FastLED.show();
    delay(70);

    if (millis() > RANDOM_PICK_INTERVAL_MSECONDS + lastChange) {
<<<<<<< HEAD
        pickSettings();
=======
        // pickDirectionAndColor();
>>>>>>> 7e35df221b369abcdd4841e58141094e13ddd85b
        lastChange = millis();
    }
}
