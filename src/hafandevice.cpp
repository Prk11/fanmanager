#include <Arduino.h>
#include <ArduinoHA.h>
#include <WiFi.h>
// #include <ESPmDNS.h>

class HAFanDevice
{
public:
    HAFanDevice(String server, String login, String password)
    {
        uint64_t chipId = ESP.getEfuseMac();
        byte deviceId[6];
        for (int i = 0; i < 17; i = i + 8)
        {
            deviceId[i / 8] = ((chipId >> (40 - i)) & 0xff) << i;
        }

        device = new HADevice(deviceId, sizeof(deviceId));
        device->setSoftwareVersion("1.0.1");

        client = new WiFiClient();
        mqtt = new HAMqtt(*client, *device);

        fan = new HAFan("IdFan", HAFan::SpeedsFeature);
        fan->setName("Вентилятор");
        fan->setSpeedRangeMin(1);
        fan->setSpeedRangeMax(3);
        this->server = server;
        this->login = login;
        this->password = password;
    }

    ~HAFanDevice()
    {
        delete fan;
        delete mqtt;
        delete client;
        delete device;
    }

    void onStateCommand(HAFAN_STATE_CALLBACK(callback))
    {
        _stateCallback = callback;
    }

    void onSpeedCommand(HAFAN_SPEED_CALLBACK(callback))
    {
        _speedCallback = callback;
    }

    HAFan *getFan()
    {
        return fan;
    }

    void begin()
    {
        fan->onStateCommand(_stateCallback);
        fan->onSpeedCommand(_speedCallback);
        this->ipServer.fromString(server);
        mqtt->begin(ipServer, 1883, login.c_str(), password.c_str());
    }

    void stop()
    {
        mqtt->disconnect();
    }

    void run()
    {
        mqtt->loop();
    }

    bool status()
    {
        // Serial.printf("MQTT State: %d, %s\n", mqtt->getState(), ipServer.toString());
        return mqtt->getState() == mqtt->StateConnected;
    }

private:
    IPAddress ipServer;
    String server;
    String login;
    String password;
    HADevice *device;
    WiFiClient *client;
    HAMqtt *mqtt;
    HAFan *fan;

    HAFAN_STATE_CALLBACK(_stateCallback);
    HAFAN_SPEED_CALLBACK(_speedCallback);
};