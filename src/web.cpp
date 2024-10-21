#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "hafandevice.cpp"
#include "esp_wifi.h"

#define host "fanmanager"
#define TIMEOUTMAX 60

enum GPIOType
{
    BUTTON,
    GPIO1,
    GPIO2,
    GPIO3
};

enum BUTTONState
{
    FREE,
    PRESSED,
    TOGGLETOFREE,
    TOGGLETOPRESSED
};

class Web
{
public:
    WebServer *server = NULL;

    Web(HardwareSerial *DBG_PORT = &Serial)
    {
        oldbutton = HIGH;
        initGPIO();
        DBG_OUTPUT_PORT = DBG_PORT;
        if (DBG_OUTPUT_PORT != NULL)
        {
            DBG_OUTPUT_PORT->begin(115200);
            DBG_OUTPUT_PORT->print("\n");
            DBG_OUTPUT_PORT->setDebugOutput(true); 
        }
        if (!SPIFFS.begin(true))
        {
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->println("SPIFFS Mount Failed");
        }
        else
        {
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->println("SPIFFS Mount OK");
        }

        if (DBG_OUTPUT_PORT != NULL)
            DBG_OUTPUT_PORT->println("Starting the WebServer");

        server = new WebServer(80);
        _haFanDevice = nullptr;
        FILESYSTEM = &SPIFFS;
        server->on("/list", HTTP_GET, [this]()
                   { this->handleFileList(); });

        server->on("/info", HTTP_GET, [this]()
                   { this->infoChip(); });

        server->on("/delete", HTTP_DELETE, [this]()
                   { this->handleFileDelete(); });
        server->on(
            "/upload", HTTP_POST, [this]()
            { this->server->send(200, "text/plain", ""); },
            [this]()
            { this->handleFileUpload(); });

        server->on("/fan", HTTP_GET, [this]()
                   { this->onChangeSpeed(); });

        server->on("/wifi", HTTP_POST, [this]()
                   { this->onSetWiFi(); });

        publishFileList("/");
        server->onNotFound([this]() {
                       File file;
                        if (this->FILESYSTEM->exists("/404.htm"))
                            file = this->FILESYSTEM->open("/404.htm");
                        else if (this->FILESYSTEM->exists("/404.html"))
                            file = this->FILESYSTEM->open("/404.html");
                        this->server->streamFile(
                                file,
                                "text/html");
                        file.close(); 
                        });
        server->on("/", HTTP_GET, [this]()
                   {
                        File file;
                        if (this->FILESYSTEM->exists("/index.htm"))
                            file = this->FILESYSTEM->open("/index.htm");
                        else if (this->FILESYSTEM->exists("/index.html"))
                            file = this->FILESYSTEM->open("/index.html");
                        this->server->streamFile(
                                file,
                                "text/html");
                        file.close(); });

        server->on("/pins", HTTP_POST, [this]()
                   { this->onSetPins(); });
        server->on("/ha", HTTP_POST, [this]()
                   { this->onHandlerHAParameters(); });

        if (DBG_OUTPUT_PORT != NULL)
            DBG_OUTPUT_PORT->println("WebServer initialization complete");

        loadConfig();
    }

    ~Web()
    {
        delete _haFanDevice;
        uint8_t button = getNumGPIO(BUTTON);
        detachInterrupt(button);
        server->close();
        delete server;
    }

    void wifiInit()
    {
        wl_status_t wifistatus = wifiInitSTA();
        if (wifistatus == WL_CONNECTED)
        {
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->println("Попытка подключиться к роутеру завершилась удачей");
        }
        else
        {
            wifiInitAP();
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->println("Попытка безуспешна, создаем точку доступа.");
        }
    }

    void wifiInitAP()
    {
        if (WiFi.getMode() == WIFI_MODE_AP)
        {
            return;
        }
        if (WiFi.getMode() == WIFI_MODE_STA && WiFi.isConnected())
        {
            WiFi.disconnect(true);
        }
        WiFi.mode(WIFI_AP);
        String SSID = jsonConfig["SSID"];
        if (jsonConfig["SSID"].isNull())
            SSID = String(host);
        WiFi.softAP(SSID);
        if ((DBG_OUTPUT_PORT != NULL))
        {
            DBG_OUTPUT_PORT->print("Точка доступа создана. SSID: ");
            DBG_OUTPUT_PORT->print(SSID);
            DBG_OUTPUT_PORT->println(". Подключение не требует пароля");
            DBG_OUTPUT_PORT->print("IP: ");
            DBG_OUTPUT_PORT->println(WiFi.softAPIP());
        }
    }

