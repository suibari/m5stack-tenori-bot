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
    size_t currentChunkSize = min(chunkSize, base64audio.length() - i);
    String chunk = base64audio.substring(i, i + currentChunkSize);
    client.print(chunk);
  }
  client.print("\"}");
  Serial.println("Request end");

  skipResponseHeaders(client);
  std::vector<uint8_t> pcm = readBody(client);
  
  if (onReadyToPlay) onReadyToPlay();
  playAudio(pcm, 24000);

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
