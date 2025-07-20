#include <M5Core2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include <SPIFFS.h>
#include "AudioManager.h"
#include "UIManager.h"
#include "NetworkManager.h"
#include "WakeWordManager.h"
#include "config.h"
#include <loadenv.hpp>

// Global variables
AudioManager audioManager;
UIManager uiManager;
NetworkManager networkManager;
WakeWordManager wakeWordManager;

// State management
enum AppState {
  STATE_IDLE,
  STATE_TOUCH_RECORDING,
  STATE_VOICE_RECORDING, // Re-enabled for wake word response
  STATE_WAKEWORD_REGISTRATION,
  STATE_WAITING_RESPONSE,
  STATE_PLAYING_RESPONSE
};

AppState currentState = STATE_IDLE;
AppState nextState = STATE_IDLE;
unsigned long recordingStartTime = 0;
bool stateChanged = false;

// --- State Transition ---
void changeState(AppState newState) {
  if (currentState != newState) {
    nextState = newState;
    stateChanged = true;
  }
}

// --- State Init Functions ---
void initIdleState() {
  Serial.println("=== Entering IDLE state ===");
  uiManager.showIdleScreen();
  wakeWordManager.startListening(); // Start listening for wake word
  wakeWordManager.reset();
}

void initTouchRecordingState() {
  Serial.println("=== Entering TOUCH_RECORDING state ===");
  wakeWordManager.stopListening(); // Stop wake word listener to free I2S
  recordingStartTime = millis();
  uiManager.showHearingScreen();
  audioManager.startRecording();
}

void initVoiceRecordingState() {
  Serial.println("=== Entering VOICE_RECORDING state (after wake word) ===");
  wakeWordManager.stopListening(); // Stop wake word listener to free I2S
  recordingStartTime = millis();
  uiManager.showHearingScreen();
  audioManager.startRecording();
}

void initWakeWordRegistrationState() {
    Serial.println("=== Entering WAKEWORD_REGISTRATION state ===");
    wakeWordManager.stopListening(); // Stop listener before starting registration
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Say the wake word...");
    M5.Lcd.println("(Press Middle Button to cancel)");
}

void initWaitingResponseState() {
  Serial.println("=== Entering WAITING_RESPONSE state ===");
  // The screen is now changed before entering this state.
  // uiManager.showThinkingScreen();
}

void initPlayingResponseState() {
  Serial.println("=== Entering PLAYING_RESPONSE state ===");
  uiManager.showSpeakingScreen();
  
  size_t responseSize = networkManager.getResponseSize();
  uint8_t* responseData = networkManager.getResponseData();
  
  audioManager.startPlayback(responseData, responseSize, 24000);
}

// --- Audio Handling ---
void stopRecordingAndSend(const char* endpoint) {
  Serial.printf("=== Stopping recording and sending to %s ===\n", endpoint);
  
  size_t dataSize = audioManager.stopRecording();
  Serial.printf("DEBUG: Recorded data size: %d bytes\n", dataSize);
  
  if (dataSize > 0) {
    // Change to thinking screen immediately after recording stops.
    uiManager.showThinkingScreen();

    uint8_t* audioData = audioManager.getRecordedData();
    networkManager.sendAudioData(audioData, dataSize, endpoint);
    changeState(STATE_WAITING_RESPONSE);
  } else {
    Serial.println("ERROR: No recorded data, returning to IDLE state");
    changeState(STATE_IDLE);
  }
}

// --- State Handler Functions ---
void handleIdleState() {
  // Listen for wake word. Button presses are handled in the main loop.
  if (wakeWordManager.listenAndDetect()) {
    // Wake word detected, start voice recording
    changeState(STATE_VOICE_RECORDING);
  }
}

void handleTouchRecordingState() {
  unsigned long recordingDuration = millis() - recordingStartTime;
  
  // Stop recording if button is released OR timeout is reached
  if (!M5.BtnA.isPressed() || recordingDuration > MAX_TOUCH_RECORDING_TIME) {
    stopRecordingAndSend("stsGoogle");
  }
}

void handleVoiceRecordingState() {
  unsigned long recordingDuration = millis() - recordingStartTime;

  // Stop recording after a fixed time, or if a button is tapped to interrupt
  if (recordingDuration > MAX_VOICE_RECORDING_TIME) {
    stopRecordingAndSend("stsWhisper");
  } else if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) {
    stopRecordingAndSend("stsWhisper");
  }
}

void handleWakeWordRegistrationState() {
    int capturedLength = wakeWordManager.captureAndRegisterWakeWord();

    if (capturedLength > 0) {
        M5.Lcd.println("Registration successful!");
    } else {
        M5.Lcd.println("Registration failed or cancelled.");
    }
    delay(2000);
    changeState(STATE_IDLE);
}

void handleWaitingResponseState() {
  if (networkManager.isResponseReady()) {
    changeState(STATE_PLAYING_RESPONSE);
  } else if (networkManager.hasError()) {
    changeState(STATE_IDLE);
  }
}

void handlePlayingResponseState() {
  if (!audioManager.isPlaying()) {
    changeState(STATE_IDLE);
  }
}

// --- Main Setup & Loop ---
void setup() {
  M5.begin();
  Serial.begin(115200);
  Serial.println("=== Bot-tan Starting ===");

  SPIFFS.begin(true);
  
  auto env = loadEnv("/.env");
  std::string ssid = env["WIFI_SSID"];
  std::string password = env["WIFI_PASSWORD"];
  networkManager.connectWiFi(ssid.c_str(), password.c_str());
  
  audioManager.init();
  uiManager.init();
  if (!wakeWordManager.init()) {
      M5.Lcd.println("WakeWordManager Init Failed!");
      while(1) delay(100);
  }
  
  M5.Axp.SetSpkEnable(true);
  networkManager.initConversation();

  // Directly set and initialize the first state
  currentState = STATE_IDLE;
  initIdleState();
}

void loop() {
  M5.update();
  
  // Handle state changes first
  if (stateChanged) {
    currentState = nextState;
    stateChanged = false;
    
    switch (currentState) {
      case STATE_IDLE: initIdleState(); break;
      case STATE_TOUCH_RECORDING: initTouchRecordingState(); break;
      case STATE_VOICE_RECORDING: initVoiceRecordingState(); break;
      case STATE_WAKEWORD_REGISTRATION: initWakeWordRegistrationState(); break;
      case STATE_WAITING_RESPONSE: initWaitingResponseState(); break;
      case STATE_PLAYING_RESPONSE: initPlayingResponseState(); break;
    }
  }
  
  // Handle triggers from IDLE state
  if (currentState == STATE_IDLE) {
    if (M5.BtnA.wasPressed()) {
      changeState(STATE_TOUCH_RECORDING);
    } else if (M5.BtnC.wasPressed()) {
      changeState(STATE_WAKEWORD_REGISTRATION);
    }
  }

  // Execute current state's handler
  switch (currentState) {
    case STATE_IDLE: handleIdleState(); break;
    case STATE_TOUCH_RECORDING: handleTouchRecordingState(); break;
    case STATE_VOICE_RECORDING: handleVoiceRecordingState(); break;
    case STATE_WAKEWORD_REGISTRATION: handleWakeWordRegistrationState(); break;
    case STATE_WAITING_RESPONSE: handleWaitingResponseState(); break;
    case STATE_PLAYING_RESPONSE: handlePlayingResponseState(); break;
  }
  
  delay(10);
}
