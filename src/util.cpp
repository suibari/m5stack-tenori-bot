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
