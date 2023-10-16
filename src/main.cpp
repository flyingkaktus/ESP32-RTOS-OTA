#define ESP32_RTOS // Uncomment this line if you want to use the code with freertos only on the ESP32
                   // Has to be done before including "OTA.h"
#define telnet     // Uncomment this line if you want non-telnet Serial.print()
// #define NEWCHARGE

// Conditional Includes
#ifdef telnet
  #include <TelnetSpy.h>
  TelnetSpy LOG;
  #define DebugOut LOG
#else
  #define DebugOut Serial
#endif

#include "OTA.h"
#include <HTTPClient.h>
#include "../secrets/credentials.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <EEPROM.h>
#include <sstream>

#define DHTPIN 2      // Pin, an den das DHT22-Modul angeschlossen ist
#define DHTTYPE DHT22 // Typ des DHT22-Moduls
#define SCK
#define SDA
#define EEPROM_SIZE 64 // 64 bytes for runtime tracking

// Constants
const int DHTPIN = 2;
const int DHTTYPE = DHT22;
const int EEPROM_SIZE = 64;
const char *NTP_SERVER = "pool.ntp.org";
const int TIME_ZONE = 1;

// Global Variables
DHT dht(DHTPIN, DHTTYPE);
unsigned long sendDataPrevMillis = 0;
unsigned int count = 0;
unsigned int updateInMinutes = 2;
float hum = 0;
float temp = 0;
uint16_t runtime0 = 0;
uint16_t runtime1 = 0;
uint8_t eeprom_cycle = 0;
String result = "000-00-00";

void writeRuntimetoEEPROM() {
    DebugOut.println("EEPROM writting..");
    EEPROM.writeShort(eeprom_cycle, runtime0 + runtime1);
    delay(250);
    EEPROM.commit();
    delay(250);
    DebugOut.println("EEPROM written at");
    DebugOut.println(eeprom_cycle);

    if (eeprom_cycle >= EEPROM_SIZE) {
        eeprom_cycle = 0;
    } else {
        eeprom_cycle += 2;
    }
}

void vTask_sendToPostgresDB() {
    if(WiFi.status() == WL_CONNECTED) {

        HTTPClient http;

        http.begin("http://82.165.120.229:8101/iot-entry");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        String httpRequestData = "humidity=" + String(hum) + "&temperature=" + String(temp) + "&dayMinutes=" + String(timePtr->tm_min) + "&dayHour=" + String(timePtr->tm_hour) + "&runtimeSession=" + String(runtime1) + "&runtimeEEPROM=" + String(runtime0);

        int httpResponseCode = http.POST(httpRequestData);
        
        if(httpResponseCode > 0) {
            DebugOut.printf("HTTP Response code: %d\n", httpResponseCode);
        }
        else {
            DebugOut.println("HTTP POST failed.");
        }
        http.end();
    }
    else {
        DebugOut.println("WiFi not connected.");
    }
}

void vTask_DHT_Sensor(void *pvParameters)
{
  for (;;)
  {
    delay((updateInMinutes)*60 * 1000);
    hum = dht.readHumidity();
    delay(100);
    temp = dht.readTemperature();

    if (isnan(hum) || isnan(temp))
    {
      DebugOut.println("Fehler beim Auslesen der Messwerte!");
      delay(30000);
    }
    runtime1 = uint32_t(esp_timer_get_time() / 1000 / 60 / 1000);
    xTaskCreate(vTask_sendToPostgresDB, "sendToPostgresql", 10000, NULL, 2, NULL);

    DebugOut.print("Temp: ");
    DebugOut.print(temp);
    DebugOut.println("°C");
    DebugOut.print("Humid: ");
    DebugOut.print(hum);
    DebugOut.println("%");
    DebugOut.print("Current Runtime (Min): ");
    DebugOut.println(runtime1);
    DebugOut.print("Runtime from last EEPROM (Min): ");
    DebugOut.println(runtime0);
    DebugOut.print("Runtime in EEPROM + now (Min): ");
    DebugOut.println(runtime0 + runtime1);
    

    // DebugOut.print("Battery Volt: ");
    // float sensorValue = analogRead(BAT);
    // float voltage = sensorValue * (5.00 / 1023.00) * 2;
    // DebugOut.println(voltage);
  }
}

