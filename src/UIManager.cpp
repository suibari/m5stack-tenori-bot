#include "UIManager.h"
#include <SPIFFS.h>

UIManager::UIManager() {
  currentScreen = SCREEN_IDLE;
}

bool UIManager::init() {
  // LCD初期化
  M5.Lcd.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  
  // タッチパネル初期化
  M5.Touch.begin();

  // 初期画面表示
  showIdleScreen();
  
  return true;
}

void UIManager::showIdleScreen() {
  changeScreen(SCREEN_IDLE, "/smile_close.jpg");
}

void UIManager::showHearingScreen() {
  changeScreen(SCREEN_HEARING, "/hearing.jpg");
}

void UIManager::showThinkingScreen() {
  changeScreen(SCREEN_THINKING, "/thinking.jpg");
}

void UIManager::showSpeakingScreen() {
  changeScreen(SCREEN_SPEAKING, "/smile_open.jpg");
}

void UIManager::changeScreen(ScreenState newScreen, const char* imagePath) {
  currentScreen = newScreen;
  
  // 画像を読み込んで表示
  if (!loadImageIfExists(imagePath)) {
    // 画像が読み込めない場合はテキストで状態を表示
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 100);
    
    switch (newScreen) {
      case SCREEN_IDLE:
        M5.Lcd.println("Ready - Touch to speak");
        break;
      case SCREEN_HEARING:
        M5.Lcd.println("Listening...");
        break;
      case SCREEN_THINKING:
        M5.Lcd.println("Processing...");
        break;
      case SCREEN_SPEAKING:
        M5.Lcd.println("Speaking...");
        break;
    }
  }
  
  Serial.printf("Screen changed to: %s\n", imagePath);
}

bool UIManager::loadImageIfExists(const char* imagePath) {
  // SPIFFS上に画像ファイルが存在するかチェック
  if (!SPIFFS.exists(imagePath)) {
    Serial.printf("Image file not found: %s\n", imagePath);
    return false;
  }
  
  // JPEGファイルを読み込んで表示
  M5.Lcd.drawJpgFile(SPIFFS, imagePath);
  
  return true;
}
