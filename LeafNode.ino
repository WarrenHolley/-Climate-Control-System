/*  Climate Control System  - Power Control Leaf Node.
    Author   Warren Holley
    Version  0.2.0
    Date     October 27,2017

    Purpose
      This component is external 'leaf' node for power control.
      Listens for a signal from the Hub node, and activates/deactivates power accordingly.
      Currently only runs in Simplex communication, though plan for duplex comms with a handshake protocol.

    Notes
      Requires the 'Radiohead' wireless comms library.
      Works with standard cheap 'RF' modules. (ASK modules, ~$2-$5/e)
*/

// DEFINITIONS:
#define UserDebugMode false //Pushes human-readable text to the Serial Monitor.

// Pins & Voltage settings.
#define VRef 5.0
#define ReceiverPin 11 //Library Default=11
#define PowerPin 7

// Sets the ID of this device. Changes the signals it will respond to.
//  Default: 1-Humidifier, 2-Heater, 3-Fan
#define NodeID 3

// Included Modules. 'RadioHead' library required.
#include <RH_ASK.h>
#include <SPI.h> // Not actualy used but needed to compile

// Generic Initializers
RH_ASK Receiver;
boolean       PowerIsOn;
unsigned long TimeStart;
int           OnPeriod;



// Checks timing and turns on power if the time limit is up.
//   Time-inefficient as hell. Recommend upgrading to an external time-controlled switch or pulse generator.
void checkTime()
{
  if ( millis() - TimeStart > OnPeriod )
  {
    digitalWrite(PowerPin, LOW);//Turn off relay
    PowerIsOn = false;
  }
}

// Turns the power on to the relay.
//  Argument is the time, in seconds, until shutdown
void powerOn( uint8_t onTime)
{
  TimeStart = millis();
  PowerIsOn = true;
  OnPeriod = onTime * 1000;
  digitalWrite(PowerPin, HIGH);

  if (UserDebugMode)  {
    Serial.print("Turning Power On For");
    Serial.println(onTime);
  }
}

// Turns the power off to the relay.
void powerOff( uint8_t offTime )
{
  if (offTime == 0 && PowerIsOn) //Immediately deactivate power.
  {
    if (UserDebugMode)
      Serial.println("Turning Power Off Now");
    digitalWrite(PowerPin, LOW); //Turn off relay.
    PowerIsOn = false;
  }
  else if (PowerIsOn) // If the system is still running, then can just reset the on-time to the new length.
  {
    TimeStart = millis();
    OnPeriod = offTime * 1000;

    if (UserDebugMode) {
      Serial.print("Turning Power Off in ");
      Serial.println(offTime);
    }
  }
  //Otherwise, power is off, and can just ignore.
}

void setup()
{
  if (!UserDebugMode)
    Receiver.init();
  else
  {
    Serial.begin(9600);  // For the purposes of debugging.
    if (!Receiver.init())
      Serial.println("init failed");
    Serial.println("Serial Comms Activated");
    Serial.println("Setup Complete");
  }

  pinMode(PowerPin, OUTPUT);
}

void loop()
{
  //Reset buffer, as the library demands.
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  //Blocking. Waits for signal, continues if one arrives or time runs out
  Receiver.waitAvailableTimeout(500);
  if (Receiver.recv(buf, &buflen)) //If message is good, continue.
  {
    if (NodeID == buf[0])
    {
      if ((boolean) buf[1] == true) //Not reduced for clairity's sake.
        powerOn(buf[2]);
      else
        powerOff(buf[2]);
    }
  }
  else if (PowerIsOn)
    checkTime(); //Every half second, check for timeout of actiavtion signal
}
