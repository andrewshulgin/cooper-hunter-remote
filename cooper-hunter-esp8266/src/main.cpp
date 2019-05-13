#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESPAsyncUDP.h>
#include <ESPAsyncTCP.h>
#include <ArduinoOTA.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <credentials.h>
#include <gree.h>

const uint16_t kPin = 4;
const uint16_t kBaud = 9600;
const uint16_t kCaptureBufferSize = 1024;
#if DECODE_AC
const uint8_t kTimeout = 50;
#else
const uint8_t kTimeout = 15;
#endif
const uint16_t kMinUnknownSize = 12;
const char *kSSID = SSID;
const char *kPSK = PSK;
const uint16_t kPort = 1234;

bool send_command = false;
IRrecv irrecv(kPin, kCaptureBufferSize, kTimeout, true);
Gree gree(kPin);
decode_results results;
AsyncUDP udp;
AsyncServer* server = new AsyncServer(kPort);
char last_state[24];
static std::vector<AsyncClient*> clients;

bool parseStateMessage(char *data)
{
    if (strnlen(data, 20) < 20)
        return false;

    if (data[1] != ',' ||
        data[3] != ',' ||
        data[6] != ',' ||
        data[8] != ',' ||
        data[10] != ',' ||
        data[12] != ',' ||
        data[14] != ',' ||
        data[16] != ',' ||
        data[18] != ',' ||
        (data[20] != '\r' && data[20] != '\n'))
        return false;

    bool power = data[0] == '1';
    uint8_t mode = data[2] - 48;
    uint8_t temp = atoi((char *)&data[4]);
    uint8_t fan = data[7] - 48;
    bool turbo = data[9] == '1';
    bool xfan = data[11] == '1';
    bool light = data[13] == '1';
    bool sleep = data[15] == '1';
    bool swingAuto = data[17] == '1';
    uint8_t swingPosition = data[19] - 48;

    gree.setPower(power);
    gree.setMode(mode);
    gree.setTemp(temp);
    gree.setFan(fan);
    gree.setTurbo(turbo);
    gree.setXFan(xfan);
    gree.setLight(light);
    gree.setSleep(sleep);
    gree.setSwingVertical(swingAuto, swingPosition);

    return true;
}

static void handleError(void* arg, AsyncClient* client, int8_t error) {
}

static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
    if (parseStateMessage((char*) data))
        send_command = true;
}

static void handleDisconnect(void* arg, AsyncClient* client) {
    clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
}

static void handleTimeOut(void* arg, AsyncClient* client, uint32_t time) {
}

static void handleNewClient(void* arg, AsyncClient* client) {
	clients.push_back(client);
	client->onData(&handleData, NULL);
	client->onError(&handleError, NULL);
	client->onDisconnect(&handleDisconnect, NULL);
	client->onTimeout(&handleTimeOut, NULL);
    if (client->space() > 32 && client->canSend()) {
        client->add(last_state, strlen(last_state));
        client->send();
    }
}

void broadcastStateMessage(Gree ac)
{
    sprintf(
        last_state,
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
        ac.getPower(),
        ac.getMode(),
        ac.getTemp(),
        ac.getFan(),
        ac.getTurbo(),
        ac.getXFan(),
        ac.getLight(),
        ac.getSleep(),
        ac.getSwingVerticalAuto(),
        ac.getSwingVerticalPosition());
    Serial.print(last_state);
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i]->space() > 32 && clients[i]->canSend()) {
            clients[i]->add(last_state, strlen(last_state));
            clients[i]->send();
        }
	}
    udp.broadcastTo(last_state, kPort);
}

void setup()
{
    Serial.begin(kBaud);

#if DECODE_HASH
    irrecv.setUnknownThreshold(kMinUnknownSize);
#endif
    irrecv.enableIRIn();

    WiFi.mode(WIFI_STA);
    WiFi.begin(kSSID, kPSK);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("WiFi Failed");
        delay(10000);
        ESP.restart();
    }

    ArduinoOTA.onStart([]() {
        Serial.printf(
            "OTA flashing %s\r\n",
            ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("OTA end\r\n");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA progress: %u%%\r\n", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA error[%u]: ", error);
        switch (error)
        {
        case OTA_AUTH_ERROR:
            Serial.printf("Auth Failed");
            break;
        case OTA_BEGIN_ERROR:
            Serial.printf("Begin Failed");
            break;
        case OTA_CONNECT_ERROR:
            Serial.printf("Connect Failed");
            break;
        case OTA_RECEIVE_ERROR:
            Serial.printf("Receive Failed");
            break;
        case OTA_END_ERROR:
            Serial.printf("End Failed");
            break;
        default:
            Serial.printf("Unknown Error");
            break;
        }
    });
    ArduinoOTA.begin();
    server->onClient(&handleNewClient, server);
    server->begin();
}

void loop()
{
    ArduinoOTA.handle();
    if (send_command)
    {
        irrecv.disableIRIn();
        pinMode(kPin, OUTPUT);
        digitalWrite(kPin, HIGH);
        delay(20);
        gree.begin();
        gree.send();
        broadcastStateMessage(gree);
        delay(20);
        digitalWrite(kPin, LOW);
        pinMode(kPin, INPUT_PULLUP);
        irrecv.enableIRIn();
        send_command = false;
    }
    if (irrecv.decode(&results))
    {
        if (results.overflow)
            yield();

        if (results.decode_type == GREE)
        {
            Gree ac(0);
            ac.setRaw(results.state);
            broadcastStateMessage(ac);
        }
        yield();
    }
}
