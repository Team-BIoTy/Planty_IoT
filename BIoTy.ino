#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <DHT.h>
#include <time.h>

// WiFi 설정
const char* ssid = "Aksen";
const char* password = "0725";

// Adafruit IO MQTT 설정
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "BIoT"
#define AIO_KEY         "aio"

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// 구독 피드
Adafruit_MQTT_Subscribe waterFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/action.water");
Adafruit_MQTT_Subscribe lightFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/action.light");
Adafruit_MQTT_Subscribe fanFeed   = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/action.fan");
Adafruit_MQTT_Subscribe refreshFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/action.refresh");

// 퍼블리시 피드
Adafruit_MQTT_Publish temperaturePub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/planty.temperature");
Adafruit_MQTT_Publish soilPub        = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/planty.soilmoisture");
Adafruit_MQTT_Publish lightPub       = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/planty.lightintensity");
Adafruit_MQTT_Publish lightAuto = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/action.light");

// 릴레이 핀
#define RELAY_WATER 19
#define RELAY_LIGHT 23
#define RELAY_FAN   18

// 센서 핀
#define SOIL_PIN 35
#define CDS_PIN  34
#define DHT_PIN  4

DHT dht(DHT_PIN, DHT11);

// NTP 설정
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600;  // 한국 시간: GMT+9
const int daylightOffset_sec = 0;

bool uploadedThisHour = false;
//bool ledIsOn = false;  // ⭐️ 현재 LED가 켜져 있는지 추적
unsigned long uploadDelayStart = 0;
bool uploadScheduled = false;

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_WATER, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(CDS_PIN, INPUT);
  dht.begin();

    // 부팅 시 릴레이가 꺼진 상태로 시작하도록 명시
  digitalWrite(RELAY_WATER, HIGH);
  digitalWrite(RELAY_LIGHT, LOW);
  digitalWrite(RELAY_FAN, HIGH);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println(" Connected!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // 구독 설정
  mqtt.subscribe(&waterFeed);
  mqtt.subscribe(&lightFeed);
  mqtt.subscribe(&fanFeed);
  mqtt.subscribe(&refreshFeed);

  connectToMQTT();
}

void loop() {
  if (!mqtt.connected()) {
    connectToMQTT();
  }

  Adafruit_MQTT_Subscribe* subscription;
  while ((subscription = mqtt.readSubscription(100))) {
    const char* value = (const char*)subscription->lastread;

    // value가 유효한지 먼저 확인
    if (value == nullptr) {
      //Serial.println("[WARN] MQTT message was null!");
      continue;
    }

    bool isOn = strcmp(value, "ON") == 0;

    if (subscription == &waterFeed) {
      Serial.print("Water: "); Serial.println(value);
      digitalWrite(RELAY_WATER, isOn ? LOW : HIGH);
    } else if (subscription == &lightFeed) {
      Serial.print("Light: "); Serial.println(value);
      digitalWrite(RELAY_LIGHT, isOn ? HIGH : LOW);
      //ledIsOn = isOn;  // ⭐️ LED 상태 업데이트
    } else if (subscription == &fanFeed) {
      Serial.print("Fan: "); Serial.println(value);
      digitalWrite(RELAY_FAN, isOn ? LOW : HIGH);
    } else if (subscription == &refreshFeed) {
      Serial.print("Refresh: "); Serial.println(value);  // ⭐ 디버깅용 출력
      uploadSensorData();  // 값이 무엇이든 무조건 업로드
    }

    // ON 신호가 오면 즉시 업로드
    if (isOn) {
      uploadDelayStart = millis();    // 10초 타이머 시작
      uploadScheduled = true;         // 업로드 예약
    }
  }
  if (uploadScheduled && millis() - uploadDelayStart >= 10000) {
    Serial.println("[INFO] ON 신호 후 10초 경과: 센서 데이터 업로드");
    uploadSensorData();
    uploadScheduled = false;  // 다시 초기화
  }

  // ⭐️ 조도 체크해서 LED 끄기 및 OFF 퍼블리시
  if (ledIsOn) {
    int lightRaw = analogRead(CDS_PIN);
    int light = 4095 - lightRaw;
    if (light > 800) {
      Serial.println("[INFO] 조도 높음 - LED OFF 전환");
      digitalWrite(RELAY_LIGHT, LOW);
      lightAuto.publish("OFF");  // ⭐️ MQTT로 OFF 메시지 전송
      ledIsOn = false;
    }
  }

  // 매 시 59분에 한 번 업로드
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_min == 59 && !uploadedThisHour) {
      Serial.println("[INFO] 59분 감지: 센서 데이터 업로드 중...");
      uploadSensorData();
      uploadedThisHour = true;
    }
    if (timeinfo.tm_min != 59) {
      uploadedThisHour = false;
    }
  }

  delay(1000);  // 1초마다 체크
}

void connectToMQTT() {
  Serial.print("Connecting to MQTT...");
  while (mqtt.connect() != 0) {
    Serial.print(".");
    mqtt.disconnect();
    delay(5000);
  }
  Serial.println(" Connected to MQTT!");
}

void uploadSensorData() {
  float temp = dht.readTemperature();
  int soilRaw = analogRead(SOIL_PIN);
  int lightRaw = analogRead(CDS_PIN);
  int light = 4095 - lightRaw;
  int soil = getSoilMoisturePercent(soilRaw);

  Serial.println("=== Uploading Sensor Data ===");
  Serial.print("Temp: "); Serial.println(temp);
  Serial.print("Soil: "); Serial.println(soil);
  Serial.print("Light: "); Serial.println(light);

  temperaturePub.publish(temp);
  soilPub.publish((int32_t)soil);
  lightPub.publish((int32_t)light);
}

float getSoilMoisturePercent(int raw) {
  float x = (float)raw;
  float percent = 100.0 - 100.0 * pow((x / 4095.0), 1.8);  // 지수 함수 기반 감쇠
  return constrain(percent, 0, 100);
}
