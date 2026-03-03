#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP32Ping.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Update.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <FS.h>
#include <SPIFFS.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "syslog_handler.h"

// Firmware version
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.0"
#endif

// HTTP Server
#define HTTP_PORT 80
#define MAX_REQUEST_BODY_SIZE (4 * 1024)  // 4KBのリクエストボディサイズ制限
AsyncWebServer server(HTTP_PORT);

// WiFi Config reset Pin
const int PIN_WIFI_RESET = 18;

struct GlobalVars
{
  bool rebootFlag = false;
  std::atomic<bool> otaInProgress{false};  // OTA実行中のフラグ（アトミック）
  std::atomic<bool> otaUrlPending{false};  // OTA URL処理待ちフラグ（アトミック）
  String SSID = "";
  String PASSWORD = "";
  String OTA_URL = "";
  int wifiErrorCount = 0;
  String NTP_SERVER = "ntp.nict.jp";
  String HOSTNAME = "ESP32-OTA";
};

GlobalVars globalVars;
std::mutex requestBodyMutex;  // リクエストボディ変数の排他制御用ミューテックス

// WiFi設定の読み込み
JsonDocument loadWiFiConfig()
{
  File file = SPIFFS.open("/wifi.json", FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return JsonDocument();
  }

  JsonDocument json;
  DeserializationError error = deserializeJson(json, file);
  if (error)
  {
    Serial.println("Failed to parse wifi.json");
    file.close();
    return JsonDocument();
  }
  file.close();

  return json;
}

// WiFi AP Mode
void wifiAPMode()
{
  Serial.println("Starting AP mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 0, 1), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("ESP32-OTA", "");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  syslogPrintf(LOG_INFO, "AP IP address: %s", WiFi.softAPIP().toString().c_str());
}

// WiFiの初期化
void initWiFi()
{
  // 設定ファイルの読み込み
  JsonDocument json = loadWiFiConfig();
  if (json.isNull())
  {
    wifiAPMode();
    return;
  }

  Serial.println("wifi.json loaded");
  Serial.println("WiFi: " + json.as<String>());
  globalVars.SSID = json["ssid"].as<String>();
  globalVars.PASSWORD = json["password"].as<String>();

  // ホスト名の設定（WiFi.begin()の前に設定する必要がある）
  if (json["hostname"] != "")
  {
    globalVars.HOSTNAME = json["hostname"].as<String>();
    WiFi.setHostname(globalVars.HOSTNAME.c_str());
  }

  String type = json["type"].as<String>();

  if (type == "static")
  {
    if (json["ipaddress"] != "" && json["gateway"] != "" && json["netmask"] != "")
    {
      IPAddress ipaddr, gateway, netmask;
      ipaddr.fromString(json["ipaddress"].as<String>());
      gateway.fromString(json["gateway"].as<String>());
      netmask.fromString(json["netmask"].as<String>());

      if (json["nameservers"].is<JsonArray>() && json["nameservers"].size() >= 2)
      {
        IPAddress dns1, dns2;
        dns1.fromString(json["nameservers"][0].as<String>());
        dns2.fromString(json["nameservers"][1].as<String>());
        WiFi.config(ipaddr, gateway, netmask, dns1, dns2);
      }
      else
      {
        WiFi.config(ipaddr, gateway, netmask);
      }
    }
    else
    {
      Serial.println("Missing static IP configuration fields");
      syslogLog("Missing static IP configuration fields", LOG_ERR);
      return;
    }
  }
  else if (type == "dhcp")
  {
    // DHCPの場合、特に何も設定しない（デフォルトでDHCPが使用される）
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }
  else
  {
    Serial.println("Invalid type field in wifi.json");
    syslogLog("Invalid type field in wifi.json", LOG_ERR);
    return;
  }

  WiFi.begin(globalVars.SSID, globalVars.PASSWORD);
  int count = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    count++;
    delay(100);
    if (count > 30)
    {
      Serial.println("Failed to connect to WiFi");
      syslogLog("Failed to connect to WiFi", LOG_ERR);
      return;
    }
  }
  IPAddress ipaddr, gateway;
  ipaddr = WiFi.localIP();
  gateway = WiFi.gatewayIP();

  Serial.print("IP Address: ");
  Serial.println(ipaddr);
  Serial.print("Gateway: ");
  Serial.println(gateway);

  // ホスト名はWiFi.begin()の前に設定済み
  if (globalVars.HOSTNAME == "")
  {
    globalVars.HOSTNAME = WiFi.getHostname();
  }
}

