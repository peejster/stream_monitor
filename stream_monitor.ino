// Please use an Arduino IDE 1.6.8 or greater

#include <OneWire.h>             // for reading data from DS18B20 sensors
#include <ESP8266WiFi.h>         // for wifi connnectivity
#include <time.h>                // for getting network time
#include "simplesample_http.h"   // for interfacing with Azure IoT SDK

const int sensorCount = 2;       // define the number of sensors
const int tempPin = 2;           // temp sensors are connected to pin 2
OneWire ds(tempPin);             // create an instance of the temp sensors
int temperature[sensorCount];    // store the temp reading from each sensor

char ssid[] = "<your ssid here>";   // your network SSID (name)
char pass[] = "<your network password here>";    // your network password (use for WPA, or use as key for WEP)

time_t epochTime;                // get the current time
char* currentTimeString;         // store the time as a string

void setup()
{
  initSerial();                  // initialize the serial monitor
  connectWifi();                 // establish wifi connection
  initTime();                    // initialize time
}

void loop()
{
  // make sure you are connected to the wifi network
  if (WiFi.status() != WL_CONNECTED)
    connectWifi();

  // get current time
  epochTime = time(NULL);
  currentTimeString = ctime(&epochTime);
  Serial.print("Current time is: ");
  Serial.println(currentTimeString);
  
  // take reading from temperature sensors
  getTemp();
  
  // run the SimpleSample from the Azure IoT Hub SDK
  simplesample_http_run(temperature[0], temperature[1], currentTimeString);

  // wait 15 minutes or 900k milliseconds
  delay(900000);
}

void initSerial() {
    // Start serial and initialize stdout
    Serial.begin(115200);
    Serial.setDebugOutput(true); // display LogInfo in serial monitor
}

void initTime()
{
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  while (true)
  {
    epochTime = time(NULL);

    if (epochTime == 0)
    {
      Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
      delay(2000);
    }
    else
    {
      Serial.print("Fetched NTP epoch time is: ");
      Serial.println(epochTime);
      break;
    }
  }
}

// connect to wifi network
void connectWifi()
{
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);

  // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected to wifi");
}

// record the temperature from all the DS18B20 sensors
void getTemp()
{
  byte data[12];
  byte addr[sensorCount][8];
  
  // send a reset pulse
  ds.reset_search();
  
  // then receive presence pulse from the devices
  for (int i = 0; i < sensorCount; i++)
  {
    if ( !ds.search(addr[i]))
    {
      Serial.println("No devices.");
      ds.reset_search();
      temperature[i] = -1000;
      return;
    }
  }

  // get readings from the devices
  for (int i = 0; i < sensorCount; i++)
  {
    // verify the device's CRC
    if (OneWire::crc8(addr[i], 7) != addr[i][7])
    {
      Serial.println("CRC is not valid!");
      temperature[i] = -1000;
      return;
    }

    // check the device type
    // Serial.print("Device ");
    // Serial.print(i);

    if (!((addr[i][0] == 0x10) || (addr[i][0] == 0x28)))
    {
      Serial.print(" is not recognized: 0x");
      Serial.println(addr[i][0],HEX);
      temperature[i] = -1000;
      return;
    }

    // Initiate a conversion on the temp sensor  
    ds.reset();
    ds.select(addr[i]);
    ds.write(0x44,1);

    // Wait for the conversion to complete
    delay(850);

    // Read from the scratchpad
    ds.reset();
    ds.select(addr[i]);
    ds.write(0xBE);

    for (int j = 0; j < 9; j++)
    {
      data[j] = ds.read();
      // Serial.print(data[j], HEX);
      // Serial.print(" ");
    }

    // Serial.println("");

    // calculate temp
    temperature[i] = (int) round(((((data[1] << 8) + data[0] )*0.0625)*9/5) + 32);

    Serial.print("Temp sensor ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(temperature[i]);
  }

  return;
}
