#pragma once
#include <M5Core2.h>

void speechToSpeech(String base64audio, std::function<void()> onReadyToPlay = nullptr);