void resetWiFi()
{
  // WiFiの設定を削除
  SPIFFS.remove("/wifi.json");
  Serial.println("WiFi settings deleted");
}

// HTTP OTA機能
void httpOTA(const String &url)
{
  globalVars.otaInProgress.store(true);  // OTA開始フラグ（アトミック）
  syslogPrintf(LOG_INFO, "Starting HTTP OTA from %s", url.c_str());
  Serial.printf("Starting HTTP OTA from %s\n", url.c_str());

  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setConnectTimeout(10000);
  http.setTimeout(30000);
  
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200)
  {
    int contentLength = http.getSize();
    syslogPrintf(LOG_INFO, "Firmware size: %d bytes", contentLength);
    Serial.printf("Firmware size: %d bytes\n", contentLength);

    if (Update.begin(contentLength))
    {
      WiFiClient *client = http.getStreamPtr();
      
      // 小さなチャンクでデータを読み込み、頻繁にタスク切替を許可
      const size_t bufferSize = 1024;
      uint8_t buffer[bufferSize];
      size_t totalWritten = 0;
      int lastProgress = 0;
      int chunkCounter = 0;
      
      while (totalWritten < contentLength && client->connected())
      {
        size_t available = client->available();
        if (available > 0)
        {
          size_t toRead = min(available, bufferSize);
          size_t bytesRead = client->read(buffer, toRead);
          
          if (bytesRead > 0)
          {
            size_t written = Update.write(buffer, bytesRead);
            if (written != bytesRead)
            {
              syslogPrintf(LOG_ERR, "Write error: %d != %d", written, bytesRead);
              Serial.printf("Write error: %d != %d\n", written, bytesRead);
              Update.abort();
              http.end();
              return;
            }
            totalWritten += written;
            
            // 進捗表示（10%ごと）
            int progress = (totalWritten * 100) / contentLength;
            if (progress != lastProgress && progress % 10 == 0)
            {
              syslogPrintf(LOG_INFO, "OTA Progress: %d%%", progress);
              Serial.printf("OTA Progress: %d%%\n", progress);
              lastProgress = progress;
            }
          }
        }
        
        // 毎回タスク切替とウォッチドッグリセット
        yield();
        esp_task_wdt_reset();
        delay(1);
        
        // 頻繁に追加のタスク切替
        chunkCounter++;
        if (chunkCounter % 10 == 0)
        {
          yield();
          delay(5);
        }
      }
      
      // 接続の確認
      if (!client->connected())
      {
        syslogPrintf(LOG_ERR, "OTA download: connection lost at %d/%d bytes", totalWritten, contentLength);
        Serial.printf("OTA download: connection lost at %d/%d bytes\n", totalWritten, contentLength);
        Update.abort();
        http.end();
        return;
      }

      if (totalWritten == contentLength)
      {
        syslogPrintf(LOG_INFO, "OTA download complete: %d bytes", totalWritten);
        Serial.printf("OTA download complete: %d bytes\n", totalWritten);

        if (Update.end())
        {
          syslogLog("OTA update finished successfully", LOG_INFO);
          Serial.println("OTA update finished successfully");
          globalVars.rebootFlag = true;
        }
        else
        {
          syslogPrintf(LOG_ERR, "OTA update failed: %s", Update.errorString());
          Serial.printf("OTA update failed: %s\n", Update.errorString());
        }
      }
      else
      {
        syslogPrintf(LOG_ERR, "OTA download failed: %d/%d bytes written", totalWritten, contentLength);
        Serial.printf("OTA download failed: %d/%d bytes written\n", totalWritten, contentLength);
        Update.abort();
      }
    }
    else
    {
      syslogPrintf(LOG_ERR, "Not enough space to begin OTA: %d bytes required", contentLength);
      Serial.printf("Not enough space to begin OTA: %d bytes required\n", contentLength);
    }
  }
  else
  {
    syslogPrintf(LOG_ERR, "HTTP GET failed: %d", httpResponseCode);
    Serial.printf("HTTP GET failed: %d\n", httpResponseCode);
  }

  http.end();
  globalVars.otaInProgress.store(false);  // OTA終了フラグ（アトミック）
}

