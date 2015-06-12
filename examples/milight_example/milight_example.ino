/*************************************************************************
Title:    Milight / LimitlessLED LED Strip Network Library Example
Authors:  Nathan Holmes <maverick@drgw.net>, Colorado, USA
          Michael Petersen <railfan@drgw.net>, Colorado, USA
File:     Milight.cpp
License:  GNU General Public License v3
*************************************************************************/

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Milight.h>

/*************************************************************************
This example assumes that you have:
1) a Milight / LimitlessLED Wifi gateway
2) the gateway is paired with an RGBW Milight LED strip driver
3) the gateway is connected to your wireless network
4) the wireless network is bridged to your wired network
5) and that the wired network is connected to a standard Ethernet Shield
    attached to your Arduino

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

EthernetClient client;
EthernetUDP udp;
Milight ml;

// Time reference and state machine for controlling LED transitions
unsigned long nextAdvance = 0;
uint8_t lighting_state;

void setup() 
{
	// Open serial communications and wait for port to open:
	Serial.begin(9600);
	// Wait for the console to connect - this check is only needed on the Leonardo
	while (!Serial);

	// start the Ethernet connection:
	if (Ethernet.begin(mac) == 0) 
	{
		Serial.println("Failed to configure Ethernet using DHCP");
		// no point in carrying on, so do nothing forevermore:
		while(1);
	}

	// print your local IP address:
	Serial.print("My IP address: ");
	for (byte thisByte = 0; thisByte < 4; thisByte++) 
	{
		// print the value of each byte of the IP address:
		Serial.print(Ethernet.localIP()[thisByte], DEC);
		Serial.print("."); 
	}
	Serial.println();
	// Initialize the EthernetUDP class with port 8899 - not really used for anything
	// but we need it up and running for sending UDP packets
	udp.begin(8899);


	// Initialize Milight / LimitlessLED controller with the chosen IP and port, as well as
	// access to the UDP object to actually send packets with
	ml.begin(&udp, &milight_ip, 8899);

	// Set nextAdvance to the current time
	nextAdvance = millis();
}

void loop() 
{
	// Milight.workQueue() must be called as often as possible
	// Its job is to send packets on prescribed time intervals without
	// stalling the main loop.  Thus, your main loop() function should be free-running
	// without any long stalls.
	ml.workQueue();

	// If it's not time to advance the state machine, just keep running around the loop
	if (millis() < nextAdvance)
		return;

	// This is the state machine that shows off what the library can do
	// Its job is to check if a certain amount of time has passed, and then change the light
	// configuration
	switch(lighting_state)
	{
		case 0:
			Serial.print("Lighting off\n");
			ml.off(MILIGHT_CH_1);
			nextAdvance = millis() + 4000;
			lighting_state = 1;
			break;

		case 1:    
			Serial.print("Lighting white at 100%\n");
			ml.on(MILIGHT_CH_1, MILIGHT_COLOR_WHITE, 100);
			nextAdvance = millis() + 3000;
			lighting_state = 2;
			break;

		case 2:    
			Serial.print("Lighting white at 75%\n");
			ml.on(MILIGHT_CH_1, MILIGHT_COLOR_WHITE, 75);
			nextAdvance = millis() + 3000;
			lighting_state = 3;
			break;

		case 3:    
			Serial.print("Lighting white at 25%\n");
			ml.on(MILIGHT_CH_1, MILIGHT_COLOR_WHITE, 25);
			nextAdvance = millis() + 3000;
			lighting_state = 4;
			break;

		case 4:    
			Serial.print("Lighting blue at 60%\n");
			ml.on(MILIGHT_CH_1, MILIGHT_COLOR_BLUE, 60);
			nextAdvance = millis() + 3000;
			lighting_state = 5;
			break;

		case 5:    
			Serial.print("Lighting green at 60%\n");
			ml.on(MILIGHT_CH_1, MILIGHT_COLOR_GREEN, 60);
			nextAdvance = millis() + 3000;
			lighting_state = 6;
			break;

		case 6:    
			Serial.print("Lighting red at 60%\n");
			ml.on(MILIGHT_CH_1, MILIGHT_COLOR_RED, 60);
			nextAdvance = millis() + 3000;
			lighting_state = 0;
			break;
	}

}


