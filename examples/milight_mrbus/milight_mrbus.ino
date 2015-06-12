/*************************************************************************
Title:    Milight / LimitlessLED LED Strip and MRBus Integration Example
Authors:  Nathan Holmes <maverick@drgw.net>, Colorado, USA
          Michael Petersen <railfan@drgw.net>, Colorado, USA
File:     Milight.cpp
License:  GNU General Public License v3
*************************************************************************/

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <MRBusArduino.h>
#include <MRBusClock.h>
#include <EEPROM.h>
#include <Milight.h>

// Global variables and objects
MRBus mrbus;
MRBusClock mrbusclock;
EthernetClient client;
EthernetUDP udp;
Milight ml;

unsigned long nextAdvance = 0;
unsigned long lastUpdateTime, lastPrintTime, lastTransmitTime, currentTime;
unsigned long updateInterval = 2000, printInterval = 500;


/*************************************************************************
This example assumes that you have:
1) a Milight / LimitlessLED Wifi gateway
2) the gateway is paired with an RGBW Milight LED strip driver
3) the gateway is connected to your wireless network
4) the wireless network is bridged to your wired network
5) and that the wired network is connected to a standard Ethernet Shield
    attached to your Arduino
6) a MRBus network attached via an Iowa Scaled Engineering MRB-ARD shield
7) an Iowa Scaled Engineering Fast Clock Master or similar MRBus-compatible
    fast clock
    
Things you'll need to configure:
1) The MAC address to be used for the Ethernet Shield (the default will work
    unless you have multiple shields using the default from the library
    example.)
2) The IP address of your milight gateway
3) The port that your milight gateway speaks on
*************************************************************************/

// CONFIGURE: MAC for the ethernet shield (change if needed)
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

// CONFIGURE: IP address of your Milight/LimitlessLED Wifi gateway
IPAddress milight_ip(192, 168, 7, 135);

// CONFIGURE: Port the gateway speaks on - default is 8899, some older units 
//  are on 50000
const unsigned int milight_port = 8899;

void setup() 
{
  uint8_t address = EEPROM.read(MRBUS_EE_DEVICE_ADDR);
  Serial.begin(9600);
  while(!Serial); // Waits for Leonardos to get the USB interface up and running

  mrbus.begin();
  mrbusclock.begin();
  
  if (0xFF == address)
  {
    EEPROM.write(MRBUS_EE_DEVICE_ADDR, 0x03);
    address = EEPROM.read(MRBUS_EE_DEVICE_ADDR);
  }

  mrbus.setNodeAddress(address);

  // Set 10 second (100 decisec) timeout on our clock object
  mrbusclock.setTimeout(100); 
  // Set the time source to the broadcast address so we'll listen to anything
  mrbusclock.setTimeSourceAddress(0xFF); 

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  // print your local IP address:
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print("."); 
  }
  Serial.println();
  udp.begin(8899);
  ml.begin(&udp, &milight_ip, 8899);
  nextAdvance = millis();
  
}

void pktActions(MRBusPacket& mrbPkt)
{
  // Is it broadcast or for us?  Otherwise ignore
  if (0xFF != mrbPkt.pkt[MRBUS_PKT_DEST] && mrbus.getNodeAddress() != mrbPkt.pkt[MRBUS_PKT_DEST])
    return;

  if (false == mrbus.doCommonPacketHandlers(mrbPkt))
  {
    if ('T' == mrbPkt.pkt[MRBUS_PKT_TYPE])
       mrbusclock.processTimePacket(mrbPkt);
  }
}

void printMRBusTime(MRBusTime time)
{
  char buffer[12];
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "%02d:%02d:%02d", time.hours, time.minutes, time.seconds);
  Serial.print(buffer);
}