    wl_status_t wifiInitSTA(String SSID="", String Password = "")
    {
        if (WiFi.getMode() == WIFI_MODE_AP && WiFi.isConnected())
        {
            WiFi.disconnect(true);
        }

        int timeout = TIMEOUTMAX * 2;
        if (DBG_OUTPUT_PORT != NULL)
            DBG_OUTPUT_PORT->println("Попытка подключиться к роутеру");
        WiFi.setHostname(host);
        WiFi.mode(WIFI_STA);
        if (SSID=="") {
            WiFi.begin();
        }
        else {
            WiFi.persistent(true);
            WiFi.begin(SSID.c_str(),Password.c_str());
        }
        wl_status_t wifistatus = WiFi.status();
        while (wifistatus != WL_CONNECTED)
        {
            delay(500);
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->print(".");
            wifistatus = WiFi.status();
            if (timeout-- < 0)
                break;
        }
        if ((DBG_OUTPUT_PORT != NULL) && (wifistatus == WL_CONNECTED))
        {
            DBG_OUTPUT_PORT->println("OK");
            DBG_OUTPUT_PORT->print("IP: ");
            DBG_OUTPUT_PORT->println(WiFi.localIP());
        } else if (DBG_OUTPUT_PORT != NULL) {
            DBG_OUTPUT_PORT->println("Error");
        }
        return wifistatus;
    }

    void haInit()
    {
        if (jsonConfig["haEnable"])
        {
            if (_haFanDevice == nullptr)
                _haFanDevice = new HAFanDevice(jsonConfig["haServer"], jsonConfig["haLogin"], jsonConfig["haPassword"]);
            _haFanDevice->onSpeedCommand(_speedCallback);
            _haFanDevice->onStateCommand(_stateCallback);
            getFanDevice()->begin();
        }
        else
        {
            delete _haFanDevice;
            _haFanDevice = nullptr;
        }
    }

    void begin()
    {
        loadConfig();
        server->begin();
        haInit();
    }

    void stop()
    {
        server->stop();
        saveConfig();
        if (_haFanDevice != nullptr)
        {
            delete _haFanDevice;
            _haFanDevice = nullptr;
        }
    }

    HAFanDevice *getFanDevice()
    {
        return _haFanDevice;
    }

    void onStateCommand(HAFAN_STATE_CALLBACK(callback))
    {
        _stateCallback = callback;
    }

    void onSpeedCommand(HAFAN_SPEED_CALLBACK(callback))
    {
        _speedCallback = callback;
    }

    void StateCommand(bool state, HAFan *sender)
    {
        if (!state)
        {
            sender->setSpeed(0);
            changeSpeed(0);
        }
        else
        {
            changeSpeed(1);
        }
        sender->setState(state);
    }

    void SpeedCommand(uint16_t speed, HAFan *sender)
    {
        changeSpeed(speed);
        if (speed > 0)
        {
            sender->setState(true);
            sender->setSpeed(speed);
        }
        else
        {
            sender->setSpeed(0);
        }
    }

    void initGPIO()
    {
        uint8_t button = getNumGPIO(BUTTON);
        uint8_t gpio1 = getNumGPIO(GPIO1);
        uint8_t gpio2 = getNumGPIO(GPIO2);
        uint8_t gpio3 = getNumGPIO(GPIO3);
        if ((button != -1) && (gpio1 != -1) && (gpio2 != -1) && (gpio3 != -1))
        {
            pinMode(button, INPUT_PULLUP);
            pinMode(gpio1, OUTPUT);
            pinMode(gpio2, OUTPUT);
            pinMode(gpio3, OUTPUT);

            digitalWrite(gpio1, HIGH);
            digitalWrite(gpio2, HIGH);
            digitalWrite(gpio3, HIGH);
            oldbutton = HIGH;
        }
        if (DBG_OUTPUT_PORT != NULL)
            DBG_OUTPUT_PORT->printf("Loading pins(Button: %d. GPIO1:%d,GPIO2:%d,GPIO3:%d)\n", button, gpio1, gpio2, gpio3);
    }

