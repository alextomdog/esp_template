#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

const char *ssid = "ESP32-Access-Point";
const char *password = "123456789";

const char HTML[] PROGMEM = R"rawliteral(
<h1>Hello world</h1>
)rawliteral";

AsyncWebServer server(80); // Create AsyncWebServer object on port 80
AsyncWebSocket ws("/ws");  // Create a WebSocket object

// 设置静态IP
IPAddress local_ip(192, 168, 192, 1); // 设置静态IP地址
IPAddress gateway(192, 168, 1, 1);  // 设置网关
IPAddress subnet(255, 255, 255, 0); // 设置子网掩码

void on_request_response_ws_message(JsonDocument &request_json_data, JsonDocument &response_json_data)
{
    response_json_data = request_json_data;
}

void handle_websocket_message_callback(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        JsonDocument request_json_data;
        JsonDocument response_json_data;
        String serialized_json_document;

        data[len] = 0;
        String message = (char *)data;

        DeserializationError error = deserializeJson(request_json_data, message);

        if (error)
        {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
        }
        else
        {
            on_request_response_ws_message(request_json_data, response_json_data);
            serializeJson(response_json_data, serialized_json_document);
            ws.textAll(serialized_json_document);
        }
    }
}

void on_event(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handle_websocket_message_callback(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

void setup()
{
    Serial.begin(115200);

    Serial.print("Setting AP (Access Point)…");
    WiFi.softAP(ssid, password);

    // 设置静态IP
    if (!WiFi.softAPConfig(local_ip, gateway, subnet)) {
        Serial.println("Failed to configure AP");
    } else {
        Serial.println("AP Static IP Configured");
    }

    // 输出AP的IP地址
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", HTML); });
    ws.onEvent(on_event);
    server.addHandler(&ws);
    server.begin();
}

void loop()
{
}
