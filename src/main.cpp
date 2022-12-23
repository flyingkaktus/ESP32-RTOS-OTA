#define ESP32_RTOS // Uncomment this line if you want to use the code with freertos only on the ESP32
                   // Has to be done before including "OTA.h"
#define telnet     // Uncomment this line if you want non-telnet Serial.print()

#ifdef telnet
#include <TelnetSpy.h>
TelnetSpy LOG;
#define DebugOut LOG
#else
#define DebugOut Serial
#endif

#include "OTA.h"
#include "secrets/credentials.h"

#include <Adafruit_Sensor.h>
#include <DHT.h>

#define DHTPIN 2      // Pin, an den das DHT22-Modul angeschlossen ist
#define DHTTYPE DHT22 // Typ des DHT22-Moduls

DHT dht(DHTPIN, DHTTYPE); // Erstellen eines DHT-Objekts
uint32_t entry;

float h;
float t;
unsigned int counter1;

void vTask_LED_builtin(void *pvParameters)
{
  for (;;)
  {
    delay(15000);
    digitalWrite(4, HIGH);
    delay(500);
    digitalWrite(4, LOW);
    delay(500);
    digitalWrite(4, HIGH);
    delay(500);
    digitalWrite(4, LOW);
    counter1++;
    DebugOut.println("Ein Hallo aus vTask_LED_builtin!");
    DebugOut.println(counter1);
    DebugOut.println(DebugOut);
  }
}

void vTask_DHT_Sensor(void *pvParameters)
{
  for (;;)
  {

    h = dht.readHumidity();
    delay(500);
    t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
      DebugOut.println("Fehler beim Auslesen der Messwerte!");
      delay(5000);
    }
    DebugOut.print("Temp:");
    DebugOut.print(t);
    DebugOut.println("°C");
    DebugOut.print("Humid:");
    DebugOut.print(h);
    DebugOut.println("%");
    delay(9500);
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
  dht.begin(); // Initialisierung des DHT22-Moduls
  pinMode(4, OUTPUT);

#ifdef telnet
  DebugOut.setWelcomeMsg(F("Welcome to the TelnetSpy example\r\n\n"));
  DebugOut.setCallbackOnConnect(telnetConnected);
  DebugOut.setCallbackOnDisconnect(telnetDisconnected);
  DebugOut.setFilter(char(1), F("\r\nNVT command: AO\r\n"), disconnectClientWrapper);
#endif

  DebugOut.begin(115200);
  DebugOut.println("Booting");

  setupOTA("ESP32-Cam-01", mySSID, myPASSWORD);

  xTaskCreate(vTask_LED_builtin, "LED blinking..", 5000, NULL, 2, NULL);
#ifdef telnet
  xTaskCreate(vTask_telnet, "vTask_telnet", 5000, NULL, 2, NULL);
#endif
  xTaskCreate(vTask_DHT_Sensor, "DHT", 10000, NULL, 2, NULL);
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