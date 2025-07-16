#include <M5Core2.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "prompts.h"
#include <util.h>

const int MAX_HISTORY_COUNT = 20;
DynamicJsonDocument conversation(16384);

// 会話初期化
void initConversation() {
  conversation.clear();

  // contents 配列作成
  conversation.createNestedArray("contents");

  // system_instruction オブジェクト追加
  JsonObject system = conversation.createNestedObject("system_instruction");
  JsonArray systemParts = system.createNestedArray("parts");
  systemParts.createNestedObject()["text"] = SYSTEM_INSTRUCTION;
}

// 古い履歴を削除する関数
void trimConversationHistory() {
  JsonArray contents = conversation["contents"].as<JsonArray>();

  // contents.size() > MAX_HISTORY_COUNT の間、古い2件（user+model）を削除
  while (contents.size() > MAX_HISTORY_COUNT) {
    // 必ず2件削除（user+model）して1往復分減らす
    contents.remove(0);
    if (contents.size() > 0 && contents[0]["role"] == "model") {
      contents.remove(0);  // 次がmodelならセットで削除
    }
  }

  // 念のため：先頭がmodelなら削除して不正を回避（本来ありえないが安全のため）
  if (contents.size() > 0 && contents[0]["role"] == "model") {
    contents.remove(0);
  }
}

// Geminiへ送信して返答取得
String askGemini(String userInput, String gemini_api_key) {
  WiFiClientSecure client;
  client.setInsecure();
  
  if (!client.connect("generativelanguage.googleapis.com", 443)) {
    return "接続失敗";
  }
  
  // ユーザ発言を履歴に追加
  JsonObject userMsg = conversation["contents"].createNestedObject();
  userMsg["role"] = "user";
  JsonArray parts = userMsg.createNestedArray("parts");
  JsonObject textPart = parts.createNestedObject();
  textPart["text"] = "ユーザーの以下の発言に対し、返答50文字以内で会話を続けてください:\n\n" + userInput + "?";

  // 履歴を削除
  trimConversationHistory();

  // JSONペイロード作成
  String payload;
  serializeJson(conversation, payload);
  
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
  String body = readHttpBody(client);
  Serial.println("=== gemini ===");
  Serial.println("JSON Body: " + body);
  
  // JSON解析
  DynamicJsonDocument result(8192);
  DeserializationError err = deserializeJson(result, body);
  if (err) return "";
  
  String reply = result["candidates"][0]["content"]["parts"][0]["text"].as<String>();

  // 返答を履歴に追加
  JsonObject modelMsg = conversation["contents"].createNestedObject();
  modelMsg["role"] = "model";
  JsonArray modelParts = modelMsg.createNestedArray("parts");
  modelParts.createNestedObject()["text"] = reply;

  return reply;
}
