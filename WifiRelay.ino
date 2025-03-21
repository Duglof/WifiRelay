// **********************************************************************************
// ESP8266 / ESP32 Wifi Relay WEB Server
// **********************************************************************************
//
// Historique : V1.0.0 2025-03-13 - Première Version Béta
//
// ===================================================================
// WifiRelay : Connection à votre réseau Wifi
// ===================================================================
//  A faire une seule fois ou après des changements dans le répertoire data
//
// Téléverser WifiRelay et le data sur votre ESP12E (Sketch Data Upload)
// Alimenter votre module ESP12E par le cable USB (ou autre) 
// Avec votre téléphone
//   Se connecter au réseau Wifi WifiRel-xxxxxx
//   Ouvrir votre navigateur préféré (Chrome ou autre)
//   Accéder à l'url http://192.168.4.1 (la page web de WifiRelay doit apparaître)
//   Sélectionner l'onglet 'Configuration'
//   Renseigner
//     Réseau Wifi
//     Clé Wifi
//     Serveur NTP
//     Time Zone (heure, heure été /heure hiver)
//   Cliquer sur 'Enregistrer'
//   Enfin dans la partie 'Avancée' cliquer sur Redémarrer WifiRelay
//   WifiRelay se connectera à votre réseau Wifi
//   Si ce n'est pas le cas c'est que ne nom du réseau ou la clé sont erronés
// ===================================================================
//
//          Environment
//           Arduino IDE 1.8.18
//             Préférences : https://arduino.esp8266.com/stable/package_esp8266com_index.json
//             Folder Arduino/tools : https://github.com/esp8266/arduino-esp8266fs-plugin/releases/download/0.5.0/ESP8266FS-0.5.0.zip
//               (Arduino/tools/ESP8266FS/tool/esp8266fs.jar)
//             Folder Arduino/libraries : WifiRelay/librairie/Syslog-master.zip : uncompress
//             Type de carte : NodeMCU 1.0 (ESP-12E Module)
//             Compilation / Téléversement
//               Options de compilation dans WifiRelay.h ( DEBUG, SYSLOG)
//               Croquis/Compiler (sans erreur !!!)
//               Tools/ESP8266 Data Upload (Arduino/tools/ESP8266FS doit être installé)
//               Croquis/Téléverser
//          Hardware : NodeMCU V1.0 Kit de développement V3 CH340 NodeMCU Esp8266 Esp-12e
//            - Relay on GPIO 12 (esp8266)
//            - Ruf relay on GPIO 14 (esp8266)
// **********************************************************************************
// Global project file
#include "WifiRelay.h"

#include "mqtt.h"
#include "ota.h"
#include <EEPROM.h>
#include <Ticker.h>
#include "relay.h"

// Relay
DigitalRelay  relay1;

#ifdef ESP8266
  ESP8266WebServer server(80);
#else
  ESP32WebServer server(80);
#endif

bool ota_blink;
String optval;    // Options de compilation

// LED Blink timers
Ticker rgb_ticker;
Ticker blu_ticker;
Ticker Every_1_Sec;
Ticker Every_1_Min;
Ticker Tick_mqtt;
Ticker Tick_jeedom;
Ticker Tick_httpRequest;

volatile boolean task_1_sec = false;
volatile boolean task_1_min = false;
volatile boolean task_mqtt = false;
volatile boolean task_jeedom = false;
volatile boolean task_httpRequest = false;
unsigned long seconds = 0;                // Nombre de secondes depuis la mise sous tension

// sysinfo data
_sysinfo sysinfo;

enum _r_errors {
  r_error_none = 0,
  r_error_send_mqtt       = 0x01,         // Bit 0
  r_error_send_jeedom     = 0x02,         // Bit 1
  r_error_send_http       = 0x04,         // Bit 2
};

/*
 * String get_t_errors_str()
 * 
 * Return Thermostat errors in human string
 */
String get_r_errors_str()
{
String s;
  if (r_errors == 0) {
    s += "none";
  }
  if (r_errors & r_error_send_mqtt) {
    if (s.length())
      s +=",";
   s += "sendMqtt";  
  }
  if (r_errors & r_error_send_jeedom) {
    if (s.length())
      s +=",";
   s += "sendJeedom";  
  }
  if (r_errors & r_error_send_http) {
    if (s.length())
      s +=",";
   s += "sendHttp";  
  }
  return(s);
}

// Relay informations
int8_t    r_relay_status = 0;             // 0 = off, 1 = on
int8_t    r_errors = 0;                   // Erreurs : bit field : ex : bit 0 : t_error_reading_sensor (no error at startup)
uint16_t  r_timeout = 0;                  // relay timeout (decreassing each seconds)

MQTT  mqtt;

OTAClass  ota_class;

NTP ntp;

#ifdef SYSLOG
WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);

char logbuffer[1024];
char waitbuffer[1024];

char *lines[50];
int in=-1;
int out=-1;

unsigned int pending = 0 ;
volatile boolean SYSLOGusable=false;
volatile boolean SYSLOGselected=false;
int plog=0;

void convert(const __FlashStringHelper *ifsh)
{
  PGM_P p = reinterpret_cast<PGM_P>(ifsh);
  plog=0;
  while (1) {
    unsigned char c = pgm_read_byte(p++);
    if (c == 0) {
      logbuffer[plog]=0;
      break;
    }
    logbuffer[plog]=c;
    plog++;
  }
}

