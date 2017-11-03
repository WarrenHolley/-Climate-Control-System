/*  Climate Control System  - Sensor Hub.
    Author   Warren Holley
    Version  0.2.0
    Date     October 29,2017

    Purpose
      This component is a central hub for sensing and communication.
      A simple climate control system for use in a small apartment or room, run by an Arduino system.
      Consists of a few small subsystems: Heating, Humidifying and a Fan for circulation.
      Designed for easy modification or expansion.
      Currently only runs in Simplex communication, though plan for duplex comms with a handshake-confirmation
        and off-site sensor slave hubs.

    Notes
      Requires the 'Radiohead' wireless comms library.
      Works with standard cheap 'RF' modules. (ASK modules, ~$2-$5/e)
*/


//DEFINITIONS:
#define UserDebugMode false //Pushes human-readable text to the Serial Monitor.

// Pin and Voltage settings
#define TemperaturePin 0  //Analog pin.
#define HumidityPin    1  //Analog Pin
#define TransmitPin    12 //Digital Pin. Library Default=12
#define VRef 5.0

//Atmosphere and Tolerances selected for book preservation/archival
#define TargetTemp     21 //*C
#define TargetHumidity 40 //%

//Tolerances
#define ToleranceTemp      1 //*C
#define ToleranceHumidity  5 //%

//Timing Values. Would recommend a PID system.
#define TimeBetween 10 //Time, in seconds, between testing conditions at the hub.

//Time, in seconds for each module to be activated for.
// System uses non-blocking power switching. These values are fail-safes.
// If comms or Hub fails, don't want powered items on permentantly.
#define TimeOnHeater     20
#define TimeOnHumidifier 15
#define TimeOnFan        30

//ID values of nodes. Must match IDs assigned to leaf nodes.
// May need to use higher/random values in case two systems within range of each other.
#define HubNode  0
#define HumNode  1
#define HeatNode 2
#define FanNode  3

//Included Modules. 'RadioHead' library required.
#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile

//Generic Initializers
RH_ASK Transmitter;


// Transmits the power instructions to the Leaf Nodes.
void powerNode(uint8_t ID, boolean powerOn, uint8_t len)
{ //Transmit Format - In Bytes uint8_t
  // 1 - ID.
  // 2 - On/Off.
  // 3 - Active time if turning on, time until shutdown if turning off.

  uint8_t transmissionPulse[3];
  transmissionPulse[0] = ID;
  transmissionPulse[1] = (uint8_t) powerOn;
  transmissionPulse[2] = len;

  Transmitter.send(transmissionPulse, 3);
  Transmitter.waitPacketSent();
  delay(100); //Allow for time between transmissions to limit packet loss.
}

// Returns float of temperature in *C
//  Designed for TMP36-GZ. Accepts 2.7-5.5V.
//  Voltage output linear to the atmospheric temperature.
float getTemp()
{
  float readVal = analogRead(TemperaturePin);
  float temp = readVal * VRef / 1024 - 0.5;
  temp *= 100;
  return temp;
}

// Returns float of True Humidity in %.
//  Designed for HIH-4030. Accepts 4-5.8V.
float getHumidity()
{
  // Vo  = Vin*(0.0062*Rh+0.16)       // Rh: Relative Humidity in %
  // Rh  = Vo/(0.0062*Vin) - 25.81
  // Th = Rh / (1.0546 - 0.00216*Tc)  // Th: True Humidity in %
  //                                     Tc: Temp in 'C
  float Vo = (float)analogRead(HumidityPin) * VRef / (float)1024;
  float Rh = -25.81 + Vo / (0.0062 * VRef);
  float Th = Rh / (1.0546 - 0.00216 * getTemp());
  return Rh;
}



void setup()
{
  if (UserDebugMode)
  {
    Serial.begin(9600);	  // For User/GUI communications
    Serial.println("Serial Comms Activated");
    Serial.println("Setup Complete");
  }
  if (!UserDebugMode)
    Transmitter.init(); // Skips following error check.
  else if (Transmitter.init() == 0) //Unlikely, as ASK components are linear components, and cannot throw errors back to the system.
    Serial.println("Wireless Initialization Failed"); // Only failure is if the driver/library is incompatible with the system this is being run on.
}


void loop()
{
  boolean HeatingOn = false;
  boolean HeatingOff = false;
  boolean HumidifierOn = false;
  boolean HumidifierOff = false;

  float temp = getTemp();
  float humidity = getHumidity();

  //Deactivate if exceeds limit
  //Active if below target.
  if (temp > TargetTemp + ToleranceTemp)
    HeatingOff = true;
  else if (temp < TargetTemp - ToleranceTemp)
    HeatingOn = true;

  if (humidity > TargetHumidity + ToleranceHumidity)
    HumidifierOff = true;
  else if (humidity < TargetHumidity - ToleranceHumidity)
    HumidifierOn = true;

  //User Output of Current Conditions.
  if (UserDebugMode)
  {
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.print("*C\nHumidity: ");
    Serial.print(humidity);
    Serial.println("%");
  }

  // Transmit Activate/Deactivate systems to Leaf Nodes
  if (HumidifierOn)
    powerNode(HumNode, true, TimeOnHumidifier);
  else if (HumidifierOff)
    powerNode(HumNode, false, 0);

  if (HeatingOn)
    powerNode(HeatNode, true, TimeOnHeater);
  else if (HeatingOff)
    powerNode(HeatNode, false, 0);

  if (HeatingOn || HumidifierOn)
    powerNode(FanNode, true, TimeOnFan);
  // Ignoring turning the fan off, as the fan should run a bit longer than the other systems to allow for circulation.
  // As the hub is currently time-memoryless, does not know if non-fan systems are currently on for this cycle.
  // Auto-shutdown (TimeOnFan) is left to handle it.

  
  if (UserDebugMode)  { //Let user know what is happening this cycle
    if (HeatingOn)
      Serial.println("Activating Heater");
    if (HeatingOff)
      Serial.println("Dectivating Heater");
    if (HumidifierOn)
      Serial.println("Activating Humidifier");
    if (HumidifierOff)
      Serial.println("Deactivating Humidifier");
    if (HumidifierOn || HeatingOn)
      Serial.println("Activating Fan");
    Serial.println(); //Leave extra line for readability.
  }

  // Sleep until next cycle. Inaccurate due to processing and transmission delays in each cycle.
  //  For higher time-accuracy, could use either interrupt through an external signal,
  //  or use frequent time-checking with millis(), though this is far more power-hungry.
  delay(TimeBetween * 1000);
}


