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
#include "../secrets/credentials.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "firebase_init.h"

#define DHTPIN 2      // Pin, an den das DHT22-Modul angeschlossen ist
#define DHTTYPE DHT22 // Typ des DHT22-Moduls
#define LED 4

DHT dht(DHTPIN, DHTTYPE); // Erstellen eines DHT-Objekts
uint32_t entry;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
unsigned long count = 0;

// NTP Servers:
const char* ntpServer = "pool.ntp.org";
const int timeZone = 1;

unsigned int updateMes = 1; // Messinterval in Minuten
float h = 0;
float t = 0;
unsigned int counter1;
time_t now;

void vTask_LED_builtin(void *pvParameters)
{
  ledcAttachPin(LED, 0);
  ledcSetup(0, 5000, 8);
  for (;;)
  {
      for(int dutyCycle = 0; dutyCycle <= 125; dutyCycle++){   
        ledcWrite(0, dutyCycle);
        delay(30);
      }
      ledcWrite(0, 0);
      delay(10000);
  }   
}

void vTask_RTDB_firebase(void *pvParameters)
{
   if (Firebase.ready())
      {
        sendDataPrevMillis = millis();
        FirebaseJson json;
        json.setDoubleDigits(3);
        json.add("Humidity", h);
        json.add("Temperature", t);
        //json.add("Time: ", now);
        //json.add("Counter", count);
        DebugOut.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, "/01 Messungen/" + String(count), &json) ? "ok" : fbdo.errorReason().c_str());
        DebugOut.println(" ");
        count++;
    }
    delay(500);
    vTaskDelete(NULL);
  }   

void vTask_DHT_Sensor(void *pvParameters)
{
  for (;;)
  {
    delay(updateMes*60*1000);
    h = dht.readHumidity();
    delay(250);
    t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
      DebugOut.println("Fehler beim Auslesen der Messwerte!");
      delay(30000);
    }

    xTaskCreate(vTask_RTDB_firebase, "RTDB Firebase", 10000, NULL, 2, NULL);

    DebugOut.print("Temp:");
    DebugOut.print(t);
    DebugOut.println("°C");
    DebugOut.print("Humid:");
    DebugOut.print(h);
    DebugOut.println("-----------");
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
  pinMode(LED, OUTPUT);

#ifdef telnet
  DebugOut.setWelcomeMsg(F("Welcome to the TelnetSpy example\r\n\n"));
  DebugOut.setCallbackOnConnect(telnetConnected);
  DebugOut.setCallbackOnDisconnect(telnetDisconnected);
  DebugOut.setFilter(char(1), F("\r\nNVT command: AO\r\n"), disconnectClientWrapper);
#endif

  DebugOut.begin(115200);
  DebugOut.println("Booting");
  setupOTA("ESP32-Cam-01", mySSID, myPASSWORD);
  configTime(3600, 0, ntpServer);
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
  //xTaskCreate(vTask_LED_builtin, "LED blinking..", 5000, NULL, 2, NULL);

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