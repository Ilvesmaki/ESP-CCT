#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <string.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>
#include <ArduinoJson.h>

#include <EEPROM.h>

#include "Ticker.h"
#include "cct_control.hpp"
#include "pin_config.h"

#define CONFIG_WIFI_SSID "ESP_CONFIG-AP"
#define CONFIG_WIFI_PASSWD "SecretKey!"

#define BAUDRATE 115200

#define PARAM_LEN 64

#define PIR_TIMER_IN_MS 30000

#define JSON_DOC_SIZE 1024
#define CREDENTIALS_FILENAME "/credentials.json"

#define MQ_USERNAME_PARAM "mq_username"
#define MQ_PASSWORD_PARAM "mq_password"
#define MQ_PORT_PARAM "mq_port"
#define MQ_SERVER_PARAM "mq_server"

#define MQ_WW_CT_RATIO_PARAM "ww_ct_ratio"
#define MQ_BRIGHTNESS_PARAM "brightness"
#define MQ_OUTPUT_STATUS_PARAM "output"
#define MQ_DELAYED_OFF_PARAM "delayed_off"

#define MQ_IN_TOPIC "KITCHEN/LIGHTS/COUNTERTOPS/IN"
#define MQ_OUT_TOPIC "KITCHEN/LIGHTS/COUNTERTOPS/OUT"

#define BACKSPACE "\b \b"

//define your default values here, if there are different values in config.json, they are overwritten.
char mq_port[PARAM_LEN] = "8883";
// Update these with values suitable for your network.
char mq_username[PARAM_LEN] = "";
char mq_password[PARAM_LEN] = "";
char mq_server[PARAM_LEN] = "localhost";

// A single, global CertStore which can be used by all connections.
// Needs to stay live the entire time any of the WiFiClientBearSSLs
// are present.
BearSSL::CertStore certStore;
BearSSL::WiFiClientSecure bear;

WiFiClientSecure espClient;
PubSubClient client;

/* EEPROM addresses */
const int CONNECTION_STATUS_ADDR = 0;
const int WW_CW_RATIO_ADDR = CONNECTION_STATUS_ADDR + sizeof(int);
const int BRIGHTNESS_ADDR = WW_CW_RATIO_ADDR + sizeof(float);

#define CONNECTION_ERROR 1
#define ALLOWED_CONNECTION_ATTEMPTS 5

#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];

unsigned int cw_val_g = 100;
unsigned int ww_val_g = 100;

cct_control CCT{&cw_val_g, &ww_val_g, OFF, ON, ANALOG_MAX_VAL};

Ticker timer1;
Ticker timer2;

ICACHE_RAM_ATTR void tickCCT()
{
    CCT.tick();
}

ICACHE_RAM_ATTR void checkPIR()
{
    int movement_detected = digitalRead(PIR_PIN);
    if(movement_detected)
    {
        CCT.delayedOff(PIR_TIMER_IN_MS);
    }
}

void valuesUpdated()
{
    float cw_ww_val = CCT.getWWCWRatio();
    float brightness = CCT.getBrightness();
    unsigned int output = (unsigned int)CCT.getOutputStatus();

    EEPROM.put(WW_CW_RATIO_ADDR, cw_ww_val);
    EEPROM.put(BRIGHTNESS_ADDR, brightness);
    EEPROM.commit();

    snprintf(msg, MSG_BUFFER_SIZE, "{\"%s\":%f, \"%s\":%f \"%s\":%ld}", MQ_WW_CT_RATIO_PARAM, cw_ww_val, MQ_BRIGHTNESS_PARAM, brightness, MQ_OUTPUT_STATUS_PARAM, output);
    client.publish(MQ_OUT_TOPIC, msg);
    Serial.println(msg);
}

void loadCredentials(){
    File f = LittleFS.open(CREDENTIALS_FILENAME, "r");
    if(!f)
    {
        Serial.print("Couldn't open file: ");
        Serial.println(CREDENTIALS_FILENAME);
        return;
    }
    if(f.size() > JSON_DOC_SIZE)
    {
        Serial.println("Error: File size is larger than allowed buffer size!");
        f.close();
        return;
    }
    char file_content[JSON_DOC_SIZE];
    f.readBytes(file_content, JSON_DOC_SIZE);
    DynamicJsonDocument doc(JSON_DOC_SIZE);
    deserializeJson(doc, file_content);

    JsonVariant param = doc[MQ_USERNAME_PARAM];
    if(!param.isNull()) {
        strcpy(mq_username, param.as<const char*>());
    }
    param = doc[MQ_PASSWORD_PARAM];
    if(!param.isNull()) {
        strcpy(mq_password, param.as<const char*>());
    }
    param = doc[MQ_PORT_PARAM];
    if(!param.isNull()) {
        strcpy(mq_port, param.as<const char*>());
    }
    param = doc[MQ_SERVER_PARAM];
    if(!param.isNull()) {
        strcpy(mq_server, param.as<const char*>());
    }
    
    f.close();
}

