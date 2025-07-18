#include <M5Core2.h>
#include "audio_config.hpp"
#include <WiFiClient.h>

void setupI2SPlayback(int sampleRate = I2S_SAMPLE_RATE);
void playAudio(const std::vector<uint8_t>& audioData, int sampleRate = I2S_SAMPLE_RATE);
void playChunkedBody(WiFiClient& client);