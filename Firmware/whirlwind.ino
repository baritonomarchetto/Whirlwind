/* 
  PC-to-Jamma Interface - Raspberry Pi Pico
  Joystick + keyboard management + sync monitor (15/25/31 kHz)
  
  Core0: state machine for inputs, shift with 2-second timeout
  Core1: H-sync frequency monitoring and video enable
  
  P1 START button:
    - Press < 2 sec → sends START on release (arcade behaviour)
    - Press ≥ 2 sec → activates SHIFT (alternate keys) while held
    
  by Barito 2022-2026
*/

#include <Joystick.h>
#include <Keyboard.h>

// ========================= CONFIGURATION =========================
#define SYNC_MONITOR_ACTIVE          // comment to disable sync control
#define SW_INPUTS 26                 // total number of inputs

// Pins
constexpr int HSyncPin = 0;
constexpr int DisablePin = 1;
constexpr int LedPin = 25;

// Frequency threshold (μs) - 15kHz = 66μs, 25kHz = 40μs, 31kHz = 32μs
constexpr int FREQ_THRESHOLD_US = 45;   // internal value for 15kHz

constexpr int DEBOUNCE_MS = 40;         // debounce in milliseconds
constexpr unsigned long USB_DELAY_MS = 8;   // delay to avoid USB packet loss
constexpr unsigned long START_HOLD_MS = 80; // START pulse duration (as in original)

// Samples for frequency filter
constexpr int SAMPLES = 100;

// ========================= DATA STRUCTURES =========================
enum class ButtonType : uint8_t { JOYPAD, KEYBOARD };

struct ButtonMapping {
  uint8_t normal;   // normal code (1-32 = joypad, >32 = ASCII)
  uint8_t shifted;  // shifted code
};

struct Input {
  uint8_t pin;
  bool lastRaw;           // last raw read (no debounce)
  bool debounced;         // debounced value
  unsigned long debTime;  // timestamp of last change
  ButtonMapping map;
};

// Input mapping 
const Input inputs[SW_INPUTS] PROGMEM = {
  // P1 START - SHIFT BUTTON (MAME default "keyboard 1": ASCII 49). Shifted value MUST be the same as non shifted value here.
  { 6, HIGH, HIGH, 0, { 1,   1 } },
  // P2 START (MAME default "keyboard 2": ASCII 50). Suggested shifted keyboard value: ESC (ASCII 177).
  { 7, HIGH, HIGH, 0, { 2,  27 } },
  // P1 UP (MAME default "up arrow": ASCII 218). Suggested shifted keyboard value: TILDE (ASCII 189).
  { 8, HIGH, HIGH, 0, { 3,  28 } },
  // P1 DWN (MAME default "down arrow": ASCII 217). Suggested shifted keyboard value: "P" (ASCII 112).
  {10, HIGH, HIGH, 0, { 4,  29 } },
  // P1 LEFT (MAME default "left arrow": ASCII 216). Suggested shifted keyboard value: TAB (ASCII 179).
  {16, HIGH, HIGH, 0, { 5,  30 } },
  // P1 RIGHT (MAME default "right arrow": ASCII 215)
  {14, HIGH, HIGH, 0, { 6,  31 } },
  // P1 B1 (MAME default "left CTRL": ASCII 128)
  {12, HIGH, HIGH, 0, { 7,  32 } },
  // P1 B2 (MAME default "left ALT": ASCII 130)
  {20, HIGH, HIGH, 0, { 8,   8 } },
  // P1 B3 (MAME default "SPACE": ASCII 180)
  {18, HIGH, HIGH, 0, { 9,   9 } },
  // P1 B4 (MAME default "left SHIFT": ASCII 129)
  {22, HIGH, HIGH, 0, {10,  10 } },
  // P1 B5 (MAME default "z": ASCII 122)
  {24, HIGH, HIGH, 0, {11, 101 } },
  // P1 B6 (MAME default "x": ASCII 120)
  {27, HIGH, HIGH, 0, {12,  12 } },
  // P2 UP (MAME default "r": ASCII 114)
  { 9, HIGH, HIGH, 0, {13,  13 } },
  // P2 DWN (MAME default "f": ASCII 102)
  {11, HIGH, HIGH, 0, {14,  14 } },
  // P2 LEFT (MAME default "d": ASCII 100)
  {17, HIGH, HIGH, 0, {15,  15 } },
  // P2 RIGHT (MAME default "g": ASCII 103)
  {15, HIGH, HIGH, 0, {16,  16 } },
  // P2 B1 (MAME default "a": ASCII 97)
  {13, HIGH, HIGH, 0, {17,  17 } },
  // P2 B2 (MAME default "s": ASCII 115)
  {21, HIGH, HIGH, 0, {18,  18 } },
  // P2 B3 (MAME default "q": ASCII 113)
  {19, HIGH, HIGH, 0, {19,  19 } },
  // P2 B4 (MAME default "w": ASCII 119)
  {23, HIGH, HIGH, 0, {20,  20 } },
  // P2 B5 (MAME default "i": ASCII 105)
  {26, HIGH, HIGH, 0, {21,  21 } },
  // P2 B6 (MAME default "k": ASCII 107)
  {28, HIGH, HIGH, 0, {22,  22 } },
  // P1 COIN (MAME default "5": ASCII 53)
  { 4, HIGH, HIGH, 0, {23,  23 } },
  // P2 COIN (MAME default "6": ASCII 54)
  { 5, HIGH, HIGH, 0, {24,  24 } },
  // SERVICE SW (MAME default "F3": ASCII 196)
  { 3, HIGH, HIGH, 0, {25,  25 } },
  // TEST SW (MAME default "F2": ASCII 195)
  { 2, HIGH, HIGH, 0, {26,  26 } }
};