// OTA処理を実行するFreeRTOSタスク関数
void otaTask(void *pvParameters)
{
  String otaUrl = *((String *)pvParameters);
  delete (String *)pvParameters;  // メモリ解放
  
  Serial.println("OTA Task started");
  syslogPrintf(LOG_INFO, "OTA Task started for %s", otaUrl.c_str());
  
  httpOTA(otaUrl);
  
  Serial.println("OTA Task finished");
  syslogLog("OTA Task finished", LOG_INFO);
  
  vTaskDelete(NULL);  // タスク自身を削除
}

void onNotFound(AsyncWebServerRequest *request)
{
  if (request->url().startsWith("/api/"))
  {
    request->send(404);
    return;
  }

  request->send(SPIFFS, "/web/index.html", "text/html");
}

String requestBody;  // リクエストボディ格納用（ミューテックスで保護）

void sendErrorResponse(AsyncWebServerRequest *request, int code, const char *message)
{
  JsonDocument json;
  json["status"] = "error";
  json["message"] = message;
  request->send(code, "application/json", json.as<String>());
}

void postWiFi(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // リクエストボディサイズチェック
  if (total > MAX_REQUEST_BODY_SIZE)
  {
    sendErrorResponse(request, 413, "Request body too large");
    return;
  }

  // ミューテックスで排他制御
  std::lock_guard<std::mutex> lock(requestBodyMutex);
  
  // WiFiのSSIDとパスワードを受け取る
  if (index == 0)
  {
    requestBody = "";
  }
  requestBody += String((char *)data).substring(0, len);

  JsonDocument json;
  if (index + len != total)
    return;

  DeserializationError error = deserializeJson(json, requestBody);
  // エラー処理
  if (error)
  {
    sendErrorResponse(request, 400, "Invalid JSON");
    return;
  }

  Serial.println("WiFi: " + json.as<String>());

  if (json["ssid"] == "")
  {
    sendErrorResponse(request, 400, "SSID is null");
    return;
  }
  else if (json["password"] == "")
  {
    sendErrorResponse(request, 400, "Password is null");
    return;
  }

  String type = json["type"].as<String>();

  if (type == "static")
  {
    if (json["ipaddress"] != "" && json["netmask"] != "" && json["gateway"] != "")
    {
      // IPアドレスの設定
      IPAddress netmask;
      if (!netmask.fromString(json["netmask"].as<String>()))
      {
        sendErrorResponse(request, 400, "Invalid netmask");
        return;
      }
      IPAddress ipaddress;
      if (!ipaddress.fromString(json["ipaddress"].as<String>()))
      {
        sendErrorResponse(request, 400, "Invalid ipaddress");
        return;
      }
      IPAddress gateway;
      if (!gateway.fromString(json["gateway"].as<String>()))
      {
        sendErrorResponse(request, 400, "Invalid gateway");
        return;
      }

      if (json["nameservers"].is<JsonArray>() && json["nameservers"].size() >= 2)
      {
        IPAddress dns1, dns2;
        dns1.fromString(json["nameservers"][0].as<String>());
        dns2.fromString(json["nameservers"][1].as<String>());
        if (!WiFi.config(ipaddress, gateway, netmask, dns1, dns2))
        {
          sendErrorResponse(request, 500, "Failed to set IP address");
          return;
        }
      }
      else
      {
        if (!WiFi.config(ipaddress, gateway, netmask))
        {
          sendErrorResponse(request, 500, "Failed to set IP address");
          return;
        }
      }
    }
    else
    {
      sendErrorResponse(request, 400, "Missing static IP configuration fields");
      return;
    }
  }
  else if (type == "dhcp")
  {
    // DHCPの場合、特に何も設定しない（デフォルトでDHCPが使用される）
  }
  else
  {
    sendErrorResponse(request, 400, "Invalid type field");
    return;
  }

  // WiFiのSSIDとパスワードを保存
  File file = SPIFFS.open("/wifi.json", FILE_WRITE);
  if (!file)
  {
    sendErrorResponse(request, 500, "Failed to open file for writing");
    return;
  }
  serializeJson(json, file);
  file.close();

  json.clear();
  json["status"] = "ok";
  json["message"] = "WiFi connected";
  json["ip"] = WiFi.localIP().toString();
  request->send(200, "application/json", json.as<String>());
  globalVars.rebootFlag = true;
}

