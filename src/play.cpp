#include <M5Core2.h>
#include <base64.h>
#include "audio_config.hpp"
#include <WiFiClient.h>

#define I2S_PLAYBACK_PORT I2S_NUM_0

void setupI2SPlayback(int sampleRate = I2S_SAMPLE_RATE) {
  i2s_driver_uninstall(I2S_PLAYBACK_PORT);  // 安全のためアンインストール

  i2s_config_t i2s_config_tx = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = I2S_BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = CONFIG_I2S_BCK_PIN,
    .ws_io_num = CONFIG_I2S_LRCK_PIN,
    .data_out_num = CONFIG_I2S_DATA_PIN,
    .data_in_num = -1
  };

  i2s_driver_install(I2S_PLAYBACK_PORT, &i2s_config_tx, 0, NULL);
  i2s_set_pin(I2S_PLAYBACK_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PLAYBACK_PORT);
}

void playAudio(const std::vector<uint8_t>& audioData, int sampleRate = I2S_SAMPLE_RATE) {
  M5.Axp.SetSpkEnable(true);
  setupI2SPlayback(sampleRate);  // 再生モードへ切り替え

  size_t bytes_written = 0;
  i2s_write(I2S_PLAYBACK_PORT, audioData.data(), audioData.size(), &bytes_written, portMAX_DELAY);

  i2s_driver_uninstall(I2S_PLAYBACK_PORT);  // 再生完了後にクリーンアップ
}

void playAudioStreamChunk(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) return;

  size_t bytes_written = 0;
  esp_err_t err = i2s_write(I2S_PLAYBACK_PORT, (const char*)data, len, &bytes_written, portMAX_DELAY);

  if (err != ESP_OK) {
    Serial.printf("i2s_write failed: %d\n", err);
  }
}

void playChunkedBody(WiFiClient& client) {
  M5.Axp.SetSpkEnable(true);
  setupI2SPlayback(24000);

  while (client.connected()) {
    // 1. チャンクサイズ行を読み取る（改行まで）
    String chunkSizeLine = client.readStringUntil('\n');
    chunkSizeLine.trim();  // \r除去や空白除去

    if (chunkSizeLine.length() == 0) {
      // 空行はスキップして次へ
      continue;
    }

    // 2. 16進数のチャンクサイズをパース
    size_t chunkSize = strtoul(chunkSizeLine.c_str(), nullptr, 16);

    if (chunkSize == 0) {
      // サイズ0は終了チャンク
      break;
    }

    size_t remaining = chunkSize;
    const size_t bufSize = 1024;
    uint8_t buffer[bufSize];

    // 3. chunkSizeバイト分読み込んで再生
    while (remaining > 0) {
      size_t toRead = (remaining > bufSize) ? bufSize : remaining;

      // WiFiClient::readBytesは待機もするので安心
      size_t readLen = client.readBytes(buffer, toRead);
      if (readLen == 0) {
        // 何も読めなかったら切断の可能性あり
        break;
      }

      playAudioStreamChunk(buffer, readLen);

      remaining -= readLen;
    }

    // 4. チャンクデータ後のCRLFを読み捨てる（2バイト）
    if (client.connected()) {
      client.read(); // \r
      client.read(); // \n
    }
  }

  M5.Axp.SetSpkEnable(false);
  i2s_driver_uninstall(I2S_PLAYBACK_PORT);
}