    BUTTONState getButton()
    {
        uint8_t button = getNumGPIO(BUTTON);
        uint8_t state = digitalRead(button);
        switch (state)
        {
        case LOW:
            if (oldbutton == HIGH)
            {
                if (DBG_OUTPUT_PORT != NULL)
                    DBG_OUTPUT_PORT->println("Switching to mode: AP");
                oldbutton = LOW;
                return TOGGLETOPRESSED;
            }
            return PRESSED;
        default:
            if (oldbutton == LOW)
            {
                if (DBG_OUTPUT_PORT != NULL)
                    DBG_OUTPUT_PORT->println("Switching to mode: STA");
                oldbutton = HIGH;
                return TOGGLETOFREE;
            }
            return FREE;
        }
    }

    void loadConfig()
    {
        File file = FILESYSTEM->open("/config.json");
        char *buf = new char[file.size()];
        file.readBytes(buf, file.size());
        deserializeJson(jsonConfig, buf);
        file.close();
        if (DBG_OUTPUT_PORT != NULL)
            DBG_OUTPUT_PORT->println("Config loaded");
    }

    void saveConfig()
    {
        File file = FILESYSTEM->open("/config.json", "w");
        String buf;
        serializeJson(jsonConfig, buf);
        file.print(buf);
        file.close();
        if (DBG_OUTPUT_PORT != NULL)
            DBG_OUTPUT_PORT->println("Config saved");
    }

    void changeSpeed(short speed)
    {
        this->speed = speed;
        uint8_t gpio1 = getNumGPIO(GPIO1);
        uint8_t gpio2 = getNumGPIO(GPIO2);
        uint8_t gpio3 = getNumGPIO(GPIO3);
        HAFan *fan = nullptr;
        if (getFanDevice())
            fan = getFanDevice()->getFan();
        switch (speed)
        {
        case 0:
            if ((gpio1 != -1) && (gpio2 != -1) && (gpio3 != -1))
            {
                digitalWrite(gpio1, HIGH);
                digitalWrite(gpio2, HIGH);
                digitalWrite(gpio3, HIGH);
                if (fan)
                {
                    fan->setState(false);
                    fan->setSpeed(0);
                    if (DBG_OUTPUT_PORT != NULL)
                        DBG_OUTPUT_PORT->printf("Speed ​​set to %d\n", speed);
                }
            }
            break;
        case 1:
            if ((gpio1 != -1) && (gpio2 != -1) && (gpio3 != -1))
            {
                digitalWrite(gpio1, LOW);
                digitalWrite(gpio2, HIGH);
                digitalWrite(gpio3, HIGH);
                if (fan)
                {
                    fan->setState(true);
                    fan->setSpeed(1);
                    if (DBG_OUTPUT_PORT != NULL)
                        DBG_OUTPUT_PORT->printf("Speed ​​set to %d\n", speed);
                }
            }
            break;
        case 2:
            if ((gpio1 != -1) && (gpio2 != -1) && (gpio3 != -1))
            {
                digitalWrite(gpio1, HIGH);
                digitalWrite(gpio2, LOW);
                digitalWrite(gpio3, HIGH);
                if (fan)
                {
                    fan->setState(true);
                    fan->setSpeed(2);
                    if (DBG_OUTPUT_PORT != NULL)
                        DBG_OUTPUT_PORT->printf("Speed ​​set to %d\n", speed);
                }
            }
            break;
        case 3:
            if ((gpio1 != -1) && (gpio2 != -1) && (gpio3 != -1))
            {
                digitalWrite(gpio1, HIGH);
                digitalWrite(gpio2, HIGH);
                digitalWrite(gpio3, LOW);
                if (fan)
                {
                    fan->setState(true);
                    fan->setSpeed(3);
                    if (DBG_OUTPUT_PORT != NULL)
                        DBG_OUTPUT_PORT->printf("Speed ​​set to %d\n", speed);
                }
            }
            break;
        }
    }

    void onSetWiFi()
    {
        String ssid = String(host);
        if (server->hasArg("SSID"))
        {
            ssid = server->arg("SSID");
        }
        String password = "";
        if (server->hasArg("Password"))
        {
            password = server->arg("Password");
        }
        wl_status_t status = wifiInitSTA(ssid,password);
        if (status == WL_CONNECTED) {
            server->send(200, "text/plain", "Connected");
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->printf("Connection to WiFi network was successful\n");
        }
        else {
            // esp_wifi_restore();
            wifiInitAP();
            server->send(207, "text/plain", "Not connected");
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->printf("Connection to WiFi network failed with error: %d\n", status);
        }
    }

