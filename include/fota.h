#pragma once

#include <Arduino.h>

#include "ESP8266HTTPClient.h"
#include "ESP8266WiFi.h"
#include "ESP8266WiFiMulti.h"
#include "ESP8266httpUpdate.h"

namespace fota
{
    void performOTAUpdate(String version, String ssid, String pass);
}