JsonDocument jsonReadFile(String fileName)
{
  File file = SPIFFS.open(fileName, FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return JsonDocument();
  }
  JsonDocument json;
  DeserializationError error = deserializeJson(json, file);
  if (error)
  {
    Serial.println("Failed to parse JSON file: " + fileName);
    file.close();
    return JsonDocument();
  }
  file.close();
  return json;
}

void getConfig(AsyncWebServerRequest *request)
{
  JsonDocument json;

  // Load wifi.json
  JsonDocument wifi = jsonReadFile("/wifi.json");
  if (wifi.isNull())
  {
    sendErrorResponse(request, 500, "Failed to read wifi.json");
    return;
  }
  json["wifi"] = wifi;

  // Get hostname from wifi.json or use default
  String hostname = wifi["hostname"].as<String>();
  if (hostname == "")
  {
    hostname = "ESP32-OTA";
  }
  json["hostname"] = hostname;

  // OTA URLを返す
  json["ota_url"] = globalVars.OTA_URL;

  // Load syslog.json
  JsonDocument syslog = jsonReadFile("/syslog.json");
  if (!syslog.isNull())
  {
    json["syslog"] = syslog;
  }

  // Return NTP server setting
  json["ntp_server"] = globalVars.NTP_SERVER;

  json["status"] = "ok";
  request->send(200, "application/json", json.as<String>());
}

void triggerOTAUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // リクエストボディサイズチェック
  if (total > MAX_REQUEST_BODY_SIZE)
  {
    sendErrorResponse(request, 413, "Request body too large");
    return;
  }

  // ミューテックスで排他制御
  std::lock_guard<std::mutex> lock(requestBodyMutex);

  // リクエストボディの処理
  if (index == 0)
  {
    requestBody = "";
  }
  requestBody += String((char *)data).substring(0, len);

  JsonDocument json;
  if (index + len != total)
    return;

  DeserializationError error = deserializeJson(json, requestBody);
  if (error)
  {
    sendErrorResponse(request, 400, "Invalid JSON");
    return;
  }

  // OTA URLを取得
  String otaUrl = json["ota_url"].as<String>();
  if (otaUrl.length() == 0)
  {
    sendErrorResponse(request, 400, "OTA URL is not set");
    return;
  }

  // OTA URLをグローバル変数に保存
  globalVars.OTA_URL = otaUrl;

  json.clear();
  json["status"] = "ok";
  json["message"] = "OTA update started";
  request->send(200, "application/json", json.as<String>());

  // OTAがすでに進行中でないか確認
  if (globalVars.otaInProgress.load())
  {
    syslogLog("OTA already in progress, ignoring new request", LOG_WARNING);
    Serial.println("OTA already in progress, ignoring new request");
    return;
  }

  // OTA処理を別タスクで開始（非同期実行）
  String *otaUrlPtr = new String(otaUrl);
  BaseType_t ret = xTaskCreate(
    otaTask,           // タスク関数
    "OTA_Task",        // タスク名
    8192,              // スタックサイズ（8KB）
    (void *)otaUrlPtr, // タスクパラメータ
    1,                 // 優先度
    NULL               // タスクハンドル（不要）
  );

  if (ret != pdPASS)
  {
    syslogLog("Failed to create OTA task", LOG_ERR);
    Serial.println("Failed to create OTA task");
    delete otaUrlPtr;
  }
}

