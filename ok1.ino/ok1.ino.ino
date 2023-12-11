#include "ESP8266WiFi.h"
#include <PubSubClient.h>
#include "ArduinoJson.h"

//#define DASHBOARD_MQTT_SERVER               "192.168.157.43"
//#define DASHBOARD_MQTT_SERVER               "test.mosquitto.org"
#define DASHBOARD_MQTT_SERVER                 "broker.hivemq.com"


#define DASHBOARD_MQTT_PORT                 1883
//#define DASHBOARD_MQTT_USER                 "Hdq"
//#define DASHBOARD_MQTT_PASSWORD             "hdq20122001"

#define DASHBOARD_MQTT_USER                 "Huong"
#define DASHBOARD_MQTT_PASSWORD             "01649583251"

#define DASHBOARD_TOPIC_TEMPERATURE         "system/temperature"
#define DASHBOARD_TOPIC_HUMIDITY            "system/humidity"
#define DASHBOARD_TOPIC_LIGHT               "system/light"
#define DASHBOARD_TOPIC_SOIL                "system/soil"
#define DASHBOARD_TOPIC_Data_sum                "data_sum"


#define DASHBOARD_TOPIC_RELAY1              "relay1/control"
#define DASHBOARD_TOPIC_RELAY2              "relay2/control"

#define DASHBOARD_TOPIC_MODE                "system/mode"

#define DASHBOARD_CONTROL_MSG_LENGTH                6

#define DASHBOARD_BYTE_ON     0x03
#define DASHBOARD_BYTE_OFF    0x04

#define DASHBOARD_BUFFER_LENGTH 6
#define DASHBOARD_DATA_BUFFER_LENGTH (DASHBOARD_BUFFER_LENGTH - 1)

WiFiClient wifiClient;
PubSubClient client(wifiClient);

uint8_t g_R1OnMsg[DASHBOARD_CONTROL_MSG_LENGTH] = {'[', 0x01, DASHBOARD_BYTE_ON, 0x02, ']', 0x00};
uint8_t g_R1OffMsg[DASHBOARD_CONTROL_MSG_LENGTH] = {'[', 0x01, DASHBOARD_BYTE_OFF, 0x05, ']', 0x00};
uint8_t g_R2OnMsg[DASHBOARD_CONTROL_MSG_LENGTH] = {'[', 0x02, DASHBOARD_BYTE_ON, 0x01, ']', 0x00};
uint8_t g_R2OffMsg[DASHBOARD_CONTROL_MSG_LENGTH] = {'[', 0x02, DASHBOARD_BYTE_OFF, 0x06, ']', 0x00};
uint8_t g_autoModeMsg[DASHBOARD_CONTROL_MSG_LENGTH] = {'[', 0x03, DASHBOARD_BYTE_OFF, 0x07, ']', 0x00};
uint8_t g_controlModeMsg[DASHBOARD_CONTROL_MSG_LENGTH] = {'[', 0x03, DASHBOARD_BYTE_ON, 0x00, ']', 0x00};

bool g_receiveFlag = false;
uint8_t g_readData;
uint8_t g_bufferRx[DASHBOARD_BUFFER_LENGTH];
uint8_t g_idx = 0;

const char* g_ssid = "Hdq";
const char* g_password = "hdq20122001";

//const char* g_ssid = "HAIDTH";
//const char* g_password = "12112000";

int g_updateTime = 0;
uint8_t g_ledSta = 0;

void setup(void)
{
    Serial.begin(9600);
    Serial.setTimeout(500);
    pinMode(16, OUTPUT);

    // Connect to WiFi
    WiFi.begin(g_ssid, g_password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("connect err");
        g_ledSta = !g_ledSta;
        digitalWrite(16, g_ledSta);
        delay(500);
    }
    client.setServer(DASHBOARD_MQTT_SERVER, DASHBOARD_MQTT_PORT);
    client.setCallback(Dashboard_Callback);
    DashBoard_ConnectToBroker();
}

void loop()
{
    client.loop();
    if (!client.connected()) {
        DashBoard_ConnectToBroker();
    }
    Dashboard_ReceiveDataFromSTM32();
}