void loop() 
{
  ml.workQueue();

  currentTime = millis();

  // Only print the time every 0.5s or so
  // This only works if you're on a Leonardo - if you're on an Uno or other 328-based Arduino,
  // this will step on MRBus.  That's what the if defined block is here to do
  if (currentTime - lastPrintTime >= printInterval)
  {
    lastPrintTime = currentTime;
    Serial.print("The time is...");
    
    if (mrbusclock.isTimedOut())
    {
      Serial.print("Timed out");
    }
    else if (mrbusclock.isOnFastTime()) 
    {
      MRBusTime currentFastTime;
      mrbusclock.getFastTime(currentFastTime);      
      Serial.print("FAST [");
      printMRBusTime(currentFastTime);      
      Serial.print("]");
    }
    else if (mrbusclock.isOnHold()) 
    {
      Serial.print("ON HOLD");
    }
    else if (mrbusclock.isOnRealTime()) 
    {
      MRBusTime currentRealTime;
      
      mrbusclock.getRealTime(currentRealTime);      
      Serial.print("REAL [");
      printMRBusTime(currentRealTime);      
      Serial.print("]");
    } else {
      Serial.print(" mysteriously broken...");
    }
    Serial.print("\n");
  }
  
  // If we have packets, try parsing them
  if (mrbus.hasRxPackets())
  {
    MRBusPacket mrbPkt;
    mrbus.getReceivedPacket(mrbPkt);
    pktActions(mrbPkt);
  }

  // If there are packets to transmit and it's been more than 20mS since our last transmission attempt, try again
  if (mrbus.hasTxPackets() && ((lastTransmitTime - currentTime) > 20))
  {
     lastTransmitTime = currentTime;
     mrbus.transmit();
  }  

  // Really, seriously, no point in checking more than every second or two, otherwise we'll just saturate the
  // the very slow Milight send queue
  
  if (currentTime - lastUpdateTime >= updateInterval)
  {
    // isOnFastTime() checks that the clock is running in fast time mode
    // isTimedOut() checks if we've timed out since we've received our last clock communication - default is 5 seconds
    if (mrbusclock.isOnFastTime() && !mrbusclock.isTimedOut())
    {
      // Retrieve the current fast time
      MRBusTime currentFastTime;
      mrbusclock.getFastTime(currentFastTime);

		// What follows is a very crude example of how you might code the different parts of a fast "day" to project different colors
		// and intensities of light onto the layout

      if (currentFastTime > MRBusTime(0, 0) && currentFastTime < MRBusTime(5, 0))
      {
        ml.on(1, 0x03, 50); // From midnight to 5 am, make the lights blue at 50%
      }
      else if (currentFastTime > MRBusTime(5, 0) && currentFastTime < MRBusTime(7, 0))
      {
        // From 5-6AM, start trending brighter and towards cyan
        uint8_t intensity = 50 + ((uint16_t)50 * ((uint16_t)(currentFastTime.hours - 5) * 60 + (uint16_t)currentFastTime.minutes)) / ( 2 * 60);
        ml.on(1, 0x03, intensity);
      }
      else if (currentFastTime > MRBusTime(7, 0) && currentFastTime < MRBusTime(13, 0))
      {
        // From 7AM to 1PM, white increasing from 70 to 100%
        uint8_t intensity = 70 + ((uint16_t)30 * ((uint16_t)(currentFastTime.hours - 7) * 60 + (uint16_t)currentFastTime.minutes)) / (6 * 60);
        ml.on(1, 0, intensity);
      }
      else if (currentFastTime > MRBusTime(13, 0) && currentFastTime < MRBusTime(19, 0))
      {
        // From 1PM to 7PM, white decreasing from 100 to 60%
        uint8_t intensity = 100 - ((uint16_t)60 * ((uint16_t)(currentFastTime.hours - 13) * 60 + (uint16_t)currentFastTime.minutes)) / (6 * 60);
        ml.on(1, 0, intensity);
      }
      else if (currentFastTime > MRBusTime(19, 0) && currentFastTime < MRBusTime(20, 0))
      {
        // From 7PM to 8PM, switch to red and fade down to 50%
        uint8_t intensity = 75 - ((uint16_t)25 * ((uint16_t)(currentFastTime.hours - 13) * 60 + (uint16_t)currentFastTime.minutes)) / (7 * 60);
        ml.on(1, 0x9C, intensity);
      }
      else if (currentFastTime > MRBusTime(20, 0) && currentFastTime < MRBusTime(23, 59))
      {
        ml.on(1, 0x03, 50); // From midnight to 5 am, make the lights blue at 50%
      }
    }
    else if (mrbusclock.isOnHold())
    {
      // If we're on hold, make the lights red.  RED ALERT!
      ml.on(1, 0xAD, 100);
    }
    else
    
    {
      // We're either on real time, or timed out.  Turn the lights on now.

      ml.off(1);
    }
    
    lastUpdateTime = currentTime;
  }

 
}