/* ======================================================================
Function: process_line
Purpose : Ajoute la chaîne dans le buffer d'attente SYSLOG et l'envoi si nécessaire
Input   : char *msg
Output  : - 
Comments: Taille maximale du message envoyé limité à sizeof(waitbuffer)
Envoi le message SYSLOG si:
 - Celui-ci est terminé par CR ou LF
 - Le buffer d'attente est plein 
   ======================================================================
 */
void process_line(char *msg) {
    // Ajouter à waitbuffer et tronquer si nécessaire à la taille maximale de waitbuffer
    // Dans tous les cas strncat() met un null à la fin
    strncat(waitbuffer,msg, sizeof(waitbuffer) - strlen(waitbuffer) - 1);
    pending=strlen(waitbuffer);
    // En cas de buffer plein, forcer le dernier caractère à LF (0x0A) ce qui forcera l'envoi
    // et permettra un affichage correct sur la console socat
    if ( strlen(waitbuffer) == (sizeof(waitbuffer) - 1) && waitbuffer[pending-1] != 0x0A)
      waitbuffer[pending-1] = 0x0A;
    // Si le dernier vaut CR ou LF ou buffer plein => on envoie le message
    if( waitbuffer[pending-1] == 0x0D || waitbuffer[pending-1] == 0x0A ) {
      //Cette ligne est complete : l'envoyer !
      for(unsigned int i=0; i < pending-1; i++) {
        if(waitbuffer[i] <= 0x20)
          waitbuffer[i] = 0x20;
      }
      syslog.log(LOG_INFO,waitbuffer);
      delay(2*pending);
      memset(waitbuffer,0,sizeof(waitbuffer));
      pending=0;
      
    }
}


// Toutes les fonctions aboutissent sur la suivante :
void Myprint(char *msg) {
  
#ifdef DEBUG
  DEBUG_SERIAL.print(msg);
#endif

  if( SYSLOGusable ) {
    process_line(msg);   
  } else if ( SYSLOGselected) {
    //syslog non encore disponible
    //stocker les messages à envoyer plus tard
    in++;
    if(in >= 50) {
      //table saturée !
      in=0;
    }
    if(lines[in]) {
      //entrée occupée : l'écraser, tant pis !
      free(lines[in]);
    }
    lines[in]=(char *)malloc(strlen(msg)+2);
    memset(lines[in],0,strlen(msg+1));
    strcpy(lines[in],msg);   
  }
}

void Myprint() {
  logbuffer[0] = 0;
  Myprint(logbuffer);
}

void Myprint(const char *msg)
{
  // strcpy(logbuffer, msg);
  // Myprint(logbuffer);
  Myprint((char *)msg);
}

void Myprint(String msg) {
  // sprintf(logbuffer,"%s",msg.c_str());
  // Myprint(logbuffer);
  Myprint((char *)msg.c_str());
}

void Myprint(int i) {
  sprintf(logbuffer,"%d", i);
  Myprint(logbuffer);
}

void Myprint(unsigned int i) {
  sprintf(logbuffer,"%u", i);
  Myprint(logbuffer);
}

void Myprint(float i)
{
  sprintf(logbuffer,"%f", i);
  Myprint(logbuffer);
}

void Myprint(double i)
{
  sprintf(logbuffer,"%lf", i);
  Myprint(logbuffer);
}

// void Myprintf(...)
void Myprintf(const char * format, ...)
{

  va_list args;
  va_start (args, format);
  vsnprintf (logbuffer,sizeof(logbuffer),format, args);
  Myprint(logbuffer);
  va_end (args);

}

void Myprintln() {
  sprintf(logbuffer,"\n");
  Myprint(logbuffer);
}

void Myprintln(char *msg)
{
  // sprintf(logbuffer,"%s\n",msg);
  // Myprint(logbuffer);
  strncpy(logbuffer,msg, sizeof(logbuffer) - 2);
  int length = strlen(logbuffer);
  logbuffer[length] = '\n';
  logbuffer[length+1] = 0;
  Myprint(logbuffer);
}

void Myprintln(const char *msg)
{
  // sprintf(logbuffer,"%s\n",msg);
  // Myprint(logbuffer);
  strncpy(logbuffer,msg, sizeof(logbuffer) - 2);
  int length = strlen(logbuffer);
  logbuffer[length] = '\n';
  logbuffer[length+1] = 0;
  Myprint(logbuffer);
}

void Myprintln(String msg) {
  // sprintf(logbuffer,"%s\n",msg.c_str());
  // Myprint(logbuffer);
  strncpy(logbuffer,msg.c_str(), sizeof(logbuffer) - 2);
  int length = strlen(logbuffer);
  logbuffer[length] = '\n';
  logbuffer[length+1] = 0;
  Myprint(logbuffer);
}

void Myprintln(const __FlashStringHelper *msg) {
  convert(msg);
  logbuffer[plog]=(char)'\n';
  logbuffer[plog+1]=0;
  Myprint(logbuffer);
}

void Myprintln(int i) {
  sprintf(logbuffer,"%d\n", i);
  Myprint(logbuffer);
}

void Myprintln(unsigned int i) {
  sprintf(logbuffer,"%u\n", i);
  Myprint(logbuffer);
}

void Myprintln(unsigned long i)
{
  sprintf(logbuffer,"%lu\n", i);
  Myprint(logbuffer);
}