void storeCredentials(){
    
    char file_content[JSON_DOC_SIZE];
    
    DynamicJsonDocument doc(JSON_DOC_SIZE);

    doc[MQ_USERNAME_PARAM] = mq_username;
    doc[MQ_PASSWORD_PARAM] = mq_password;
    doc[MQ_PORT_PARAM] = mq_port;
    doc[MQ_SERVER_PARAM] = mq_server;

    serializeJson(doc, file_content);

    File f = LittleFS.open(CREDENTIALS_FILENAME, "w");
    if(!f)
    {
        Serial.print("Couldn't open file: ");
        Serial.println(CREDENTIALS_FILENAME);
        return;
    }
    
    f.write(file_content, strlen(file_content));
    
    f.close();
}

void setupCredentials(){
    WiFiManager wm;
    WiFiManagerParameter custom_mq_server("server", "mqtt server", mq_server, PARAM_LEN);
    WiFiManagerParameter custom_mq_port("port", "mqtt port", mq_port, PARAM_LEN);
    WiFiManagerParameter custom_mqtt_username("username", "mqtt username", mq_username, PARAM_LEN);
    WiFiManagerParameter custom_mqtt_password("mq_password", "mqtt password", mq_password, PARAM_LEN);
    wm.addParameter(&custom_mq_server);
    wm.addParameter(&custom_mq_port);
    wm.addParameter(&custom_mqtt_username);
    wm.addParameter(&custom_mqtt_password);

    int connection_status;
    EEPROM.get(CONNECTION_STATUS_ADDR, connection_status);
    int config_button_status = digitalRead(CONFIG_BUTTON_PIN);
    /* use wm.autoConnect to automatically connect to the servers or */  
    if(connection_status == CONNECTION_ERROR || config_button_status == 0)
    {
        if (!wm.startConfigPortal(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWD)) {
            Serial.println("failed to connect and hit timeout");
            delay(3000);
            //reset and try again, or maybe put it to deep sleep
            ESP.restart();
            delay(5000);
        }
        connection_status = 0;
        EEPROM.put(CONNECTION_STATUS_ADDR, connection_status);
        EEPROM.commit();
    }
    else
    {
        if (!wm.autoConnect(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWD)) {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        ESP.restart();
        delay(5000);
    }
    }

    //read updated parameters
    strcpy(mq_server, custom_mq_server.getValue());
    strcpy(mq_port, custom_mq_port.getValue());
    strcpy(mq_username, custom_mqtt_username.getValue());
    strcpy(mq_password, custom_mqtt_password.getValue());
}


