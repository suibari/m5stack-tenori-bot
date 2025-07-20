#ifndef CONFIG_H
#define CONFIG_H

// サーバ設定
#define SERVER_URL "https://192.168.1.200:5050"

// 音声録音設定
#define MAX_TOUCH_RECORDING_TIME 10000  // タッチ録音の最大時間（ミリ秒）
#define MAX_VOICE_RECORDING_TIME 5000   // 音声起動録音の最大時間（ミリ秒）

// 音声検出設定
#define SOFTWARE_GAIN 2.0 // マイクのソフトウェアゲイン（増幅率）
#define VOICE_DETECTION_THRESHOLD 3300 // 音声検出の閾値。この値より大きい音を検出すると録音開始: 常時3100~3200くらい

// デバッグ設定
#define DEBUG_VOICE_DETECTION false
#define DEBUG_NETWORK_COMMUNICATION true

#endif
