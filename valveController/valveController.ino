//#define DEBUG_MODE
#define serialInit 0
#define serialReport 1
#define serialValveState 2
#define serialElapsedTime 3

//PinMap - Arduino Pro Micro
	const int pinSensor = A3;
	const int pinValve = A2;

//Setting
	const int interval = 10000;
	const int thresholdLo = 30; //percent;
	const int thresholdHi = 70; //percent;

//MoistureSensor
	const int dMax = 400;
	const int dMin = 200;
	const int sampleCount = 10;
	int dataBuffer[sampleCount] = {};
	int dataBufferIndex;
	int reading;
	int measure_TTL;
	int dataCount;
	int measure;
	int measure_p;

//Display
	#include <SPI.h>
	#include <Wire.h>
	#include <Adafruit_GFX.h>
	#include <Adafruit_SSD1306.h>
	const int screenAddress = 0x3C;
	const int screenWidth = 128;
	const int screenHeight = 64;
	const int oledReset = -1; // reset pin shared w/ arduino
	Adafruit_SSD1306 display(screenWidth, screenHeight, &Wire, oledReset);

//WaterValve
	#include <VarSpeedServo.h>
	VarSpeedServo valve;
	const int valveClose = 0; // degree of servo
	const int valveOpen = 90;
	const int valveSpeed = 15; // 0~255
	int valveState;
	unsigned long lastValveChg;
	unsigned long elapsed;
	String unit;
	unsigned long elapsedUnit;

void setup() {
	display.begin(SSD1306_SWITCHCAPVCC, screenAddress);
	display.clearDisplay();
	pinMode(pinSensor, INPUT);
	valve.attach(pinValve);
	valve.write(0, 15);
	debugMode(serialInit);
}

void loop() {
	//read
		reading = analogRead(pinSensor);
		if ( reading >= dMax) reading = dMax;
		else if (reading <= dMin) reading = dMin;

	//store
		dataBuffer[dataBufferIndex++] = reading;
		if (dataBufferIndex == sampleCount) dataBufferIndex = 0;

	//calculate
		dataCount = 0;
		measure_TTL = 0;
		for (int i = 0; i < sampleCount; i++) {
			measure_TTL += dataBuffer[i];
			if (dataBuffer[i] > 0) dataCount++;
		}
		measure = measure_TTL / dataCount;
		measure_p = 99 - map(measure, dMin, dMax, 0, 99);

	//report
		elapsedTime();
		display.setCursor(0, 0);
		display.clearDisplay();
		display.setTextColor(SSD1306_WHITE);
		display.setTextSize(7); display.print(measure_p);
		display.setTextSize(3); display.print("%");
		if(elapsedUnit < 10){
			display.setCursor(104, 40);
			display.setTextSize(3); display.print(elapsedUnit);
			display.setTextSize(1); if(valveState == 1) display.print("O");
									if(valveState == 0) display.print("X");
		}else{
			display.setCursor(86, 40);
			display.setTextSize(3); display.print(elapsedUnit);
			display.setTextSize(1); if(valveState == 1) display.print("O");
									if(valveState == 0) display.print("X");
		}
		display.setCursor(122, 54);
		display.print(unit);
		display.display();
		debugMode(serialElapsedTime);
		debugMode(serialReport);

	//motor ctrl and array re-init
		if (dataCount == sampleCount){
			if ( measure_p <= thresholdLo && valveState == 0){
				valve.write(valveOpen, valveSpeed);
				valveState = 1;
				lastValveChg = millis();
				for (int j = 0; j < sampleCount; j++) dataBuffer[j] = 0;
				debugMode(serialValveState);
			} else if ( measure_p >= thresholdHi && valveState == 1){
				valve.write(valveClose, valveSpeed);
				valveState = 0;
				lastValveChg = millis();
				for (int j = 0; j < sampleCount; j++) dataBuffer[j] = 0;
				debugMode(serialValveState);
			}
			dataBufferIndex = 0;
		}
	delay(interval);
}

void elapsedTime(){
	elapsed = millis() - lastValveChg;
	switch(elapsed){
		case 0 ... 3600000:
			elapsedUnit = elapsed / 60000;
			unit = "M";
			break;

		case 3600001 ... 86400000:
			elapsedUnit = elapsed / 3600000;
			unit = "H";
			break;

		case 86400001 ... 4320000000:
			elapsedUnit = elapsed / 86400000;
			unit = "D";
			break;
	}
}

void debugMode(int i) {
#ifdef DEBUG_MODE
	switch(i) {
		case serialInit:
			Serial.begin(9600);
			while(!Serial){}
			Serial.print("Device Ready");
			break;

		case serialElapsedTime:
			Serial.print(elapsedUnit); Serial.print(unit);
			//Serial.println(elapsed);
			break;

		case serialReport:
			Serial.print(dataCount);
			Serial.print(": ");
			Serial.print(measure_p);
			Serial.print("%");
			int measure_raw_p = 99 - map(reading, dMin, dMax, 0, 99);
			Serial.print(" (raw:");
			Serial.print(measure_raw_p);
			Serial.print("%)");
			/*
			for(int i = 0; i < sampleCount; i++){
				Serial.print(dataBuffer[i]);
				Serial.print(", ");
			}
			*/
			break;

		case serialValveState:
			if (valveState == 1) {
				Serial.print("ValveOPENED @ "); Serial.print(measure_p); Serial.println("%");
			}
			if (valveState == 0) {
				Serial.print("ValveCLOSED @ "); Serial.print(measure_p); Serial.println("%");
			}
			break;
	}
	Serial.println();
#endif
}
