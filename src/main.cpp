#include <M5Core2.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "loadenv.hpp"
#include "prompts.h"

// フォント設定
TFT_eSPI tft = M5.Lcd;
#include "efontESP32.h"

// Gemini API
const char* apiKey = "AIzaSyAH5KmrBD57mRvebeLynmWZc6SOFjDKZjk";

void setup() {
  M5.begin();

  // フォント準備
  M5.Lcd.setTextSize(1);

  // 環境変数読み込み
  SPIFFS.begin(true);
  auto env = loadEnv("/.env");
  std::string ssid = env["WIFI_SSID"];
  std::string password = env["WIFI_PASSWORD"];
  String api_key = String(env["GEMINI_API_KEY"].c_str());

  // WiFi接続
  M5.Lcd.print("Connecting to WiFi");
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }
  M5.Lcd.println("\nConnected!");

  // HTTPClient準備
  HTTPClient http;
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-goog-api-key", api_key);
  http.setTimeout(30000); // -11エラーを抑制

  // JSONドキュメントの作成
  DynamicJsonDocument doc(8192);
  JsonObject contents = doc.createNestedArray("contents").createNestedObject();
  contents["parts"][0]["text"] = "こんにちは。自己紹介して。";
  JsonObject systemInst = doc.createNestedObject("system_instruction");
  systemInst["parts"][0]["text"] = String(SYSTEM_INSTRUCTION); // SYSTEM_INSTRUCTIONを使用
  String payload;
  serializeJson(doc, payload);

  // HTTP送信
  int httpResponseCode = http.POST(payload);
  
  // レスポンス処理
  if (httpResponseCode > 0) {
    String response = http.getString();
    
    M5.Lcd.clear();
    M5.Lcd.setCursor(0, 0);
    
    // JSONをパース（大きなバッファを使用）
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
      M5.Lcd.println("JSON Parse Error:");
      M5.Lcd.println(error.c_str());
      Serial.println("Response:");
      Serial.println(response);
      return;
    }
    
    // レスポンスからテキストを抽出
    if (doc["candidates"].size() > 0) {
      const char* replyText = doc["candidates"][0]["content"]["parts"][0]["text"];
      M5.Lcd.println("Bot-tan says:");
      printEfont(const_cast<char*>(replyText));
    } else {
      M5.Lcd.println("No response received");
      Serial.println(response);
    }
    
  } else {
    M5.Lcd.println("HTTP Error:");
    M5.Lcd.println(httpResponseCode);
  }
  
  http.end();
}

void loop() {
  // 特に繰り返し処理はしない
}
