#include <M5Core2.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <base64.h>
#include <util.h>
#include <loadenv.hpp>

#define PROJECT_ID "m5stack-gemini"
#define PHRASE_SET_ID "phrase-set-0" // 別スクリプトで作成したフレーズセットID

// 自前のトークン取得APIにアクセス
String getToken() {
  auto env = loadEnv("/.env");
  std::string token_host = env["SST_GETTOKEN_HOST"];

  WiFiClientSecure client;
  client.setInsecure();
  if (!client.connect(token_host.c_str(), 443)) return "";

  client.println("GET /get_token HTTP/1.1");
  client.println("Host: " + String(token_host.c_str()));
  client.println("Connection: close");
  client.println();

  String body = readHttpBody(client);
  
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, body);
  String token = doc["access_token"];
  
  return token;
}

// Google STTへ送信してテキスト取得
String transcribeSpeech(String base64audio, String google_api_key) {
  WiFiClientSecure client;
  client.setInsecure(); // 証明書検証をスキップ（本番では適切な証明書設定を推奨）
  
  // Google Speech API接続
  if (!client.connect("speech.googleapis.com", 443)) {
    Serial.println("STT接続失敗");
    return "";
  }
  
  // JSONペイロードの前半・後半部分を準備
  String phraseSetPath = "projects/" + String(PROJECT_ID) + "/locations/global/phraseSets/" + String(PHRASE_SET_ID);
  String jsonPrefix = 
    "{\"config\":{"
      "\"encoding\":\"LINEAR16\","
      "\"sampleRateHertz\":16000,"
      "\"languageCode\":\"ja-JP\","
      "\"adaptation\":{"
        "\"phrase_set_references\":[\"" + phraseSetPath + "\"]"
      "}"
    "},"
    "\"audio\":{\"content\":\"";
  String jsonSuffix = "\"}}";
  
  // Content-Lengthを計算
  size_t totalLen = jsonPrefix.length() + base64audio.length() + jsonSuffix.length();
  
  // OAuthトークン取得
  String token = getToken();

  // HTTPヘッダー送信
  String headers = "POST /v1/speech:recognize?key=" + google_api_key + " HTTP/1.1\r\n";
  headers += "Host: speech.googleapis.com\r\n";
  if (!token.isEmpty()) {
    headers += "Authorization: Bearer " + token + "\r\n";
  }
  headers += "Content-Type: application/json\r\n";
  headers += "Content-Length: " + String(totalLen) + "\r\n";
  headers += "Connection: close\r\n\r\n";
  
  if (!sendChunk(client, headers)) {
    client.stop();
    Serial.println("ヘッダー送信失敗");
    return "";
  }
  
  // JSON前半部分送信
  if (!sendChunk(client, jsonPrefix)) {
    client.stop();
    Serial.println("JSON前半送信失敗");
    return "";
  }
  
  // Base64データをチャンク分けして送信
  const size_t chunkSize = 1024;
  for (size_t i = 0; i < base64audio.length(); i += chunkSize) {
    size_t currentChunkSize = min(chunkSize, base64audio.length() - i);
    String chunk = base64audio.substring(i, i + currentChunkSize);
    
    if (!sendChunk(client, chunk)) {
      client.stop();
      Serial.println("Base64データ送信失敗");
      return "";
    }
    
    // メモリ使用量を抑えるため、チャンク送信後に少し待機
    delay(10);
  }
  
  // JSON後半部分送信
  if (!sendChunk(client, jsonSuffix)) {
    client.stop();
    Serial.println("JSON後半送信失敗");
    return "";
  }
  
  // レスポンス受信
  String body = readHttpBody(client);

  Serial.println("=== stt ===");
  Serial.println("JSON Body: " + body);

  // JSON解析
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.println("JSON解析失敗");
    return "";
  };
  
  if (!doc["results"].isNull() && doc["results"].size() > 0) {
    return doc["results"][0]["alternatives"][0]["transcript"].as<String>();
  }
  
  return "";
}
