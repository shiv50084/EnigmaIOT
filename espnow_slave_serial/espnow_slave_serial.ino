#include <WifiEspNow.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include "lib/cryptModule.h"
#include "lib/helperFunctions.h"
#include <CRC32.h>

#define BLUE_LED 2
#define RED_LED 16
bool flashRed = false;
bool flashBlue = false;

enum status_t {
	INIT,
	WAIT_FOR_SERVER_HELLO,
	WAIT_FOR_CIPHER_FINISHED,
	WAIT_FOR_KEY_EXCH_FINISHED,
	WAIT_FOR_DOWNLINK,
	WAIT_FOR_SENSOR_DATA,
	SLEEP
};

enum messageType_t {
	SENSOR_DATA = 0x01,
	CLIENT_HELLO = 0xFF,
	SERVER_HELLO = 0xFE,
	KEY_EXCHANGE_FINISHED = 0xFD,
	CYPHER_FINISHED = 0xFC,
	INVALIDATE_KEY = 0xFB
};

struct node_instance {
	uint8_t mac[6];
	uint16_t nodeId;
	uint8_t key[32];
	time_t lastMessage;
	bool keyInvalid;
};

typedef struct node_instance node_t;

node_t node;

bool serverHello (byte *key) {
	byte buffer[KEY_LENGTH + 5];
	uint32_t crc32;

	if (!key) {
		return false;
	}

	buffer[0] = SERVER_HELLO; // Server hello message

	for (int i = 0; i < KEY_LENGTH; i++) {
		buffer[i + 1] = key[i];
	}

	crc32 = CRC32::calculate (buffer, KEY_LENGTH + 1);
	Serial.printf ("%s CRC32 = 0x%08X\n", __FUNCTION__, crc32);

	// int is little indian mode on ESP platform
	uint32_t *crcField = (uint32_t*)&(buffer[KEY_LENGTH + 1]);

	*crcField = crc32;
	Serial.printf ("%u %s Server Hello message: ", millis(), __FUNCTION__);
	printHexBuffer (buffer, KEY_LENGTH + 5);
	flashRed = true;
	return WifiEspNow.send (node.mac, buffer, KEY_LENGTH + 5);
}

bool checkCRC (const uint8_t *buf, size_t count, uint32_t *crc) {
	uint32_t _crc = CRC32::calculate (buf, count);
	Serial.printf ("CRC32 =  Calc: 0x%08X Recvd: 0x%08X\n", _crc, *crc);
	return (_crc == *crc);
}

bool processClientHello (const uint8_t mac[6], const uint8_t* buf, size_t count) {
	uint8_t myPublicKey[KEY_LENGTH];

	if (!checkCRC (buf, count - 4, (uint32_t*)(buf + count - 4))) {
		Serial.printf ("%s Wrong CRC", __FUNCTION__);
		return false;
	}
		
	memcpy (node.mac, mac, 6);
	memcpy (node.key, &(buf[1]), KEY_LENGTH);
	Crypto.getDH1 ();
	memcpy (myPublicKey, Crypto.getPubDHKey (), KEY_LENGTH);
	Crypto.getDH2 (node.key);
	Serial.printf ("[%s] Node key: ", __FUNCTION__);
	printHexBuffer (node.key, KEY_LENGTH);

	return serverHello (myPublicKey);
}

void manageMessage (const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg) {
	Serial.printf ("%u: Reveived message. Origin MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", millis(), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
	Serial.print ("Received data: ");
	printHexBuffer ((byte *)buf, count);
	Serial.print ("Received CRC: ");
	printHexBuffer ((byte *)(buf+count-4), 4);
	flashBlue = true;
	if (count <= 1) {
		return;
	}

	switch (buf[0]) {
	case CLIENT_HELLO:
		Serial.println ("Recibido Client Hello");
		WifiEspNow.addPeer (mac);
		processClientHello (mac, buf, count);
	}
}

void initEspNow () {
	bool ok = WifiEspNow.begin ();
	if (!ok) {
		Serial.println ("WifiEspNow.begin() failed");
		ESP.restart ();
	}
	WifiEspNow.onReceive (manageMessage, NULL);
}

void setup () {
	//***INICIALIZACIÓN DEL PUERTO SERIE***//
	Serial.begin (115200); Serial.println (); Serial.println ();

	pinMode (BLUE_LED, OUTPUT);
	digitalWrite (BLUE_LED, HIGH);
	pinMode (RED_LED, OUTPUT);
	digitalWrite (RED_LED, HIGH);

	initWiFi ();
	initEspNow ();

	//Crypto.getDH1 ();

	//clientHello (Crypto.getPubDHKey ());
}

void loop () {
#define LED_PERIOD 100
	static unsigned long blueOntime;
	static unsigned long redOntime;

	if (flashBlue) {
		blueOntime = millis ();
		digitalWrite (BLUE_LED, LOW);
		flashBlue = false;
	}

	if (!digitalRead(BLUE_LED) && millis () - blueOntime > LED_PERIOD) {
		digitalWrite (BLUE_LED, HIGH);
	}

	if (flashRed) {
		redOntime = millis ();
		digitalWrite (RED_LED, LOW);
		flashRed = false;
	}

	if (!digitalRead (RED_LED) && millis () - redOntime > LED_PERIOD) {
		digitalWrite (RED_LED, HIGH);
	}
}