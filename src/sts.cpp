#include <M5Core2.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <play.hpp>

void speechToSpeech(String base64audio, std::function<void()> onReadyToPlay = nullptr) {
  HTTPClient http;
  http.begin("http://192.168.1.200:5050/sts");
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(4096);
  doc["audio"] = base64audio;
  String body;
  serializeJson(doc, body);

  int res = http.POST(body);
  if (res == 200) {
    WiFiClient* stream = http.getStreamPtr();
    std::vector<uint8_t> pcm;
    uint8_t buf[256];
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
  }
  http.end();
}