// Handler for POST /api/syslog
void postSyslog(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // リクエストボディサイズチェック
  if (total > MAX_REQUEST_BODY_SIZE)
  {
    sendErrorResponse(request, 413, "Request body too large");
    return;
  }

  // ミューテックスで排他制御
  std::lock_guard<std::mutex> lock(requestBodyMutex);
  static String localRequestBody;  // ローカルバッファ

  if (index == 0)
  {
    localRequestBody = "";
  }
  localRequestBody += String((char *)data).substring(0, len);

  JsonDocument json;
  if (index + len != total)
    return;

  DeserializationError error = deserializeJson(json, localRequestBody);
  if (error)
  {
    sendErrorResponse(request, 400, "Invalid JSON");
    return;
  }

  // Update hostname if provided
  if (!json["hostname"].isNull())
  {
    String newHostname = json["hostname"].as<String>();
    if (newHostname.length() > 0)
    {
      globalVars.HOSTNAME = newHostname;
      WiFi.setHostname(globalVars.HOSTNAME.c_str());
      
      // Update wifi.json to persist the new hostname
      JsonDocument wifiJson = loadWiFiConfig();
      wifiJson["hostname"] = globalVars.HOSTNAME;
      File wifiFile = SPIFFS.open("/wifi.json", FILE_WRITE);
      if (wifiFile)
      {
        serializeJson(wifiJson, wifiFile);
        wifiFile.close();
      }
    }
  }

  // Save Syslog settings
  File file = SPIFFS.open("/syslog.json", FILE_WRITE);
  if (!file)
  {
    sendErrorResponse(request, 500, "Failed to open file for writing");
    return;
  }
  serializeJson(json, file);
  file.close();

  // Reinitialize Syslog with new settings (no reboot required)
  reinitSyslog(globalVars.HOSTNAME);

  json.clear();
  json["status"] = "ok";
  json["message"] = "Syslog config updated and applied";
  request->send(200, "application/json", json.as<String>());
  // Note: No reboot required for Syslog changes
}

// Handler for POST /api/ntp
void postNtp(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // リクエストボディサイズチェック
  if (total > MAX_REQUEST_BODY_SIZE)
  {
    sendErrorResponse(request, 413, "Request body too large");
    return;
  }

  // ミューテックスで排他制御
  std::lock_guard<std::mutex> lock(requestBodyMutex);
  static String localRequestBody;  // ローカルバッファ

  if (index == 0)
  {
    localRequestBody = "";
  }
  localRequestBody += String((char *)data).substring(0, len);

  JsonDocument json;
  if (index + len != total)
    return;

  DeserializationError error = deserializeJson(json, localRequestBody);
  if (error)
  {
    sendErrorResponse(request, 400, "Invalid JSON");
    return;
  }

  // 保存用JSONドキュメント作成
  JsonDocument ntpJson;
  ntpJson["ntp_server"] = json["ntp_server"].as<String>();

  // 保存
  File file = SPIFFS.open("/ntp.json", FILE_WRITE);
  if (!file)
  {
    sendErrorResponse(request, 500, "Failed to open file for writing");
    return;
  }
  serializeJson(ntpJson, file);
  file.close();

  // メモリ内の変数も更新
  globalVars.NTP_SERVER = json["ntp_server"].as<String>();

  // NTP設定を動的に適用（再起動なし）
  configTime(9 * 3600L, 0, globalVars.NTP_SERVER.c_str());

  JsonDocument responseJson;
  responseJson["status"] = "ok";
  responseJson["message"] = "NTP config updated and applied";
  request->send(200, "application/json", responseJson.as<String>());
}

