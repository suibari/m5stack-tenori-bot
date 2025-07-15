#include <WiFiClientSecure.h>
#include <vector>
#include <M5Core2.h>
#include "audio_config.hpp"
#include <play.hpp>
#include <util.h>

// WAVヘッダーを解析し、PCMバッファを抽出
bool extractPCMFromWav(const std::vector<uint8_t>& wavData, std::vector<uint8_t>& pcmData) {
  if (wavData.size() < 44) return false;

  // Check "RIFF" header
  if (memcmp(&wavData[0], "RIFF", 4) != 0 || memcmp(&wavData[8], "WAVE", 4) != 0) {
    Serial.println("Invalid WAV header");
    return false;
  }

  size_t idx = 12;
  while (idx + 8 <= wavData.size()) {
    char chunkId[5] = {0};
    memcpy(chunkId, &wavData[idx], 4);
    uint32_t chunkSize = *(uint32_t*)&wavData[idx + 4];

    // 安全チェック
    if (idx + 8 + chunkSize > wavData.size()) {
      Serial.println("Chunk size exceeds file");
      return false;
    }

    if (strncmp(chunkId, "data", 4) == 0) {
      pcmData.assign(wavData.begin() + idx + 8, wavData.begin() + idx + 8 + chunkSize);
      return true;
    }

    // WAVのチャンクは偶数境界に揃える必要がある
    idx += 8 + ((chunkSize + 1) & ~1);
  }

  Serial.println("No data chunk found");
  return false;
}

// VOICEVOXにテキストを送信して再生
void playVoiceVox(String text, String api_key) {
  WiFiClientSecure client;
  client.setInsecure();

  String host = "deprecatedapis.tts.quest";
  String path = "/v2/voicevox/audio/?text=" + urlEncode(text) + "&key=" + api_key + "&speaker=8";

  Serial.println("Connecting to VOICEVOX...");
  if (!client.connect(host.c_str(), 443)) {
    Serial.println("Connection failed");
    return;
  }

  // HTTPリクエスト送信
  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  // ヘッダー読み飛ばし
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  // WAVデータ受信
  std::vector<uint8_t> rawWav;
  uint8_t buffer[256];

  while (client.connected() || client.available()) {
    int len = client.read(buffer, sizeof(buffer));
    if (len > 0) {
      rawWav.insert(rawWav.end(), buffer, buffer + len);
    } else {
      delay(1);
    }
  }
  client.stop();
  Serial.printf("Received %d bytes\n", rawWav.size());

  // RIFFヘッダーを探して切り出す
  std::vector<uint8_t> wavData;
  for (size_t i = 0; i + 4 < rawWav.size(); ++i) {
    if (memcmp(&rawWav[i], "RIFF", 4) == 0) {
      wavData.assign(rawWav.begin() + i, rawWav.end());
      break;
    }
  }

  if (wavData.empty()) {
    Serial.println("Failed to find RIFF header");
    return;
  }

  // WAV -> PCM変換
  std::vector<uint8_t> pcmData;
  if (!extractPCMFromWav(wavData, pcmData)) {
    Serial.println("Failed to extract PCM from WAV");
    return;
  }

  Serial.println("PCM size: " + String(pcmData.size()));
  playAudio(pcmData, 24000);
}
