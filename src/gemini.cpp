#include <M5Core2.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "prompts.h"
#include <util.h>

// Geminiへ送信して返答取得
String askGemini(String text, String gemini_api_key) {
  WiFiClientSecure client;
  client.setInsecure();
  
  if (!client.connect("generativelanguage.googleapis.com", 443)) {
    return "接続失敗";
  }
  
  // JSONペイロードを構築
  DynamicJsonDocument doc(4096);
  JsonObject content = doc.createNestedArray("contents").createNestedObject();
  content["parts"][0]["text"] = text;
  
  JsonObject system = doc.createNestedObject("system_instruction");
  system["parts"][0]["text"] = SYSTEM_INSTRUCTION;
  
  String payload;
  serializeJson(doc, payload);
  
  // HTTPヘッダー送信
  String headers = "POST /v1beta/models/gemini-2.5-flash:generateContent HTTP/1.1\r\n";
  headers += "Host: generativelanguage.googleapis.com\r\n";
  headers += "Content-Type: application/json\r\n";
  headers += "x-goog-api-key: " + gemini_api_key + "\r\n";
  headers += "Content-Length: " + String(payload.length()) + "\r\n";
  headers += "Connection: close\r\n\r\n";
  
  if (!sendChunk(client, headers + payload)) {
    client.stop();
    return "送信失敗";
  }
  
  // レスポンス受信
  String response = "";
  unsigned long timeout = millis() + 30000;
  
  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      response += client.readString();
      break;
    }
    delay(100);
  }
  
  client.stop();
  Serial.println("Gemini Response:");
  Serial.println(response);

  // HTTPヘッダーをスキップしてJSONボディを取得
  int bodyIndex = response.indexOf("\r\n\r\n");
  if (bodyIndex == -1) {
    Serial.println("Invalid HTTP response (no header-body separator)");
    return "";
  }
  
  String bodyRaw = response.substring(bodyIndex + 4);
  String jsonBody = removeChunkHeaderFooter(bodyRaw);
  Serial.println("JSON Body: " + jsonBody);
  
  // JSON解析
  DynamicJsonDocument result(8192);
  DeserializationError err = deserializeJson(result, jsonBody);
  if (err) return "";
  
  return result["candidates"][0]["content"]["parts"][0]["text"].as<String>();
}
