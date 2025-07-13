#include <M5Core2.h>
#include <SPI.h>
#include <WiFi.h>
#include "loadenv.hpp"
#include "record.hpp"
#include "stt.hpp"
#include "gemini.hpp"

// フォント設定
TFT_eSPI tft = M5.Lcd;
#include "efontESP32.h"

// 初期化
void setup() {
  M5.begin();
  M5.Lcd.setTextSize(1);
  M5.Lcd.clear();

  // SPIFFSと.env読み込み
  SPIFFS.begin(true);
  auto env = loadEnv("/.env");
  std::string ssid = env["WIFI_SSID"];
  std::string password = env["WIFI_PASSWORD"];
  String gemini_api_key = String(env["GEMINI_API_KEY"].c_str());
  String google_api_key = String(env["GOOGLE_API_KEY"].c_str());

  WiFi.begin(ssid.c_str(), password.c_str());
  M5.Lcd.print("WiFi Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    M5.Lcd.print(".");
  }
  M5.Lcd.println("Connected to WiFi!");

  delay(1000);

  M5.Lcd.clear();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Touch to start recording...");
}

// メインループ
void loop() {
  M5.update();
  if (M5.Touch.ispressed()) {
    String audioBase64 = recordAudioAndEncodeBase64();
    if (audioBase64 == "") return;

    auto env = loadEnv("/.env");
    String stt_api_key = String(env["SST_API_KEY"].c_str());
    String gemini_api_key = String(env["GEMINI_API_KEY"].c_str());

    String recognized = transcribeSpeech(audioBase64, stt_api_key);
    if (recognized == "") {
      M5.Lcd.clear();
      M5.Lcd.println("Failed to recognize speech");
      return;
    }

    M5.Lcd.clear();
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("You:");
    printEfont(const_cast<char*>(recognized.c_str()));

    // AI思考時間
    M5.Lcd.clear();
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print("Bot-tan Thinking...");

    String reply = askGemini(recognized, gemini_api_key);

    // AI結果表示
    M5.Lcd.clear();
    M5.Lcd.setCursor(0, 0);
    printEfont(const_cast<char*>(reply.c_str()), 0, 0);
  }

  delay(200);
}
