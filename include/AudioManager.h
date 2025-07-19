#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <driver/i2s.h>

class AudioManager {
private:
  static const int SAMPLE_RATE = 16000;
  static const int BITS_PER_SAMPLE = 16;
  static const int CHANNELS = 1;
  static const int BUFFER_SIZE = 1024;
  static const int MAX_RECORD_SIZE = SAMPLE_RATE * 10 * 2; // 10秒分のバッファ
  
  uint8_t* recordBuffer;
  size_t recordedSize;
  size_t currentRecordPos;
  bool isRecording;
  bool isPlayingAudio;
  
  // I2S設定
  i2s_config_t i2sConfig;
  i2s_pin_config_t pinConfig;
  
public:
  AudioManager();
  ~AudioManager();
  
  bool init();
  void startRecording();
  size_t stopRecording();
  uint8_t* getRecordedData();
  size_t getRecordedSize();
  
  void startPlayback(uint8_t* data, size_t size, int sampleRate = 16000);
  void stopPlayback();
  bool isPlaying();
  
  void recordingTask();
  void playbackTask(uint8_t* data, size_t size);

private:
  void configureI2SForRecording();
  void configureI2SForPlayback(int sampleRate = 16000);
};

#endif
