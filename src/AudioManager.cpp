#include "AudioManager.h"
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Speaker.h>

// 静的メンバ変数
static AudioManager* instance = nullptr;

// FreeRTOSタスク用の静的関数
static void recordingTaskWrapper(void* param) {
  ((AudioManager*)param)->recordingTask();
}

static void playbackTaskWrapper(void* param) {
  struct PlaybackParams {
    AudioManager* manager;
    uint8_t* data;
    size_t size;
  };
  PlaybackParams* params = (PlaybackParams*)param;
  params->manager->playbackTask(params->data, params->size);
  delete params;
  vTaskDelete(NULL);
}

AudioManager::AudioManager() {
  recordBuffer = nullptr;
  recordedSize = 0;
  currentRecordPos = 0;
  isRecording = false;
  isPlayingAudio = false;
  instance = this;
}

AudioManager::~AudioManager() {
  if (recordBuffer) {
    free(recordBuffer);
  }
}

bool AudioManager::init() {
  // 録音バッファ確保
  recordBuffer = (uint8_t*)malloc(MAX_RECORD_SIZE);
  if (!recordBuffer) {
    Serial.println("Failed to allocate record buffer");
    return false;
  }
  
  // I2S設定の初期化
  i2sConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
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
    .bck_io_num = CONFIG_I2S_BCK_PIN,
    .ws_io_num = CONFIG_I2S_LRCK_PIN,
    .data_out_num = CONFIG_I2S_DATA_PIN,
    .data_in_num = CONFIG_I2S_DATA_IN_PIN
  };
  
  return true;
}

void AudioManager::startRecording() {
  if (isRecording) {
    Serial.println("DEBUG: Already recording, ignoring startRecording()");
    return;
  }
  
  Serial.println("DEBUG: Starting recording...");
  configureI2SForRecording();
  
  recordedSize = 0;
  currentRecordPos = 0;
  isRecording = true;
  
  // 録音タスクを作成
  xTaskCreate(recordingTaskWrapper, "RecordingTask", 8192, this, 5, NULL);
  Serial.println("DEBUG: Recording task created");
}

size_t AudioManager::stopRecording() {
  isRecording = false;
  i2s_driver_uninstall(I2S_NUM_0);
  
  // 少し待ってタスクの終了を待つ
  vTaskDelay(pdMS_TO_TICKS(100));
  
  return recordedSize;
}

uint8_t* AudioManager::getRecordedData() {
  return recordBuffer;
}

size_t AudioManager::getRecordedSize() {
  return recordedSize;
}

void AudioManager::startPlayback(uint8_t* data, size_t size, int sampleRate) {
  if (isPlayingAudio) return;
  
  configureI2SForPlayback(sampleRate);
  isPlayingAudio = true;
  
  // 再生タスクを作成
  struct PlaybackParams {
    AudioManager* manager;
    uint8_t* data;
    size_t size;
  };
  
  PlaybackParams* params = new PlaybackParams();
  params->manager = this;
  params->data = data;
  params->size = size;
  
  xTaskCreate(playbackTaskWrapper, "PlaybackTask", 8192, params, 5, NULL);
}

void AudioManager::stopPlayback() {
  isPlayingAudio = false;
  i2s_driver_uninstall(I2S_NUM_0);
}

bool AudioManager::isPlaying() {
  return isPlayingAudio;
}

void AudioManager::configureI2SForRecording() {
  Serial.println("DEBUG: Configuring I2S for recording...");
  i2s_driver_uninstall(I2S_NUM_0);
  
  i2sConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  i2sConfig.sample_rate = 16000;
  i2sConfig.tx_desc_auto_clear = false;
  pinConfig.data_in_num = CONFIG_I2S_DATA_IN_PIN;

  esp_err_t result = i2s_driver_install(I2S_NUM_0, &i2sConfig, 0, NULL);
  if (result != ESP_OK) {
    Serial.printf("ERROR: I2S driver install failed: %d\n", result);
    return;
  } else {
    Serial.println("DEBUG: I2S driver installed successfully");
  }
  
  result = i2s_set_pin(I2S_NUM_0, &pinConfig);
  if (result != ESP_OK) {
    Serial.printf("ERROR: I2S set pin failed: %d\n", result);
    return;
  } else {
    Serial.println("DEBUG: I2S pins configured successfully");
  }
  
  i2s_zero_dma_buffer(I2S_NUM_0);
  Serial.println("DEBUG: I2S configuration complete");
}

void AudioManager::configureI2SForPlayback(int sampleRate) {
  i2s_driver_uninstall(I2S_NUM_0);
  
  i2sConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2sConfig.sample_rate = sampleRate;
  i2sConfig.tx_desc_auto_clear = true;
  pinConfig.data_in_num = I2S_PIN_NO_CHANGE;
  
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

// Make sure this method is declared public in AudioManager.h
void AudioManager::recordingTask() {
  uint8_t buffer[BUFFER_SIZE];
  size_t bytesRead = 0;
  
  while (isRecording && currentRecordPos < MAX_RECORD_SIZE - BUFFER_SIZE) {
    esp_err_t result = i2s_read(I2S_NUM_0, buffer, BUFFER_SIZE, &bytesRead, portMAX_DELAY);
    // Serial.printf("i2s_read result: %d, bytesRead: %d, buffer: %d\n", result, bytesRead, buffer);

    // int16_t* samples = (int16_t*)buffer;
    // int16_t maxSample = 0;
    // int16_t minSample = 0;

    // for (int i = 0; i < BUFFER_SIZE / 2; ++i) {
    //   if (samples[i] > maxSample) maxSample = samples[i];
    //   if (samples[i] < minSample) minSample = samples[i];
    // }
    // Serial.printf("Sample range: %d ~ %d\n", minSample, maxSample);

    if (result == ESP_OK && bytesRead > 0) {
      int16_t* samples = (int16_t*)buffer;
      size_t sampleCount = bytesRead / sizeof(int16_t);

      // ソフトウェアゲインを適用
      for (size_t i = 0; i < sampleCount; i++) {
        int32_t amplified_sample = (int32_t)samples[i] * SOFTWARE_GAIN;
        if (amplified_sample > 32767) amplified_sample = 32767;
        if (amplified_sample < -32768) amplified_sample = -32768;
        samples[i] = (int16_t)amplified_sample;
      }
      
      memcpy(recordBuffer + currentRecordPos, buffer, bytesRead);
      currentRecordPos += bytesRead;
      recordedSize = currentRecordPos;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  
  vTaskDelete(NULL);
}

void AudioManager::playbackTask(uint8_t* data, size_t size) {
  const size_t chunkSize = BUFFER_SIZE;
  size_t bytesWritten = 0;
  size_t totalWritten = 0;
  
  while (isPlayingAudio && totalWritten < size) {
    size_t remainingBytes = size - totalWritten;
    size_t currentChunk = (remainingBytes < chunkSize) ? remainingBytes : chunkSize;
    
    esp_err_t result = i2s_write(I2S_NUM_0, data + totalWritten, currentChunk, &bytesWritten, portMAX_DELAY);
    
    if (result == ESP_OK) {
      totalWritten += bytesWritten;
    } else {
      Serial.printf("I2S write failed: %d\n", result);
      break;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  
  isPlayingAudio = false;
  i2s_driver_uninstall(I2S_NUM_0);
}
