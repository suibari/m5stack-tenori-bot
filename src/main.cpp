#include <M5Core2.h>
#include "efont.h"
#include "efontESP32.h"
#include "efontEnableJa.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "loadenv.hpp"
#include "prompts.h"

// Gemini API
const char* apiKey = "AIzaSyAH5KmrBD57mRvebeLynmWZc6SOFjDKZjk";

void setup() {
  M5.begin();

  M5.Lcd.setTextSize(1);
  M5.Lcd.print("Connecting to WiFi");

  // 環境変数読み込み
  SPIFFS.begin(true);
  auto env = loadEnv("/.env");
  std::string ssid = env["WIFI_SSID"];
  std::string password = env["WIFI_PASSWORD"];
  String api_key = String(env["GEMINI_API_KEY"].c_str());

  Serial.println(ssid.c_str());
  Serial.println(password.c_str());
  Serial.println(api_key);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }
  M5.Lcd.println("\nConnected!");

  // HTTPClientを使用（chunked transferを自動処理）
  HTTPClient http;
  
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-goog-api-key", api_key);
  http.setTimeout(30000); // -11エラーを抑制

  // JSONドキュメントの作成
  DynamicJsonDocument doc(8192);
  JsonObject contents = doc.createNestedArray("contents").createNestedObject();
  contents["parts"][0]["text"] = "Explain how AI works in a few words";
  JsonObject systemInst = doc.createNestedObject("system_instruction");
  systemInst["parts"][0]["text"] = String(SYSTEM_INSTRUCTION); // SYSTEM_INSTRUCTIONを使用
  String payload;
  serializeJson(doc, payload);

  int httpResponseCode = http.POST(payload);
  
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
