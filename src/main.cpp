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
#include <voicevox.hpp>

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

  WiFi.begin(ssid.c_str(), password.c_str());
  M5.Lcd.print("WiFi Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    M5.Lcd.print(".");
  }
  M5.Lcd.println("Connected to WiFi!");

  delay(1000);

  initConversation();
  M5.Lcd.println("initialized conversation!");

  M5.Lcd.clear();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Touch to start recording...");
}

// メインループ
void loop() {
  M5.update();

  // botたん笑顔
  M5.Lcd.drawJpgFile(SPIFFS, "/smile_close.jpg");

  if (M5.Touch.ispressed()) {
    // botたんヒアリング
    M5.Lcd.drawJpgFile(SPIFFS, "/hearing.jpg");

    String audioBase64 = recordAudioAndEncodeBase64();
    if (audioBase64 == "") return;

    // tic
    unsigned long start = millis();

    // botたん思考中
    M5.Lcd.drawJpgFile(SPIFFS, "/thinking.jpg");

    auto env = loadEnv("/.env");
    String stt_api_key = String(env["SST_API_KEY"].c_str());
    String gemini_api_key = String(env["GEMINI_API_KEY"].c_str());

    String recognized = transcribeSpeech(audioBase64, stt_api_key);
    if (recognized == "") {
      M5.Lcd.clear();
      M5.Lcd.println("Failed to recognize speech");
      delay(2000);
      return;
    }

    String reply = askGemini(recognized, gemini_api_key);

    // VOICEVOX読み上げ
    String voicevox_api_key = String(env["VOICEVOX_API_KEY"].c_str());
    playVoiceVox(
      reply,
      voicevox_api_key,
      [start]() {
        M5.Lcd.drawJpgFile(SPIFFS, "/smile_open.jpg"); // コールバックで渡し、発声直前に画面変える

        // toc
        unsigned long end = millis();
        Serial.printf("Request took %d ms\n", end - start);
      }
    );
  }

  delay(200);
}
