// 
// 
// 

#include "EspNowSensor.h"

void EspNowSensorClass::setLed (uint8_t led, time_t onTime) {
    this->led = led;
    ledOnTime = onTime;
}

void EspNowSensorClass::begin (Comms_halClass *comm) {
    pinMode (led, OUTPUT);
    digitalWrite (led, HIGH);

    initWiFi ();
    this->comm = comm;
    comm->begin (gateway,channel);
    comm->onDataRcvd (rx_cb);
    comm->onDataSent (tx_cb);

    //Crypto.getDH1 ();
    //node.setStatus (INIT);
    //uint8_t macAddress[6];
    //if (wifi_get_macaddr (0, macAddress)) {
    //    node.setMacAddress (macAddress);
    //}
    clientHello (/*Crypto.getPubDHKey ()*/);

}

void EspNowSensorClass::handle () {
#define LED_PERIOD 100
    static unsigned long blueOntime;

    if (led >= 0) {
        if (flashBlue) {
            blueOntime = millis ();
            digitalWrite (led, LOW);
            flashBlue = false;
        }

        if (!digitalRead (led) && millis () - blueOntime > ledOnTime) {
            digitalWrite (led, HIGH);
        }
    }

}

void EspNowSensorClass::rx_cb (u8 *mac_addr, u8 *data, u8 len) {
    EspNowSensor.manageMessage (mac_addr, data, len);
}

void EspNowSensorClass::tx_cb (u8 *mac_addr, u8 status) {
    EspNowSensor.getStatus (mac_addr, status);
}

bool EspNowSensorClass::checkCRC (const uint8_t *buf, size_t count, uint32_t *crc) {
    uint32 recvdCRC;

    memcpy (&recvdCRC, crc, sizeof (uint32_t));
    //DEBUG_VERBOSE ("Received CRC32: 0x%08X", *crc);
    uint32_t _crc = CRC32::calculate (buf, count);
    DEBUG_VERBOSE ("CRC32 =  Calc: 0x%08X Recvd: 0x%08X Length: %d", _crc, recvdCRC, count);
    return (_crc == recvdCRC);
}

bool EspNowSensorClass::clientHello () {
    /*
    * -------------------------------------------------------
    *| msgType (1) | random (16) | DH Kmaster (32) | CRC (4) |
    * -------------------------------------------------------
    */
    uint8_t msgType_idx =     0;
    uint8_t iv_idx =          1;
    uint8_t publicKey_idx =   iv_idx + IV_LENGTH;
    uint8_t crc_idx =         publicKey_idx + KEY_LENGTH;

#define CHMSG_LEN (1 + IV_LENGTH + KEY_LENGTH + CRC_LENGTH)

    uint8_t buffer[CHMSG_LEN];
    uint32_t crc32;

    Crypto.getDH1 ();
    node.setStatus (INIT);
    uint8_t macAddress[6];
    if (wifi_get_macaddr (0, macAddress)) {
        node.setMacAddress (macAddress);
    }
    uint8_t *key = Crypto.getPubDHKey ();

    if (!key) {
        return false;
    }

    buffer[msgType_idx] = CLIENT_HELLO; // Client hello message

    CryptModule::random (&buffer[iv_idx], IV_LENGTH);

    DEBUG_VERBOSE ("IV: %s", printHexBuffer (&buffer[iv_idx], IV_LENGTH));

    for (int i = 0; i < KEY_LENGTH; i++) {
        buffer[publicKey_idx + i] = key[i];
    }

    crc32 = CRC32::calculate (buffer, CHMSG_LEN - CRC_LENGTH);
    DEBUG_VERBOSE ("CRC32 = 0x%08X", crc32);

    memcpy (&buffer[crc_idx], &crc32, CRC_LENGTH);

    DEBUG_VERBOSE ("Client Hello message: %s", printHexBuffer (buffer, CHMSG_LEN));

    node.setStatus (WAIT_FOR_SERVER_HELLO);

    DEBUG_INFO (" -------> CLIENT HELLO");

    return comm->send (gateway, buffer, CHMSG_LEN) == 0;
}


bool EspNowSensorClass::processServerHello (const uint8_t mac[6], const uint8_t* buf, size_t count) {
    /*
    * ------------------------------------------------------
    *| msgType (1) | random (16) | DH Kslave (32) | CRC (4) |
    * ------------------------------------------------------
    */
    uint8_t msgType_idx = 0;
    uint8_t iv_idx = 1;
    uint8_t pubKey_idx = iv_idx + IV_LENGTH;
    uint8_t crc_idx = pubKey_idx + KEY_LENGTH;

#define SHMSG_LEN (1 + IV_LENGTH + KEY_LENGTH + CRC_LENGTH)

    uint8_t myPublicKey[KEY_LENGTH];
    uint32_t crc;
    //uint8_t key[KEY_LENGTH];

    if (count < SHMSG_LEN) {
        DEBUG_WARN ("Message too short");
        return false;
    }

    memcpy (&crc, &buf[crc_idx], CRC_LENGTH);

    if (!checkCRC (buf, count - CRC_LENGTH, &crc)) {
        DEBUG_WARN ("Wrong CRC");
        return false;
    }

    Crypto.getDH2 (&buf[pubKey_idx]);
    node.setEncryptionKey (&buf[pubKey_idx]);
    DEBUG_INFO ("Node key: %s", printHexBuffer (node.getEncriptionKey (), KEY_LENGTH));

    return true;
}

