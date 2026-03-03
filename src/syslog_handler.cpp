#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <cstdarg>
#include "syslog_handler.h"

// Syslog client
WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);  // Use IETF protocol to avoid BSD format bugs

// Syslog server settings
bool syslogEnabled = false;              // Syslog enabled/disabled
String syslogServer = "";
int syslogPort = 514;
String syslogFacility = "user";          // Default facility (loaded from config)
uint16_t syslogFacilityValue = LOG_USER; // Default facility value
String syslogAppName = "OTA";            // Default app name
String syslogHostnameBuffer = "";        // Global buffer to keep hostname valid (avoid dangling pointer)

void initSyslog(String hostname)
{
  // Load syslog configuration from file
  File file = SPIFFS.open("/syslog.json", FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open syslog.json for reading");
  }
  else
  {
    JsonDocument json;
    DeserializationError error = deserializeJson(json, file);
    if (!error)
    {
      syslogServer = json["server"].as<String>();
      
      if (json["port"].as<int>())
      {
        int port = json["port"];
        if (port >= 1 && port <= 65535)
        {
          syslogPort = port;
        }
        else
        {
          Serial.println("Invalid syslog port number. Using default port 514.");
        }
      }

      // Get enabled setting, default to false if not specified
      syslogEnabled = json["enabled"].as<bool>();

      // Get facility setting, default to "user" if not specified
      syslogFacility = json["facility"].as<String>();
      if (syslogFacility == "")
      {
        syslogFacility = "user";
      }

      // Get app name setting, default to "OTA" if not specified
      syslogAppName = json["appname"].as<String>();
      if (syslogAppName == "")
      {
        syslogAppName = "OTA";
      }
    }
    file.close();
  }

  // Set facility value based on configuration
  if (syslogFacility == "user")
  {
    syslogFacilityValue = LOG_USER;
  }
  else if (syslogFacility == "local0")
  {
    syslogFacilityValue = LOG_LOCAL0;
  }
  else if (syslogFacility == "local1")
  {
    syslogFacilityValue = LOG_LOCAL1;
  }
  else if (syslogFacility == "local2")
  {
    syslogFacilityValue = LOG_LOCAL2;
  }
  else if (syslogFacility == "local3")
  {
    syslogFacilityValue = LOG_LOCAL3;
  }
  else if (syslogFacility == "local4")
  {
    syslogFacilityValue = LOG_LOCAL4;
  }
  else if (syslogFacility == "local5")
  {
    syslogFacilityValue = LOG_LOCAL5;
  }
  else if (syslogFacility == "local6")
  {
    syslogFacilityValue = LOG_LOCAL6;
  }
  else if (syslogFacility == "local7")
  {
    syslogFacilityValue = LOG_LOCAL7;
  }
  else
  {
    syslogFacilityValue = LOG_USER; // Default to USER if invalid
    Serial.println("Invalid syslog facility. Using default facility LOG_USER.");
  }

  if (syslogEnabled && syslogServer.length() > 0)
  {
    syslog.server(syslogServer.c_str(), syslogPort);
    syslogHostnameBuffer = hostname;  // Store hostname in global buffer to keep it valid
    syslog.deviceHostname(syslogHostnameBuffer.c_str());
    syslog.appName(syslogAppName.c_str());
    Serial.println("Syslog initialized with server: " + syslogServer + ", port: " + String(syslogPort) + ", facility: " + syslogFacility + ", appname: " + syslogAppName + ", hostname: " + hostname);
  }
  else if (syslogEnabled)
  {
    Serial.println("Syslog is enabled but server not configured");
  }
  else
  {
    Serial.println("Syslog is disabled");
  }
}

// Reinitialize Syslog with current configuration (for dynamic updates)
void reinitSyslog(String hostname)
{
  // Load syslog configuration from file
  File file = SPIFFS.open("/syslog.json", FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open syslog.json for reading in reinitSyslog");
    return;
  }
  
  JsonDocument json;
  DeserializationError error = deserializeJson(json, file);
  file.close();
  
  if (!error)
  {
    syslogServer = json["server"].as<String>();
    
    if (json["port"].as<int>())
    {
      int port = json["port"];
      if (port >= 1 && port <= 65535)
      {
        syslogPort = port;
      }
    }

    // Get enabled setting
    syslogEnabled = json["enabled"].as<bool>();

    // Get facility setting
    syslogFacility = json["facility"].as<String>();
    if (syslogFacility == "")
    {
      syslogFacility = "user";
    }

    // Get app name setting
    syslogAppName = json["appname"].as<String>();
    if (syslogAppName == "")
    {
      syslogAppName = "OTA";
    }
  }

  // Set facility value based on configuration
  if (syslogFacility == "user")
  {
    syslogFacilityValue = LOG_USER;
  }
  else if (syslogFacility == "local0")
  {
    syslogFacilityValue = LOG_LOCAL0;
  }
  else if (syslogFacility == "local1")
  {
    syslogFacilityValue = LOG_LOCAL1;
  }
  else if (syslogFacility == "local2")
  {
    syslogFacilityValue = LOG_LOCAL2;
  }
  else if (syslogFacility == "local3")
  {
    syslogFacilityValue = LOG_LOCAL3;
  }
  else if (syslogFacility == "local4")
  {
    syslogFacilityValue = LOG_LOCAL4;
  }
  else if (syslogFacility == "local5")
  {
    syslogFacilityValue = LOG_LOCAL5;
  }
  else if (syslogFacility == "local6")
  {
    syslogFacilityValue = LOG_LOCAL6;
  }
  else if (syslogFacility == "local7")
  {
    syslogFacilityValue = LOG_LOCAL7;
  }
  else
  {
    syslogFacilityValue = LOG_USER;
  }

  // Reconfigure Syslog client if enabled
  if (syslogEnabled && syslogServer.length() > 0)
  {
    syslog.server(syslogServer.c_str(), syslogPort);
    syslogHostnameBuffer = hostname;  // Store hostname in global buffer to keep it valid
    syslog.deviceHostname(syslogHostnameBuffer.c_str());
    syslog.appName(syslogAppName.c_str());
    Serial.println("Syslog reinitialized with server: " + syslogServer + ", port: " + String(syslogPort) + ", facility: " + syslogFacility + ", appname: " + syslogAppName + ", hostname: " + hostname);
  }
  else
  {
    Serial.println("Syslog disabled or server not configured");
  }
}

void syslogLog(const char *message, int severity)
{
  if (syslogEnabled && syslogServer.length() > 0)
  {
    syslog.log(LOG_MAKEPRI(syslogFacilityValue, severity), message);
  }
}

void syslogPrintf(int severity, const char *format, ...)
{
  if (syslogEnabled && syslogServer.length() > 0)
  {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    syslog.log(LOG_MAKEPRI(syslogFacilityValue, severity), buffer);
  }
}
