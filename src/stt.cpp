#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// Google STTへ送信してテキスト取得
String transcribeSpeech(String base64audio, String google_api_key) {
  HTTPClient http;
  http.begin("https://speech.googleapis.com/v1/speech:recognize?key=" + google_api_key);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument sttDoc(8192);
  sttDoc["config"]["encoding"] = "LINEAR16";
  sttDoc["config"]["sampleRateHertz"] = 16000;
  sttDoc["config"]["languageCode"] = "ja-JP";
  sttDoc["audio"]["content"] = base64audio;

  String json;
  serializeJson(sttDoc, json);
  int httpCode = http.POST(json);
  String response = http.getString();
  http.end();

  if (httpCode != 200) {
    M5.Lcd.println("STT Error: " + String(httpCode));
    Serial.println("HTTP Error: " + httpCode);
    return "";
  }
  
  Serial.println(response.substring(0, 200));

  DynamicJsonDocument resDoc(8192);
  DeserializationError err = deserializeJson(resDoc, response);
  if (err) return "";

  return resDoc["results"][0]["alternatives"][0]["transcript"].as<String>();
}
