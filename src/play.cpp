#include <M5Core2.h>
#include <base64.h>
#include "audio_config.hpp"

#define I2S_PLAYBACK_PORT I2S_NUM_0

void setupI2SPlayback(int sampleRate = I2S_SAMPLE_RATE) {
  i2s_driver_uninstall(I2S_PLAYBACK_PORT);  // 安全のためアンインストール

  i2s_config_t i2s_config_tx = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
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
