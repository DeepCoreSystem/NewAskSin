#define SER_DBG																				// serial debug messages

//- load library's --------------------------------------------------------------------------------------------------------
#include <AS.h>																				// ask sin framework
#include "hardware.h"																		// hardware definition
#include "register.h"																		// configuration sheet
#include <Relay.h>																			// relay class module


//- load modules ----------------------------------------------------------------------------------------------------------
AS hm;																						// stage the asksin framework
Relay relay;																				// stage a dummy module


//- arduino functions -----------------------------------------------------------------------------------------------------
void setup() {
	#ifdef SER_DBG
	dbgStart();																				// serial setup
	dbg << F("HM_LC_SW1_BA_PCB\n");															// ...and some information
	#endif
	
	// - Hardware setup ---------------------------------------
	// everything off
	ADCSRA = 0;																				// ADC off
	power_all_disable();																	// and everything else
	DDRB = DDRC = DDRD = 0x00;																// everything as input
	PORTB = PORTC = PORTD = 0x00;															// pullup's off

	// enable only what is really needed
	power_spi_enable();																		// SPI port for transceiver communication
	power_timer0_enable();																	// timer0 for getMillis and waitTimer
	power_usart0_enable();																	// serial port for debugging

	// initialize the hardware, functions to be found in hardware.cpp
	initMillis();																			// milli timer start
	initPCINT();																			// pin change interrupts
	ccInitHw();																				// transceiver hardware
	initLeds();																				// leds
	initConfKey();																			// config key pin and interrupt
	//initExtBattMeasurement();																// external battery measurement

	
	// - AskSin related ---------------------------------------
	// init the homematic framework and register user modules
	hm.init();																				// init the asksin framework

#if defined(__AVR_ATmega328P__)
	hm.confButton.config(2,0,0);															// configure the config button, mode, pci byte and pci bit
#elif defined(__AVR_ATmega32U4__)
	hm.confButton.config(2,0,6);															// configure the config button, mode, pci byte and pci bit
#endif
	
	hm.ld.init(2, &hm);																		// set the led
	hm.ld.set(welcome);																		// show something
	
	hm.pw.setMode(0);																		// set power management mode
	hm.bt.set(1, 27, 1800000);		// 1800000 = 0,5h										// set battery check

	relay.regInHM(1, 3, &hm);																// register relay module on channel 1, with a list3 and introduce asksin instance
	relay.config(&initRly, &switchRly);														// hand over the relay functions of main sketch
	

	// - user related -----------------------------------------


	dbg << F("HMID: ") << _HEX(HMID,3) << F(", MAID: ") << _HEX(MAID,3) << F("\n\n");		// some debug
	sei();																					// enable interrupts
}

void loop() {
	// - AskSin related ---------------------------------------
	hm.poll();																				// poll the homematic main loop
	

	// - user related -----------------------------------------

}


//- user functions --------------------------------------------------------------------------------------------------------
void initRly() {
// setting the relay pin as output, could be done also by pinMode(3, OUTPUT)

	pinOutput(DDRD,3);																		// init the relay pins
	setPinLow(PORTD,3);																		// set relay pin to ground
}
void switchRly(uint8_t status) {
// switching the relay, could be done also by digitalWrite(3,HIGH or LOW)

	if (status) setPinHigh(PORTD,3);														// check status and set relay pin accordingly
	else setPinLow(PORTD,3);
}


//- predefined functions --------------------------------------------------------------------------------------------------
void serialEvent() {
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
		
		if (i % 2 == 0) hm.sn.buf[i/2] = inChar << 4;										// high byte
		else hm.sn.buf[i/2] |= inChar;														// low byte
		
		i++;
	}
	#endif
}
