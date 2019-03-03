#include "EspNowSensor.h"
#include "espnow_hal.h"
#define BLUE_LED 2
uint8_t gateway[6] = { 0x5E, 0xCF, 0x7F, 0x80, 0x34, 0x75 };

void connectEventHandler () {
    Serial.println ("Connected");
}

void disconnectEventHandler () {
    Serial.println ("Disconnected");
}

void setup () {
    Serial.begin (115200); Serial.println (); Serial.println ();

    EspNowSensor.setLed (BLUE_LED);
    EspNowSensor.onConnected (connectEventHandler);
    EspNowSensor.onDisconnected (disconnectEventHandler);
    EspNowSensor.begin (&Espnow_hal, gateway);

    char *message = "Hello World!!!";

    Serial.printf ("Trying to send: %s\n", message);
    EspNowSensor.sendData ((uint8_t *)message, strlen (message));

    EspNowSensor.sleep (5000000);
}

void loop () {

    EspNowSensor.handle ();

}