bool EspNowSensorClass::processCipherFinished (const uint8_t mac[6], const uint8_t* buf, size_t count) {
    /*
    * -----------------------------------------------------------
    *| msgType (1) | IV (16) | nodeId (2) | random (4) | CRC (4) |
    * -----------------------------------------------------------
    */
    uint8_t msgType_idx = 0;
    uint8_t iv_idx = 1;
    uint8_t nodeId_idx = iv_idx + IV_LENGTH;
    uint8_t nonce_idx = nodeId_idx + sizeof (int16_t);
    uint8_t crc_idx = nonce_idx + RANDOM_LENGTH;

#define CFMSG_LEN (1 + IV_LENGTH + sizeof(uint16_t) + RANDOM_LENGTH + CRC_LENGTH)

    uint16_t nodeId;
    //uint8_t *iv;
    uint32_t crc;

    if (count < CFMSG_LEN) {
        DEBUG_WARN ("Wrong message length --> Required: %d Received: %d", CFMSG_LEN, count);
        return false;
    }

    //iv = (uint8_t *)(buf + 1);
    //uint8_t *crypt_buf = (uint8_t *)(buf + 1 + IV_LENGTH);
    Crypto.decryptBuffer (
        const_cast<uint8_t *>(&buf[nodeId_idx]),
        &buf[nodeId_idx],
        CFMSG_LEN - IV_LENGTH - 1,
        &buf[iv_idx],
        IV_LENGTH,
        node.getEncriptionKey (),
        KEY_LENGTH
    );
    DEBUG_VERBOSE ("Decripted Cipher Finished message: %s", printHexBuffer (const_cast<uint8_t *>(buf), CFMSG_LEN));

    memcpy (&crc, &buf[crc_idx], CRC_LENGTH);

    if (!checkCRC (buf, CFMSG_LEN - 4, &crc)) {
        DEBUG_WARN ("Wrong CRC");
        return false;
    }

    memcpy (&nodeId, &buf[nodeId_idx], sizeof (uint16_t));
    node.setNodeId (nodeId);
    DEBUG_VERBOSE ("Node ID: %u", node.getNodeId ());
    return true;
}

bool EspNowSensorClass::keyExchangeFinished () {
    /*
    * ----------------------------------------------
    *| msgType (1) | IV (16) | random (4) | CRC (4) |
    * ----------------------------------------------
    */

    uint8_t msgType_idx = 0;
    uint8_t iv_idx = 1;
    uint8_t nonce_idx = iv_idx + IV_LENGTH;
    uint8_t crc_idx = nonce_idx + RANDOM_LENGTH;

#define KEFMSG_LEN (1 + IV_LENGTH + RANDOM_LENGTH + CRC_LENGTH)

    uint8_t buffer[KEFMSG_LEN];
    uint32_t crc32;
    uint32_t nonce;
    uint8_t *iv;

    //memset (buffer, 0, KEFMSG_LEN);

    buffer[msgType_idx] = KEY_EXCHANGE_FINISHED;

    iv = Crypto.random (&buffer[iv_idx], IV_LENGTH);
    DEBUG_VERBOSE ("IV: %s", printHexBuffer (&buffer[iv_idx], IV_LENGTH));

    nonce = Crypto.random ();

    memcpy (&buffer[nonce_idx], &nonce, RANDOM_LENGTH);

    crc32 = CRC32::calculate (buffer, KEFMSG_LEN - CRC_LENGTH);
    DEBUG_VERBOSE ("CRC32 = 0x%08X", crc32);

    //uint8_t *crcField = (uint8_t*)(buffer + 1 + IV_LENGTH + RANDOM_LENGTH);
    memcpy (&buffer[crc_idx], &crc32, CRC_LENGTH);

    DEBUG_VERBOSE ("Key Exchange Finished message: %s", printHexBuffer (buffer, KEFMSG_LEN));

    //uint8_t *crypt_buf = buffer + 1 + IV_LENGTH;

    Crypto.encryptBuffer (&buffer[nonce_idx], &buffer[nonce_idx], KEFMSG_LEN - IV_LENGTH - 1, &buffer[iv_idx], IV_LENGTH, node.getEncriptionKey (), KEY_LENGTH);

    DEBUG_VERBOSE ("Encripted Key Exchange Finished message: %s", printHexBuffer (buffer, KEFMSG_LEN));
    DEBUG_INFO (" -------> KEY_EXCHANGE_FINISHED");
    return comm->send (gateway, buffer, KEFMSG_LEN) == 0;
}

bool EspNowSensorClass::sendData (const uint8_t *data, size_t len) {
    if (node.getStatus () == REGISTERED && node.isKeyValid ()) {
        DEBUG_INFO ("Data sent: %s", printHexBuffer (data, len));
        dataMessage ((uint8_t *)data, len);
    }
    flashBlue = true;
}

