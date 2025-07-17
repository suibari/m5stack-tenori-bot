#include <M5Core2.h>
#include "audio_config.hpp"

void playAudio(const std::vector<uint8_t>& audioData, int sampleRate = I2S_SAMPLE_RATE);
void setupI2SPlayback(int sampleRate = I2S_SAMPLE_RATE);
void playAudioStreamChunk(const uint8_t* data, size_t len);
