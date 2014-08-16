//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <trilu@gmx.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- AskSin protocol functions ---------------------------------------------------------------------------------------------
//- with a lot of support from martin876 at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

#define AS_DBG
//#define AS_DBG_EX
#include "AS.h"


// public:		//---------------------------------------------------------------------------------------------------------
AS::AS() {
}

void AS::init(void) {
	#ifdef AS_DBG																		// only if cc debug is set
	dbg.begin(57600);																	// dbg setup
	dbg << F("\n....\n");																// ...and some information
	dbg << F("AS.\n");																	// ...and some information
	#endif
	
	ee.init();																			// eeprom init
	cc.init();																			// init the rf module

	rv.HMID = HMID;																		// hand over the pointer to HMID for checking up if a message is for us
	rv.MAID = ee.MAID;																	// hand over the pointer to Master ID for checking if a message comes from Master

	initMillis();																		// start the millis counter

	// everything is setuped, enable RF functionality
	_enableGDO0Int;																		// enable interrupt to get a signal while receiving data
}
void AS::poll(void) {

	// check if something received
	if (ccGDO0()) {																		// check if something was received
		cc.rcvData(rv.buf);																// copy the data into the receiver module
		if (rv.hasData) {
			rv.decode();																// decode the string
			received();																	// and jump in the received function
		}
	}

	// check if something is to send

	// check if we could go to standby
	
	// some sanity poll routines
	
}
void AS::received(void) {
	uint8_t bIntend = ee.getIntend(rv.reID,rv.toID);									// get the intend of the message

	// some debugs
	#ifdef AS_DBG																		// only if AS debug is set
	dbg << (char)bIntend << F("> ") << pHex(rv.buf,rv.len) << '\n';
	#endif
	#ifdef AS_DBG_EX																	// only if extended AS debug is set

	dbg << F("   ");																	// save some byte and send 3 blanks once, instead of having it in every if
	
	if        ((rv.msgTyp == 0x00)) {
		dbg << F("DEVICE_INFO; fw: ") << pHex((rv.buf+10),1) << F(", type: ") << pHex((rv.buf+11),2) << F(", serial: ") << pHex((rv.buf+13),10) << '\n';
		dbg << F("              , class: ") << pHexB(rv.buf[23]) << F(", pCnlA: ") << pHexB(rv.buf[24]) << F(", pCnlB: ") << pHexB(rv.buf[25]) << F(", na: ") << pHexB(rv.buf[26]);

		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x01)) {
		dbg << F("CONFIG_PEER_ADD; cnl: ") << pHexB(rv.buf[10]) << F(", peer: ") << pHex((rv.buf+12),3) << F(", pCnlA: ") << pHexB(rv.buf[15]) << F(", pCnlB: ") << pHexB(rv.buf[16]);

		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x02)) {
		dbg << F("CONFIG_PEER_REMOVE; cnl: ") << pHexB(rv.buf[10]) << F(", peer: ") << pHex((rv.buf+12),3) << F(", pCnlA: ") << pHexB(rv.buf[15]) << F(", pCnlB: ") << pHexB(rv.buf[16]);

		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x03)) {
		dbg << F("CONFIG_PEER_LIST_REQ; cnl: ") << pHexB(rv.buf[10]);

		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x04)) {
		dbg << F("CONFIG_PARAM_REQ; cnl: ") << pHexB(rv.buf[10]) << F(", peer: ") << pHex((rv.buf+12),3) << F(", pCnl: ") << pHexB(rv.buf[15]) << F(", lst: ") << pHexB(rv.buf[16]);

		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x05)) {
		dbg << F("CONFIG_START; cnl: ") << pHexB(rv.buf[10]) << F(", peer: ") << pHex((rv.buf+12),3) << F(", pCnl: ") << pHexB(rv.buf[15]) << F(", lst: ") << pHexB(rv.buf[16]);

		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x06)) {
		dbg << F("CONFIG_END; cnl: ") << pHexB(rv.buf[10]);

		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x08)) {
		dbg << F("CONFIG_WRITE_INDEX; cnl: ") << pHexB(rv.buf[10]) << F(", data: ") << pHex((rv.buf+12),(rv.len-11));

		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x09)) {
		dbg << F("CONFIG_SERIAL_REQ");
		
		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x0A)) {
		dbg << F("PAIR_SERIAL, serial: ") << pHex((rv.buf+12),10);

		} else if ((rv.msgTyp == 0x01) && (rv.by11 == 0x0E)) {
		dbg << F("CONFIG_STATUS_REQUEST, cnl: ") << pHexB(rv.buf[10]);

		} else if ((rv.msgTyp == 0x02) && (rv.by10 == 0x00)) {
		if (rv.len == 0x0A) dbg << F("ACK");
		else dbg << F("ACK; data: ") << pHex((rv.buf+11),rv.len-10);

		} else if ((rv.msgTyp == 0x02) && (rv.by10 == 0x01)) {
		dbg << F("ACK_STATUS; cnl: ") << pHexB(rv.buf[11]) << F(", status: ") << pHexB(rv.buf[12]) << F(", down/up/loBat: ") << pHexB(rv.buf[13]);
		if (rv.len > 13) dbg << F(", rssi: ") << pHexB(rv.buf[14]);

		} else if ((rv.msgTyp == 0x02) && (rv.by10 == 0x02)) {
		dbg << F("ACK2");
		
		} else if ((rv.msgTyp == 0x02) && (rv.by10 == 0x04)) {
		dbg << F("ACK_PROC; para1: ") << pHex((rv.buf+11),2) << F(", para2: ") << pHex((rv.buf+13),2) << F(", para3: ") << pHex((rv.buf+15),2) << F(", para4: ") << pHexB(rv.buf[17]);

		} else if ((rv.msgTyp == 0x02) && (rv.by10 == 0x80)) {
		dbg << F("NACK");

		} else if ((rv.msgTyp == 0x02) && (rv.by10 == 0x84)) {
		dbg << F("NACK_TARGET_INVALID");
		
		} else if ((rv.msgTyp == 0x03)) {
		dbg << F("AES_REPLY; data: ") << pHex((rv.buf+10),rv.len-9);
		
		} else if ((rv.msgTyp == 0x04) && (rv.by10 == 0x01)) {
		dbg << F("TOpHMLAN:SEND_AES_CODE; cnl: ") << pHexB(rv.buf[11]);

		} else if ((rv.msgTyp == 0x04)) {
		dbg << F("TO_ACTOR:SEND_AES_CODE; code: ") << pHexB(rv.buf[11]);
		
		} else if ((rv.msgTyp == 0x10) && (rv.by10 == 0x00)) {
		dbg << F("INFO_SERIAL; serial: ") << pHex((rv.buf+11),10);

		} else if ((rv.msgTyp == 0x10) && (rv.by10 == 0x01)) {
		dbg << F("INFO_PEER_LIST; peer1: ") << pHex((rv.buf+11),4);
		if (rv.len >= 19) dbg << F(", peer2: ") << pHex((rv.buf+15),4);
		if (rv.len >= 23) dbg << F(", peer3: ") << pHex((rv.buf+19),4);
		if (rv.len >= 27) dbg << F(", peer4: ") << pHex((rv.buf+23),4);

		} else if ((rv.msgTyp == 0x10) && (rv.by10 == 0x02)) {
		dbg << F("INFO_PARAM_RESPONSE_PAIRS; data: ") << pHex((rv.buf+11),rv.len-10);

		} else if ((rv.msgTyp == 0x10) && (rv.by10 == 0x03)) {
		dbg << F("INFO_PARAM_RESPONSE_SEQ; offset: ") << pHexB(rv.buf[11]) << F(", data: ") << pHex((rv.buf+12),rv.len-11);

		} else if ((rv.msgTyp == 0x10) && (rv.by10 == 0x04)) {
		dbg << F("INFO_PARAMETER_CHANGE; cnl: ") << pHexB(rv.buf[11]) << F(", peer: ") << pHex((rv.buf+12),4) << F(", pLst: ") << pHexB(rv.buf[16]) << F(", data: ") << pHex((rv.buf+17),rv.len-16);

		} else if ((rv.msgTyp == 0x10) && (rv.by10 == 0x06)) {
		dbg << F("INFO_ACTUATOR_STATUS; cnl: ") << pHexB(rv.buf[11]) << F(", status: ") << pHexB(rv.buf[12]) << F(", na: ") << pHexB(rv.buf[13]);
		if (rv.len > 13) dbg << F(", rssi: ") << pHexB(rv.buf[14]);
		
		} else if ((rv.msgTyp == 0x11) && (rv.by10 == 0x02)) {
		dbg << F("SET; cnl: ") << pHexB(rv.buf[11]) << F(", value: ") << pHexB(rv.buf[12]) << F(", rampTime: ") << pHex((rv.buf+13),2) << F(", duration: ") << pHex((rv.buf+15),2);

		} else if ((rv.msgTyp == 0x11) && (rv.by10 == 0x03)) {
		dbg << F("STOP_CHANGE; cnl: ") << pHexB(rv.buf[11]);

		} else if ((rv.msgTyp == 0x11) && (rv.by10 == 0x04) && (rv.by11 == 0x00)) {
		dbg << F("RESET");

		} else if ((rv.msgTyp == 0x11) && (rv.by10 == 0x80)) {
		dbg << F("LED; cnl: ") << pHexB(rv.buf[11]) << F(", color: ") << pHexB(rv.buf[12]);

		} else if ((rv.msgTyp == 0x11) && (rv.by10 == 0x81) && (rv.by11 == 0x00)) {
		dbg << F("LED_ALL; Led1To16: ") << pHex((rv.buf+12),4);
		
		} else if ((rv.msgTyp == 0x11) && (rv.by10 == 0x81)) {
		dbg << F("LED; cnl: ") << pHexB(rv.buf[11]) << F(", time: ") << pHexB(rv.buf[12]) << F(", speed: ") << pHexB(rv.buf[13]);
		
		} else if ((rv.msgTyp == 0x11) && (rv.by10 == 0x82)) {
		dbg << F("SLEEPMODE; cnl: ") << pHexB(rv.buf[11]) << F(", mode: ") << pHexB(rv.buf[12]);
		
		} else if ((rv.msgTyp == 0x12)) {
		dbg << F("HAVE_DATA");
		
		} else if ((rv.msgTyp == 0x3E)) {
		dbg << F("SWITCH; dst: ") << pHex((rv.buf+10),3) << F(", na: ") << pHexB(rv.buf[13]) << F(", cnl: ") << pHexB(rv.buf[14]) << F(", counter: ") << pHexB(rv.buf[15]);
		
		} else if ((rv.msgTyp == 0x3F)) {
		dbg << F("TIMESTAMP; na: ") << pHex((rv.buf+10),2) << F(", time: ") << pHex((rv.buf+12),2);
		
		} else if ((rv.msgTyp == 0x40)) {
		dbg << F("REMOTE; button: ") << pHexB((rv.buf[10] & 0x3F)) << F(", long: ") << (rv.buf[10] & 0x40 ? 1:0) << F(", lowBatt: ") << (rv.buf[10] & 0x80 ? 1:0) << F(", counter: ") << pHexB(rv.buf[11]);
		
		} else if ((rv.msgTyp == 0x41)) {
		dbg << F("SENSOR_EVENT; button: ") <<pHexB((rv.buf[10] & 0x3F)) << F(", long: ") << (rv.buf[10] & 0x40 ? 1:0) << F(", lowBatt: ") << (rv.buf[10] & 0x80 ? 1:0) << F(", value: ") << pHexB(rv.buf[11]) << F(", next: ") << pHexB(rv.buf[12]);
		
		} else if ((rv.msgTyp == 0x53)) {
		dbg << F("SENSOR_DATA; cmd: ") << pHexB(rv.buf[10]) << F(", fld1: ") << pHexB(rv.buf[11]) << F(", val1: ") << pHex((rv.buf+12),2) << F(", fld2: ") << pHexB(rv.buf[14]) << F(", val2: ") << pHex((rv.buf+15),2) << F(", fld3: ") << pHexB(rv.buf[17]) << F(", val3: ") << pHex((rv.buf+18),2) << F(", fld4: ") << pHexB(rv.buf[20]) << F(", val4: ") << pHex((rv.buf+21),2);
		
		} else if ((rv.msgTyp == 0x58)) {
		dbg << F("CLIMATE_EVENT; cmd: ") << pHexB(rv.buf[10]) << F(", valvePos: ") << pHexB(rv.buf[11]);
		
		} else if ((rv.msgTyp == 0x70)) {
		dbg << F("WEATHER_EVENT; temp: ") << pHex((rv.buf+10),2) << F(", hum: ") << pHexB(rv.buf[12]);

		} else {
		dbg << F("Unknown Message, please report!");
	}
	dbg << F("\n\n");
	#endif

	// filter out unknown or not for us
	if ((bIntend == 'l') || (bIntend == 'u')) {											// not for us, or sender unknown
		rv.buf[0] = 0;																	// clear receive buffer
		return;
	}
	
	// filter out repeated messages
	
	
	// check which type of message was received
	if         ((rv.msgTyp == 0x01) && (rv.by11 == 0x01)) {								// CONFIG_PEER_ADD
		//CHANNEL        => "00,2",
		//PEER_ADDRESS   => "04,6",
		//PEER_CHANNEL_A => "10,2",
		//PEER_CHANNEL_B => "12,2", }},

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x02)) {								// CONFIG_PEER_REMOVE
		//CHANNEL        => "00,2",
		//PEER_ADDRESS   => '04,6,$val=CUL_HM_id2Name($val)',
		//PEER_CHANNEL_A => "10,2",
		//PEER_CHANNEL_B => "12,2", } },

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x03)) {								// CONFIG_PEER_LIST_REQ
		//CHANNEL => "0,2", },},

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x04)) {								// CONFIG_PARAM_REQ
		//CHANNEL        => "00,2",
		//PEER_ADDRESS   => "04,6",
		//PEER_CHANNEL   => "10,2",
		//PARAM_LIST     => "12,2", },},

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x05)) {								// CONFIG_START
		//CHANNEL        => "00,2",
		//PEER_ADDRESS   => "04,6",
		//PEER_CHANNEL   => "10,2",
		//PARAM_LIST     => "12,2", } },

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x06)) {								// CONFIG_END
		//CHANNEL => "0,2", } },

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x07)) {								// CONFIG_WRITE_INDEX
		//CHANNEL => "0,2",
		//ADDR => "4,2",
		//DATA => '6,,$val =~ s/(..)/ $1/g', } },

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x08)) {								// CONFIG_WRITE_INDEX
		//CHANNEL => "0,2",
		//DATA => '4,,$val =~ s/(..)(..)/ $1:$2/g', } },

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x09)) {								// CONFIG_SERIAL_REQ

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x0A)) {								// PAIR_SERIAL
		//SERIALNO       => '04,,$val=pack("H*",$val)', } },

	} else if  ((rv.msgTyp == 0x01) && (rv.by11 == 0x0E)) {								// CONFIG_STATUS_REQUEST
		//CHANNEL => "0,2", } },

	} else if  ((rv.msgTyp == 0x02) && (rv.by10 == 0x00)) {								// ACK
		
	} else if  ((rv.msgTyp == 0x02) && (rv.by10 == 0x01)) {								// ACK_STATUS
		//CHANNEL        => "02,2",
		//STATUS         => "04,2",
		//DOWN           => '06,02,$val=(hex($val)&0x20)?1:0',
		//UP             => '06,02,$val=(hex($val)&0x10)?1:0',
		//LOWBAT         => '06,02,$val=(hex($val)&0x80)?1:0',
		//RSSI           => '08,02,$val=(-1)*(hex($val))', }},

	} else if  ((rv.msgTyp == 0x02) && (rv.by10 == 0x02)) {								// ACK2 - smokeDetector pairing only?

 	} else if  ((rv.msgTyp == 0x02) && (rv.by10 == 0x04)) {								// ACK-proc - connected to AES??
		//Para1          => "02,4",
		//Para2          => "06,4",
		//Para3          => "10,4",
		//Para4          => "14,2",}}, # remote?
 
 	} else if  ((rv.msgTyp == 0x02) && (rv.by11 == 0x80)) {								// NACK
		 
 	} else if  ((rv.msgTyp == 0x02) && (rv.by11 == 0x84)) {								// NACK_TARGET_INVALID

	} else if  (rv.msgTyp == 0x12) {													// HAVE_DATA
		
	} else if  (rv.msgTyp == 0x3E) {													// SWITCH
		//DST      => "00,6",
		//UNKNOWN  => "06,2",
		//CHANNEL  => "08,2",
		//COUNTER  => "10,2", } },

	} else if  (rv.msgTyp == 0x3F) {													// TimeStamp
		//UNKNOWN  => "00,4",
		//TIME     => "04,2", } },

	} else if  (rv.msgTyp == 0x40) {													// REMOTE
		//BUTTON   => '00,2,$val=(hex($val)&0x3F)',
		//LONG     => '00,2,$val=(hex($val)&0x40)?1:0',
		//LOWBAT   => '00,2,$val=(hex($val)&0x80)?1:0',
		//COUNTER  => "02,2", } },

	} else if  (rv.msgTyp == 0x41) {													// Sensor_event
		//BUTTON   => '00,2,$val=(hex($val)&0x3F)',
		//LONG     => '00,2,$val=(hex($val)&0x40)?1:0',
		//LOWBAT   => '00,2,$val=(hex($val)&0x80)?1:0',
		//NBR      => '02,2,$val=(hex($val))',
		//VALUE    => '04,2,$val=(hex($val))',} },

	} else if  (rv.msgTyp == 0x53) {													// SensorData
		//CMD => "00,2",
		//Fld1=> "02,2",
		//Val1=> '04,4,$val=(hex($val))',
		//Fld2=> "08,2",
		//Val2=> '10,4,$val=(hex($val))',
		//Fld3=> "14,2",
		//Val3=> '16,4,$val=(hex($val))',
		//Fld4=> "20,2",
		//Val4=> '24,4,$val=(hex($val))'} },

 	} else if  (rv.msgTyp == 0x58) {													// ClimateEvent
		//CMD      => "00,2",
		//ValvePos => '02,2,$val=(hex($val))', } },
 
 	} else if  (rv.msgTyp == 0x59) {													// setTeamTemp
		//CMD      => "00,2",
		//desTemp  => '02,2,$val=((hex($val)>>2) /2)',
		//mode     => '02,2,$val=(hex($val) & 0x3)',} },

	} else if  (rv.msgTyp == 0x70) {													// WeatherEvent
		//TEMP     => '00,4,$val=((hex($val)&0x3FFF)/10)*((hex($val)&0x4000)?-1:1)',
		//HUM      => '04,2,$val=(hex($val))', } },

	}


}


AS hm;



// public:		//---------------------------------------------------------------------------------------------------------
uint8_t  MilliTimer::poll(uint16_t ms) {
	uint8_t ready = 0;
	if (armed) {
		uint16_t remain = next - getMillis();
		if (remain <= 60000) return 0;	
		ready = -remain;
	}
	set(ms);
	return ready;
}
uint16_t MilliTimer::remaining() const {
	uint16_t remain = armed ? next - getMillis() : 0;
	return remain <= 60000 ? remain : 0;
}
void     MilliTimer::set(uint16_t ms) {
	armed = ms != 0;
	if (armed)
	next = getMillis() + ms - 1;
}

