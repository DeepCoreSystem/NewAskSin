#define SER_DBG


//- load library's --------------------------------------------------------------------------------------------------------
#include <AS.h>
#include "hardware.h"																		// hardware definition
#include "register.h"																		// configuration sheet
#include <THSensor.h>


//- load modules ----------------------------------------------------------------------------------------------------------
AS hm;																						// stage the asksin framework
THSensor thsens;																			// stage a dummy module

waitTimer xt;


//- load user modules -----------------------------------------------------------------------------------------------------
#include <Wire.h>
#define I2C_ADDR     (0x29)
#define REG_CONTROL  0x00
#define REG_CONFIG   0x01
#define REG_DATALOW  0x04
#define REG_DATAHIGH 0x05
#define REG_ID       0x0A
static uint8_t M = 0;


uint8_t thVal = 128;																				// variable which holds the measured value


//- arduino functions -----------------------------------------------------------------------------------------------------
void setup() {
	#ifdef SER_DBG
	dbgStart();																				// serial setup
	dbg << F("Main\n");																		// ...and some information
	#endif
	
	// - Hardware setup ---------------------------------------
	// - everything off ---------------------------------------
	//ADCSRA = 0;																				// ADC off
	//power_all_disable();																	// and everything else
	
	//DDRB = DDRC = DDRD = 0x00;																// everything as input
	//PORTB = PORTC = PORTD = 0x00;															// pullup's off

	//power_spi_enable();																		// enable only needed functions
	//power_timer0_enable();
	//power_usart0_enable();

	initMillis();																			// milli timer start
	initPCINT();																			// initialize the pin change interrupts
	ccInitHw();																				// initialize transceiver hardware
	initLeds();																				// initialize the leds
	initConfKey();																			// initialize the port for getting config key interrupts
	//initExtBattMeasurement();																// initialize the external battery measurement
	
	
	// - AskSin related ---------------------------------------
	// init the homematic framework and register user modules
	hm.init();																				// init the asksin framework
	hm.confButton.config(1,0,0);															// configure the config button, mode, pci byte and pci bit
	
	hm.ld.init(2, &hm);																		// set the led
	hm.ld.set(welcome);																		// show something
	
	hm.pw.setMode(0);																		// set power management mode
	hm.bt.set(1, 27, 3600000);		// 3600000 = 1h											// set battery check, internal, 2.7 reference, measurement each hour

	thsens.regInHM(1, 4, &hm);																// register sensor module on channel 1, with a list4 and introduce asksin instance
	thsens.config(&initTH1, &measureTH1, &thVal);											// configure the user class and handover addresses to respective functions and variables
	thsens.timing(0, 0, 0);																	// mode 0 transmit based on timing or 1 on level change; level change value; while in mode 1 timing value will stay as minimum delay on level change   

	sei();																					// enable interrupts
	
	// - user related -----------------------------------------
	//uint8_t x[5] = {0x01,0x11,0x02,0x22,0x03};
	//dbg << "a:" << _HEX(x,5) << _TIME << '\n';
	//dbg << "b:" << _HEXB(0xff) << '\n';

	xt.set(100);
	initTSL();
}

void loop() {
	// - AskSin related ---------------------------------------
	hm.poll();																				// poll the homematic main loop


	// - user related -----------------------------------------
	//if (xt.done()) {
	//	dbg << getBatteryVoltageInternal() << '\n';
	//	xt.set(1000);
	//}
	
	/*if (xt.done()) {
	//	dbg << getBatteryVoltageExternal() << '\n';
		xt.set(1000);

		uint32_t lux;
		lux = readTSL();
   
		// automatically adjust range
		if (((lux & 0xc000) == 0xc000) && (M<2)) {
			M++;
			configTSL();
			return;
		}

		if (!(lux & 0xc000) && (M>0)) {
			M--;
			configTSL();
			return;
		}
   
		lux *= (1<<M);
		Serial.print("Lux: ");
		Serial.println(lux, DEC);

	}*/
}


//- user functions --------------------------------------------------------------------------------------------------------
void initTH1() {
	dbg << "init th1\n";
	
}
void measureTH1() {
	dbg << "measure th1 " << _TIME << '\n';

}

static void     initTSL(void) {
	digitalWrite(5, HIGH);
	Wire.begin();

	Serial.print("ID: ");
	Wire.beginTransmission(I2C_ADDR);
	Wire.write(0x80|REG_ID);
	Wire.endTransmission();
	Wire.requestFrom(I2C_ADDR, 1); //request 1 byte
	while(Wire.available()) {
		unsigned char c = Wire.read();
		Serial.print(c&0xF0, HEX);
	}
	Serial.println("");

	Serial.println("Power on...");
	Wire.beginTransmission(I2C_ADDR);
	Wire.write(0x80|REG_CONTROL);
	Wire.write(0x03); //power on
	Wire.endTransmission();

	Serial.println("Config...");
	M = 0;
	Wire.beginTransmission(I2C_ADDR);
	Wire.write(0x80|REG_CONFIG);
	Wire.write(M); //M=1 T=400ms
	Wire.endTransmission();	
}
static uint16_t readTSL(void) {
	uint16_t l, h, lx;

	Wire.beginTransmission(I2C_ADDR);
	Wire.write(0x80|REG_DATALOW);
	Wire.endTransmission();
	Wire.requestFrom(I2C_ADDR, 2); //request 2 bytes
	l = Wire.read();
	h = Wire.read();
	while(Wire.available()){ Wire.read(); } //received more bytes?

	lx  = (h<<8) | (l<<0);
	return lx;
}
static void     configTSL(void) {
	Serial.print("re-Config... M = ");
	Serial.println(M, DEC);
	Wire.beginTransmission(I2C_ADDR);
	Wire.write(0x80|REG_CONFIG);
	Wire.write(M); //M=1 T=400ms
	Wire.endTransmission();
	delay(500);
	readTSL(); // dummy read
	delay(500);
}


//- predefined functions --------------------------------------------------------------------------------------------------
void serialEvent(void) {
	#ifdef SER_DBG
	
	static uint8_t i = 0;																	// it is a high byte next time
	while (Serial.available()) {
		uint8_t inChar = (uint8_t)Serial.read();											// read a byte
		if (inChar == '\n') {																// send to receive routine
			i = 0;
			hm.sn.active = 1;
		}
		
		if      ((inChar>96) && (inChar<103)) inChar-=87;									// a - f
		else if ((inChar>64) && (inChar<71))  inChar-=55;									// A - F
		else if ((inChar>47) && (inChar<58))  inChar-=48;									// 0 - 9
		else continue;
		
		//if (i % 2 == 0) hm.sn.buf[i/2] = inChar << 4;										// high byte
		//else hm.sn.buf[i/2] |= inChar;														// low byte
		
		i++;
	}
	#endif
}

