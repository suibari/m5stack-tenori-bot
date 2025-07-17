#include <WiFiClientSecure.h>
#include <vector>

// チャンク送信用のヘルパー関数
bool sendChunk(WiFiClientSecure& client, const String& data) {
  size_t written = 0;
  size_t totalLen = data.length();
  
  while (written < totalLen) {
    size_t toWrite = min(totalLen - written, (size_t)1024);
    size_t result = client.write((const uint8_t*)(data.c_str() + written), toWrite);
    if (result == 0) return false;
    written += result;
  }
  return true;
}

void skipResponseHeaders(WiFiClient& client, unsigned long timeout_ms = 5000) {
  unsigned long deadline = millis() + timeout_ms;
  while (millis() < deadline) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {  // 空行＝ヘッダー終端
      break;
    }
  }
}

// チャンクされたHTTPレスポンスのボディだけを
// WiFiClientからバイナリで受け取り、チャンクを解除して返す
std::vector<uint8_t> readChunkedBody(WiFiClient& client, unsigned long timeout_ms = 30000) {
  std::vector<uint8_t> body;
  unsigned long deadline = millis() + timeout_ms;

  while (millis() < deadline) {
    // チャンクサイズ行を読み取る
    String line = client.readStringUntil('\n');
    line.trim(); // \rなど除去
    if (line.length() == 0) continue;

    // 16進数でチャンクサイズを取得
    int chunkSize = (int) strtol(line.c_str(), nullptr, 16);
    if (chunkSize == 0) break; // チャンク終了

    // chunkSizeバイトを受け取る
    int bytesRead = 0;
    while (bytesRead < chunkSize) {
      if (client.available()) {
        int toRead = chunkSize - bytesRead;
        if (toRead > 512) toRead = 512; // 適当なバッファサイズ
        uint8_t buffer[512];
        int len = client.read(buffer, toRead);
        if (len > 0) {
          body.insert(body.end(), buffer, buffer + len);
          bytesRead += len;
        }
      } else {
        delay(1);
      }
      if (millis() > deadline) return body; // タイムアウト
    }

    // チャンク末尾のCRLFを読む（2バイト）
    client.read(); // \r
    client.read(); // \n
  }

  return body;
}

// チャンクでなければ普通に全部読むだけの関数
std::vector<uint8_t> readBody(WiFiClient& client, unsigned long timeout_ms = 30000) {
  std::vector<uint8_t> body;
  unsigned long deadline = millis() + timeout_ms;

  while (millis() < deadline) {
    while (client.available()) {
      uint8_t b = client.read();
      body.push_back(b);
    }
    if (!client.connected()) break;
    delay(1);
  }
  return body;
}

String urlEncode(const String& str) {
  String encoded = "";
  char c;
  char code0, code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}
