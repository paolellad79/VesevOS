#pragma once
#include <Arduino.h>
#include <functional>
enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
enum HTTPUploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { HTTPUploadStatus status; String filename; uint8_t* buf; size_t currentSize; };
class WebServer {
 public:
  WebServer(int port = 80) {}
  void begin();
  void handleClient();
  void on(const char* uri, HTTPMethod m, std::function<void()> f);
  void on(const char* uri, HTTPMethod m, std::function<void()> f, std::function<void()> up);
  void onNotFound(std::function<void()> f);
  void collectHeaders(const char** h, size_t n);
  String header(const char* n);
  String arg(const char* n);
  void sendHeader(const String& n, const String& v);
  void send(int code, const char* ct, const String& body);
  void send_P(int code, const char* ct, const char* body);
  HTTPUpload& upload();
};
