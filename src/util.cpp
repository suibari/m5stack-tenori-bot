#include <WiFiClientSecure.h>

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

String removeChunkHeaderFooter(const String& raw) {
  int headerEnd = raw.indexOf("\r\n");
  int footerStart = raw.lastIndexOf("\r\n0\r\n");

  if (headerEnd == -1 || footerStart == -1 || footerStart <= headerEnd) {
    return "";
  }

  return raw.substring(headerEnd + 2, footerStart);
}
