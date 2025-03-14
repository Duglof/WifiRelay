// **********************************************************************************
// ESP8266 / ESP32 Wifi Therùostat WEB Server configuration Include file
// **********************************************************************************
//
// Historique : V1.0.0 2025-03-13 - Première release
//
// Warning EEPROM size limited at 2048 bytes by (Voir EEPROM.begin(2048)
//
// **********************************************************************************
#ifndef __CONFIG_H__
#define __CONFIG_H__

// Include main project include file
#include "WifiRelay.h"

#define CFG_SSID_SIZE 		      32
#define CFG_PSK_SIZE  		      64
#define CFG_HOSTNAME_SIZE       16
#define CFG_NTP_SERVER_SIZE     32
#define CFG_TZ_SIZE             32

#ifdef ESP8266
  #define CFG_RELAY_GPIO        12
#endif
#ifdef ESP32
  // Analog A5 used as Digital Output
  #define CFG_RELAY_GPIO        A5
#endif

#define CFG_MQTT_HOST_SIZE 		  32
#define CFG_MQTT_PSWD_SIZE 	    64
#define CFG_MQTT_USER_SIZE      32
#define CFG_MQTT_TOPIC_SIZE     32
#define CFG_MQTT_DEFAULT_HOST   "monserveur.org"
#define CFG_MQTT_DEFAULT_PSWD   "password"
#define CFG_MQTT_DEFAULT_USER   "user"
#define CFG_MQTT_DEFAULT_TOPIC  "WIFI-RELAY"
#define CFG_MQTT_DEFAULT_PORT   1883

#define MQTT_RELAY_RELAY_1      "relay/relay1"
#define MQTT_RELAY_ERRORS       "relay/errors"

#define CFG_JDOM_HOST_SIZE      32
#define CFG_JDOM_APIKEY_SIZE    64
#define CFG_JDOM_URL_SIZE       183
#define CFG_JDOM_DEFAULT_PORT   80
#define CFG_JDOM_DEFAULT_HOST   "jeedom.local"

// Default url to call a scenario
#define CFG_JDOM_DEFAULT_URL     "/core/api/jeeApi.php?plugin=virtual&type=event&id=337&value=%REL1%"

#define CFG_HTTPREQ_HOST_SIZE    32
#define CFG_HTTPREQ_PATH_SIZE    150
#define CFG_HTTPREQ_DEFAULT_PORT 80
#define CFG_HTTPREQ_DEFAULT_HOST "127.0.0.1"
#define CFG_HTTPREQ_DEFAULT_PATH  "/json.htm?type=command&param=udevice&idx=1&nvalue=0&svalue=%REL1%;%TARG%;0;0;%MODE%;0"

#define CFG_SYSLOG_HOST_SIZE    32

// Default values
// Port pour l'OTA
#define CFG_DEFAULT_TZ            "CET-1CEST,M3.5.0,M10.5.0/3"
#define CFG_DEFAULT_OTA_PORT      8266
#define CFG_DEFAULT_OTA_AUTH      "OTA_WifiRel"
//#define CFG_DEFAULT_OTA_AUTH    ""
#define CFG_DEFAULT_SYSLOG_PORT   514
#define CFG_DEFAULT_NTP_SERVER    "pool.ntp.org"

// Web Interface Etat
#define CFG_FORM_ETAT_WDAY_FR           FPSTR("jour_fr")
#define CFG_FORM_ETAT_WDAY_EN           FPSTR("jour_en")
#define CFG_FORM_ETAT_DATE              FPSTR("date")
#define CFG_FORM_ETAT_TEMP              FPSTR("temperature")
#define CFG_FORM_ETAT_TARG              FPSTR("consigne")
#define CFG_FORM_ETAT_HUM               FPSTR("humidity")
#define CFG_FORM_ETAT_PROG_ITEM         FPSTR("prog_item")
#define CFG_FORM_ETAT_RELAY             FPSTR("relay")
#define CFG_FORM_ERREUR_THER            FPSTR("errors")

// Web Interface Configuration Form field names
#define CFG_FORM_SSID                   FPSTR("ssid")
#define CFG_FORM_PSK                    FPSTR("psk")
#define CFG_FORM_HOST                   FPSTR("host")
#define CFG_FORM_NTP_HOST               FPSTR("ntp_server");
#define CFG_FORM_TZ                     FPSTR("tz")
#define CFG_FORM_AP_PSK                 FPSTR("ap_psk")
#define CFG_FORM_OTA_AUTH               FPSTR("ota_auth")
#define CFG_FORM_OTA_PORT               FPSTR("ota_port")
#define CFG_FORM_SYSLOG_HOST            FPSTR("syslog_host")
#define CFG_FORM_SYSLOG_PORT            FPSTR("syslog_port")

#define CFG_FORM_RELAY_MODE             FPSTR("relay_mode")
#define CFG_FORM_RELAY_CONFIG           FPSTR("relay_config")
#define CFG_FORM_RELAY_TIMEOUT          FPSTR("relay_timeout")

#define CFG_RELAY_DEFAULT_TIMEOUT       180
#define CFG_RELAY_DEFAULT_MODE          1
#define CFG_RELAY_DEFAULT_CONFIG        1

#define CFG_FORM_MQTT_HOST  FPSTR("mqtt_host")
#define CFG_FORM_MQTT_PORT  FPSTR("mqtt_port")
#define CFG_FORM_MQTT_USER  FPSTR("mqtt_user")
#define CFG_FORM_MQTT_PSWD  FPSTR("mqtt_pswd")
#define CFG_FORM_MQTT_TOPIC FPSTR("mqtt_topic")
#define CFG_FORM_MQTT_FREQ  FPSTR("mqtt_freq")

