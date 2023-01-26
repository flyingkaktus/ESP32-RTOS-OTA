#define ESP32_RTOS // Uncomment this line if you want to use the code with freertos only on the ESP32
                   // Has to be done before including "OTA.h"
#define telnet     // Uncomment this line if you want non-telnet Serial.print()
// #define NEWCHARGE

#ifdef telnet
#include <TelnetSpy.h>
TelnetSpy LOG;
#define DebugOut LOG
#else
#define DebugOut Serial
#endif

#include "OTA.h"
#include "firebase_init.h"
#include "../secrets/credentials.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <EEPROM.h>
#include <sstream>
#include <String>

#define DHTPIN 2      // Pin, an den das DHT22-Modul angeschlossen ist
#define DHTTYPE DHT22 // Typ des DHT22-Moduls
// #define LED 4
// #define BAT 16
#define SCK
#define SDA
#define EEPROM_SIZE 64 // 64 bytes for runtime tracking

DHT dht(DHTPIN, DHTTYPE); // Erstellen eines DHT-Objekts
uint32_t entry;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
unsigned int count = 0;

// NTP Servers:
const char *ntpServer = "pool.ntp.org";
const int timeZone = 1;

unsigned int updateMes = 2; // Messinterval in Minuten
float hum = 0;
float temp = 0;
uint16_t runtime0 = 0;
uint16_t runtime1 = 0;
uint8_t eeprom_cycle = 0;
uint16_t counter1 = 0;
time_t now;
uint8_t valuesteps = 2;

String result = "000-00-00";

void writeRuntimetoEEPROM()
{
  DebugOut.println("EEPROM writting..");
  EEPROM.writeShort(eeprom_cycle, runtime0 + runtime1);
  delay(250);
  EEPROM.commit();
  delay(250);
  DebugOut.println("EEPROM written at");
  DebugOut.println(eeprom_cycle);

  if (eeprom_cycle >= EEPROM_SIZE)
  {
    eeprom_cycle = 0;
  }
  else
  {
    eeprom_cycle += valuesteps;
  }
}

// void vTask_LED_builtin(void *pvParameters)
// {
//   ledcAttachPin(LED, 0);
//   ledcSetup(0, 5000, 8);
//   for (;;)
//   {
//       for(int dutyCycle = 0; dutyCycle <= 125; dutyCycle++){
//         ledcWrite(0, dutyCycle);
//         delay(30);
//       }
//       ledcWrite(0, 0);
//       delay(10000);
//   }
// }

void vTask_RTDB_firebase(void *pvParameters)
{
  WiFi.setSleep(false);
  delay(250);

  if (Firebase.ready())
  {
    time_t t = time(nullptr);
    tm *timePtr = localtime(&t);

    count++;
    String pathToWrite;
    FirebaseJson json;
    json.setDoubleDigits(3);
    json.add("Humidity", hum);
    json.add("Temperature", temp);
    json.add("Day Minutes", timePtr->tm_min);
    json.add("Day Hour", timePtr->tm_hour);
    json.add("ESP32 Runtime SESSION", runtime1);
    json.add("ESP32 Runtime EEPROM", runtime0);
    json.add("ESP32 Runtime COMPLETE", runtime0 + runtime1);

    pathToWrite = result + String("/") + String(count);

    DebugOut.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, pathToWrite, &json) ? "ok" : fbdo.errorReason().c_str());

    if (fbdo.httpCode() >= 400)
    {
      DebugOut.println("Es trat ein Fehler auf. Der Token wird erneuert.");
      DebugOut.println(pathToWrite);
      Firebase.refreshToken(&config);
      DebugOut.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, pathToWrite, &json) ? "ok" : fbdo.errorReason().c_str());
    }

    DebugOut.println("---");

    writeRuntimetoEEPROM();
    json.clear();
  }

  delay(250);
  WiFi.setSleep(true);
  vTaskDelete(NULL);
}

void vTask_DHT_Sensor(void *pvParameters)
{
  for (;;)
  {
    delay((updateMes)*60 * 1000);
    hum = dht.readHumidity();
    delay(100);
    temp = dht.readTemperature();

    if (isnan(hum) || isnan(temp))
    {
      DebugOut.println("Fehler beim Auslesen der Messwerte!");
      delay(30000);
    }
    runtime1 = uint32_t(esp_timer_get_time() / 1000 / 60 / 1000);
    xTaskCreate(vTask_RTDB_firebase, "RTDB Firebase", 10000, NULL, 2, NULL);

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
  // pinMode(LED, OUTPUT);
  // pinMode(BAT, INPUT);

  EEPROM.begin(EEPROM_SIZE);
  uint16_t lastruntime = 0;

  setupOTA("ESP32-Cam-01", mySSID, myPASSWORD);
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

  config.api_key = API_KEY_FB;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DB_URL_FB;
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

#ifdef telnet
  xTaskCreate(vTask_telnet, "vTask_telnet", 5000, NULL, 2, NULL);
#endif
  // xTaskCreate(vTask_LED_builtin, "LED blinking..", 5000, NULL, 2, NULL);
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

void loop()
{

#if defined(ESP32_RTOS) && defined(ESP32)

#else // If you do not use FreeRTOS, you have to regulary call the handle method.
  ArduinoOTA.handle();
  delay(1000);
#endif

  // Your code here
}