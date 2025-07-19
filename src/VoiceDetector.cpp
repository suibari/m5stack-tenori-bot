#include "VoiceDetector.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

VoiceDetector::VoiceDetector() {
  isContinuousRecording = false;
  voiceDetected = false;
  voiceStartTime = 0;
  lastVoiceTime = 0;
  averageVolume = 0.0;
}

bool VoiceDetector::init() {
  // I2S設定の初期化
  i2sConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  
  pinConfig = {
    .bck_io_num = 19,
    .ws_io_num = 0,
    .data_out_num = 22,
    .data_in_num = 23
  };
  
  return true;
}

void VoiceDetector::startContinuousRecording() {
  if (isContinuousRecording) return;
  
  configureI2S();
  isContinuousRecording = true;
  voiceDetected = false;
  voiceStartTime = 0;
  lastVoiceTime = 0;
  
  // 音声検出タスクを作成
  xTaskCreate(continuousRecordingTaskWrapper, "VoiceDetectionTask", 4096, this, 4, NULL);
  
  Serial.println("Continuous voice detection started");
}

void VoiceDetector::stopContinuousRecording() {
  if (!isContinuousRecording) return;
  
  isContinuousRecording = false;
  voiceDetected = false;
  
  // I2Sドライバを停止
  i2s_driver_uninstall(I2S_NUM_0);
  
  Serial.println("Continuous voice detection stopped");
}

bool VoiceDetector::isVoiceDetected() {
  return voiceDetected;
}

void VoiceDetector::configureI2S() {
  i2s_driver_uninstall(I2S_NUM_0);
  
  esp_err_t result = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
  if (result != ESP_OK) {
    Serial.printf("I2S driver install failed: %d\n", result);
    return;
  }
  
  result = i2s_set_pin(I2S_NUM_0, &pinConfig);
  if (result != ESP_OK) {
    Serial.printf("I2S set pin failed: %d\n", result);
    return;
  }
  
  i2s_zero_dma_buffer(I2S_NUM_0);
}

float VoiceDetector::calculateVolume(int16_t* samples, size_t sampleCount) {
  if (sampleCount == 0) return 0.0;
  
  float sum = 0.0;
  for (size_t i = 0; i < sampleCount; i++) {
    sum += abs(samples[i]);
  }
  
  return sum / sampleCount;
}

void VoiceDetector::continuousRecordingTask() {
  int16_t buffer[BUFFER_SIZE];
  size_t bytesRead = 0;
  
  while (isContinuousRecording) {
    esp_err_t result = i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
    
    if (result == ESP_OK && bytesRead > 0) {
      size_t sampleCount = bytesRead / sizeof(int16_t);
      float currentVolume = calculateVolume(buffer, sampleCount);
      
      // 移動平均でノイズを軽減
      averageVolume = (averageVolume * 0.9) + (currentVolume * 0.1);
      
      unsigned long currentTime = millis();
      
      if (currentVolume > VOICE_THRESHOLD) {
        // 音声検出
        if (!voiceDetected) {
          voiceStartTime = currentTime;
          voiceDetected = true;
          Serial.println("Voice detected - start");
        }
        lastVoiceTime = currentTime;
      } else {
        // 無音状態
        if (voiceDetected) {
          unsigned long silenceDuration = currentTime - lastVoiceTime;
          unsigned long voiceDuration = currentTime - voiceStartTime;
          
          if (silenceDuration > SILENCE_DURATION_MS && voiceDuration > MIN_VOICE_DURATION_MS) {
            // 十分な長さの音声の後に無音が続いた場合、音声終了とみなす
            Serial.printf("Voice ended - duration: %lu ms\n", voiceDuration);
            // voiceDetectedはメインループで参照されるので、ここでは変更しない
          }
        }
      }
      
      // デバッグ用（必要に応じてコメントアウト）
      // Serial.printf("Volume: %.2f, Threshold: %d, Voice: %s\n", 
      //               currentVolume, VOICE_THRESHOLD, voiceDetected ? "YES" : "NO");
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  vTaskDelete(NULL);
}

void VoiceDetector::continuousRecordingTaskWrapper(void* param) {
  ((VoiceDetector*)param)->continuousRecordingTask();
}