void Dashboard_ReceiveDataFromSTM32(void)
{
    if (Serial.available() > 0)
    {
        g_readData = Serial.read();
        if (g_readData == '[' && g_receiveFlag == false)
        {
            g_receiveFlag = true;
            g_idx = 0;
            g_bufferRx[DASHBOARD_DATA_BUFFER_LENGTH] = '\0';
        }
        else
        {
            if (g_receiveFlag == true)
            {
                if (g_idx < DASHBOARD_DATA_BUFFER_LENGTH)
                {
                    g_bufferRx[g_idx++] = g_readData;
                }
                else if (g_readData == ']' || g_readData == '\n' || g_idx == DASHBOARD_DATA_BUFFER_LENGTH)
                {
                    g_bufferRx[g_idx] = '\0';
                    Dashboard_HandleDataFromSTM32();
                    g_receiveFlag = false;
                    g_idx = 0;
                }
            }
        }
    }
}

void Dashboard_HandleDataFromSTM32(void)
{
    uint8_t checksumVal = 0;
//    char tem[4];
//    char hum[4];
//    char light[4];
//    char soil[4];

    checksumVal = g_bufferRx[0] ^ g_bufferRx[1] ^ g_bufferRx[2] ^ g_bufferRx[3];
    if (checksumVal == g_bufferRx[4])
    { 
//        sprintf(tem, "%d", g_bufferRx[0]);
//        sprintf(hum, "%d", g_bufferRx[1]);
//        sprintf(light, "%d", g_bufferRx[2]);
//        sprintf(soil, "%d", g_bufferRx[3]);
//          Serial.print("%d", g_bufferRx[0]);
      //viết tài liệu JSON vào serial port
        StaticJsonDocument<100> doc; //cấp phát vùng bộ nhớ tại chỗ
        doc["Temperature"] = g_bufferRx[0];
        doc["Humidity"] = g_bufferRx[1]; // 
        doc["Light"] = g_bufferRx[2];
        doc["Soil"] = g_bufferRx[3];
        char buffer[256]; //biến đệm lưu gtri
        serializeJson(doc, buffer); // tuần tự các vùng nhớ của sensor
        client.publish("data_sum", buffer);
        Serial.println(buffer);
    }
}

void DashBoard_ConnectToBroker() {
    while (!client.connected()) {
        String clientId = "ESP8266";
        clientId += String(random(0xffff), HEX);
        if (client.connect(clientId.c_str(), DASHBOARD_MQTT_USER, DASHBOARD_MQTT_PASSWORD)) {
            client.subscribe(DASHBOARD_TOPIC_RELAY1);
            client.subscribe(DASHBOARD_TOPIC_RELAY2);
            client.subscribe(DASHBOARD_TOPIC_MODE);            
            digitalWrite(16, HIGH);
            Serial.print("not connect to Broker");
        } else {
            g_ledSta = !g_ledSta;
            digitalWrite(16, g_ledSta);
            delay(1000);
        }
    }
}

void Dashboard_Callback(char* topic, byte *payload, unsigned int length) {
    char statusMsg[length + 1];
    memcpy(statusMsg, payload, length);
    statusMsg[length] = NULL;
    String topicMsg(statusMsg);
    char buffMsg[1];

    if (String(topic) == DASHBOARD_TOPIC_RELAY1)
    {
        if (topicMsg == "on")
        {
          buffMsg[0] = 0x00;
          Serial.print((char*)buffMsg);          
//            Serial.print((char*)g_R1OnMsg);
            
        }
        else if (topicMsg == "off")
        {
          buffMsg[1] = 0x01;
          Serial.print((char*)buffMsg);          
          
//            Serial.print((char*)g_R1OffMsg);
        }
    }
    else if (String(topic) == DASHBOARD_TOPIC_RELAY2)
    {
        if (topicMsg == "on")
        {
            Serial.print((char*)g_R2OnMsg);
        }
        else if (topicMsg == "off")
        {
            Serial.print((char*)g_R2OffMsg);
        }
    }
    else if (String(topic) == DASHBOARD_TOPIC_MODE)
    {
        if (topicMsg == "on")
        {
            Serial.print((char*)g_autoModeMsg);
        }
        else if (topicMsg == "off")
        {
            Serial.print((char*)g_controlModeMsg);
            
        }
    }    
}
