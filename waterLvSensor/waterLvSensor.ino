//Netwk
#include <FirebaseESP8266.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "wifiCredential.h"
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
int interval = 15000; // init value. set value on Firebase
String slocInterval = "Aquarium/!Settings/!Interval/!Interval";


struct Var{
	int pinTrig;
	int pinEcho;
	int aboveTank;
	int tankHeight;
	String slocLv;
	String slocTime;
	String slocIntervalStatus;
	String slocV;
	};
struct Var Main = {0, 3, 20, 30, "Aquarium/Main/WaterLv", "Aquarium/!Timestamp/Main/WaterLv", "Aquarium/!Settings/Reflection/Interval/Main", "Aquarium/Main/MonitoringStatus/SensorVoltage"};
struct Var Sub  = {0, 3, 10, 22, "Aquarium/Sub/WaterLv", "Aquarium/!Timestamp/Sub/WaterLv", "Aquarium/!Settings/Reflection/Interval/Sub", "Aquarium/Sub/MonitoringStatus/SensorVoltage"};
struct Var Res  = {0, 3, 5, 30, "Aquarium/Reservoir/WaterLv", "Aquarium/!Timestamp/Reservoir/WaterLv", "Aquarium/!Settings/Reflection/Interval/Reservoir", "Aquarium/Reservoir/MonitoringStatus/SensorVoltage"};

struct Var tank = Res;

ADC_MODE(ADC_VCC);

void setup() {
pinMode (tank.pinTrig, OUTPUT);
pinMode (tank.pinEcho, INPUT);
pinMode (LED_BUILTIN, OUTPUT);

digitalWrite (LED_BUILTIN, LOW);
delay(1000);
digitalWrite (LED_BUILTIN, HIGH);

//Netwk
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	Firebase.begin(&config, &auth);
	Firebase.reconnectWiFi(false); // Comment or "false" when WiFi reconnection will control by your code or third party library
	Firebase.setDoubleDigits(2);

for (int i = 0; i < 4; i++) {
	digitalWrite (LED_BUILTIN, LOW);
	delay(500);
	digitalWrite (LED_BUILTIN, HIGH);
	delay(500);
}
}


void loop() {
	if (Firebase.ready() && (millis() - sendDataPrevMillis > interval || sendDataPrevMillis == 0)) {
		sendDataPrevMillis = millis();

		getInterval();

		int value = measureWaterLv(tank);
		Firebase.setInt(fbdo, tank.slocLv, value);
		Firebase.setTimestamp(fbdo, tank.slocTime);

		Firebase.setFloat(fbdo, tank.slocV, ESP.getVcc());

	}
}


void getInterval() {
	Firebase.setString(fbdo, tank.slocIntervalStatus, "");
	if (Firebase.getInt(fbdo, slocInterval)) {
		interval = fbdo.intData();
		Firebase.setString(fbdo, tank.slocIntervalStatus, "OK");
	} else {
		Firebase.setString(fbdo, tank.slocIntervalStatus, fbdo.errorReason().c_str());
	}
}


int measureWaterLv(Var &v){
	digitalWrite(v.pinTrig, HIGH);
	delayMicroseconds(10);
	digitalWrite(v.pinTrig, LOW);
	float time = pulseIn(v.pinEcho, HIGH);
	float dist = time * 340 / 2 /10000;
	int waterLv = map(dist, v.aboveTank, v.aboveTank + v.tankHeight, 100, 0);
	if (waterLv > 100) waterLv = 100;
	if (waterLv < 0) waterLv = 0;
	return waterLv;
}
