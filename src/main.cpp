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
unsigned long recordingStartTime = 0;
bool isTouchPressed = false;
bool isVoiceDetected = false;

void startTouchRecording() {
  Serial.println("Starting touch recording");
  currentState = STATE_TOUCH_RECORDING;
  recordingStartTime = millis();
  isTouchPressed = true;
  isVoiceDetected = false;
  
  voiceDetector.stopContinuousRecording();
  audioManager.startRecording();
  uiManager.showHearingScreen();
}

void startVoiceRecording() {
  Serial.println("Starting voice recording");
  currentState = STATE_VOICE_RECORDING;
  recordingStartTime = millis();
  isVoiceDetected = true;
  
  audioManager.startRecording();
  // 常時録音中なので画面は変更しない
}

void stopRecordingAndSend(const char* endpoint) {
  Serial.printf("Stopping recording and sending to %s\n", endpoint);
  
  // 録音停止
  size_t dataSize = audioManager.stopRecording();
  
  if (dataSize > 0) {
    // サーバに送信
    currentState = STATE_WAITING_RESPONSE;
    uiManager.showThinkingScreen();
    
    uint8_t* audioData = audioManager.getRecordedData();
    Serial.printf("rawAudioDataSize: %d\n", dataSize);

    // 再生: ユーザタッチ時=Google送信時のみ再生
    if (String(endpoint) == "stsGoogle") {
      audioManager.startPlayback(audioData, dataSize);
    }

    networkManager.sendAudioData(audioData, dataSize, endpoint);
  } else {
    // 録音データが無い場合はアイドル状態に戻る
    currentState = STATE_IDLE;
    uiManager.showIdleScreen();
    // voiceDetector.startContinuousRecording();
  }
}

void startPlayingResponse() {
  Serial.println("Starting response playback");
  currentState = STATE_PLAYING_RESPONSE;
  uiManager.showSpeakingScreen();
  
  size_t responseSize = networkManager.getResponseSize();
  uint8_t* responseData = networkManager.getResponseData();
  
  // サーバ応答は24000Hz
  audioManager.startPlayback(responseData, responseSize, 24000);
}

void handleIdleState(bool touchPressed) {
  if (touchPressed && !isTouchPressed) {
    // タッチ開始 - タッチ録音モードに移行
    startTouchRecording();
  } else if (!touchPressed) {
    // 常時録音での音声検出
    if (voiceDetector.isVoiceDetected()) {
      if (!isVoiceDetected) {
        // 音声検出開始
        // startVoiceRecording();
      }
    } else {
      isVoiceDetected = false;
    }
  }
  
  isTouchPressed = touchPressed;
}

void handleTouchRecordingState(bool touchPressed) {
  if (!touchPressed) {
    // タッチ終了 - 録音終了してサーバに送信
    stopRecordingAndSend("stsGoogle");
  } else if (millis() - recordingStartTime > MAX_RECORDING_TIME) {
    // 10秒経過 - 自動的に録音終了
    stopRecordingAndSend("stsGoogle");
  }
}

void handleVoiceRecordingState(bool touchPressed) {
  if (touchPressed && !isTouchPressed) {
    // タッチされた場合は優先してタッチ録音モードに移行
    audioManager.stopRecording();
    startTouchRecording();
  } else if (!voiceDetector.isVoiceDetected() || 
             millis() - recordingStartTime > MAX_RECORDING_TIME) {
    // 音声終了または10秒経過 - 録音終了してサーバに送信
    stopRecordingAndSend("stsWhisper");
  }
  
  isTouchPressed = touchPressed;
}

void handleWaitingResponseState() {
  // NetworkManagerがレスポンスを処理中
  if (networkManager.isResponseReady()) {
    // レスポンス受信完了 - 再生開始
    startPlayingResponse();
  } else {
    // エラー時
    currentState = STATE_IDLE;
    uiManager.showIdleScreen();
    // voiceDetector.startContinuousRecording();
  }
}

void handlePlayingResponseState() {
  if (!audioManager.isPlaying()) {
    // 再生終了 - アイドル状態に戻る
    currentState = STATE_IDLE;
    uiManager.showIdleScreen();
    // voiceDetector.startContinuousRecording();
  }
}

// ---------------------
// main: setup & loop
// ---------------------
void setup() {
  M5.begin();
  Serial.begin(115200);
  
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
  
  // 初期画面表示
  uiManager.showIdleScreen();
  
  // スピーカー起動
  M5.Axp.SetSpkEnable(true);

  // 会話削除
  networkManager.initConversation();

  Serial.println("Bot-tan initialized successfully");
}

void loop() {
  M5.update();
  
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
