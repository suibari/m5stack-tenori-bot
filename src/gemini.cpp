#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "prompts.h"

// Geminiへ送信して返答取得
String askGemini(String text, String gemini_api_key) {
  HTTPClient http;
  http.begin("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-goog-api-key", gemini_api_key);
  http.setTimeout(30000);

  DynamicJsonDocument doc(8192);
  JsonObject content = doc.createNestedArray("contents").createNestedObject();
  content["parts"][0]["text"] = text;

  JsonObject system = doc.createNestedObject("system_instruction");
  system["parts"][0]["text"] = SYSTEM_INSTRUCTION;

  String payload;
  serializeJson(doc, payload);
  int res = http.POST(payload);
  String response = http.getString();
  http.end();

  if (res <= 0) return "Gemini送信失敗";

  DynamicJsonDocument result(8192);
  DeserializationError err = deserializeJson(result, response);
  if (err) return "Gemini応答解析失敗";

  return result["candidates"][0]["content"]["parts"][0]["text"].as<String>();
}