    void onChangeSpeed()
    {
        String speed = String(this->speed);
        if (server->hasArg("speed"))
        {
            speed = server->arg("speed");
            this->speed = (short)speed.toInt();
            changeSpeed(this->speed);
        }
        server->send(200, "text/json", "{\"speed\":" + speed + "}");
    }

    void onSetPins()
    {
        if (server->args() != 1)
        {
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->println("Error while passing parameters");
            server->send(203, "text/plain", "Error while passing parameters");
            return;
        }

        String response = server->arg(0);
        JsonDocument jsonResponse;
        deserializeJson(jsonResponse, response);

        if (!jsonResponse["GPIOButton"].is<JsonInteger>() ||
            !jsonResponse["GPIOSpeed1"].is<JsonInteger>() ||
            !jsonResponse["GPIOSpeed2"].is<JsonInteger>() ||
            !jsonResponse["GPIOSpeed3"].is<JsonInteger>())
        {
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->println("The parameter does not match the expected value.");
            server->send(203, "text/plain", "The parameter does not match the expected value.");
            return;
        }
        jsonConfig["GPIOButton"] = jsonResponse["GPIOButton"];
        jsonConfig["GPIOSpeed1"] = jsonResponse["GPIOSpeed1"];
        jsonConfig["GPIOSpeed2"] = jsonResponse["GPIOSpeed2"];
        jsonConfig["GPIOSpeed3"] = jsonResponse["GPIOSpeed3"];
        saveConfig();
        server->send(200, "text/plain", "OK");
        initGPIO();
    }

    void onHandlerHAParameters()
    {
        if (server->args() != 1)
        {
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->println("Error while passing parameters");
            server->send(203, "text/plain", "Error while passing parameters");
            return;
        }

        String response = server->arg(0);
        JsonDocument jsonResponse;
        deserializeJson(jsonResponse, response);

        if (!jsonResponse["haServer"].is<JsonString>() ||
            !jsonResponse["haEnable"].is<bool>())
        {
            if (DBG_OUTPUT_PORT != NULL)
                DBG_OUTPUT_PORT->println("The parameter does not match the expected value.");
            server->send(203, "text/plain", "The parameter does not match the expected value.");
            return;
        }

        jsonConfig["haServer"] = jsonResponse["haServer"];
        jsonConfig["haLogin"] = jsonResponse["haLogin"];
        jsonConfig["haPassword"] = jsonResponse["haPassword"];
        jsonConfig["haEnable"] = jsonResponse["haEnable"];
        saveConfig();
        haInit();
        if (getFanDevice() && getFanDevice()->status())
            server->send(200, "text/plain", "OK");
        else
            server->send(207, "text/plain", "MQTT not connected");
    }

    short getNumGPIO(GPIOType type)
    {
        switch (type)
        {
        case BUTTON:
            return jsonConfig["GPIOButton"];
        case GPIO1:
            return jsonConfig["GPIOSpeed1"];
        case GPIO2:
            return jsonConfig["GPIOSpeed2"];
        case GPIO3:
            return jsonConfig["GPIOSpeed3"];
        }
        return -1;
    }

    bool exists(String path)
    {
        bool yes = false;
        File file = FILESYSTEM->open(path, "r");
        if (!file.isDirectory())
        {
            yes = true;
        }
        file.close();
        return yes;
    }

    void handleFileUpload()
    {

        HTTPUpload &upload = server->upload();
        if (upload.status == UPLOAD_FILE_START)
        {
            String filename = upload.filename;

            if (server->hasArg("filename"))
            {
                filename = server->arg("filename");
            }

            if (!filename.startsWith("/"))
            {
                filename = "/" + filename;
            }
            if (DBG_OUTPUT_PORT != NULL)
            {
                DBG_OUTPUT_PORT->print("Attempt to upload file: ");
                DBG_OUTPUT_PORT->println(filename);
            }
            fsUploadFile = FILESYSTEM->open(filename, "w");
            filename = String();
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            if (fsUploadFile)
            {
                fsUploadFile.write(upload.buf, upload.currentSize);
            }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
            if (fsUploadFile)
            {
                fsUploadFile.close();
            }
            if (DBG_OUTPUT_PORT != NULL)
            {
                DBG_OUTPUT_PORT->print("Upload file size: ");
                DBG_OUTPUT_PORT->println(upload.totalSize);
            }
        }
    }

