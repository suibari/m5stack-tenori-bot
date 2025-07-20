#include "VoiceDetector.h"
#include "config.h"
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
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
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
    .bck_io_num = -1,       // 内部マイクのため不要
    .ws_io_num = 0,
    .data_out_num = -1,     // 出力は使用しない
    .data_in_num = 34       // M5Core2の標準DINピン
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
  xTaskCreate(continuousRecordingTaskWrapper, "VoiceDetectionTask", 4096, this, 4, &recordingTaskHandle);
  
  Serial.println("Continuous voice detection started");
}

void VoiceDetector::stopContinuousRecording() {
  if (!isContinuousRecording || recordingTaskHandle == NULL) return;

  Serial.println("DEBUG: Requesting VoiceDetectionTask to stop...");
  isContinuousRecording = false;

  // タスクが自己終了するのを待つ（最大1秒）
  unsigned long startTime = millis();
  while (recordingTaskHandle != NULL && millis() - startTime < 1000) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  if (recordingTaskHandle != NULL) {
    Serial.println("ERROR: VoiceDetectionTask did not terminate, forcing deletion.");
    vTaskDelete(recordingTaskHandle);
    recordingTaskHandle = NULL;
    i2s_driver_uninstall(I2S_NUM_0); // フォールバックとしてドライバをアンインストール
  } else {
    Serial.println("DEBUG: VoiceDetectionTask stopped successfully.");
  }
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
    // voiceDetectedがtrueになったら、状態がリセットされるまでVAD処理を停止
    if (!voiceDetected) {
      // Serial.println("DEBUG: VAD loop running, trying to read I2S...");
      esp_err_t result = i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY);
      
      if (result != ESP_OK) {
        Serial.printf("ERROR: i2s_read failed with code: %d\n", result);
      }

      if (bytesRead > 0) {
        int16_t* samples = (int16_t*)buffer;
        size_t sampleCount = bytesRead / sizeof(int16_t);

        // ソフトウェアゲインを適用
        for (size_t i = 0; i < sampleCount; i++) {
          int32_t amplified_sample = (int32_t)samples[i] * SOFTWARE_GAIN;
          if (amplified_sample > 32767) amplified_sample = 32767;
          if (amplified_sample < -32768) amplified_sample = -32768;
          samples[i] = (int16_t)amplified_sample;
        }

        float currentVolume = calculateVolume(samples, sampleCount);
        
        // デバッグ用に現在の音量とサンプルデータを表示
        Serial.printf("Volume: %.2f (Gain: x%.1f, Samples: %d, %d, ...)\n", 
                      currentVolume, SOFTWARE_GAIN, samples[0], samples[1]);

        // 平滑化せず、直接的な音量で判断することで応答性を向上
        if (currentVolume > VOICE_DETECTION_THRESHOLD) {
          voiceDetected = true;
          Serial.printf(">>> Voice detected! (Volume: %.2f > Threshold: %d)\n", currentVolume, VOICE_DETECTION_THRESHOLD);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  // クリーンアップ処理
  i2s_driver_uninstall(I2S_NUM_0);
  Serial.println("DEBUG: VoiceDetectionTask cleaned up and is exiting.");
  recordingTaskHandle = NULL; // ハンドルをNULLにしてタスクの終了を通知
  vTaskDelete(NULL);          // タスク自身を削除
}

void VoiceDetector::continuousRecordingTaskWrapper(void* param) {
  ((VoiceDetector*)param)->continuousRecordingTask();
}