// Global state variables (RAM)
static Input runtimeInputs[SW_INPUTS];   // modifiable copy in RAM
static bool lastSentState[SW_INPUTS];    // last state sent via USB

// SHIFT management (P1 START, index 0)
static enum class ShiftState : uint8_t {
  IDLE,      // start released
  PRESSING,  // pressed for less than 2 seconds
  ACTIVE     // pressed for ≥2 seconds (shift active)
} shiftState = ShiftState::IDLE;
static unsigned long shiftPressStart = 0;
static bool startBlock = false;          // blocks START sending if shifted key used

// Variables for sync monitor (core1)
static int periodSum = 0, periodCounter = 0;
static bool videoEnabled = false, lastVideoEnabled = false;

// ========================= PROTOTYPES =========================
void initInputs();
void updateDebounce();
void processStartButton();
void processOtherButtons();
void sendCommand(uint8_t code, bool pressed);
void sendStartCommand();
void releaseAllShifted();
void syncMonitorTask();   // core1

// ========================= SETUP =========================
void setup() {
  initInputs();
  
  pinMode(HSyncPin, INPUT_PULLUP);
  pinMode(LedPin, OUTPUT);
  pinMode(DisablePin, OUTPUT);
  
#ifdef SYNC_MONITOR_ACTIVE
  digitalWrite(DisablePin, HIGH);  // video disabled at startup
  digitalWrite(LedPin, LOW);
#else
  digitalWrite(DisablePin, LOW);   // video always enabled
  digitalWrite(LedPin, HIGH);
#endif

  Joystick.begin();
  Keyboard.begin();
  // Serial.begin(115200); // debug
}

// Core1: only sync monitoring
#ifdef SYNC_MONITOR_ACTIVE
void setup1() {}
void loop1() { syncMonitorTask(); }
#endif

// ========================= MAIN LOOP (CORE0) =========================
void loop() {
  updateDebounce();               // update all inputs with debounce
  processStartButton();          // state machine for shift + START send
  processOtherButtons();         // handle remaining 25 inputs
}

// ========================= BASIC FUNCTIONS =========================
void initInputs() {
  for (int i = 0; i < SW_INPUTS; i++) {
    memcpy_P(&runtimeInputs[i], &inputs[i], sizeof(Input));
    pinMode(runtimeInputs[i].pin, INPUT_PULLUP);
    bool raw = digitalRead(runtimeInputs[i].pin);
    runtimeInputs[i].lastRaw = raw;
    runtimeInputs[i].debounced = raw;
    runtimeInputs[i].debTime = millis();
    lastSentState[i] = raw;       // initially equal to physical state
  }
}

