/*
 * AudioGuard - WiFire Sketch
 *
 * Based on: https://github.com/ricklon/deIPcK/blob/master/DEWFcK/examples/WiFiUDPEchoClient/WiFiUDPEchoClient.pde
 */

#include <MRF24G.h>
#include <DEIPcK.h>
#include <DEWFcK.h>

// CONSTANTS

// SSID
const char *szSsid = "waifai";

// select 1 for the security you want, or none for no security
#define USE_WPA2_PASSPHRASE
//#define USE_WPA2_KEY
//#define USE_WEP40
//#define USE_WEP104
//#define USE_WF_CONFIG_H

// modify the security key to what you have.
#if defined(USE_WPA2_PASSPHRASE)

    const char * szPassPhrase = "";
    #define WiFiConnectMacro() deIPcK.wfConnect(szSsid, szPassPhrase, &status)

#elif defined(USE_WPA2_KEY)

    DEWFcK::WPA2KEY key = { 0x27, 0x2C, 0x89, 0xCC, 0xE9, 0x56, 0x31, 0x1E, 
                            0x3B, 0xAD, 0x79, 0xF7, 0x1D, 0xC4, 0xB9, 0x05, 
                            0x7A, 0x34, 0x4C, 0x3E, 0xB5, 0xFA, 0x38, 0xC2, 
                            0x0F, 0x0A, 0xB0, 0x90, 0xDC, 0x62, 0xAD, 0x58 };
    #define WiFiConnectMacro() deIPcK.wfConnect(szSsid, key, &status)

#elif defined(USE_WEP40)

    const int iWEPKey = 0;
    DEWFcK::WEP40KEY keySet = {    0xBE, 0xC9, 0x58, 0x06, 0x97,     // Key 0
                                    0x00, 0x00, 0x00, 0x00, 0x00,     // Key 1
                                    0x00, 0x00, 0x00, 0x00, 0x00,     // Key 2
                                    0x00, 0x00, 0x00, 0x00, 0x00 };   // Key 3
    #define WiFiConnectMacro() deIPcK.wfConnect(szSsid, keySet, iWEPKey, &status)

#elif defined(USE_WEP104)

    const int iWEPKey = 0;
    DEWFcK::WEP104KEY keySet = {   0x3E, 0xCD, 0x30, 0xB2, 0x55, 0x2D, 0x3C, 0x50, 0x52, 0x71, 0xE8, 0x83, 0x91,   // Key 0
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // Key 1
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // Key 2
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // Key 3
    #define WiFiConnectMacro() deIPcK.wfConnect(szSsid, keySet, iWEPKey, &status)

#elif defined(USE_WF_CONFIG_H)

    #define WiFiConnectMacro() deIPcK.wfConnect(0, &status)

#else   // no security - OPEN

    #define WiFiConnectMacro() deIPcK.wfConnect(szSsid, &status)

#endif

typedef enum
{
    CONNECT,
    RESOLVEENDPOINT,
    WRITE,
    ERR
} STATE;

STATE state = CONNECT;

UDPSocket udpClient;
IPSTATUS status = ipsSuccess;
IPEndPoint epRemote;

// AudioGuard config

#define LED_PIN 13
#define MICROPHONE_PIN A0
#define SAMPLE_FREQ 100
const char *agServerIp = "192.168.1.110";
const uint16_t agServerPort = 9001;
double refVoltage = 1;

void blinkForever()
{
    for (;;)
    {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(500);   
    }
}

void setup()
{
    Serial.begin(9600);
    Serial.println("AudioGuard client 1.0");
    Serial.println("");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    Serial.println("Reading microphone voltage value for 40dB");
    double v = readMicrophoneVoltage();
    Serial.println("Microphone voltage at 40dB is:");
    Serial.println(v);
    refVoltage = v;

    Serial.println("Connecting to WiFi...");
}

void loop() {
    switch(state)
    {
        case CONNECT:
            if (WiFiConnectMacro())
            {
                Serial.println("WiFi connected.");
                deIPcK.begin();
                state = RESOLVEENDPOINT;
                digitalWrite(LED_PIN, LOW);
            }
            else if(IsIPStatusAnError(status))
            {
                Serial.print("Unable to connection, status: ");
                Serial.println(status, DEC);
                state = ERR;
            }
            break;

        case RESOLVEENDPOINT:
            if (deIPcK.resolveEndPoint(agServerIp, agServerPort, epRemote, &status))
            {
                if (deIPcK.udpSetEndPoint(epRemote, udpClient, portDynamicallyAssign, &status))
                {
                    state = WRITE;
                }
            }

            if (IsIPStatusAnError(status))
            {
                Serial.print("Unable to resolve endpoint, error: 0x");
                Serial.println(status, HEX);
                state = ERR;
            }
            break;

       case WRITE:
            double v, volume;
            long int written;
            if (deIPcK.isIPReady(&status))
            {
                v = readMicrophoneVoltage();
                volume = 20 * log(v / refVoltage);
                Serial.println("Writing out Datagram, volume:");
                Serial.println(volume);
  
                written = udpClient.writeDatagram((byte*)&volume, sizeof(double));
                if (written == 0)
                {
                    Serial.println("Unable to write datagram.");
                    state = ERR;
                }
            }
            else if (IsIPStatusAnError(status))
            {
                Serial.print("Lost the network, error: 0x");
                Serial.println(status, HEX);
                state = ERR;
            }
            break;

        case ERR:
        default:
            udpClient.close();
            delay(100);
            state = RESOLVEENDPOINT;
            break;
    }

    // keep the stack alive each pass through the loop()
    DEIPcK::periodicTasks();
}

double readMicrophoneVoltage()
{
    unsigned long startMillis = millis();
    unsigned int peakToPeak = 0;

    unsigned int signalMax = 0;
    unsigned int signalMin = 1024;

    while (millis() - startMillis < SAMPLE_FREQ)
    {
        unsigned int sample = analogRead(MICROPHONE_PIN);
        if (sample < 1024)
        {
            if (sample > signalMax)
            {
                signalMax = sample;
            }
            else if (sample < signalMin)
            {
                signalMin = sample;
            }
        }
    }

    peakToPeak = signalMax - signalMin;
    return (peakToPeak * 5.0) / 1024;
}