void setDateTime() {
    // You can use your own timezone, but the exact time is not used at all.
    // Only the date is needed for validating the certificates.
    configTime(TZ_Europe_Helsinki, "pool.ntp.org", "time.nist.gov");

    Serial.print("Waiting for NTP time sync: ");
    time_t now = time(nullptr);
    unsigned int counter = 0;
    while (now < 8 * 3600 * 2) {
        delay(100);
        if(counter == 3)
        {
            Serial.print(BACKSPACE);
            delay(20);
            Serial.print(BACKSPACE);
            delay(20);
            Serial.print(BACKSPACE);
            delay(20);
            counter = 0;
        }
        else
        {
            Serial.print(".");
            counter++;
        }
        now = time(nullptr);
    }
    Serial.println();

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

void parseMessage(byte * payload, unsigned int length)
{
    DynamicJsonDocument doc(JSON_DOC_SIZE);
    JsonVariant param;

    deserializeJson(doc, (char *)payload);

    param = doc[MQ_WW_CT_RATIO_PARAM];
    if(!param.isNull()) {
        float ww_cw_ratio = param.as<float>();
        CCT.setWWCWRatio(ww_cw_ratio);
    }

    param = doc[MQ_BRIGHTNESS_PARAM];
    if(!param.isNull()) {
        float brightness = param.as<float>();
        CCT.setBrightness(brightness);
    }

    param = doc[MQ_OUTPUT_STATUS_PARAM];
    if(!param.isNull()) {
        unsigned int output = param.as<unsigned int>();
        if(output == 1)
        {
            CCT.setOutputOn();
        }
        else
        {
            CCT.setOutputOff();
        }
        
    }
    param = doc[MQ_DELAYED_OFF_PARAM];
    if(!param.isNull()) {
        unsigned int time = param.as<unsigned int>();
        if(time != 0)
        {
            CCT.delayedOff(time);
        }
        else
        {
            CCT.delayedOff(PIR_TIMER_IN_MS);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]: ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    if(strcmp((const char*)topic, MQ_IN_TOPIC) == 0)
    {
        parseMessage(payload, length);
    }
}


void reconnect() {
    // Loop until we’re reconnected
    int attempts = 0;
    while (!client.connected()) {
        if(attempts >= ALLOWED_CONNECTION_ATTEMPTS)
        {
            int connection_status = CONNECTION_ERROR;
            EEPROM.put(CONNECTION_STATUS_ADDR, connection_status);
            EEPROM.commit();
            ESP.restart();
            delay(5000);
        }
        Serial.print("Attempting MQTT connection…");
        char client_id[32]; 
        uint32_t flash_id = ESP.getFlashChipId();
        snprintf (client_id, 32, "ESP8266Client-%ld", flash_id);
        // Attempt to connect
        // Insert your password
        if (client.connect(client_id, mq_username, mq_password)) {
            Serial.print(" connectet with client id: ");
            Serial.println(client_id);
            // Once connected, publish an announcement…
            valuesUpdated();
            client.subscribe(MQ_IN_TOPIC);
        } else {
            Serial.print("failed, rc = ");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
        attempts++;
    }
}

status_t last_btn_status;

ICACHE_RAM_ATTR void buttonChangeISR(void)
{
    status_t new_button_status = digitalRead(BUTTON_PIN) == 0 ? ON : OFF;
    
    digitalWrite(LED_BUILTIN, new_button_status == ON ? LOW: HIGH);
    CCT.setButtonStatus(new_button_status);
}

void setupCCT()
{
    float cw_ww_val;
    float brightness;
    
    EEPROM.get(WW_CW_RATIO_ADDR, cw_ww_val);
    EEPROM.get(BRIGHTNESS_ADDR, brightness);

    snprintf(msg, MSG_BUFFER_SIZE, "loaded values; cw_ww_val:%f brightness:%f", cw_ww_val, brightness);
    Serial.println(msg);
    cw_ww_val = fmax(0.0f, fmin(1.0f, cw_ww_val));
    brightness = fmax(0.0f, fmin(1.0f, brightness));

    last_btn_status = digitalRead(BUTTON_PIN) == 0 ? ON : OFF;

    CCT.setBrightness(brightness);
    CCT.setWWCWRatio(cw_ww_val);
    CCT.setButtonStatus(last_btn_status);
    CCT.start();
    CCT.setUpdateCB(valuesUpdated);

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonChangeISR, CHANGE);
    timer1.attach_ms_scheduled(1, tickCCT);
    timer2.attach_ms_scheduled(1000, checkPIR);

}

void setup() {
    delay(500);
    // When opening the Serial Monitor, select BAUDRATE Baud
    Serial.begin(BAUDRATE);
    delay(500);

    setupPins();

    LittleFS.begin();
    EEPROM.begin(64);

    loadCredentials();

    setupCredentials();

    storeCredentials();

    setDateTime();
    
    setupCCT();
    // you can use the insecure mode, when you want to avoid the certificates
    //espclient->setInsecure();

    int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
    Serial.printf("Number of CA certs read: %d\n", numCerts);
    if (numCerts == 0) {
        Serial.printf("No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory before running?\n");
        return; // Can't connect to anything w/o certs!
    }

    
    // Integrate the cert store with this connection
    bear.setCertStore(&certStore);

    client.setClient(bear);

    client.setServer(mq_server, atoi(mq_port));
    client.setCallback(callback);
    
}

unsigned long lastBtnChange = 0;

void loop() {
    analogWrite(CW_LED_PIN, (int)cw_val_g);
    analogWrite(WW_LED_PIN, (int)ww_val_g);

    client.loop();

    if (!client.connected()) {
        reconnect();
    }
}