void Myprintln(float i)
{
  sprintf(logbuffer,"%f\n", i);
  Myprint(logbuffer);
}

void Myprintln(double i)
{
  sprintf(logbuffer,"%lf\n", i);
  Myprint(logbuffer);
}

void Myflush() {
#ifdef DEBUG
  DEBUG_SERIAL.flush();
#endif
}

#endif // SYSLOG

/* ======================================================================
Function: UpdateSysinfo 
Purpose : update sysinfo variables
Input   : true if first call
          true if needed to print on serial debug
Output  : - 
Comments: -
====================================================================== */
void UpdateSysinfo(boolean first_call, boolean show_debug)
{
  char buff[64];

  int sec = seconds;
  int min = sec / 60;
  int hr = min / 60;
  long day = hr / 24;

  sprintf_P( buff, PSTR("%ld days %02d h %02d m %02d sec"),day, hr % 24, min % 60, sec % 60);
  sysinfo.sys_uptime = buff;
}

/* ======================================================================
Function: Task_1_Sec 
Purpose : Mise à jour du nombre de seconds depuis la mise sous tension
Input   : -
Output  : Incrément variable globale seconds 
Comments: -
====================================================================== */
void Task_1_Sec()
{
  task_1_sec = true;
  seconds++;
}

/* ======================================================================
Function: Task_1_Min 
Purpose : Lecture température et mise à jour état du relais
Input   : -
Output  : 
Comments: -
====================================================================== */
void Task_1_Min()
{
  task_1_min = true;
}

/* ======================================================================
Function: Task_mqtt
Purpose : callback of mqtt ticker
Input   : 
Output  : -
Comments: Like an Interrupt, need to be short, we set flag for main loop
====================================================================== */
void Task_mqtt()
{
  task_mqtt = true;
}

/* ======================================================================
Function: Task_jeedom
Purpose : callback of jeedom ticker
Input   : 
Output  : -
Comments: Like an Interrupt, need to be short, we set flag for main loop
====================================================================== */
void Task_jeedom()
{
  task_jeedom = true;
}

/* ======================================================================
Function: Task_httpRequest
Purpose : callback of http request ticker
Input   : 
Output  : -
Comments: Like an Interrupt, need to be short, we set flag for main loop
====================================================================== */
void Task_httpRequest()
{
  task_httpRequest = true;
}

/* ======================================================================
Function: LedON
Purpose : callback called after led blink delay
Input   : led (defined in term of PIN)
Output  : - 
Comments: -
====================================================================== */
void LedON(int led)
{
  #ifdef BLU_LED_PIN
  if (led==BLU_LED_PIN)
    LedBluON();
  #endif
  #ifdef RED_LED_PIN
  if (led==RED_LED_PIN)
    LedRedON();
  #endif
}

// Set flash_blue_led_count to make BLU_LED flashing
int flash_blue_led_count = 0;

/* ======================================================================
Function: void FlashBlueLed(int param)
Input   : int param : not used
Output  : -
Comments: -
====================================================================== */
void FlashBlueLed(int param) // Non-blocking ticker for Blue LED
{
  if (flash_blue_led_count != 0) {
    // Change state each call
    int state = digitalRead(BLU_LED_PIN);   // get the current state of the blue_led pin
    digitalWrite(BLU_LED_PIN, !state);      // set pin to the opposite state

    // BLU_LED_PIN = HIGH => LED éteinte
    if ((!state) == HIGH) {
      flash_blue_led_count--;
    }
  }
}

/* ======================================================================
Function: LedOFF 
Purpose : callback called after led blink delay
Input   : led (defined in term of PIN)
Output  : - 
Comments: -
====================================================================== */
void LedOFF(int led)
{
  #ifdef BLU_LED_PIN
  if (led==BLU_LED_PIN)
    LedBluOFF();
  #endif
  #ifdef RED_LED_PIN
  if (led==RED_LED_PIN)
    LedRedOFF();
  #endif
}

/* ======================================================================
Function: ResetConfig
Purpose : Set configuration to default values
Input   : -
Output  : -
Comments: -
====================================================================== */
void ResetConfig(void) 
{
  // Start cleaning all that stuff
  memset(&config, 0, sizeof(_Config));

  // Set default Hostname
#ifdef ESP8266
  // ESP8266
  sprintf_P(config.host, PSTR("WifiRel-%06X"), ESP.getChipId());
#else
  //ESP32
  int ChipId;
  uint64_t macAddress = ESP.getEfuseMac();
  uint64_t macAddressTrunc = macAddress << 40;
  ChipId = macAddressTrunc >> 40;
  sprintf_P(config.host, PSTR("WifiRel-%06X"), ChipId);
#endif

  strcpy_P(config.ntp_server, CFG_DEFAULT_NTP_SERVER);
  strcpy_P(config.tz, CFG_DEFAULT_TZ);

  // Mqtt
  strcpy_P(config.mqtt.host, CFG_MQTT_DEFAULT_HOST);
  config.mqtt.port = CFG_MQTT_DEFAULT_PORT;
  strcpy_P(config.mqtt.pswd, CFG_MQTT_DEFAULT_PSWD);
  strcpy_P(config.mqtt.user, CFG_MQTT_DEFAULT_USER);
  strcpy_P(config.mqtt.topic, CFG_MQTT_DEFAULT_TOPIC);

  // Relay
  config.relay.r_timeout = CFG_RELAY_DEFAULT_TIMEOUT;
  config.relay.mode = CFG_RELAY_DEFAULT_MODE;
  config.relay.config = CFG_RELAY_DEFAULT_CONFIG;
    
  // Jeedom
  strcpy_P(config.jeedom.host, CFG_JDOM_DEFAULT_HOST);
  config.jeedom.port = CFG_JDOM_DEFAULT_PORT;
  strcpy_P(config.jeedom.url, CFG_JDOM_DEFAULT_URL);

  // HTTP Request
  strcpy_P(config.httpReq.host, CFG_HTTPREQ_DEFAULT_HOST);
  config.httpReq.port = CFG_HTTPREQ_DEFAULT_PORT;
  strcpy_P(config.httpReq.path, CFG_HTTPREQ_DEFAULT_PATH);

  // Avancé
  strcpy_P(config.ota_auth, PSTR(CFG_DEFAULT_OTA_AUTH));
  config.ota_port = CFG_DEFAULT_OTA_PORT ;
  config.syslog_port = CFG_DEFAULT_SYSLOG_PORT;
  
  // save back
  saveConfig();
}

