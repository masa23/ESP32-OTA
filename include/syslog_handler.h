#ifndef SYSLOG_HANDLER_H
#define SYSLOG_HANDLER_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <Syslog.h>

// Syslog client
extern WiFiUDP udpClient;
extern Syslog syslog;

// Function declarations
void initSyslog(String hostname);
void reinitSyslog(String hostname);  // Reinitialize Syslog with new settings
void syslogLog(const char* message, int severity = LOG_INFO);
void syslogPrintf(int severity, const char *format, ...);

#endif