void updateDebounce() {
  unsigned long now = millis();
  for (int i = 0; i < SW_INPUTS; i++) {
    bool raw = digitalRead(runtimeInputs[i].pin);
    if (raw != runtimeInputs[i].lastRaw) {
      runtimeInputs[i].lastRaw = raw;
      runtimeInputs[i].debTime = now;
    }
    if ((now - runtimeInputs[i].debTime) >= DEBOUNCE_MS) {
      runtimeInputs[i].debounced = raw;
    }
  }
}

// Send a command (joystick or keyboard) with the necessary USB delay
void sendCommand(uint8_t code, bool pressed) {
  if (code <= 32) {  // joystick
    Joystick.button(code, pressed);
  } else {           // keyboard
    if (pressed) Keyboard.press(code);
    else Keyboard.release(code);
  }
  delay(USB_DELAY_MS);
}

// Send START command (80ms pulse as in the original)
void sendStartCommand() {
  if (runtimeInputs[0].map.normal <= 32) {
    Joystick.button(runtimeInputs[0].map.normal, true);
    delay(START_HOLD_MS);
    Joystick.button(runtimeInputs[0].map.normal, false);
  } else {
    Keyboard.press(runtimeInputs[0].map.normal);
    delay(START_HOLD_MS);
    Keyboard.release(runtimeInputs[0].map.normal);
  }
  delay(USB_DELAY_MS);
}

// Release all shifted keys (called when START is released)
void releaseAllShifted() {
  for (int i = 1; i < SW_INPUTS; i++) {
    uint8_t code = runtimeInputs[i].map.shifted;
    sendCommand(code, false);
  }
}

// ========================= P1 START (SHIFT) MANAGEMENT =========================
void processStartButton() {
  static bool lastDebounced = HIGH;  // previous state of the button
  bool current = runtimeInputs[0].debounced;  // LOW = pressed
  
  // Detect falling edge (press) and rising edge (release)
  if (current != lastDebounced) {
    lastDebounced = current;
    
    if (current == LOW) {  // button pressed
      shiftPressStart = millis();
      shiftState = ShiftState::PRESSING;
      startBlock = false;        // reset block for this press
    } 
    else {  // button released
      // If still in PRESSING state (no shift activated) and startBlock false, send START
      if (shiftState == ShiftState::PRESSING && !startBlock) {
        sendStartCommand();
      }
      // Deactivate shift and release any shifted keys
      shiftState = ShiftState::IDLE;
      releaseAllShifted();
      startBlock = false;
    }
  }
  
  // Transition from PRESSING to ACTIVE after 2 seconds (if still pressed)
  if (shiftState == ShiftState::PRESSING && current == LOW) {
    if (millis() - shiftPressStart >= 2000) {
      shiftState = ShiftState::ACTIVE;
    }
  }
}

// ========================= OTHER BUTTONS MANAGEMENT =========================
void processOtherButtons() {
  for (int i = 1; i < SW_INPUTS; i++) {
    bool current = runtimeInputs[i].debounced;
    if (current != lastSentState[i]) {
      lastSentState[i] = current;
      
      // Choose normal or shifted code based on SHIFT state
      uint8_t code;
      if (shiftState == ShiftState::ACTIVE) {
        code = runtimeInputs[i].map.shifted;
        // If shifted code differs from normal, block START sending on release
        if (code != runtimeInputs[i].map.normal) {
          startBlock = true;
        }
      } else {
        code = runtimeInputs[i].map.normal;
      }
      
      // Send command: current == LOW means pressed (pullup)
      sendCommand(code, (current == LOW));
    }
  }
}

// ========================= SYNC MONITOR (CORE1) =========================
void syncMonitorTask() {
  int duration = pulseIn(HSyncPin, HIGH);
  if (duration == 0) return;
  periodSum += duration;
  periodCounter++;
  
  if (periodCounter >= SAMPLES) {
    bool newEnable = (periodSum > FREQ_THRESHOLD_US * SAMPLES);
    periodSum = 0;
    periodCounter = 0;
    
    if (newEnable != lastVideoEnabled) {
      lastVideoEnabled = newEnable;
      digitalWrite(DisablePin, !newEnable);  // DisablePin LOW = video ON
      digitalWrite(LedPin, newEnable);
    }
  }
}