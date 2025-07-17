#include <M5Core2.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <play.hpp>
#include <util.h>

#define I2S_PLAYBACK_PORT I2S_NUM_0

// STS処理
void speechToSpeech(String base64audio, std::function<void()> onReadyToPlay = nullptr) {
  WiFiClient client;
  const char* host = "192.168.1.200";
  const uint16_t port = 5050;

  if (!client.connect(host, port)) {
    Serial.println("Connection failed");
    return;
  }

  int contentLength = base64audio.length() + strlen("{\"audio\":\"\"}");
  String reqStart = String("POST /sts HTTP/1.1\r\n") +
                    "Host: " + host + "\r\n" +
                    "Content-Type: application/json\r\n" +
                    "Connection: close\r\n" +
                    "Content-Length: " + String(contentLength) + "\r\n\r\n" +
                    "{\"audio\":\"";

  Serial.println("Request start");
  client.print(reqStart);

  // Base64をチャンク送信
  const size_t chunkSize = 1024;
  for (size_t i = 0; i < base64audio.length(); i += chunkSize) {
    client.print(base64audio.substring(i, i + chunkSize));
  }
  client.print("\"}");
  Serial.println("Request end");

  skipResponseHeaders(client);

  // 再生初期化
  M5.Axp.SetSpkEnable(true);
  setupI2SPlayback(24000);

  if (onReadyToPlay) onReadyToPlay();

  // ストリーミング再生
  const size_t bufSize = 512;
  uint8_t buffer[bufSize];
  while (client.connected()) {
    if (client.available()) {
      size_t len = client.readBytes(buffer, bufSize);
      if (len > 0) {
        playAudioStreamChunk(buffer, len);
      }
    } else {
      delay(1);
    }
  }

  M5.Axp.SetSpkEnable(false);
  i2s_driver_uninstall(I2S_PLAYBACK_PORT);
  client.stop();
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
