//Netwk
#include <FirebaseESP8266.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "wifiCredential.h"
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
unsigned long pumpOnDuration = 0;
struct FB {
	String sloc;
	String status;
	int value;
};
struct FB OpStatus		= {"Aquarium/!!STATUS"};
struct FB Timestamp		= {"Aquarium/!Timestamp/Control"};
struct FB Interval		= {"Aquarium/!Settings/!Interval/!Interval", "Aquarium/!Settings/Reflection/Interval/Control/Interval", 30000};
struct FB IntervalOn	= {"Aquarium/!Settings/!Interval/whenPumpOn", "Aquarium/!Settings/Reflection/Interval/Control/whenPumpOn", 15000};
struct FB IntervalOff	= {"Aquarium/!Settings/!Interval/whenPumpOff", "Aquarium/!Settings/Reflection/Interval/Control/whenPumpOff", 300000};
struct FB LvMax			= {"Aquarium/!Settings/!WaterLv/!Max", "Aquarium/!Settings/Reflection/WaterLv/!Max", 85};
struct FB LvMin			= {"Aquarium/!Settings/!WaterLv/!Min", "Aquarium/!Settings/Reflection/WaterLv/!Min", 80};
struct FB Count			= {"Aquarium/!Settings/!FailSafe/TimestampCheckCount", "Aquarium/!Settings/Reflection/FailSafe/TimestampCheckCount", 5};
struct FB Millis		= {"Aquarium/!Settings/!FailSafe/PumpingMillis", "Aquarium/!Settings/Reflection/FailSafe/PumpingMillis", 3600000};
struct FB Reservoir		= {"Aquarium/Reservoir/WaterLv", "", 100};

struct Var {
	int pin;
	String slocLv;
	String slocTimestamp;
	String slocPumpStatus;
	String slocStatusMillis;
	String slocStatusCount;

	// init-as-zero vars
	int pumpState;
	unsigned long timeLast;
	unsigned long timeNow;
	int count;
	unsigned long pumpStartMillis;
	};
struct Var Main = {D5, "Aquarium/Main/WaterLv", "Aquarium/!Timestamp/Main/WaterLv", "Aquarium/Main/Pump", "Aquarium/Main/MonitoringStatus/PumpDurationLeft", "Aquarium/Main/MonitoringStatus/TimestampChkLeft"};
struct Var Sub  = {D0, "Aquarium/Sub/WaterLv", "Aquarium/!Timestamp/Sub/WaterLv", "Aquarium/Sub/Pump", "Aquarium/Sub/MonitoringStatus/PumpDurationLeft", "Aquarium/Sub/MonitoringStatus/TimestampChkLeft"};

void setup() {
	pinMode(Main.pin, OUTPUT);
	pinMode(Sub.pin, OUTPUT);
	digitalWrite(Main.pin, LOW);
	digitalWrite(Sub.pin, LOW);
	pinMode(LED_BUILTIN, OUTPUT);
	blink(1000, 1);

	//Netwk
		WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		config.api_key = CONFIG_API_KEY;
		config.database_url = CONFIG_DATABASE_URL;
		config.signer.tokens.legacy_token = CONFIG_SIGNER_TOKENS_LEGACY_TOKEN;
		Firebase.begin(&config, &auth);
		Firebase.reconnectWiFi(true); // Comment or "false" when WiFi reconnection will control by your code or third party library
		Firebase.setDoubleDigits(2);

	initValues();

	blink(500, 2);
}

void loop() {
	if (millis() - sendDataPrevMillis > Interval.value || sendDataPrevMillis == 0) {
		sendDataPrevMillis = millis();
		digitalWrite(LED_BUILTIN, LOW);
		Firebase.setTimestamp(fbdo, Timestamp.sloc);
		Firebase.setString(fbdo, OpStatus.sloc, "Under Operation");

		loadSettings();

		reservoirLvChk();

		pumpingJudgement(Main);
		pumpingJudgement(Sub);

		setInterval();

		digitalWrite(LED_BUILTIN, HIGH);
	}
}

void blink(int i, int j) {
	for (int k = 0; k <= j; k++) {
		digitalWrite(LED_BUILTIN, LOW);
		delay(i);
		digitalWrite(LED_BUILTIN, HIGH);
		delay(i);
	}
}

