/**
  * @file EnigmaIoTconfig.h
  * @version 0.2.0
  * @date 28/06/2019
  * @author German Martin
  * @brief Parameter configuration
  */

#ifndef _CONFIG_h
#define _CONFIG_h

// Global configuration. Physical layer settings
#define MAX_MESSAGE_LENGTH 250 ///< @brief Maximum payload size on ESP-NOW
static const char ENIGMAIOT_PROT_VERS[] = "0.2.0"; ///< @brief EnitmaIoT Version
static const uint8_t DEFAULT_CHANNEL = 3; ///< @brief WiFi channel to be used on ESP-NOW

// Gateway configuration
#define MAX_KEY_VALIDITY 86400000 ///< @brief After this time (in ms) a nude is unregistered.
#define MAX_NODE_INACTIVITY 86400000U ///< @brief After this time (in ms) a node is marked as gone

// Sensor configuration
static const uint16_t RECONNECTION_PERIOD = 2500; ///< @brief Time to retry Gateway connection
static const uint16_t DOWNLINK_WAIT_TIME = 400; ///< @brief Time to wait for downlink message before sleep
static const uint32_t DEFAULT_SLEEP_TIME = 10; ///< @brief Default sleep time if it was not set
static const uint32_t OTA_TIMEOUT_TIME = 10000; ///< @brief Timeout between OTA messages. In milliseconds
static const time_t IDENTIFY_TIMEOUT = 10000;

//Crypro configuration
const uint8_t KEY_LENGTH = 32; ///< @brief Key length used by selected crypto algorythm. The only tested value is 32. Change it only if you know what you are doing
const uint8_t IV_LENGTH = 16; ///< @brief Initalization vector length used by selected crypto algorythm
#define BLOCK_CYPHER Speck
#define CYPHER_TYPE CFB<BLOCK_CYPHER>

//Debug
#define DEBUG_ESP_PORT Serial ///< @brief Stream to output debug info. It will normally be `Serial`
#define DEBUG_LEVEL WARN ///< @brief Possible values VERBOSE, DBG, INFO, WARN, ERROR, NONE

#endif