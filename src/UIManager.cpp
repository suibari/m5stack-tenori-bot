#include "UIManager.h"
#include <SPIFFS.h>

UIManager::UIManager() : sprite(&M5.Lcd) {
  currentScreen = SCREEN_INIT;
}

bool UIManager::init() {
  // LCD初期化
  M5.Lcd.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  
  // スプライトを作成
  sprite.createSprite(M5.Lcd.width(), M5.Lcd.height());
  
  // タッチパネル初期化
  M5.Touch.begin();
  
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
  if (currentScreen == newScreen) return;

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
  Serial.printf("Attempting to load: %s\n", imagePath);
  
  // SPIFFS上に画像ファイルが存在するかチェック
  if (!SPIFFS.exists(imagePath)) {
    Serial.printf("❌ Image file not found: %s\n", imagePath);
    return false;
  }
  
  Serial.printf("✅ File exists, loading: %s\n", imagePath);
  
  // スプライトに画像を描画
  sprite.fillSprite(BLACK);
  sprite.drawJpgFile(SPIFFS, imagePath);
  sprite.pushSprite(0, 0);
  
  Serial.printf("🎨 Image display complete: %s\n", imagePath);
  
  return true;
}
