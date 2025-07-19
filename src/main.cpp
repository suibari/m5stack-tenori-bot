#include <M5Core2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include <SPIFFS.h>
#include "AudioManager.h"
#include "UIManager.h"
#include "NetworkManager.h"
#include "VoiceDetector.h"
#include "config.h"
#include <loadenv.hpp>

// グローバル変数
AudioManager audioManager;
UIManager uiManager;
NetworkManager networkManager;
VoiceDetector voiceDetector;

// 状態管理
enum AppState {
  STATE_IDLE,
  STATE_TOUCH_RECORDING,
  STATE_VOICE_RECORDING,
  STATE_WAITING_RESPONSE,
  STATE_PLAYING_RESPONSE
};

AppState currentState = STATE_IDLE;
AppState nextState = STATE_IDLE;
unsigned long recordingStartTime = 0;
bool isTouchPressed = false;
bool isVoiceDetected = false;
bool stateChanged = false;

// 状態遷移を管理する関数
void changeState(AppState newState) {
  if (currentState != newState) {
    nextState = newState;
    stateChanged = true;
  }
}

// 各状態の初期化処理
void initIdleState() {
  Serial.println("=== Entering IDLE state ===");
  uiManager.showIdleScreen();
  voiceDetector.startContinuousRecording();
  isTouchPressed = false;
  isVoiceDetected = false;
}

void initTouchRecordingState() {
  Serial.println("=== Entering TOUCH_RECORDING state ===");
  recordingStartTime = millis();
  isTouchPressed = true;
  isVoiceDetected = false;
  
  voiceDetector.stopContinuousRecording(); // I2S音声を止める
  uiManager.showHearingScreen();           // I2S画像を表示
  audioManager.startRecording();
}

void initVoiceRecordingState() {
  Serial.println("=== Entering VOICE_RECORDING state ===");
  recordingStartTime = millis();
  isVoiceDetected = true;
  
  voiceDetector.stopContinuousRecording();
  audioManager.startRecording();
  // 常時録音中なので画面は変更しない
}

void initWaitingResponseState() {
  Serial.println("=== Entering WAITING_RESPONSE state ===");
  voiceDetector.stopContinuousRecording();
  uiManager.showThinkingScreen();
}

void initPlayingResponseState() {
  Serial.println("=== Entering PLAYING_RESPONSE state ===");
  voiceDetector.stopContinuousRecording();
  uiManager.showSpeakingScreen();
  
  size_t responseSize = networkManager.getResponseSize();
  uint8_t* responseData = networkManager.getResponseData();
  
  // サーバ応答は24000Hz
  audioManager.startPlayback(responseData, responseSize, 24000);
}

// 録音停止とサーバ送信
void stopRecordingAndSend(const char* endpoint) {
  Serial.printf("=== Stopping recording and sending to %s ===\n", endpoint);
  
  // 録音停止
  size_t dataSize = audioManager.stopRecording();
  
  if (dataSize > 0) {
    uint8_t* audioData = audioManager.getRecordedData();
    Serial.printf("rawAudioDataSize: %d\n", dataSize);

    // 再生: ユーザタッチ時=Google送信時のみ再生
    if (String(endpoint) == "stsGoogle") {
      audioManager.startPlayback(audioData, dataSize, 16000);
    }

    networkManager.sendAudioData(audioData, dataSize, endpoint);
    changeState(STATE_WAITING_RESPONSE);
  } else {
    // 録音データが無い場合はアイドル状態に戻る
    changeState(STATE_IDLE);
  }
}

// 各状態のメイン処理
void handleIdleState(bool touchPressed) {
  // デバッグ情報（最初の数秒だけ）
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime > 2000) {
    Serial.printf("IDLE: touch=%d, voice=%d\n", 
                  touchPressed, voiceDetector.isVoiceDetected());
    lastDebugTime = millis();
  }
  
  // STATE_IDLEでのみVADが有効
  if (touchPressed && !isTouchPressed) {
    // タッチ開始 - タッチ録音モードに移行
    changeState(STATE_TOUCH_RECORDING);
  } else if (!touchPressed) {
    // 常時録音での音声検出
    if (voiceDetector.isVoiceDetected()) {
      if (!isVoiceDetected) {
        // 音声検出開始
        changeState(STATE_VOICE_RECORDING);
      }
    } else {
      isVoiceDetected = false;
    }
  }
  
  isTouchPressed = touchPressed;
}

