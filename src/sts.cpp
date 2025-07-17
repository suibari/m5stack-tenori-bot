#include <M5Core2.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <play.hpp>

#define I2S_PLAYBACK_PORT I2S_NUM_0

// STS処理(ストリーミング受信)
void speechToSpeech(String base64audio, std::function<void()> onReadyToPlay = nullptr) {
  HTTPClient http;
  http.begin("http://192.168.1.200:5050/sts");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000);

  DynamicJsonDocument doc(base64audio.length() + 1024); // ここが少ないとPCMデータが切れてNoneになる
  doc["audio"] = base64audio;
  String body;
  serializeJson(doc, body);

  int res = http.POST(body);
  if (res == 200) {
    Serial.println("Received STS response, processing...");
    WiFiClient* stream = http.getStreamPtr();
    std::vector<uint8_t> pcm;
    uint8_t buf[1024];
    while (stream->connected()) {
      int len = stream->read(buf, sizeof(buf));
      if (len > 0) {
        pcm.insert(pcm.end(), buf, buf + len);
      } else {
        delay(1);
      }
    }

    if (onReadyToPlay) {
      onReadyToPlay();
    }

    playAudio(pcm, 24000);
  } else {
    Serial.println("Error during POST");
    Serial.println(String("Response Code: ") + res);
  }
  http.end();
}

void initConversation() {
  HTTPClient http;
  http.begin("http://192.168.1.200:5050/initConversation");
  int response = http.POST("");
  
  if (response == 200) {
    Serial.println("Conversation reset successfully.");
  } else {
    Serial.printf("Failed to reset conversation. HTTP code: %d\n", response);
  }

  http.end();
}
