#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h> // 引入文件系统库

#define CONFIG_FILENAME "/config.json"
JsonDocument config_json;

String ssid = "ESP32-Access-Point";
String password = "123456789";

AsyncWebServer server(80); // Create AsyncWebServer object on port 80
AsyncWebSocket ws("/ws");  // Create a WebSocket object

// 设置静态IP
IPAddress local_ip(192, 168, 192, 1); // 设置静态IP地址
IPAddress gateway(192, 168, 1, 1);	  // 设置网关
IPAddress subnet(255, 255, 255, 0);	  // 设置子网掩码

// ===================== HTML ===========================
const char HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>WebSocket Demo</title></head><body><h1>Message: </h1><h2 id="info"></h2><button onclick="sayHello()">say hello</button><script>var ws = null;var info = msg => document.getElementById("info").innerHTML = msg;function connect_ws() {try {ws = new WebSocket("ws://192.168.192.1/ws");ws.onopen = function (evt) {info("Connection open...");};ws.onmessage = on_message;ws.onclose = function (evt) {info("Connection closed.");ws = null;};ws.onerror = function (evt) {info("Connection error.");ws = null;};} catch (e) {ws = null;info("Connection error.");}};function send_message(json_data) {if (ws === null || ws.readyState !== WebSocket.OPEN) {connect_ws();} else {ws.send(JSON.stringify(json_data));}};function on_message(evt) {let data = JSON.parse(evt.data);info(data.message);};function sayHello() {send_message({ "message": "Hello, server!" });}window.onload = function () {connect_ws();};</script></body></html>
)rawliteral";

// ===================== 文件操作 ===========================

void write_json_file(String filename, JsonDocument json_document)
{
	File file = SPIFFS.open(filename, "w");
	serializeJson(json_document, file);
	file.close();
}

JsonDocument read_json_file(String filename)
{
	File file = SPIFFS.open(filename, "r");
	JsonDocument json_document;
	deserializeJson(json_document, file);
	file.close();
	return json_document;
}

// 配置文件初始化函数，接收回调函数作为参数
void init_json_file(const char *json_filename, void (*config_func)(JsonDocument &))
{
	// 判断文件是否存在
	if (!SPIFFS.exists(json_filename))
	{
		Serial.print("create config file: ");
		Serial.println(json_filename);

		JsonDocument json_document;
		// 调用传入的回调函数来修改 json_document
		config_func(json_document);

		// 写入 JSON 文件
		write_json_file(json_filename, json_document);
	}
	else
	{
		Serial.print("config file: ");
		Serial.print(json_filename);
		Serial.println(" has been exist.");
	}
}

void handle_config_file_init_callback(JsonDocument &json_document)
{
	// 在这里初始化 JSON 文档
	json_document["ssid"] = ssid;
	json_document["password"] = password;
}
void spiff_file_system_setup()
{
	if (!SPIFFS.begin())
	{
		Serial.println("An Error has occurred while mounting SPIFFS");
		Serial.println("try to format SPIFFS partition");
		if (SPIFFS.format())
		{
			Serial.println("SPIFFS partition formatted successfully");
			ESP.restart();
		}
		else
		{
			Serial.println("SPIFFS partition format failed");
		}
		return;
	}
	else
	{
		Serial.println("SPIFFS initalized successfully");
		init_json_file(CONFIG_FILENAME, handle_config_file_init_callback);
	}
}

// ===================================== JSON 解析 =====================================
String extract_value_from_json(String message, String key)
{
	int startIndex = message.indexOf(key);
	String result = "";

	if (startIndex != -1)
	{
		startIndex += key.length(); // 跳过 key 的长度

		// 跳过 ": " 的部分
		while (startIndex < message.length())
		{
			char currentChar = message.charAt(startIndex);
			if (currentChar == ':')
			{
				startIndex++;
				// 跳过可能存在的空格或引号
				if (message.charAt(startIndex) == ' ' || message.charAt(startIndex) == '"')
				{
					startIndex++;
				}
				break;
			}
			startIndex++;
		}

		// 提取字符串内容，直到遇到引号、逗号或大括号结束
		while (startIndex < message.length())
		{
			char currentChar = message.charAt(startIndex);
			if (currentChar == '"' || currentChar == ',' || currentChar == '}')
			{
				break;
			}
			result += currentChar;
			startIndex++;
		}
	}

	result.trim();

	return result; // 去除可能的多余空格
}

// ===================================== 处理 WebSocket 消息 =====================================

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

void handle_websocket_connected_callback(AsyncWebSocketClient *client)
{
	Serial.println("an user connect the websocket");
	// 向当前连接的用户发送消息
	JsonDocument response_json_data_for_connection;
	response_json_data_for_connection["message"] = "Connected";
	String serialized_json_document;
	serializeJson(response_json_data_for_connection, serialized_json_document);
	client->text(serialized_json_document);
}

void on_event(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
	switch (type)
	{
	case WS_EVT_CONNECT:
		Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
		handle_websocket_connected_callback(client);
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

void setup_wifi()
{
	Serial.print("Setting AP (Access Point)…");
	const char *ssid_ = config_json["ssid"];
	const char *password_ = config_json["password"];
	WiFi.softAP(ssid_, password_);

	// 设置静态IP
	if (!WiFi.softAPConfig(local_ip, gateway, subnet))
	{
		Serial.println("Failed to configure AP");
	}
	else
	{
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

void setup()
{
	Serial.begin(115200);

	spiff_file_system_setup();

	config_json = read_json_file(CONFIG_FILENAME);

	Serial.print("config_json: ");
	Serial.print("ssid: ");
	Serial.print((String)config_json["ssid"]);
	Serial.print("; password:");
	Serial.print((String)config_json["password"]);
	Serial.println();

	setup_wifi();
}

void loop()
{
}