#ifdef telnet
void vTask_telnet(void *pvParameters)
{
  for (;;)
  {
    DebugOut.handle();
    delay(1000);
  }
}

void telnetConnected()
{
  DebugOut.println(F("Telnet connection established."));
}

void telnetDisconnected()
{
  DebugOut.println(F("Telnet connection closed."));
}

void disconnectClientWrapper()
{
  DebugOut.disconnectClient();
}

#endif

void setup()
{
#ifdef telnet
  DebugOut.setWelcomeMsg(F("Welcome to the TelnetSpy\r\n\n"));
  DebugOut.setCallbackOnConnect(telnetConnected);
  DebugOut.setCallbackOnDisconnect(telnetDisconnected);
  DebugOut.setFilter(char(1), F("\r\nNVT command: AO\r\n"), disconnectClientWrapper);
#endif

  DebugOut.begin(115200);
  DebugOut.println("Booting");

  dht.begin(); // Initialisierung des DHT22-Moduls

  EEPROM.begin(EEPROM_SIZE);
  uint16_t lastruntime = 0;

  setupOTA("ESP32-Hygrometer", mySSID, myPASSWORD);
  configTime(3600, 0, ntpServer);
  delay(10000);
  time_t t_start = time(nullptr);
  delay(500);
  time_t t = time(nullptr);
  delay(500);
  tm *timePtr = localtime(&t);
  delay(500);
  String year = "0000";
  String yday = "000";
  String dhour = "00";
  String dmin = "00";

  year = timePtr->tm_year + 1900;

  if (timePtr->tm_yday < 100)
  {
    yday = String(0) + String(timePtr->tm_yday + 1);
  }
  else
  {
    if (timePtr->tm_yday < 10)
    {
      yday = String(00) + String(timePtr->tm_yday + 1);
    }
    else
    {
      yday = String(timePtr->tm_yday + 1);
    }
  }

  if (timePtr->tm_hour < 10)
  {
    dhour = String(0) + String(timePtr->tm_hour);
  }
  else
  {
    dhour = String(timePtr->tm_hour);
  }

  if (timePtr->tm_min < 10)
  {
    dmin = String(0) + String(timePtr->tm_min);
  }
  else
  {
    dmin = String(timePtr->tm_min);
  }

  result = year + yday + dhour + dmin;

  DebugOut.println(year);
  DebugOut.println(result);

  #ifdef telnet
    xTaskCreate(vTask_telnet, "vTask_telnet", 5000, NULL, 2, NULL);
  #endif

  xTaskCreate(vTask_DHT_Sensor, "DHT", 10000, NULL, 2, NULL);

  WiFi.setSleep(true);

  #ifdef NEWCHARGE
    DebugOut.println("Start clearing EEPROM...");
    for (int i = 0; i < EEPROM_SIZE; i++)
    {
      EEPROM.write(i, 0);
      delay(100);
    }
    EEPROM.commit();
    DebugOut.println("Done clearing EEPROM...");
  #endif

  try
  {
    for (int i = 0; i <= EEPROM_SIZE; i += valuesteps)
    {
      uint8_t lowByte = EEPROM.readByte(i);
      delay(50);
      uint8_t highByte = EEPROM.readByte(i + 1);
      delay(50);
      lastruntime = ((highByte << 8) | lowByte);
      DebugOut.print("Value found:");
      DebugOut.println(lastruntime);
      DebugOut.print("i-ter Lauf:");
      DebugOut.println(i);
      delay(50);
      if (lastruntime > runtime0)
      {
        runtime0 = lastruntime;
        eeprom_cycle = i + valuesteps;
      }
    }
  }
  catch (const std::exception &e)
  {
    // code to handle the error goes here
    DebugOut.printf(e.what());
  }
}

void loop() {
    // Your main loop
#if !defined(ESP32_RTOS) || !defined(ESP32)
    ArduinoOTA.handle();
    delay(1000);
#endif
}