bool EspNowSensorClass::dataMessage (const uint8_t *data, size_t len) {
    /*
    * --------------------------------------------------------------------------------------
    *| msgType (1) | IV (16) | length (2) | NodeId (2) | Random (4) | Data (....) | CRC (4) |
    * --------------------------------------------------------------------------------------
    */

    uint8_t buffer[200];
    uint32_t crc32;
    uint32_t nonce;
    uint16_t nodeId = node.getNodeId ();

    uint8_t *msgType_p = buffer;
    uint8_t *iv_p = buffer + 1;
    uint8_t *length_p = buffer + 1 + IV_LENGTH;
    uint8_t *nodeId_p = buffer + 1 + IV_LENGTH + sizeof (int16_t);
    uint8_t *nonce_p = buffer + 1 + IV_LENGTH + sizeof (int16_t) + sizeof (int16_t);
    uint8_t *data_p = buffer + 1 + IV_LENGTH + sizeof (int16_t) + sizeof (int16_t) + RANDOM_LENGTH;

    if (!data) {
        return false;
    }

    *msgType_p = (uint8_t)SENSOR_DATA;

    CryptModule::random (iv_p, IV_LENGTH);

    DEBUG_VERBOSE ("IV: %s", printHexBuffer (iv_p, IV_LENGTH));

    memcpy (nodeId_p, &nodeId, sizeof (uint16_t));

    nonce = Crypto.random ();

    memcpy (nonce_p, (uint8_t *)&nonce, RANDOM_LENGTH);

    memcpy (data_p, data, len);

    uint16_t packet_length = 1 + IV_LENGTH + sizeof (int16_t) + sizeof (int16_t) + RANDOM_LENGTH + len;

    memcpy (length_p, &packet_length, sizeof (uint16_t));

    crc32 = CRC32::calculate (buffer, packet_length);
    DEBUG_VERBOSE ("CRC32 = 0x%08X", crc32);

    // int is little indian mode on ESP platform
    uint8_t *crc_p = (uint8_t*)(buffer + packet_length);

    memcpy (crc_p, &crc32, CRC_LENGTH);

    DEBUG_VERBOSE ("Data message: %s", printHexBuffer (buffer, packet_length + CRC_LENGTH));

    uint8_t *crypt_buf = buffer + 1 + IV_LENGTH;

    size_t cryptLen = packet_length + CRC_LENGTH - 1 - IV_LENGTH;

    Crypto.encryptBuffer (crypt_buf, crypt_buf, cryptLen, iv_p, IV_LENGTH, node.getEncriptionKey (), KEY_LENGTH);

    DEBUG_VERBOSE ("Encrypted data message: %s", printHexBuffer (buffer, packet_length + CRC_LENGTH));

    DEBUG_INFO (" -------> DATA");

    return comm->send (gateway, buffer, packet_length + CRC_LENGTH) == 0;
}

bool EspNowSensorClass::processInvalidateKey (const uint8_t mac[6], const uint8_t* buf, size_t count) {
#define IKMSG_LEN 2
    if (buf && count < IKMSG_LEN) {
        return false;
    }
    DEBUG_VERBOSE ("Invalidate key request. Reason: %u", buf[1]);
    return true;
}

void EspNowSensorClass::manageMessage (const uint8_t *mac, const uint8_t* buf, uint8_t count) {
    DEBUG_INFO ("Reveived message. Origin MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    DEBUG_VERBOSE ("Received data: %s", printHexBuffer (const_cast<uint8_t *>(buf), count));
    flashBlue = true;

    if (count <= 1) {
        return;
    }

    switch (buf[0]) {
    case SERVER_HELLO:
        DEBUG_INFO (" <------- SERVER HELLO");
        if (node.getStatus () == WAIT_FOR_SERVER_HELLO) {
            if (processServerHello (mac, buf, count)) {
                keyExchangeFinished ();
                node.setStatus (WAIT_FOR_CIPHER_FINISHED);
            } else {
                node.reset ();
            }
        } else {
            node.reset ();
        }
        break;
    case CYPHER_FINISHED:
        DEBUG_INFO ("Cypher Finished received");
        if (node.getStatus () == WAIT_FOR_CIPHER_FINISHED) {
            DEBUG_INFO (" <------- CIPHER_FINISHED");
            if (processCipherFinished (mac, buf, count)) {
                // mark node as registered
                node.setKeyValid (true);
                node.setKeyValidFrom (millis ());
                node.setStatus (REGISTERED);
#if DEBUG_LEVEL >= INFO
                node.printToSerial (&DEBUG_ESP_PORT);
#endif
                // TODO: Store node data on EEPROM, SPIFFS or RTCMEM
            } else {
                node.reset ();
            }
        } else {
            node.reset ();
        }
        break;
    case INVALIDATE_KEY:
        DEBUG_INFO (" <------- INVALIDATE KEY");
        processInvalidateKey (mac, buf, count);
        clientHello ();
    }
}

void EspNowSensorClass::getStatus (u8 *mac_addr, u8 status) {
    DEBUG_VERBOSE ("SENDStatus %u", status);
}


EspNowSensorClass EspNowSensor;