    void handleFileDelete()
    {
        if (server->args() == 0)
        {
            return server->send(500, "text/plain", "BAD ARGS");
        }
        String path = server->arg(0);
        if (DBG_OUTPUT_PORT != NULL)
            DBG_OUTPUT_PORT->println("Attempt to delete file: " + path);

        if (path == "/")
        {
            return server->send(500, "text/plain", "BAD PATH");
        }
        if (!exists(path))
        {
            return server->send(404, "text/plain", "FileNotFound");
        }
        FILESYSTEM->remove(path);
        server->send(200, "text/plain", "");
        path = String();
    }

    void handleFileList()
    {
        String path = "/";
        if (server->hasArg("dir"))
        {
            path = server->arg("dir");
        }

        if (DBG_OUTPUT_PORT != NULL)
            DBG_OUTPUT_PORT->println("Getting a list of files in a folder: " + path);

        File root = FILESYSTEM->open(path);
        path = String();

        int filesize = FILESYSTEM->usedBytes();
        int freesize = FILESYSTEM->totalBytes() - filesize;
        String output = "{\"filelist\":[";
        bool firstCycle = true;
        if (root.isDirectory())
        {
            File file = root.openNextFile();
            while (file)
            {
                if (!firstCycle) output += ','; 
                else firstCycle = false;
                output += "{\"type\":\"";
                output += (file.isDirectory()) ? "dir" : "file";
                output += "\",\"name\":\"";
                output += String(file.path()).substring(1);
                output += "\",\"size\":\"";
                output += formatBytes(file.size());
                output += "\"}";
                file = root.openNextFile();
            }
        }
        output += "], \"filesize\":\""+formatBytes(filesize) +"\",\"freesize\":\""+formatBytes(freesize)+"\"}";
        server->send(200, "text/json", output);
    }

    void publishFileList(String path)
    {
        if (DBG_OUTPUT_PORT != NULL)
            DBG_OUTPUT_PORT->println("Access to folder granted: " + path);
        File root = FILESYSTEM->open(path);

        if (root.isDirectory())
        {
            File file = root.openNextFile();
            while (file)
            {
                if (!file.isDirectory())
                {
                    String fpath = String(file.path());
                    if (DBG_OUTPUT_PORT != NULL)
                        DBG_OUTPUT_PORT->println("File access granted: " + String(fpath));
                    server->on(Uri(fpath), HTTP_GET,
                               [this]()
                               {
                                   String fpath = this->server->uri();

                                   String contentType = this->getContentType(fpath);
                                   if (this->DBG_OUTPUT_PORT != NULL)
                                       this->DBG_OUTPUT_PORT->println("File issued: " + fpath);
                                   File file = this->FILESYSTEM->open(fpath);
                                   this->server->streamFile(
                                       file,
                                       contentType);
                                   file.close();

                               });
                }
                else
                {
                    publishFileList(String(file.path()));
                }
                file = root.openNextFile();
            }
        }

        path = String();
    }

