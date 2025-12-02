# 🌱 Planty_IoT

ESP32와 Adafruit IO(MQTT)를 활용하여 구축한 **스마트 화분 관리 시스템**입니다.
이 프로젝트는 실시간으로 식물의 생육 환경(온도, 습도, 조도)을 모니터링하고, 클라우드 대시보드를 통해 물 주기, 조명, 환기 팬을 원격으로 제어할 수 있도록 설계되었습니다.

## 📋 주요 기능 (Key Features)

### 1. 실시간 환경 모니터링
* **온도 (Temperature)**: DHT11 센서를 통해 대기 온도를 측정합니다.
* **토양 습도 (Soil Moisture)**: 아날로그 정전식 센서 값을 지수 함수로 보정하여 0~100%로 변환합니다.
* **조도 (Light Intensity)**: CDS 센서를 통해 주변 밝기를 0~4095 단계로 측정합니다.

### 2. 원격 제어 및 자동화
* **MQTT 제어**: Adafruit IO 대시보드에서 Water(물), Light(식물 생장등), Fan(환기)을 개별 제어합니다.
* **스마트 조명 관리**: 생장등이 켜져 있더라도 외부 조도가 충분히 밝으면(값 > 2000) 에너지를 절약하기 위해 자동으로 소등하고 상태를 동기화합니다.
* **반응형 데이터 갱신**: 액추에이터(물/팬 등)가 작동하면 10초 대기 후 변화된 환경 값을 자동으로 측정하여 서버로 전송합니다.
* **정기 리포트**: NTP 서버 동기화를 통해 매시간 59분마다 전체 센서 데이터를 자동으로 기록합니다.

## 🔌 하드웨어 구성 (Pinout)

ESP32 보드의 GPIO 핀 설정은 다음과 같습니다.

| 구분 | 부품 (Component) | 핀 번호 (GPIO) | 비고 |
| :--- | :--- | :---: | :--- |
| **Output** | **Water Pump Relay** | 19 | Active Low/High 설정 확인 필요 |
| **Output** | **Grow Light Relay** | 23 | 스마트 꺼짐 기능 포함 |
| **Output** | **Fan Relay** | 18 | 공기 순환용 |
| **Input** | **Soil Sensor** | 35 | Analog Input |
| **Input** | **CDS Sensor** | 34 | Analog Input |
| **Input** | **DHT11** | 4 | Digital Temp Sensor |

## 🛠 필수 라이브러리
아두이노 IDE의 라이브러리 매니저에서 다음 항목들을 설치해야 합니다.
- `Adafruit MQTT Library` (MQTT 통신)
- `DHT sensor library` (온습도 센서)
- `Adafruit Unified Sensor` (센서 통합 드라이버)

## ☁️ Adafruit IO 설정 (Feeds)

대시보드와 연동하기 위해 Adafruit IO에서 아래의 **Feed Key**를 생성해야 합니다.

* **Publish (ESP32 → Cloud)**
    * `planty.temperature` : 온도 데이터
    * `planty.soilmoisture` : 토양 습도 데이터 (%)
    * `planty.lightintensity` : 조도 데이터
    * `action.light` : 조명 상태 피드백 (자동 꺼짐 시 사용)

* **Subscribe (Cloud → ESP32)**
    * `action.water` : 물 펌프 ON/OFF 스위치
    * `action.light` : 조명 ON/OFF 스위치
    * `action.fan` : 팬 ON/OFF 스위치
    * `action.refresh` : 즉시 데이터 갱신 요청 버튼

## ⚙️ 설치 및 실행 (Setup)

1.  **WiFi 및 MQTT 설정**: 소스 코드 상단의 인증 정보를 본인의 환경에 맞게 수정합니다.

    ```cpp
    // WiFi 설정
    const char* ssid = "YOUR_WIFI_SSID";       // 와이파이 이름
    const char* password = "YOUR_WIFI_PASSWORD"; // 와이파이 비밀번호

    // Adafruit IO 설정
    #define AIO_USERNAME    "YOUR_AIO_USERNAME" // Adafruit 사용자명
    #define AIO_KEY         "YOUR_AIO_KEY"      // Adafruit AIO Key
    ```

2.  **업로드**: ESP32 보드에 코드를 업로드합니다.
3.  **확인**: 시리얼 모니터(115200 baud)를 열어 `Connected to MQTT!` 메시지를 확인합니다.

---
*Created for the Planty Project based on ESP32 & Adafruit IO.*
