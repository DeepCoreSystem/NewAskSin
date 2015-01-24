#define SER_DBG


//- load library's --------------------------------------------------------------------------------------------------------
#include <AS.h>
#include "hardware.h"																		// hardware definition
#include "register.h"																		// configuration sheet
#include <THSensor.h>


//- load modules ----------------------------------------------------------------------------------------------------------
AS hm;																						// stage the asksin framework
THSensor thsens;																			// stage a dummy module
waitTimer xt;																				// sensor timer


//- load user modules -----------------------------------------------------------------------------------------------------
#include <Wire.h>																			// library to communicate with i2c sensor
#define I2C_ADDR     (0x29)
#define REG_CONTROL  0x00
#define REG_CONFIG   0x01
#define REG_DATALOW  0x04
#define REG_DATAHIGH 0x05
#define REG_ID       0x0A
static uint8_t M = 0;

uint8_t thVal = 0;																			// variable which holds the measured value


//- arduino functions -----------------------------------------------------------------------------------------------------
void setup() {
	// - Hardware setup ---------------------------------------
	// - everything off ---------------------------------------
	//ADCSRA = 0;																			// ADC off
	//power_all_disable();																	// and everything else
	
	//DDRB = DDRC = DDRD = 0x00;																// everything as input
	//PORTB = PORTC = PORTD = 0x00;															// pullup's off

	// enable only what is really needed
	//power_spi_enable();																	// enable only needed functions
	//power_timer0_enable();
	//power_usart0_enable();

	#ifdef SER_DBG
	dbgStart();																				// serial setup
	dbg << F("CUSTOM_LUX\n");																		// ...and some information
	#endif

	initMillis();																			// milli timer start
	initPCINT();																			// initialize the pin change interrupts
	ccInitHw();																				// initialize transceiver hardware
	initLeds();																				// initialize the leds
	initConfKey();																			// initialize the port for getting config key interrupts
	//initExtBattMeasurement();																// initialize the external battery measurement
	
	
	// - AskSin related ---------------------------------------
	// init the homematic framework and register user modules
	hm.init();																				// init the asksin framework

	hm.confButton.config(2, confKeyPCIE, confKeyINT);										// configure the config button, mode, pci byte and pci bit
	
	hm.ld.init(2, &hm);																		// set the led
	hm.ld.set(welcome);																		// show something
	
	hm.pw.setMode(0);																		// set power management mode
	hm.bt.set(1, 27, 3600000);		// 3600000 = 1h											// set battery check, internal, 2.7 reference, measurement each hour

	thsens.regInHM(1, 4, &hm);																// register sensor module on channel 1, with a list4 and introduce asksin instance
	thsens.config(&initTH1, &measureTH1, &thVal);											// configure the user class and handover addresses to respective functions and variables
	thsens.timing(0, 0, 0);																	// mode 0 transmit based on timing or 1 on level change; level change value; while in mode 1 timing value will stay as minimum delay on level change   


	// - user related -----------------------------------------


	dbg << F("HMID: ") << _HEX(HMID,3) << F(", MAID: ") << _HEX(MAID,3) << F("\n\n");		// some debug
	sei();																					// enable interrupts
}

void loop() {
	// - AskSin related ---------------------------------------
	hm.poll();																				// poll the homematic main loop


	// - user related -----------------------------------------
	//if (xt.done()) {
	//	uint16_t lux = readTSL();
	//	lux /= 32;
	//	dbg << "l: " << lux << '\n';
	//	xt.set(1000);
	//}
	

}


//- user functions --------------------------------------------------------------------------------------------------------
void initTH1() {
	initTSL();																				// init the sensor

	#ifdef SER_DBG
	dbg << "init th1\n";
	#endif
}
void measureTH1() {
	uint16_t lux = readTSL();																// read the sensor
	lux /= 32;																				// further /4 due to setup
	thVal = lux & 0xff;																		// mask the value to get a byte value
	
	#ifdef SER_DBG
	dbg << "lux/128: " << lux << ' ' << _TIME << '\n';
	#endif
}

static void     initTSL(void) {
	digitalWrite(5, HIGH);																	// set port pin
	Wire.begin();																			// start the i2c library

	#ifdef SER_DBG																			// some debug
		Wire.beginTransmission(I2C_ADDR);													// start i2c communication with device 0x29
		Wire.write(0x80|REG_ID);															// identify chip - write 0x80 to register 0x0a 
		Wire.endTransmission();																// 1000 TSL45317, 1001 TSL45313, 1010 TSL45315,	1011 TSL45311

		Wire.requestFrom(I2C_ADDR, 1);														// request 1 byte
		while(Wire.available()) {															// reading and printing the identification byte
			uint8_t c = Wire.read();
			dbg << F("chip id: ") << _HEXB(c & 0xF0) << '\n';								// print result
		}
	#endif
	
	
	#ifdef SER_DBG																			// some debug
		dbg << F("power on...\n");
	#endif
	Wire.beginTransmission(I2C_ADDR);														// start i2c com again
	Wire.write(0x80|REG_CONTROL);															// control register, 00 Power Down,	10 Run a single ADC cycle and return to PowerDown, 11 Normal Operation
	Wire.write(0x03);																		// power on
	Wire.endTransmission();

	#ifdef SER_DBG																			// some debug
		dbg << F("configure...\n");
	#endif
	M = 0x02;																				// BIT3 PowerSave Mode, BIT0:1 00 T=400 ms, 01 T=200 ms, 10 T=100ms
	Wire.beginTransmission(I2C_ADDR);														// start i2c communication
	Wire.write(0x80|REG_CONFIG);															// write to configuration register
	Wire.write(M);																			// M=1 T=400ms	
	Wire.endTransmission();	
}
static uint16_t readTSL(void) {
	uint16_t lx;

	Wire.beginTransmission(I2C_ADDR);														// start communication
	Wire.write(0x80|REG_DATALOW);															// access the low register
	Wire.endTransmission();																	// end transmition
	Wire.requestFrom(I2C_ADDR, 2);															// request 2 bytes
	lx  = (Wire.read()<<0) | (Wire.read()<<8);												// reading two bytes and writing in integer variable
	//l = Wire.read();																		
	//h = Wire.read();
	while(Wire.available()){ Wire.read(); }													// received more bytes?

	//lx  = (h<<8) | (l<<0);
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