void handleTouchRecordingState(bool touchPressed) {
  unsigned long recordingDuration = millis() - recordingStartTime;
  Serial.printf("Touch recording: duration=%lu ms, touch=%d\n", recordingDuration, touchPressed);

  if (!touchPressed) {
    // タッチ終了 - 録音終了してサーバに送信
    stopRecordingAndSend("stsGoogle");
  } else if (millis() - recordingStartTime > MAX_RECORDING_TIME) {
    // 10秒経過 - 自動的に録音終了
    stopRecordingAndSend("stsGoogle");
  }
}

void handleVoiceRecordingState(bool touchPressed) {
  unsigned long recordingDuration = millis() - recordingStartTime;
  bool voiceStillDetected = voiceDetector.isVoiceDetected();

  if (touchPressed && !isTouchPressed) {
    // タッチされた場合は優先してタッチ録音モードに移行
    audioManager.stopRecording();
    changeState(STATE_TOUCH_RECORDING);
  } else if (!voiceDetector.isVoiceDetected() || 
             millis() - recordingStartTime > MAX_RECORDING_TIME) {
    // 音声終了または10秒経過 - 録音終了してサーバに送信
    stopRecordingAndSend("stsWhisper");
  }
  
  isTouchPressed = touchPressed;
}

void handleWaitingResponseState() {
  // デバッグ情報
  static unsigned long lastStatusTime = 0;
  if (millis() - lastStatusTime > 1000) {
    Serial.printf("Waiting: ready=%d, error=%d\n", 
                  networkManager.isResponseReady(), networkManager.hasError());
    lastStatusTime = millis();
  }

  // NetworkManagerがレスポンスを処理中
  if (networkManager.isResponseReady()) {
    // レスポンス受信完了 - 再生開始
    changeState(STATE_PLAYING_RESPONSE);
  } else if (networkManager.hasError()) {
    // エラー時
    changeState(STATE_IDLE);
  }
}

void handlePlayingResponseState() {
  bool stillPlaying = audioManager.isPlaying();
  static unsigned long lastPlaybackStatus = 0;
  if (millis() - lastPlaybackStatus > 500) {
    Serial.printf("Playing response: still_playing=%d\n", stillPlaying);
    lastPlaybackStatus = millis();
  }

  if (!audioManager.isPlaying()) {
    // 再生終了 - アイドル状態に戻る
    changeState(STATE_IDLE);
  }
}

// ---------------------
// main: setup & loop
// ---------------------
void setup() {
  M5.begin();
  Serial.begin(115200);
  
  Serial.println("=== Bot-tan Starting ===");

  // SPIFFS初期化
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  
  // WiFi接続
  SPIFFS.begin(true);
  auto env = loadEnv("/.env");
  std::string ssid = env["WIFI_SSID"];
  std::string password = env["WIFI_PASSWORD"];
  networkManager.connectWiFi(ssid.c_str(), password.c_str());
  
  // 各モジュール初期化
  audioManager.init();
  uiManager.init();
  voiceDetector.init();
  
  // スピーカー起動
  M5.Axp.SetSpkEnable(true);

  // 会話削除
  networkManager.initConversation();

  // 初期状態に設定（初期化処理は最初のループで実行される）
  currentState = STATE_IDLE;
  nextState = STATE_IDLE;
  stateChanged = true;

  Serial.println("=== Bot-tan initialized successfully ===");
}

void loop() {
  M5.update();
  
  // 状態変更があった場合の初期化処理
  if (stateChanged) {
    currentState = nextState;
    stateChanged = false;
    
    switch (currentState) {
      case STATE_IDLE:
        initIdleState();
        break;
      case STATE_TOUCH_RECORDING:
        initTouchRecordingState();
        break;
      case STATE_VOICE_RECORDING:
        initVoiceRecordingState();
        break;
      case STATE_WAITING_RESPONSE:
        initWaitingResponseState();
        break;
      case STATE_PLAYING_RESPONSE:
        initPlayingResponseState();
        break;
    }
  }
  
  // タッチ状態チェック
  bool touchPressed = M5.Touch.ispressed();
  
  // 状態に応じた処理
  switch (currentState) {
    case STATE_IDLE:
      handleIdleState(touchPressed);
      break;
      
    case STATE_TOUCH_RECORDING:
      handleTouchRecordingState(touchPressed);
      break;
      
    case STATE_VOICE_RECORDING:
      handleVoiceRecordingState(touchPressed);
      break;
      
    case STATE_WAITING_RESPONSE:
      handleWaitingResponseState();
      break;
      
    case STATE_PLAYING_RESPONSE:
      handlePlayingResponseState();
      break;
  }
  
  delay(10);
}