    void infoChip()
    {
        String chipId = String(ESP.getEfuseMac());
        String mac = WiFi.macAddress();
        String localIp = WiFi.localIP().toString();
        String chipModel = ESP.getChipModel();
        String chipRevision = String(ESP.getChipRevision());
        String sdkVersion = ESP.getSdkVersion();
        String cpuCore = String(ESP.getChipCores());
        String flashSize = String(ESP.getFlashChipSize());
        String flashSpeed = String(ESP.getFlashChipSpeed());
        String cpuSpeed = String(ESP.getCpuFreqMHz());
        String wifiMode = "Не подключено";
        String ssid = WiFi.SSID();
        switch (WiFi.getMode())
        {
        case WIFI_MODE_STA:
            wifiMode = "Подключено (STA)";
            break;
        case WIFI_MODE_AP:
            wifiMode = "Точка доступа (AP)";
            localIp = WiFi.softAPIP().toString();
            ssid = String(host);
            break;
        case WIFI_MODE_APSTA:
            wifiMode = "Ретранслятор? (APSTA)";
            break;
        case WIFI_MODE_MAX:
            wifiMode = "? (MAX)";
            break;
        }
        String rssi = String(WiFi.RSSI());
        String totalBytes = String(FILESYSTEM->totalBytes());
        String usedBytes = String(FILESYSTEM->usedBytes());
        String freeBytes = String(FILESYSTEM->totalBytes() - FILESYSTEM->usedBytes());
        String output = "{";
        output += "\"chipId\":\"" + chipId + "\",";
        output += "\"MAC\":\"" + mac + "\",";
        output += "\"IP\":\"" + localIp + "\",";
        output += "\"chipModel\":\"" + chipModel + "\",";
        output += "\"chipRevision\":\"" + chipRevision + "\",";
        output += "\"sdkVersion\":\"" + sdkVersion + "\",";
        output += "\"cpuCore\":\"" + cpuCore + "\",";
        output += "\"flashSize\":\"" + flashSize + "\",";
        output += "\"flashSpeed\":\"" + flashSpeed + "\",";
        output += "\"cpuSpeed\":\"" + cpuSpeed + "\",";
        output += "\"fsTotalSize\":\"" + totalBytes + "\",";
        output += "\"fsUsedSize\":\"" + usedBytes + "\",";
        output += "\"fsFreeSize\":\"" + freeBytes + "\",";
        output += "\"wifiMode\":\"" + wifiMode + "\",";
        output += "\"RSSI\":\"" + rssi + "\",";
        output += "\"SSID\":\"" + ssid + "\",";
        if (jsonConfig["haEnable"])
            output += "\"haEnable\":true,";
        else
            output += "\"haEnable\":false,";

        if (getFanDevice() && getFanDevice()->status())
            output += "\"haStatus\":true";
        else
            output += "\"haStatus\":false";
        output += "}";
        server->send(200, "text/json", output);
    }

    void run()
    {
        if (_haFanDevice)
            _haFanDevice->run();
        server->handleClient();
        delay(2);
    }

protected:
    String formatBytes(size_t bytes)
    {
        if (bytes < 1024)
        {
            return String(bytes) + "B";
        }
        else if (bytes < (1024 * 1024))
        {
            return String(bytes / 1024.0) + "KB";
        }
        else if (bytes < (1024 * 1024 * 1024))
        {
            return String(bytes / 1024.0 / 1024.0) + "MB";
        }
        else
        {
            return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
        }
    }

    String getContentType(String filename)
    {
        if (server->hasArg("download"))
        {
            return "application/octet-stream";
        }
        else if (filename.endsWith(".htm"))
        {
            return "text/html";
        }
        else if (filename.endsWith(".html"))
        {
            return "text/html";
        }
        else if (filename.endsWith(".css"))
        {
            return "text/css";
        }
        else if (filename.endsWith(".js"))
        {
            return "application/javascript";
        }
        else if (filename.endsWith(".json"))
        {
            return "application/json";
        }
        else if (filename.endsWith(".png"))
        {
            return "image/png";
        }
        else if (filename.endsWith(".gif"))
        {
            return "image/gif";
        }
        else if (filename.endsWith(".jpg"))
        {
            return "image/jpeg";
        }
        else if (filename.endsWith(".ico"))
        {
            return "image/x-icon";
        }
        else if (filename.endsWith(".svg"))
        {
            return "image/svg+xml";
        }
        else if (filename.endsWith(".xml"))
        {
            return "text/xml";
        }
        else if (filename.endsWith(".md"))
        {
            return "text/markdown";
        }
        else if (filename.endsWith(".pdf"))
        {
            return "application/x-pdf";
        }
        else if (filename.endsWith(".zip"))
        {
            return "application/x-zip";
        }
        else if (filename.endsWith(".gz"))
        {
            return "application/x-gzip";
        }
        else if (filename.endsWith(".woff2"))
        {
            return "font/woff2";
        }
        return "text/plain";
    }

private:
    HardwareSerial *DBG_OUTPUT_PORT = NULL;
    fs::SPIFFSFS *FILESYSTEM = NULL;
    File fsUploadFile;
    short speed = 0;
    JsonDocument jsonConfig;
    uint8_t oldbutton;
    HAFanDevice *_haFanDevice;
    HAFAN_STATE_CALLBACK(_stateCallback);
    HAFAN_SPEED_CALLBACK(_speedCallback);
};