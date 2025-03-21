// **********************************************************************************
// ESP8266 / ESP32 Wifi Relay WEB Server
// **********************************************************************************
//
// Historique : V1.0.0 2025-03-13 - Première Version Béta
//
// **********************************************************************************

#include "config.h" 

// =======================================================
// Configuration structure for whole program
// =======================================================

// Config values
_Config config;

// ================== RELAY MODE ====================
const char *r_mode_str[] = {
  "stopped",
  "started",
};

int sizeof_r_mode_str = sizeof(r_mode_str) / sizeof(r_mode_str[0]);

// ================== THERMOSTAT CONFIG ==================
const char *r_config_str[] = {
    "permanent",
    "timeout",
};

int sizeof_r_config_str = sizeof(r_config_str) / sizeof(r_config_str[0]);

// ================= THERMOSTAT RELAY STATUS ==================
const char *r_relay_status_str[] = {
    "off",
    "on",
};

int sizeof_r_relay_status_str = sizeof(r_relay_status_str) / sizeof(r_relay_status_str[0]);


uint16_t crc16Update(uint16_t crc, uint8_t a)
{
  int i;
  crc ^= a;
  for (i = 0; i < 8; ++i)  {
    if (crc & 1)
      crc = (crc >> 1) ^ 0xA001;
    else
      crc = (crc >> 1);
  }
  return crc;
}

/* ======================================================================
Function: eeprom_dump
Purpose : dump eeprom value to serial 
Input 	: -
Output	: -
Comments: -
====================================================================== */
void eepromDump(uint8_t bytesPerRow) 
{
  uint16_t i;
  uint16_t j=0 ;
  
  // default to 16 bytes per row
  if (bytesPerRow==0)
    bytesPerRow=16;

  Debugln();
    
  // loop thru EEP address
  for (i = 0; i < sizeof(_Config); i++) {
    // First byte of the row ?
    if (j==0) {
			// Display Address
      Debugf("%04X : ", i);
    }

    // write byte in hex form
    Debugf("%02X ", EEPROM.read(i));

		// Last byte of the row ?
    // start a new line
    if (++j >= bytesPerRow) {
			j=0;
      Debugln();
		}
  }
}

/* ======================================================================
Function: readConfig
Purpose : fill config structure with data located into eeprom
Input 	: true if we need to clear actual struc in case of error
Output	: true if config found and crc ok, false otherwise
Comments: -
====================================================================== */
bool readConfig (bool clear_on_error) 
{
	uint16_t crc = ~0;
	uint8_t * pconfig = (uint8_t *) &config ;
	uint8_t data ;

	// For whole size of config structure
	for (uint16_t i = 0; i < sizeof(_Config); ++i) {
		// read data
		data = EEPROM.read(i);
		
		// save into struct
		*pconfig++ = data ;
		
		// calc CRC
		crc = crc16Update(crc, data);
	}
	
	// CRC Error ?
	if (crc != 0) {
		// Clear config if wanted
    if (clear_on_error)
		  memset(&config, 0, sizeof( _Config ));
		return false;
	}
	
	return true ;
}

/* ======================================================================
Function: saveConfig
Purpose : save config structure values into eeprom
Input 	: -
Output	: true if saved and readback ok
Comments: once saved, config is read again to check the CRC
====================================================================== */
bool saveConfig(void) 
{
  uint8_t * pconfig ;
  bool ret_code;

  //eepromDump(32);

  // Init pointer 
  pconfig = (uint8_t *) &config ;
	
	// Init CRC
  config.crc = ~0;

	// For whole size of config structure, pre-calculate CRC
  for (uint16_t i = 0; i < sizeof (_Config) - 2; ++i)
    config.crc = crc16Update(config.crc, *pconfig++);

	// Re init pointer 
  pconfig = (uint8_t *) &config ;

  // For whole size of config structure, write to EEP
  for (uint16_t i = 0; i < sizeof(_Config); ++i) 
    EEPROM.write(i, *pconfig++);

  // Physically save
  EEPROM.commit();
  
  // Read Again to see if saved ok, but do 
  // not clear if error this avoid clearing
  // default config and breaks OTA
  ret_code = readConfig(false);
  
  Debug(F("Write config "));
  
  if (ret_code)
    Debugln(F("OK!"));
  else
    Debugln(F("Error!"));

  //eepromDump(32);
  
  // return result
  return (ret_code);
}

/* ======================================================================
Function: showConfig
Purpose : display configuration
Input 	: -
Output	: -
Comments: -
====================================================================== */
void showConfig() 
{
  Debugln("");
  DebuglnF("===== Wifi"); 
  DebugF("ssid     :"); Debugln(config.ssid); 
  DebugF("psk      :"); Debugln(config.psk); 
  DebugF("host     :"); Debugln(config.host); 
  DebugF("tz       :"); Debugln(config.tz); 
  DebuglnF("===== Avancé"); 
  DebugF("ap_psk   :"); Debugln(config.ap_psk); 
  DebugF("OTA auth :"); Debugln(config.ota_auth); 
  DebugF("OTA port :"); Debugln(config.ota_port); 
  DebugF("syslog host :"); Debugln(config.syslog_host); 
  DebugF("syslog port :"); Debugln(config.syslog_port); 

  DebuglnF("===== relay"); 
  DebugF("config     :"); Debugln(config.relay.config);
  DebugF("mode       :"); Debugln(config.relay.mode);
  DebugF("timeout    : "); Debugln(config.relay.r_timeout);

  DebuglnF("===== Mqtt");
  DebugF("host     :"); Debugln(config.mqtt.host);
  DebugF("port     :"); Debugln((int)config.mqtt.port);
  DebugF("user     :"); Debugln(config.mqtt.user);
  DebugF("pswd     :"); Debugln(config.mqtt.pswd);
  DebugF("topic    :"); Debugln(config.mqtt.topic);
  DebugF("freq     :"); Debugln(config.mqtt.freq);

  DebuglnF("\r\n===== Jeedom");
  DebugF("host     :"); Debugln(config.jeedom.host); 
  DebugF("port     :"); Debugln(config.jeedom.port); 
  DebugF("url      :"); Debugln(config.jeedom.url); 
  DebugF("key      :"); Debugln(config.jeedom.apikey); 
  DebugF("freq     :"); Debugln(config.jeedom.freq); 

  DebuglnF("===== HTTP request"); 
  DebugF("host     :"); Debugln(config.httpReq.host); 
  DebugF("port     :"); Debugln(config.httpReq.port); 
  DebugF("path     :"); Debugln(config.httpReq.path); 
  DebugF("freq     :"); Debugln(config.httpReq.freq); 
  DebugF("sw idx   :"); Debugln(config.httpReq.swidx);

  delay(1000);
}
