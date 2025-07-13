#pragma once

// I2S設定共通
constexpr int I2S_SAMPLE_RATE     = 16000;
constexpr int I2S_SAMPLE_BITS     = 16;
constexpr int I2S_CHANNEL_NUM     = 1; // モノラル
constexpr int I2S_BUFFER_SIZE     = 1024;
constexpr int MAX_RECORDING_SIZE  = 65536 * 3/4; // HTTPClientの制限とbase64エンコード後のサイズを考慮
