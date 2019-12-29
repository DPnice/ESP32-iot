/**********************************************************
*    文件: ESP32-iot.ino      by 零知实验室(www.lingzhilab.com)
*    -^^- 零知开源，让电子制作变得更简单！ -^^-
*    时间: 2019/12/14 14:34
*    说明:
************************************************************/

#include <WiFi.h>
#include <SimpleDHT.h>
#include "Servo.h"

//FreeRTOS是一个迷你的实时操作系统内核
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

// Change the MQTT_HOST variable to your IP address
#define MQTT_HOST IPAddress(49, 233, 131, 191)
#define MQTT_PORT 1883

// Create objects to handle MQTT client
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

//舵机 SG90
Servo myservo;
// GPIO the servo is attached to
static const int servoPin = 13;

//温度湿度 for DHT11
//      VCC: 5V or 3V
//      GND: GND
//      DATA: 2
int pinDHT11 = 2;
SimpleDHT11 dht11(pinDHT11);

int pumpin = 16;//续电器引脚 低电平触发
int isOpen = 0;//加湿器是否开启 0关闭

String temperatureString = "";   // Variable to hold the temperature reading 保持温度读数的变量
unsigned long previousMillis = 0;   // Stores last time temperature was published 存储上次发布温度的时间
const long interval = 5000;        // interval at which to publish sensor readings 发布传感器读数的间隔

int retryCount = 6;//wifi重试次数 重试失败进行智能配网

void connectToWifi() {
	//自动配网
	autoConfig();
}

void connectToMqtt() {
	Serial.println("Connecting to MQTT...");
	mqttClient.connect();
}


void autoConfig()
{
	WiFi.begin();
}

void smartConfig()
{
	
	WiFi.mode(WIFI_AP_STA);
	Serial.println("\r\nWait for Smartconfig");
	WiFi.beginSmartConfig();
	while (1)
	{
		Serial.print(".");
		if (WiFi.smartConfigDone())
		{
			Serial.println("SmartConfig Success");
			Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
			Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
			WiFi.setAutoConnect(true);  // 配网成功后自动连接
			break;
		}
		delay(1000); // 这个地方一定要加延时，否则极易崩溃重启
	}
}

void WiFiEvent(WiFiEvent_t event) {
	Serial.print("Retries remaining:");
	Serial.println(retryCount);
	Serial.printf("[WiFi-event] event: %d\n", event);
	if(retryCount<=0){
		//停止重连
		xTimerStop(wifiReconnectTimer, 0);
		//智能配网
		Serial.println("AutoConfig Faild!" );
		Serial.println("Start smartConfig..." );
		smartConfig();
		retryCount=6;
	}else{
		switch(event) {
			case SYSTEM_EVENT_STA_GOT_IP:
			Serial.println("AutoConfig Success");
			Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
			Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
			Serial.println("WiFi connected");
			Serial.print("IP address: ");
			Serial.println(WiFi.localIP());
			WiFi.printDiag(Serial);
			connectToMqtt();
			retryCount=6;
			break;
			case SYSTEM_EVENT_STA_DISCONNECTED:
			Serial.println("WiFi lost connection");
			//确保在重新连接到Wi-Fi时不重新连接到MQTT
			xTimerStop(mqttReconnectTimer, 0);
			xTimerStart(wifiReconnectTimer, 0);
			break;
			default :
			retryCount--;
		}
	}
	
}

