#include <M5Core2.h>

void playAudio(const std::vector<uint8_t>& audioData, int sampleRate = I2S_SAMPLE_RATE);
void setupI2SPlayback(int sampleRate = I2S_SAMPLE_RATE);