void initValues() {
	Firebase.setString(fbdo, OpStatus.sloc, "Controller Booting");
	Firebase.setInt(fbdo, Main.slocStatusCount, Count.value - Main.count);
	Firebase.setInt(fbdo, Sub.slocStatusCount, Count.value - Sub.count);
	Firebase.setInt(fbdo, Main.slocStatusMillis, Millis.value - pumpOnDuration);
	Firebase.setInt(fbdo, Sub.slocStatusMillis, Millis.value - pumpOnDuration);
	Firebase.setString(fbdo, Main.slocPumpStatus, "OFF");
	Firebase.setString(fbdo, Sub.slocPumpStatus, "OFF");
}

void loadSettings() {
	load(IntervalOn);
	load(IntervalOff);
	load(LvMax);
	load(LvMin);
	load(Count);
	load(Millis);
}

void load(FB &v) {
	if (Firebase.getInt(fbdo, v.sloc)) {
		v.value = fbdo.intData();
		Firebase.setString(fbdo, v.status, "OK");
	} else {
		Firebase.setString(fbdo, v.status, fbdo.errorReason().c_str());
	}
}

void reservoirLvChk() {
	Firebase.getInt(fbdo, Reservoir.sloc);
	Reservoir.value = fbdo.intData();
	if (Reservoir.value < 5) emStop(3);
}

void setInterval() {
	pumpOnDuration == 0 ? (Interval.value = IntervalOff.value) : (Interval.value = IntervalOn.value);
	Firebase.setInt(fbdo, Interval.sloc, Interval.value);
	Firebase.setString(fbdo, Interval.status, "OK");
}

void pumpingJudgement(Var &v) {
	timestampChk(v);
	pumpMillisChk(v);

	Firebase.getInt(fbdo, v.slocLv);
	int waterLv = fbdo.intData();
	if (waterLv < LvMin.value && pumpOnDuration == 0){
		digitalWrite(v.pin, HIGH);
		v.pumpStartMillis = millis() - 1;
		Firebase.setString(fbdo, v.slocPumpStatus, "ON");
		pumpOnDuration = millis() - v.pumpStartMillis;
		Firebase.setInt(fbdo, v.slocStatusMillis, Millis.value - pumpOnDuration);
	}
	if(waterLv > LvMax.value && pumpOnDuration != 0){
		digitalWrite(v.pin, LOW);
		v.pumpStartMillis = 0;
		pumpOnDuration = 0;
		Firebase.setString(fbdo, v.slocPumpStatus, "OFF");
		Firebase.setInt(fbdo, v.slocStatusMillis, Millis.value - pumpOnDuration);
	}
}

void timestampChk(Var &v){
	Firebase.getInt(fbdo, v.slocTimestamp);
	v.timeNow = fbdo.intData();
	if(v.timeNow == v.timeLast) {
		v.count++;
		Firebase.setInt(fbdo, v.slocStatusCount, Count.value - v.count);
	} else {
		v.count = 0;
		v.timeLast = v.timeNow;
		Firebase.setInt(fbdo, v.slocStatusCount, Count.value - v.count);
	}
	if (v.count > Count.value) {
		Firebase.setInt(fbdo, v.slocStatusCount, Count.value - v.count);
		emStop(1);
	}
}

void pumpMillisChk(Var &v){
	if(v.pumpStartMillis !=0){
		pumpOnDuration = millis() - v.pumpStartMillis;
		Firebase.setInt(fbdo, v.slocStatusMillis, Millis.value - pumpOnDuration);
	}
	if (pumpOnDuration > Millis.value) {
		Firebase.setInt(fbdo, v.slocStatusMillis, Millis.value - pumpOnDuration);
		emStop(2);
	}
}

void emStop(int i){
	digitalWrite(Main.pin, LOW);
	Firebase.setString(fbdo, Main.slocPumpStatus, "OFF");
	digitalWrite(Sub.pin, LOW);
	Firebase.setString(fbdo, Sub.slocPumpStatus, "OFF");
	String statusMessage;
	switch (i) {
		case 0:
			break;
		case 1:
			statusMessage = "timestamp not updating";
			break;
		case 2:
			statusMessage = "exceeded max pumping duration";
			break;
		case 3:
			statusMessage = "reservoir tank water level low";
			break;
	}
	Firebase.setString(fbdo, OpStatus.sloc, "EmergencyStop - " + statusMessage);
	while(true){
		blink(100, 1);
	}

}