void getStatus(AsyncWebServerRequest *request)
{
  JsonDocument json;

  // Get hostname
  String hostname = ArduinoOTA.getHostname();
  json["hostname"] = hostname;

  // Firmware version and build info
  json["firmware_version"] = FIRMWARE_VERSION;
  
  // Build date in ISO 8601 format
  const char months[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char dateStr[20];
  int year, month, day, hour, minute, second;
  sscanf(__DATE__, "%s %d %d", dateStr, &day, &year);
  for (month = 0; month < 12; month++) {
    if (strcmp(dateStr, months[month]) == 0) break;
  }
  month++;
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
  sprintf(dateStr, "%04d-%02d-%02dT%02d:%02d:%02d", year, month, day, hour, minute, second);
  json["build_date"] = dateStr;

  // Get IP address
  IPAddress ip = WiFi.localIP();
  json["ip_address"] = ip.toString();

  // Get subnet mask
  IPAddress subnet = WiFi.subnetMask();
  json["subnet_mask"] = subnet.toString();

  // Get gateway IP
  IPAddress gateway = WiFi.gatewayIP();
  json["gateway"] = gateway.toString();

  // Get DNS server IP
  IPAddress dns = WiFi.dnsIP();
  json["dns_server"] = dns.toString();

  // Get system time (ESP32 internal time from NTP)
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  char timeStr[64];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S%z", timeinfo);
  json["system_time"] = timeStr;

  json["status"] = "ok";
  request->send(200, "application/json", json.as<String>());
}

void postConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  // リクエストボディサイズチェック
  if (total > MAX_REQUEST_BODY_SIZE)
  {
    sendErrorResponse(request, 413, "Request body too large");
    return;
  }

  // ミューテックスで排他制御
  std::lock_guard<std::mutex> lock(requestBodyMutex);

  // 設定ファイルの更新
  if (index == 0)
  {
    requestBody = "";
  }
  requestBody += String((char *)data).substring(0, len);

  JsonDocument json;
  if (index + len != total)
    return;

  Serial.println("Request Body: " + requestBody);
  DeserializationError error = deserializeJson(json, requestBody);
  // エラー処理
  if (error)
  {
    sendErrorResponse(request, 400, "Invalid JSON");
    return;
  }

  // WiFi 設定を保存
  if (!json["wifi"].isNull())
  {
    File wifiFile = SPIFFS.open("/wifi.json", FILE_WRITE);
    if (wifiFile)
    {
      serializeJson(json["wifi"], wifiFile);
      wifiFile.close();
    }
  }

  // OTA URLを保存
  if (!json["ota_url"].isNull())
  {
    globalVars.OTA_URL = json["ota_url"].as<String>();
  }

  // Syslog 設定を保存
  if (!json["syslog"].isNull())
  {
    File syslogFile = SPIFFS.open("/syslog.json", FILE_WRITE);
    if (syslogFile)
    {
      serializeJson(json["syslog"], syslogFile);
      syslogFile.close();
    }
  }

  json.clear();
  json["status"] = "ok";
  json["message"] = "Config updated";
  request->send(200, "application/json", json.as<String>());
  globalVars.rebootFlag = true;
}

bool prefixMatch(const char *str, const char *prefix)
{
  size_t prefixLen = strlen(prefix);
  size_t strLen = strlen(str);
  if (strLen < prefixLen)
    return false;
  return strncmp(str, prefix, prefixLen) == 0;
}

bool suffixMatch(const char *str, const char *suffix)
{
  size_t suffixLen = strlen(suffix);
  size_t strLen = strlen(str);
  if (strLen < suffixLen)
    return false;
  return strcmp(str + strLen - suffixLen, suffix) == 0;
}

void webServer()
{
  // Web Server
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, postWiFi);
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, postConfig);
  server.on("/api/syslog", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, postSyslog);
  server.on("/api/ntp", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, postNtp);
  server.on("/api/config", HTTP_GET, getConfig);
  server.on("/api/status", HTTP_GET, getStatus);
  server.on("/api/ota-update", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, triggerOTAUpdate);
  server.serveStatic("/", SPIFFS, "/web/").setDefaultFile("index.html");
  server.onNotFound(onNotFound);
  server.begin();
}

