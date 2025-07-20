#ifndef VOICE_DETECTOR_H
#define VOICE_DETECTOR_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class VoiceDetector {
private:
  static const int SAMPLE_RATE = 16000;
  static const int BUFFER_SIZE = 512;
  static const int VOICE_THRESHOLD = 1600;  // 音声検出閾値
  static const int SILENCE_DURATION_MS = 2000;  // 無音判定時間(2秒)
  static const int MIN_VOICE_DURATION_MS = 500;  // 最小音声継続時間
  
  bool isContinuousRecording;
  bool voiceDetected;
  unsigned long voiceStartTime;
  unsigned long lastVoiceTime;
  float averageVolume;
  
  // I2S設定
  i2s_config_t i2sConfig;
  i2s_pin_config_t pinConfig;

  // FreeRTOSタスクハンドル
  TaskHandle_t recordingTaskHandle;
  
public:
  VoiceDetector();
  
  bool init();
  void startContinuousRecording();
  void stopContinuousRecording();
  bool isVoiceDetected();
  
private:
  void configureI2S();
  float calculateVolume(int16_t* samples, size_t sampleCount);
  void continuousRecordingTask();
  static void continuousRecordingTaskWrapper(void* param);
};

#endif
