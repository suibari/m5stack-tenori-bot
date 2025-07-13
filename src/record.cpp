#include <M5Core2.h>
#include <base64.h>

// 録音設定
#define I2S_SAMPLE_RATE     16000
#define I2S_SAMPLE_BITS     16
#define I2S_CHANNEL_NUM     I2S_CHANNEL_MONO
#define I2S_BUFFER_SIZE     1024
#define MAX_RECORDING_SIZE 32768

// I2Sマイクのセットアップ
void setupI2SMic() {
  i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = I2S_BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = 0,
    .ws_io_num = 32,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = 33
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// 音声を録音してBase64エンコード
String recordAudioAndEncodeBase64() {
  M5.Lcd.clear();
  M5.Lcd.print("Recording");

  setupI2SMic();

  std::vector<uint8_t> audioData;
  audioData.reserve(MAX_RECORDING_SIZE);
  uint8_t buffer[I2S_BUFFER_SIZE];

  while (M5.Touch.ispressed() && audioData.size() < MAX_RECORDING_SIZE) {
    M5.Lcd.print(".");

    size_t bytes_read;
    esp_err_t result = i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytes_read, 100);
    
    if (result == ESP_OK && bytes_read > 0) {
      // バッファのデータを動的配列に追加
      audioData.insert(audioData.end(), buffer, buffer + bytes_read);
    }
  }
  
  i2s_driver_uninstall(I2S_NUM_0);
  M5.Lcd.println("Stopped recording, processing...");

  // Base64エンコード
  String base64data = base64::encode(audioData.data(), audioData.size());

  Serial.println("Recorded audio size: " + String(audioData.size()));
  Serial.println("Base64 size: " + String(base64data.length()));
  
  return base64data;
}