void setup()
{
  // WiFi DeleteのPINを設定 (プルアップ抵抗を使用)
  pinMode(PIN_WIFI_RESET, INPUT_PULLUP);

  Serial.begin(115200);
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  // WiFiの初期化
  initWiFi();

  // Syslogの初期化
  initSyslog(globalVars.HOSTNAME);

  // Web Serverの初期化
  webServer();

  // OTAの初期化
  JsonDocument wifiConfig = loadWiFiConfig();
  String hostname = wifiConfig["hostname"].as<String>();
  if (hostname == "")
  {
    hostname = "ESP32";
  }

  // NTPサーバ設定を読み込み
  JsonDocument ntp = jsonReadFile("/ntp.json");
  if (!ntp.isNull())
  {
    globalVars.NTP_SERVER = ntp["ntp_server"].as<String>();
  }
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.onStart([]()
                     {Serial.println("Start OTA"); 
                      syslogLog("Start OTA", LOG_INFO); });
  ArduinoOTA.onEnd([]()
                   {Serial.println("\nEnd OTA");
                    syslogLog("End OTA", LOG_INFO); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                         syslogPrintf(LOG_INFO, "OTA Progress: %u%%", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    syslogPrintf(LOG_ERR, "OTA Error[%u]", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
      syslogLog("OTA Auth Failed", LOG_ERR);
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
      syslogLog("OTA Begin Failed", LOG_ERR);
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
      syslogLog("OTA Connect Failed", LOG_ERR);
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
      syslogLog("OTA Receive Failed", LOG_ERR);
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
      syslogLog("OTA End Failed", LOG_ERR);
    } });
  ArduinoOTA.begin();

  // タイムゾーンの設定
  configTime(9 * 3600L, 0, globalVars.NTP_SERVER.c_str());

  syslogPrintf(LOG_INFO, "started with IP: %s", WiFi.localIP().toString().c_str());
}

void loop()
{
  // OTA実行中はその他の処理をスキップ
  if (globalVars.otaInProgress.load())
  {
    ArduinoOTA.handle();
    delay(100);
    return;
  }

  // WiFiのリセット
  // リセットボタンが押された場合（LOWでアクティブ）、WiFiの設定を削除して再起動
  if (digitalRead(PIN_WIFI_RESET) == LOW)
  {
    Serial.println("WiFi reset");
    syslogLog("WiFi reset", LOG_WARNING);
    resetWiFi();
    ESP.restart();
  }

  // WiFiの状態監視
  if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP)
  {
    globalVars.wifiErrorCount++;
    if (globalVars.wifiErrorCount > 50)
    {
      Serial.println("WiFi error");
      globalVars.wifiErrorCount = 0;
      ESP.restart();
    }
  }
  if (WiFi.status() == WL_CONNECTED && WiFi.getMode() != WIFI_AP)
  {
    // gatewayにpingを送信
    if (!Ping.ping(WiFi.gatewayIP(), 5))
    {
      Serial.println("Ping failed");
      globalVars.wifiErrorCount++;
      if (globalVars.wifiErrorCount > 5)
      {
        Serial.println("WiFi error");
        globalVars.wifiErrorCount = 0;
        ESP.restart();
      }
    }
    else
    {
      globalVars.wifiErrorCount = 0;
    }
  }
  if (globalVars.rebootFlag)
  {
    delay(1000);
    Serial.println("Rebooting...");
    ESP.restart();
  }

  // OTA URLが設定されている場合、HTTP OTAを実行（排他制御済み）
  if (globalVars.OTA_URL.length() > 0 &&
      !globalVars.otaInProgress.load() &&
      !globalVars.otaUrlPending.exchange(true))  // アトミックにフラグを設定
  {
    String otaUrl = globalVars.OTA_URL;
    globalVars.OTA_URL = ""; // OTA URLをクリア
    globalVars.otaUrlPending.store(false); // フラグを解放
    httpOTA(otaUrl);
  }

  // OTAの処理
  ArduinoOTA.handle();
  delay(100);
}