#define CFG_FORM_JDOM_HOST    FPSTR("jdom_host")
#define CFG_FORM_JDOM_PORT    FPSTR("jdom_port")
#define CFG_FORM_JDOM_URL     FPSTR("jdom_url")
#define CFG_FORM_JDOM_KEY     FPSTR("jdom_apikey")
#define CFG_FORM_JDOM_FREQ    FPSTR("jdom_freq")

#define CFG_FORM_HTTPREQ_HOST  FPSTR("httpreq_host")
#define CFG_FORM_HTTPREQ_PORT  FPSTR("httpreq_port")
#define CFG_FORM_HTTPREQ_PATH  FPSTR("httpreq_path")
#define CFG_FORM_HTTPREQ_FREQ  FPSTR("httpreq_freq")
#define CFG_FORM_HTTPREQ_SWIDX FPSTR("httpreq_swidx")

#define CFG_FORM_IP  FPSTR("wifi_ip")
#define CFG_FORM_GW  FPSTR("wifi_gw")
#define CFG_FORM_MSK FPSTR("wifi_msk")


#pragma pack(push)  // push current alignment to stack
#pragma pack(1)     // set alignment to 1 byte boundary

// Config for mqtt
// 256 Bytes

typedef struct 
{
  char  host[CFG_MQTT_HOST_SIZE+1]; 	  // FQDN 
  char  user[CFG_MQTT_USER_SIZE+1];     // Secret
  char  pswd[CFG_MQTT_PSWD_SIZE+1];     // Post URL
  char  topic[CFG_MQTT_TOPIC_SIZE+1];   // Post URL
  uint16_t port;    					          // Protocol port (1883)
  uint32_t freq;                        // refresh rate
} _mqtt;

// Config du relay
//
//


// Config for relay
// 64 Bytes
typedef struct 
{
  uint16_t  r_timeout;                        // Durée du timeout en secondes
  uint8_t   config;                           // config;  ARRET / MARCHE
  uint8_t   mode;                             // mode: en : PERMANENT / TIMEOUT
  uint8_t   options;                          // not used
  uint8_t filler[59];                         // in case adding data in config avoiding loosing current conf by bad crc*/

} _relay;

// Config for jeedom
// 298 Bytes
typedef struct 
{
  char  host[CFG_JDOM_HOST_SIZE+1];           // FQDN 
  char  apikey[CFG_JDOM_APIKEY_SIZE+1];       // Secret
  char  url[CFG_JDOM_URL_SIZE+1];             // Post URL
  uint16_t port;                              // Protocol port (HTTP/HTTPS)
  uint32_t freq;                              // refresh rate
  uint8_t filler[10];                         // in case adding data in config avoiding loosing current conf by bad crc*/
} _jeedom;

// Config for http request
// 192 Bytes
typedef struct 
{
  char  host[CFG_HTTPREQ_HOST_SIZE+1];        // FQDN 
  char  path[CFG_HTTPREQ_PATH_SIZE+1];        // Path
  uint16_t port;                              // Protocol port (HTTP/HTTPS) 
  uint32_t freq;                              // refresh rate
  uint16_t swidx;                             // Switch index (into Domoticz)
//  uint8_t filler[22];                       // in case adding data in config avoiding loosing current conf by bad crc*/
} _httpRequest;


// Config saved into eeprom
// 1024 bytes total including CRC
// Warning La librairie semble limiter la taille à 1024 pour un ESP8266
typedef struct 
{
  char          ssid[CFG_SSID_SIZE+1]; 	                // SSID     
  char          psk[CFG_PSK_SIZE+1]; 		                // Pre shared key
  char          host[CFG_HOSTNAME_SIZE+1];              // Hostname
  char          ntp_server[CFG_NTP_SERVER_SIZE+1];      // NTP server
  char          tz[CFG_TZ_SIZE+1];                      // Time zone string : ex : CET-1CEST,M3.5.0,M10.5.0/3
  char          ap_psk[CFG_PSK_SIZE+1];                 // Access Point Pre shared key
  char          ota_auth[CFG_PSK_SIZE+1];               // OTA Authentication password
  uint32_t      config;           		                  // Bit field register 
  uint16_t      ota_port;         		                  // OTA port 
  char          syslog_host[CFG_SYSLOG_HOST_SIZE+1];    // Adresse IP ou DNS du serveur rsyslog
  uint16_t      syslog_port;                            // port rsyslog (generalement 514)
  _relay        relay;                                  // thermostat config
  _mqtt         mqtt;                                   // mqtt configuration
  _jeedom       jeedom;                                 // jeedom configuration
  _httpRequest  httpReq;                                // HTTP request
  uint8_t       filler[32];      		                    // in case adding data in config avoiding loosing current conf by bad crc
  uint16_t      crc;
} _Config;


// Exported variables/object instancied in main sketch
// ===================================================
extern _Config config;

#pragma pack(pop)
 
// Declared exported function
// ===================================================
extern bool readConfig(bool clear_on_error=true);
extern bool saveConfig(void);
extern void showConfig(void);
extern const char *r_mode_str[];
extern const char *r_config_str[];
extern const char *r_relay_status_str[];
extern int sizeof_r_mode_str;
extern int sizeof_r_config_str;

#endif 