// =========================================================
// =         ota callbacks                                 =
// =========================================================
  void otaOnStart() { 
    LedON(BLU_LED_PIN);
    DebuglnF("Update Started");
    ota_blink = true;
  }

  void otaOnProgress(unsigned int progress, unsigned int total)
  {
    if (ota_blink) {
      LedON(BLU_LED_PIN);
    } else {
      LedOFF(BLU_LED_PIN);
    }
    ota_blink = !ota_blink;
    //Debugf.printf("Progress: %u%%\n", (progress / (total / 100)));
  }

  // void ArduinoOTAClass::THandlerFunction otaOnEnd()
  void otaOnEnd()
  {
    DebuglnF("Update finished restarting");
  }

  void otaOnError(ota_error_t error)
  {
    LedON(BLU_LED_PIN);
    Debugf("Update Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DebuglnF("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DebuglnF("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DebuglnF("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DebuglnF("Receive Failed");
    else if (error == OTA_END_ERROR) DebuglnF("End Failed");
    ESP.restart(); 
  }

/* ======================================================================
Function: WifiHandleConn
Purpose : Handle Wifi connection / reconnection and OTA updates
Input   : setup true if we're called 1st Time from setup
Output  : state of the wifi status
Comments: -
====================================================================== */
int WifiHandleConn(boolean setup = false) 
{
  int ret = WiFi.status();

  if (setup) {
    #ifdef ESP32
      // ESP32 Doc espressif : https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html
      // The setHostname() function must be called BEFORE Wi-Fi is started with WiFi.begin(), WiFi.softAP(), WiFi.mode(), or WiFi.run() 
      if(*config.host) {
        WiFi.setHostname(config.host);
      }
    #endif
    
    // Pourquoi ce n'était pas appelé avant V1.0.9 et précédente
    WiFi.mode(WIFI_STA);

    #ifdef ESP8266
      // With WiFi.hostname(config.host);
      //  ping WifiRel-23178F
      //    ok idem ci-dessous
      //  ping WifiRel-23178F.local
      //    PING WifiRel-23178F.local (192.168.1.28) 56(84) bytes of data.
      //    64 bytes from WifiRel-23178F (192.168.1.28): icmp_seq=1 ttl=255 time=2.28 ms
      if(*config.host) {
        WiFi.hostname(config.host);
      }
    #endif
    
    DebuglnF("========== WiFi.printDiag Start"); 
    WiFi.printDiag(DEBUG_SERIAL);
    DebuglnF("========== WiFi.printDiag End"); 
    Debugflush();

    // no correct SSID
    if (!*config.ssid) {
      DebugF("no Wifi SSID in config, trying to get SDK ones..."); 

      // Let's see of SDK one is okay
      if ( WiFi.SSID() == "" ) {
        DebuglnF("Not found may be blank chip!"); 
      } else {
        *config.psk = '\0';

        // Copy SDK SSID
        strcpy(config.ssid, WiFi.SSID().c_str());

        // Copy SDK password if any
        if (WiFi.psk() != "")
          strcpy(config.psk, WiFi.psk().c_str());

        DebuglnF("found one!"); 

        // save back new config
        saveConfig();
      }
    }

    // correct SSID
    if (*config.ssid) {
      uint8_t timeout ;

      DebugF("Connecting to: "); 
      Debug(config.ssid);
      Debugflush();

      // Do wa have a PSK ?
      if (*config.psk) {
        // protected network
        Debug(F(" with key '"));
        Debug(config.psk);
        Debug(F("'..."));
        Debugflush();
        WiFi.begin(config.ssid, config.psk);
      } else {
        // Open network
        Debug(F("unsecure AP"));
        Debugflush();
        WiFi.begin(config.ssid);
      }

      // From https://github.com/esp8266/Arduino/issues/6308
      // You're only waiting for 5 seconds (10x 500 msec), which may be a bit short.
      // timeout = 25; // 25 * 200 ms = 5 sec time out
      timeout = 50; // 50 * 200 ms = 10 sec time out
      // 200 ms loop
      while ( ((ret = WiFi.status()) != WL_CONNECTED) && timeout )
      {
        // Orange LED
        LedON(BLU_LED_PIN);
        delay(50);
        LedOFF(BLU_LED_PIN);
        delay(150);
        --timeout;
      }
    }

    // connected ? disable AP, client mode only
    if (ret == WL_CONNECTED)
    {
      DebuglnF("connected!");
      // WiFi.mode(WIFI_STA); Ca ne sert à rien, c'est fait au début

      DebugF("IP address   : "); Debugln(WiFi.localIP().toString());
      DebugF("MAC address  : "); Debugln(WiFi.macAddress());
 #ifdef SYSLOG
    if (*config.syslog_host) {
      SYSLOGselected=true;
      // Create a new syslog instance with LOG_KERN facility
      syslog.server(config.syslog_host, config.syslog_port);
      syslog.deviceHostname(config.host);
      syslog.appName(APP_NAME);
      syslog.defaultPriority(LOG_KERN);
      memset(waitbuffer,0,sizeof(waitbuffer));
      pending=0;
      SYSLOGusable=true;
    } else {
      SYSLOGusable=false;
      SYSLOGselected=false;
    }
#endif
   
    // not connected ? start AP
    } else {
      char ap_ssid[32];
      DebuglnF("Error!");
      Debugflush();

      // STA+AP Mode without connected to STA, autoconnect will search
      // other frequencies while trying to connect, this is causing issue
      // to AP mode, so disconnect will avoid this

      // Disable auto retry search channel
      WiFi.disconnect(); 

      // SSID = hostname
      strcpy(ap_ssid, config.host );
      DebugF("Switching to AP ");
      Debugln(ap_ssid);
      Debugflush();
      
      // protected network
      if (*config.ap_psk) {
        DebugF(" with key '");
        Debug(config.ap_psk);
        DebuglnF("'");
        WiFi.softAP(ap_ssid, config.ap_psk);
      // Open network
      } else {
        DebuglnF(" with no password");
        WiFi.softAP(ap_ssid);
      }
      WiFi.mode(WIFI_AP_STA);

      DebugF("IP address   : "); Debugln(WiFi.softAPIP().toString());
      DebugF("MAC address  : "); Debugln(WiFi.softAPmacAddress());
    }
    
    // Version 1.0.7 : Use auto reconnect Wifi
#ifdef ESP8266
    // Ne semble pas exister pour ESP32
    WiFi.setAutoConnect(true);
#endif
    WiFi.setAutoReconnect(true);
    DebuglnF("auto-reconnect armed !");
      
    // ota : new code    
    ota_class.Begin(  config.host,
                      config.ota_port,
                      config.ota_auth,
                      otaOnStart,
                      otaOnProgress,
                      otaOnEnd,
                      otaOnError);

    // just in case your sketch sucks, keep update OTA Available
    // Trust me, when coding and testing it happens, this could save
    // the need to connect FTDI to reflash
    // Usefull just after 1st connexion when called from setup() before
    // launching potentially buggy main()
    for (uint8_t i=0; i<= 10; i++) {
      LedON(BLU_LED_PIN);
      delay(100);
      LedOFF(BLU_LED_PIN);
      delay(200);
      ota_class.Loop();
    }

  } // if setup

  return WiFi.status();
}


/* ======================================================================
Function: mqttStartupLogs (called one at startup, if mqtt activated)
Purpose : Send logs to mqtt ()
Input   : 
Output  : true if post returned OK
Comments: Send in {mqtt.topic}/log Relay Version, Local IP and Datetime
====================================================================== */
boolean mqttStartupLogs()
{
String topic,payload;
boolean ret = false;

  if (*config.mqtt.host && config.mqtt.freq != 0) {
    if (*config.mqtt.topic) {
        // Publish Startup
        topic= String(config.mqtt.topic);
        topic+= "/log";
        String payload = "WifiRelay Startup V";
        payload += WIFIRELAY_VERSION;
        // And submit all to mqtt
        Debug("mqtt ");
        Debug(topic);
        Debug( " value ");
        Debug(payload);
        ret = mqtt.Publish(topic.c_str(), payload.c_str() , true);      
        if (ret)
          Debugln(" OK");
        else
          Debugln(" KO");

        // Publish IP
        topic= String(config.mqtt.topic);
        topic+= "/log";
        payload = WiFi.localIP().toString();
        // And submit all to mqtt
        Debug("mqtt ");
        Debug(topic);
        Debug( " value ");
        Debug(payload);
        ret &= mqtt.Publish(topic.c_str(), payload.c_str() , true);      
        if (ret)
          Debugln(" OK");
        else
          Debugln(" KO");
      
        //Publish Date Heure
        topic= String(config.mqtt.topic);
        topic+= "/log";
        struct tm timeinfo;
        if(ntp.getTime(timeinfo)){
          char buf[20];
          strftime(buf,sizeof(buf),"%FT%H:%M:%S",&timeinfo);
          payload = buf;
          // And submit all to mqtt
          Debug("mqtt ");
          Debug(topic);
          Debug( " value ");
          Debug(payload);
          ret &= mqtt.Publish(topic.c_str(), payload.c_str() , true);      
          if (ret)
            Debugln(" OK");
          else
            Debugln(" KO");
        }
    } else {
      Debugln("mqtt TOPIC non configuré");
    }
  }
  return(ret);  
}

/* ======================================================================
Function: mqttPost (called by main sketch on timer, if activated)
Purpose : Do a http post to mqtt
Input   : 
Output  : true if post returned OK
Comments: -
====================================================================== */
boolean mqttPost(String i_sub_topic, String i_value)
{
String topic;
boolean ret = false;

  // Some basic checking
  if (config.mqtt.freq != 0) {
    Debug("mqtt.Publish ");
    if (*config.mqtt.host) {
        
      if (*config.mqtt.topic) {
        topic = String(config.mqtt.topic);
        topic += "/";
        topic += i_sub_topic;
        Debug(topic);
        Debug( " value = '");
        Debug(i_value);
        // And submit to mqtt
        ret = mqtt.Publish(topic.c_str(), i_value.c_str() , true);             
        if (ret)
          Debugln("' OK");
        else
          Debugln("' KO");
      } else {
        Debugln("config.mqtt.topic is empty");
      }
    } else {
       Debugln("config.mqtt.host is empty");    
    } // if host freq
  } // if 
  return ret;
}

/* ======================================================================
Function: mqttCallback
Purpose : Déclenche les actions à la réception d'un message mqtt
          D'après http://m2mio.tumblr.com/post/30048662088/a-simple-example-arduino-mqtt-m2mio
Input   : topic (complete topic string : ex : WIFI-RELAY/set/setmode)
          payload 
          length (of the payload)
Output  : - 
Comments: For topic = 'WIFI-RELAY' in WifiRelay Setup mqtt 
            All mqtt message with topic like WIFI-RELAY/set/# will be received
            ex : Message arrived on topic WIFI-RELAY : [WIFI-RELAY/set/setmode], 2

====================================================================== */
void mqttCallback(char* topic, byte* payload, unsigned int length) {


 #ifdef DEBUG 
   Debug("mqttCallback : Message recu =>  topic: ");
   Debug(String(topic));
   Debug(" | longueur: ");
   Debugln(String(length,DEC));
 #endif

  String mytopic = (char*)topic;
  payload[length] = '\0'; // Null terminator used to terminate the char array
  String  message = (char*)payload;
  bool    ret = false;
  int     i;
  
  Debug("Message arrived on topic: [");
  Debug(mytopic);
  Debug("], ");
  Debugln(message);

  // ========================================
  // Update config.mode.mode via mqtt
  // ========================================
  if (mytopic.indexOf("setmode") >= 0) {
    for ( i = 0 ; i < sizeof_r_mode_str ; i++) {
      // Debugf("i = %d message = %s r_mode_str[i] = %s\r\n",i,message.c_str(), r_mode_str[i]);
      if (message == String(r_mode_str[i])) {
        config.relay.mode = i;
        ret = true;
        Debugf("mqttCallback setmode %s (%d)\r\n", r_mode_str[i], config.relay.mode); 
        break;
      }
    }
    if (!ret) {
      Debugf("mqttCallback setmode %s : invalid\r\n", message.c_str()); 
    }       
  } else

  // ========================================
  // Update config.relay.config via mqtt
  // ========================================
  if (mytopic.indexOf("setconfig") >= 0) {
    for ( i = 0 ; i < sizeof_r_config_str ; i++) {
      // Debugf("i = %d message = %s r_config_str[i] = %s\r\n",i,message.c_str(), r_config_str[i]);
      if (message == String(r_config_str[i])) {
        config.relay.config = i;
        ret = true;
        Debugf("mqttCallback setconfig %s (%d)\r\n", r_config_str[i], config.relay.config); 
        break;
      }
    }
    if (!ret) {
      Debugf("mqttCallback setconfig %s : invalid\r\n", message.c_str()); 
    }

  } else

  if (mytopic.indexOf("setrelay1") >= 0) {
    for ( i = 0 ; i < sizeof_r_relay_status_str ; i++) {
      if (message == String(r_relay_status_str[i])) {
        r_relay_status = i;
        if(config.relay.mode == r_config_timeout) {
          if (r_relay_status == 1) { 
            r_timeout = config.relay.r_timeout;
          } else {
            r_timeout = 0;
          }
        }
        ret = true;
        Debugf("mqttCallback setrelay1 %s (%d)\r\n", r_relay_status_str[i], r_relay_status); 
        break;
      }
    }
    if (!ret) {
      Debugf("mqttCallback setrelay1 %s : invalid\r\n", message.c_str()); 
    }
  } else {
    Debugf("mqttCallback invalid topic %s : invalid\r\n", mytopic.c_str()); 
  }

  if(ret) {
    // Save new config in LittleFS file system
    saveConfig();
  }
}


/* ======================================================================
Function: littleFSlistAllFilesInDir
Purpose : List au files in folder and subfolders
Input   : dir_path
Output  : - 
Comments: -
====================================================================== */
void littleFSlistAllFilesInDir(String dir_path)
{
#ifdef ESP8266
  Dir dir = LittleFS.openDir(dir_path);

    while(dir.next()) {
      if (dir.isFile()) {
        String fileName = dir_path + dir.fileName();
        size_t fileSize = dir.fileSize();
        Debugf("FS File: %s, size: %d\n", fileName.c_str(), fileSize);
      }
      if (dir.isDirectory()) {
        littleFSlistAllFilesInDir(dir_path + dir.fileName() + "/");
      }
    }

#else
  File root = LittleFS.open(dir_path.c_str());

  File file;
  
  if (root) {
    while ((file = root.openNextFile())) {
      if (file.isDirectory()) {
        littleFSlistAllFilesInDir(String(file.path()) + "/");
      } else {
        String fileName = dir_path + file.name();
        size_t fileSize = file.size();
        Debugf("FS File: %s, size: %d\n", fileName.c_str(), fileSize);
      }      
    }
  }
#endif  // ESP8266

}

/* ======================================================================
Function: setup
Purpose : Setup I/O and other one time startup stuff
Input   : -
Output  : - 
Comments: -
====================================================================== */
void setup()
{
  // Set CPU speed to 160MHz
#ifdef ESP8266
  // ESP8266
  system_update_cpu_freq(160);
#else
  //ESP32
  setCpuFrequencyMhz(160);
#endif

  // Init the serial TXD0
  DEBUG_SERIAL.begin(115200);

  delay(2000);
  
  Debugln("\r\n\r\n");

#ifdef SYSLOG
  for(int i=0; i<50; i++)
    lines[i]=0;
  in=-1;
  out=-1;

  SYSLOGselected=true;  //Par défaut, au moins stocker les premiers msg debug
  SYSLOGusable=false;   //Tant que non connecté, ne pas émettre sur réseau
#endif

  optval = "";

#if THER_SIMU
  optval += "SIMU, ";
#elif defined(THER_DS18B20)
  optval += "DS18B20, ";
#elif defined(THER_BME280)
  optval += "BME280, ";
#elif defined(THER_HTU21)
  optval += "HTU21, ";
#endif

#ifdef DEBUG
  optval += "DEBUG, ";
#else
  optval += "No DEBUG, ";
#endif

#ifdef SYSLOG
  optval += "SYSLOG";
#else
  optval += "No SYSLOG";
#endif


  Debugln(F("=============="));
  Debug(F("WifiRelay V"));
  Debugln(F(WIFIRELAY_VERSION));
  Debugln();
  Debugflush();

  // Clear our global flags
  config.config = 0;

  // Our configuration is stored into EEPROM
  EEPROM.begin(2048);  // Taille max de la config

  DebugF("Config size=");   Debug(sizeof(_Config));
  DebugF("  (mqtt=");       Debug(sizeof(_mqtt));
  DebugF("  relay=");       Debug(sizeof(_relay));
  DebugF("  jeedom=");      Debug(sizeof(_jeedom));
  DebugF("  httpRequest="); Debug(sizeof(_httpRequest));
  Debugln(')');
  Debugflush();

  // Check File system init 
  if (!LittleFS.begin())
  {
    // Serious problem
    Debugf("%s Mount failed\n", "LittleFS");
  } else {
   
    Debugf("%s Mount succesfull\n", "LittleFS");

    // LittleFS (ESP8266 or ESP32)
    littleFSlistAllFilesInDir("/");
    
    DebuglnF("");
  }

  // Read Configuration from EEP
  if (readConfig()) {
      DebuglnF("Good CRC, not set!");
  } else {
    // Reset Configuration
    ResetConfig();

    // save back
    saveConfig();

    DebuglnF("Reset to default");
  }

  relay1.begin(PIN_RELAY_1, false);   // non inverted relay

#ifdef RED_LED_PIN
  pinMode(RED_LED_PIN, OUTPUT); 
  LedRedOFF();
#endif

#ifdef BLU_LED_PIN
  pinMode(BLU_LED_PIN, OUTPUT); 
  LedBluOFF();
#endif 

  // start Wifi connect or soft AP
  WifiHandleConn(true);

#ifdef SYSLOG
  //purge previous debug message,
  if(SYSLOGselected) {
    if(in != out && in != -1) {
        //Il y a des messages en attente d'envoi
        out++;
        while( out <= in ) {
          process_line(lines[out]);
          free(lines[out]);
          lines[out]=0;
          out++;
        }
        DebuglnF("syslog buffer empty");
    }
  } else {
    DebuglnF("syslog not activated !");
  }
#endif

  // Update sysinfo variable and print them
  UpdateSysinfo(true, true);

  server.on("/", handleRoot);
  server.on("/config_form.json", handleFormConfig);
  server.on("/json", sendJSON);
  server.on("/tinfo.json", tinfoJSON);
  server.on("/system.json", sysJSONTable);
  server.on("/config.json", confJSONTable);
  server.on("/spiffs.json", spiffsJSONTable);
  server.on("/wifiscan.json", wifiScanJSON);
  server.on("/factory_reset", handleFactoryReset);
  server.on("/reset", handleReset);
  server.on("/setrelay", handleSetRelay);

  // handler for the hearbeat
  server.on("/hb.htm", HTTP_GET, [&](){
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/html", R"(OK)");
  });

  // handler for the /update form POST (once file upload finishes)
  server.on("/update", HTTP_POST, 
    // handler once file upload finishes
    [&]() {
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
      ESP.restart();
    },
    // handler for upload, get's the sketch bytes, 
    // and writes them through the Update object
    [&]() {
      HTTPUpload& upload = server.upload();

      if(upload.status == UPLOAD_FILE_START) {
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
#ifdef ESP8266
        WiFiUDP::stopAll();
#else
        // xxxxxxxx ESP32 : je ne sais pas
      #pragma message "xxxxxxx WiFiUDP::stopAll() n'existe pas pour ESP32 : que mettre ?"
#endif
        Debugf("Update: %s\n", upload.filename.c_str());
        LedON(BLU_LED_PIN);
        ota_blink = true;

        //start with max available size
        if(!Update.begin(maxSketchSpace)) 
          Update.printError(DEBUG_SERIAL);

      } else if(upload.status == UPLOAD_FILE_WRITE) {
        if (ota_blink) {
          LedON(BLU_LED_PIN);
        } else {
          LedOFF(BLU_LED_PIN);
        }
        ota_blink = !ota_blink;
        Debug(".");
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) 
          Update.printError(DEBUG_SERIAL);

      } else if(upload.status == UPLOAD_FILE_END) {
        //true to set the size to the current progress
        if(Update.end(true)) 
          Debugf("Update Success: %u\nRebooting...\n", upload.totalSize);
        else 
          Update.printError(DEBUG_SERIAL);


      } else if(upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        DebuglnF("Update was aborted");
      }
      delay(0);
    }
  );

  // All other not known 
  server.onNotFound(handleNotFound);
  
  // serves all SPIFFS / LittleFS Web file with 24hr max-age control
  // to avoid multiple requests to ESP
  server.serveStatic("/font", LittleFS, "/font","max-age=86400"); 
  server.serveStatic("/js",   LittleFS, "/js"  ,"max-age=86400"); 
  server.serveStatic("/css",  LittleFS, "/css" ,"max-age=86400"); 
  server.begin();

  // Display configuration
  showConfig();

  Debugln(F("HTTP server started"));
 
  // Light off
  LedOFF(BLU_LED_PIN);

  // xxxxxxx to be completed : param 1 ????
  blu_ticker.attach(1, FlashBlueLed, 1);

  // Update sysinfo every second
  Every_1_Sec.attach(1, Task_1_Sec);

  Every_1_Min.attach(60, Task_1_Min);

  // Init and get the time
  ntp.begin(config.tz,config.ntp_server);

  struct tm timeinfo;
  if(!ntp.getTime(timeinfo)){
    Debugln("Failed to obtain time");
  } else {
    char buf[20];
    strftime(buf,sizeof(buf),"%FT%H:%M:%S",&timeinfo);
    Debugln(buf);
  }
     
  // Mqtt Update if needed
  if (config.mqtt.freq) {
    Tick_mqtt.attach(config.mqtt.freq, Task_mqtt);
    mqtt.Connect(mqttCallback, config.mqtt.host, config.mqtt.port, config.mqtt.topic, config.mqtt.user, config.mqtt.pswd,true);
    mqttStartupLogs();  //send startup logs to mqtt
  }
  
  // Jeedom Update if needed
  if (config.jeedom.freq) 
    Tick_jeedom.attach(config.jeedom.freq, Task_jeedom);

  // HTTP Request Update if needed
  if (config.httpReq.freq) 
    Tick_httpRequest.attach(config.httpReq.freq, Task_httpRequest);

}


/* ======================================================================
Function: loop
Purpose : infinite loop main code
Input   : -
Output  : - 
Comments: -
====================================================================== */
void loop()
{
bool     ret;

  // Do all related network stuff
  server.handleClient();
  
  ota_class.Loop();
  
  // Only once task per loop, let system do it own task
  if (task_1_sec) { 
    UpdateSysinfo(false, false); 
    if (config.mqtt.freq) {
      mqtt.Loop();              // Called for receive subscribe updates
    }

    // gestion du timeout du relais
    if (r_timeout && r_relay_status) {
      r_timeout--;
      if (r_timeout == 0) {
        // Le timout est arrivé à échéance : mise du relay à Off (zéro)
        r_relay_status = 0;
      }
    }
    
    task_1_sec = false; 

  } else if (task_1_min) {

    // ====== update relay status ==========

    // Update relay
    if (r_relay_status)
      relay1.On();
    else
      relay1.Off();

    // Debugf("r_errors = %02x\r\n", r_errors);
    mqttPost(MQTT_RELAY_ERRORS, get_r_errors_str().c_str());
 
    // ==================================
    // Gestion clignotement LED blue
    // Blink only once ; all is ok
    // ==================================
    if (r_errors == 0) {
      flash_blue_led_count = 1; 
    } else { 
        if (r_errors & r_error_send_mqtt) {
          flash_blue_led_count = 3;
        } else {
          if (r_errors & r_error_send_jeedom) {
            flash_blue_led_count = 4; 
          } else {
            if (r_errors & r_error_send_http) {
              flash_blue_led_count = 5;
            }
          }
        }  
    }  

    // Debugln("task_1_min : end");
    task_1_min = false;
  } else if (task_mqtt) {

    ret = mqtt.Connect(mqttCallback, config.mqtt.host, config.mqtt.port, config.mqtt.topic, config.mqtt.user, config.mqtt.pswd,true);
    // Mqtt Publier les données; 
    ret &= mqttPost(MQTT_RELAY_RELAY_1, r_relay_status_str[r_relay_status]);

    if (ret) {
      // Si tout est ok
      r_errors &= ~r_error_send_mqtt;
    } else {
      // Si Connect ou mqttPost a échoué ...
      r_errors |= r_error_send_mqtt;
    }
    
    task_mqtt=false; 
  } else if (task_jeedom) { 
    ret = jeedomPost();
    if (ret) {
      r_errors &= ~r_error_send_jeedom;
    } else {
      r_errors |= r_error_send_jeedom;
    }
    task_jeedom=false;
  } else if (task_httpRequest) { 
    ret = httpRequest();
    if (ret) {
      r_errors &= ~r_error_send_http;
    } else {
      r_errors |= r_error_send_http;
    }
      
    task_httpRequest=false;
  } 

  delay(10);
}
