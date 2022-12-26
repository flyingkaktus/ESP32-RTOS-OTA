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
#include <EEPROM.h>

#define DHTPIN 2      // Pin, an den das DHT22-Modul angeschlossen ist
#define DHTTYPE DHT22 // Typ des DHT22-Moduls
#define LED 4
//#define BAT 16
#define SCK 
#define SDA
#define EEPROM_SIZE 12

DHT dht(DHTPIN, DHTTYPE); // Erstellen eines DHT-Objekts
uint32_t entry;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
unsigned int count = 0;

// NTP Servers:
const char* ntpServer = "pool.ntp.org";
const int timeZone = 1;

unsigned int updateMes = 1; // Messinterval in Minuten
float hum = 0;
float temp = 0;
uint32_t runtime0;
uint32_t runtime1;
unsigned int counter1;
time_t now;

void writeRuntimetoEEPROM()
{
  EEPROM.writeUInt(1,runtime0+runtime1);
  delay(500);
  EEPROM.commit();
  DebugOut.println("EEPROM written..");
  delay(500);
}

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
        time_t t = time(nullptr);
        tm* timePtr = localtime(&t);
        sendDataPrevMillis = millis();
        count++;
        runtime1 = uint32_t(esp_timer_get_time()/1000/60/1000);
        FirebaseJson json;
        json.setDoubleDigits(3);
        json.add("Humidity", hum);
        json.add("Temperature", temp);
        json.add("Day Minutes: ", timePtr->tm_min);
        json.add("Day Hour: ", timePtr->tm_hour);
        json.add("ESP32 Runtime", runtime0+runtime1);
        for (int i = 0; i < 100; i++)
        {
          count++;
          delay(250);
          DebugOut.println(count);
        }
        
        DebugOut.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, "/03 Messungen/" + String(count), &json) ? "ok" : fbdo.errorReason().c_str());
        DebugOut.println(" ");
        writeRuntimetoEEPROM();
    }
    delay(500);
    vTaskDelete(NULL);
  }   

void vTask_DHT_Sensor(void *pvParameters)
{
  for (;;)
  {
    delay(updateMes*60*1000);
    hum = dht.readHumidity();
    delay(250);
    temp = dht.readTemperature();

    if (isnan(hum) || isnan(temp))
    {
      DebugOut.println("Fehler beim Auslesen der Messwerte!");
      delay(30000);
    }

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
    DebugOut.println(runtime0+runtime1);

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

  dht.begin(); // Initialisierung des DHT22-Moduls
  pinMode(LED, OUTPUT);
  //pinMode(BAT, INPUT);

  EEPROM.begin(EEPROM_SIZE);
  runtime0 = EEPROM.readUInt(1);

  DebugOut.begin(115200);
  DebugOut.println("Booting");
  setupOTA("ESP32-Cam-01", mySSID, myPASSWORD);
  configTime(3600, 0, ntpServer);
  time_t t_start = time(nullptr);

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