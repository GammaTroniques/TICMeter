#include "shell.h"

void shellParser(char *strInput, Config *config, Mqtt *mqtt)
{
    String input = String(strInput);
    String command = input.substring(0, input.indexOf(' '));
    String args[10];
    for (int i = 0; i < 10; i++)
    {
        args[i] = input.substring(input.indexOf(' ') + 1, input.indexOf(' ', input.indexOf(' ') + 1));
        input = input.substring(input.indexOf(' ', input.indexOf(' ') + 1));
    }

    // set-wifi <ssid> <password>
    // set-web <url> <post-url> <config-url> <token>
    // set-mqtt <url> <port> <username> <password> <topic>

    command.replace("\r", "");
    command.replace("\n", "");

    for (int i = 0; i < 10; i++)
    {
        args[i].replace("\r", "");
        args[i].replace("\n", "");
    }

    if (command == "set-wifi")
    {
        strcpy(config->values.ssid, args[0].c_str());
        strcpy(config->values.password, args[1].c_str());
        config->write();
        Serial.println("WiFi credentials saved");
    }
    else if (command == "get-wifi")
    {
        Serial.print("SSID: ");
        Serial.println(config->values.ssid);
        Serial.print("Password: ");
        Serial.println(config->values.password);
    }
    else if (command == "set-web")
    {
        strcpy(config->values.web.host, args[0].c_str());
        strcpy(config->values.web.postUrl, args[1].c_str());
        strcpy(config->values.web.configUrl, args[2].c_str());
        strcpy(config->values.web.token, args[3].c_str());
        config->write();
        Serial.println("Web credentials saved");
    }
    else if (command == "get-web")
    {
        Serial.print("Host: ");
        Serial.println(config->values.web.host);
        Serial.print("Post URL: ");
        Serial.println(config->values.web.postUrl);
        Serial.print("Config URL: ");
        Serial.println(config->values.web.configUrl);
        Serial.print("Token: ");
        Serial.println(config->values.web.token);
    }
    else if (command == "set-mqtt")
    {
        strcpy(config->values.mqtt.host, args[0].c_str());
        config->values.mqtt.port = args[1].toInt();
        strcpy(config->values.mqtt.username, args[2].c_str());
        strcpy(config->values.mqtt.password, args[3].c_str());
        strcpy(config->values.mqtt.topic, args[4].c_str());
        config->write();
        Serial.println("MQTT credentials saved");
    }
    else if (command == "get-mqtt")
    {
        Serial.print("Host: ");
        Serial.println(config->values.mqtt.host);
        Serial.print("Port: ");
        Serial.println(config->values.mqtt.port);
        Serial.print("Username: ");
        Serial.println(config->values.mqtt.username);
        Serial.print("Password: ");
        Serial.println(config->values.mqtt.password);
        Serial.print("Topic: ");
        Serial.println(config->values.mqtt.topic);
    }
    else if (command == "set-mode")
    {
        config->values.mode = args[0].toInt();
        config->write();
        Serial.println("Mode saved");
    }
    else if (command == "get-mode")
    {
        Serial.print("Mode: ");
        Serial.println(config->values.mode);
    }
    else if (command == "mqtt-connect")
    {
        mqtt->connect(config->values.mqtt.host, config->values.mqtt.port, config->values.mqtt.username, config->values.mqtt.password);
    }
    else if (command == "mqtt-send")
    {
        mqtt->send(args[0].c_str(), args[1].c_str());
    }
    else if (command == "get-config")
    {
        config->read();
        for (int i = 0; i < sizeof(config->values); i++)
        {
            Serial.printf("%x ", ((uint8_t *)&config->values)[i]);
        }
    }
    else if (command == "set-config")
    {
        config->write();
    }
    else if (command == "wifi-connect")
    {
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(config->values.ssid, config->values.password);
        uint32_t timeout = millis() + 10000;
        while (WiFi.status() != WL_CONNECTED && millis() < timeout)
        {
            delay(500);
            Serial.print(".");
        }
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
    else if (command == "wifi-disconnect")
    {
        WiFi.disconnect();
    }
    else if (command == "wifi-status")
    {
        Serial.println(WiFi.status());
    }
    else
    {
        Serial.println("Command not found");
    }
}

void shellLoop(void *pvParameters)
{
    shell_t *shell = (shell_t *)pvParameters;
    Config *config = shell->config;
    Mqtt *mqtt = shell->mqtt;

    char input[100];
    uint8_t inputIndex = 0;
    while (1)
    {
        if (Serial.available() > 0)
        {
            char c = Serial.read();
            if (c == '\b' || c == 127)
            {
                if (inputIndex > 0)
                {
                    inputIndex--;
                    Serial.print("\b \b");
                }
                continue;
            }
            input[inputIndex++] = c;
            Serial.print(c);
            if (c == '\n')
            {
                input[inputIndex] = '\0';
                inputIndex = 0;
                shellParser(input, config, mqtt);
                Serial.printf("\n$ ");
            }
        }
        delay(10);
    }
}