// Add more topics that want your ESP32 to be subscribed to
void onMqttConnect(bool sessionPresent) {
	Serial.println("Connected to MQTT");
	Serial.print("Session present: ");
	Serial.println(sessionPresent);
	// 控制舵机的topic
	uint16_t packetIdSub = mqttClient.subscribe("esp32/feed", 0);
	Serial.print("Subscribing to esp32/feed at QoS 0, packetId: ");
	Serial.println(packetIdSub);
	
	uint16_t packetIdSub1 = mqttClient.subscribe("esp32/led", 0);
	Serial.print("Subscribing to esp32/led at QoS 0, packetId: ");
	Serial.println(packetIdSub1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
	Serial.println("Disconnected from MQTT.");
	if (WiFi.isConnected()) {
		//如果wifi连接 就开始mqtt重新连接
		xTimerStart(mqttReconnectTimer, 0);
	}
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
	Serial.println("Subscribe acknowledged.");
	Serial.print("  packetId: ");
	Serial.println(packetId);
	Serial.print("  qos: ");
	Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
	Serial.println("Unsubscribe acknowledged.");
	Serial.print("  packetId: ");
	Serial.println(packetId);
}

void onMqttPublish(uint16_t packetId) {
	Serial.println("Publish acknowledged.");
	Serial.print("  packetId: ");
	Serial.println(packetId);
}

// You can modify this function to handle what happens when you receive a certain message in a specific topic
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
	String messageTemp;
	for (int i = 0; i < len; i++) {
		//Serial.print((char)payload[i]);
		messageTemp += (char)payload[i];
	}
	// topic 是否是舵机
	if (strcmp(topic, "esp32/feed") == 0) {
		//执行投食
		Serial.println("执行投食");
		myservo.write(75);
		delay(500);
		myservo.write(0);

		
	}
	
	if (strcmp(topic, "esp32/led") == 0) {
		//加湿器亮灯
		//判断继电器是否开启状态
		int isHigh = digitalRead(pumpin);
		if(isHigh == HIGH){
			digitalWrite(pumpin,LOW);//打开/关闭led
			delay(3000);//3秒打开led
			digitalWrite(pumpin,HIGH);
		}
	}
	
	Serial.println("Publish received.");
	Serial.print("  message: ");
	Serial.println(messageTemp);
	Serial.print("  topic: ");
	Serial.println(topic);
	Serial.print("  qos: ");
	Serial.println(properties.qos);
	Serial.print("  dup: ");
	Serial.println(properties.dup);
	Serial.print("  retain: ");
	Serial.println(properties.retain);
	Serial.print("  len: ");
	Serial.println(len);
	Serial.print("  index: ");
	Serial.println(index);
	Serial.print("  total: ");
	Serial.println(total);
}



void setup() {
	
	Serial.begin(115200);
	//名称、定时周期、pdTRUE表示该定时器为自动重载定时器pdFALSE为一次性定时器、定时器ID的初始值、回调函数
	mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
	wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
	
	WiFi.onEvent(WiFiEvent);
	
	mqttClient.onConnect(onMqttConnect);
	mqttClient.onDisconnect(onMqttDisconnect);
	mqttClient.onSubscribe(onMqttSubscribe);
	mqttClient.onUnsubscribe(onMqttUnsubscribe);
	mqttClient.onMessage(onMqttMessage);
	mqttClient.onPublish(onMqttPublish);
	mqttClient.setServer(MQTT_HOST, MQTT_PORT);
	//必须先连接wifi才能触发onEvent
	connectToWifi();
	//初始化舵机
	myservo.attach(servoPin);
	//设置继电器
	pinMode(pumpin,OUTPUT);
	digitalWrite(pumpin,HIGH);//加湿器关闭
	Serial.println("启动完成");
	
}

void loop() {
	if (WiFi.status() == WL_CONNECTED){
		//生产消息
		unsigned long currentMillis = millis();
		// Every X number of seconds (interval = 5 seconds) 每隔x秒
		// 发布新的数据给温度topic
		if (currentMillis - previousMillis >= interval) {
			// Save the last time a new reading was published
			previousMillis = currentMillis;
			// New temperature readings
			byte temperature, humidity;
			int err = SimpleDHTErrSuccess;
			
			//读取温度湿度
			if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
				Serial.print("Read DHT11 failed, err=");
				Serial.println(err);
				return;
			}
			Serial.print("Sample OK: ");
			Serial.print((int)temperature); Serial.print(" *C, ");
			Serial.print((int)humidity); Serial.println(" H");
			
			//温度 湿度 水量
			temperatureString = String(temperature)+","+String(humidity)+","+"100";
			Serial.println(temperatureString);
			//发布温度湿度 消息质量 2 刚好一次
			//Publish an MQTT message on topic esp32/t&h&w with Celsius and Fahrenheit temperature readings
			uint16_t packetIdPub2 = mqttClient.publish("esp32-test/t&h&w", 2, true, temperatureString.c_str());
			Serial.print("Publishing on topic esp32-test/t&h&w at QoS 2, packetId: ");
			Serial.println(packetIdPub2);
			
			if((int)humidity<50){
				if(isOpen == 0){
					Serial.println("打开加湿器");
					digitalWrite(pumpin,LOW);//打开加湿器
					delay(1000);//持续1s
					digitalWrite(pumpin,HIGH);
					isOpen=1;
				}
				
			}else{
				if(isOpen == 1){
					Serial.println("关闭加湿器");
					digitalWrite(pumpin,LOW);
					delay(1000);
					digitalWrite(pumpin,HIGH);//两次关闭
					delay(500);
					digitalWrite(pumpin,LOW);
					delay(1000);
					digitalWrite(pumpin,HIGH);
					
					isOpen=0;
				}
			}
		}
		
	}
}
