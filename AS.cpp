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

waitTimer sndTimer;																			// send timer functionality

// public:		//---------------------------------------------------------------------------------------------------------
AS::AS() {
}
void AS::init(void) {
	#ifdef AS_DBG																			// only if cc debug is set
	dbg.begin(57600);																		// dbg setup
	dbg << F("\n....\n");																	// ...and some information
	dbg << F("AS.\n");																		// ...and some information
	#endif
	
	ee.init();																				// eeprom init
	cc.init();																				// init the rf module

	initMillis();																			// start the millis counter

	// everything is setuped, enable RF functionality
	_enableGDO0Int;																			// enable interrupt to get a signal while receiving data
}

// - poll functions --------------------------------
void AS::poll(void) {

	// check if something received
	if (ccGetGDO0()) {																		// check if something was received
		cc.rcvData(rcvBuf);																	// copy the data into the receiver module
		if (rcvHasData) {
			decode(rcvBuf);																	// decode the string
		}
	}

	if (rcvHasData) received();																// check if there is something in the received buffer

	if (sndStc.active) sender();															// check if something is to send
	if (slcList.active) sendSlcList();														// poll the slice list send function
	if (peerMsg.active) sendPeerMsg();														// poll the peer message sender
	
	// check if we could go to standby
	
	// some sanity poll routines
	
}
void AS::sender(void) {																		// handles the send queue
	#define maxRetries    3
	#define maxTime       300
	
	// set right amount of retries
	if (!sndStc.retr) {																		// first time run, check message type and set retries
		if (reqACK) sndStc.retr = maxRetries;												// if BIDI is set, we have three retries
		else sndStc.retr = 1;
	}
	
	// send something while timer is not busy with waiting for an answer and max tries are not done 
	if ((sndStc.cntr < sndStc.retr) && (sndTimer.done() )) {								// not all sends done and timing is OK

		// some sanity
		sndStc.timeOut = 0;																	// not timed out because just started
		sndStc.mCnt = snd.mCnt;																// copy the message count to identify the ACK
		sndStc.cntr++;																		// increase counter while send out

		// check if we should send an internal message
		if (cmpAry(snd.toID,HMID,3)) {														// message is addressed to us
			memcpy(rcvBuf, sndBuf, sndLen);													// copy send buffer to received buffer
			sndStc.cntr = 0xff;																// ACK not required, because internal
						
			#ifdef AS_DBG																	// only if AS debug is set
			dbg << F("<i ");
			#endif
		} else {																			// send it external
			uint8_t tBurst = snd.mFlg.Burst;												// get burst flag, while string will get encoded
			encode(sndBuf);																	// encode the string
			_disableGDO0Int
			cc.sndData(sndBuf,tBurst);														// send to communication module
			_enableGDO0Int
			decode(sndBuf);																	// decode the string, so it is readable next time
			
			if (reqACK) sndTimer.set(maxTime);												// set the time out for the message
			
			#ifdef AS_DBG																	// only if AS debug is set
			dbg << F("<- ");
			#endif
		}

		#ifdef AS_DBG																		// only if AS debug is set
		dbg << pHex(sndBuf,sndLen) << ' ' << pTime << '\n';
		#endif

	} else if (sndStc.cntr == 0xff) {														// answer was received, clean up the structure
		sndStc.timeOut = 0;
		sndStc.cntr = 0;
		sndStc.retr = 0;
		sndStc.active = 0;
		sndTimer.set(0);

	} else if ((sndStc.cntr >= sndStc.retr) && (sndTimer.done() )) {						// max retries achieved, but seems to have no answer
		sndStc.cntr = 0;
		sndStc.retr = 0;
		sndStc.active = 0;
		if (!reqACK) return;
		
		sndStc.timeOut = 1;																	// set the time out only while an ACK or answer was requested

		#ifdef AS_DBG																		// only if AS debug is set
		dbg << F("  timed out") << ' ' << pTime << '\n';
		#endif
	}


/*	
	// setting some variables
	powr.state = 1;																		// remember TRX module status, after sending it is always in RX mode
	if ((powr.mode > 0) && (powr.nxtTO < (millis() + powr.minTO))) stayAwake(powr.minTO); // stay awake for some time

	if (pevt.act == 1) {
		hm.statusLed.set(STATUSLED_BOTH, STATUSLED_MODE_BLINKFAST, 1);					// blink led 1 and led 2 once after key press
	}

	if (pevt.act == 1) {
		hm.statusLed.stop(STATUSLED_BOTH);
		hm.statusLed.set(STATUSLED_2, STATUSLED_MODE_BLINKSLOW, 1);						// blink the led 2 once if keypress before
	}*/
}
void AS::sendSlcList(void) {
	if (sndStc.active) return;																// check if send function has a free slot, otherwise return

	uint8_t cnt;

	if        (slcList.peer) {																// INFO_PEER_LIST
		cnt = ee.getPeerListSlc(slcList.cnl,slcList.curSlc,sndBuf+11);						// get the slice and the amount of bytes
		sendINFO_PEER_LIST(cnt);															// create the body		
		slcList.curSlc++;																	// increase slice counter
		//dbg << "peer slc: " << pHex(sndBuf,cnt) << '\n';									// write to send buffer

	} else if (slcList.reg2) {																// INFO_PARAM_RESPONSE_PAIRS
		cnt = ee.getRegListSlc(slcList.cnl,slcList.lst,slcList.idx,slcList.curSlc,sndBuf+11);// get the slice and the amount of bytes
		sendINFO_PARAM_RESPONSE_PAIRS(cnt);	
		slcList.curSlc++;																	// increase slice counter
		//dbg << "reg2 slc: " << pHex(sndBuf,cnt) << '\n';									// write to send buffer
		
	} else if (slcList.reg3) {																// INFO_PARAM_RESPONSE_SEQ

	}

	if (slcList.curSlc == slcList.totSlc) {													// if everything is send, we could empty the struct
		memset((void*)&slcList,0,10);														// by memset
		//dbg << "end: " << slcList.active << slcList.peer << slcList.reg2 << slcList.reg3 << '\n';
	}
}
void AS::sendPeerMsg(void) {
	// burst to be implemented - check on 6 Button HM Switch
	//
	#define maxRetries    3

	if (sndStc.active) return;															// check if send function has a free slot, otherwise return
	
	// first run, prepare amount of slots
	if (!peerMsg.maxIdx) peerMsg.maxIdx = ee.getPeerSlots(peerMsg.cnl);					// get amount of messages of peer channel

	// all slots of channel processed, start next round or end processing
	if (peerMsg.curIdx >= peerMsg.maxIdx) {												// check if all peer slots are done
		peerMsg.rnd++;																	// increase the round counter
		
		if ((peerMsg.rnd >= maxRetries) || (isEmty(peerMsg.slt,8))) {					// all rounds done or all peers reached
			//dbg << "through\n";
			sndCnt++;																	// increase the send message counter
			memset((void*)&peerMsg, 0, sizeof(s_peerMsg));								// clean out and return
			
		} else {																		// start next round
			//dbg << "next round\n";
			peerMsg.curIdx = 0;

		}
		return;

	} else if ((peerMsg.curIdx) && (!sndStc.timeOut)) {									// peer index is >0, first round done and no timeout
		peerMsg.slt[(peerMsg.curIdx-1) >> 3] &=  ~(1<<((peerMsg.curIdx-1) & 0x07));		// clear bit, because message got an ACK		

	}
	
	// set respective bit to check if ACK was received
	if (!peerMsg.rnd) peerMsg.slt[peerMsg.curIdx >> 3] |= (1<<(peerMsg.curIdx & 0x07));	// set bit in slt table										// clear bit in slt and increase counter


	// exit while bit is not set
	if (!peerMsg.slt[peerMsg.curIdx >> 3] & (1<<(peerMsg.curIdx & 0x07))) {
		peerMsg.curIdx++;																// increase counter for next time
		return;
	}

	uint8_t tPeer[4];																	// get the respective peer
	ee.getPeerByIdx(peerMsg.cnl,peerMsg.curIdx,tPeer);
		
	if (isEmty(tPeer,4)) {																// if peer is 0, set done bit in slt and skip
		peerMsg.slt[peerMsg.curIdx >> 3] &=  ~(1<<(peerMsg.curIdx & 0x07));				// remember empty peer in slt table										// clear bit in slt and increase counter
		peerMsg.curIdx++;																// increase counter for next time
		return;																			// wait for next round
	}

	// if we are here, there is something to send
	//dbg << "cnl:" << peerMsg.cnl << " cIdx:" << peerMsg.curIdx << " mIdx:" << peerMsg.maxIdx << " slt:" << pHex(peerMsg.slt,8) << '\n';
	
	// description --------------------------------------------------------
	//    len  cnt  flg  typ  reID      toID      pl
	// l> 0B   0A   A4   40   23 70 EC  1E 7A AD  02 01
	snd.mLen = peerMsg.lenPL +9;														// set message len
	snd.mCnt = sndCnt;																	// set message counter
	memcpy((void*)&snd.mFlg, (void*)&peerMsg.mFlg, 1);									// message flag
	snd.mTyp = peerMsg.mTyp;															// message type
	//uint8_t t1[] = {0x23,0x70,0xD8};
	//memcpy(snd.reID, t1, 3);															// sender id
	memcpy(snd.reID, HMID, 3);															// sender id
	memcpy(snd.toID, tPeer, 3);															// receiver id
	memcpy(sndBuf+10, peerMsg.pL, peerMsg.lenPL);										// payload
	
	sndStc.retr = 1;																	// send only one time
	sndStc.active = 1;																	// make send active
	
	if (!snd.mFlg.BIDI) peerMsg.slt[peerMsg.curIdx >> 3] &=  ~(1<<(peerMsg.curIdx & 0x07));// clear bit, because it is a message without need to be repeated
	peerMsg.curIdx++;																	// increase counter for next time
}
	
// - received functions ----------------------------
void AS::received(void) {
	static uint8_t last_rCnt;
	
	uint8_t bIntend = ee.getIntend(rcv.reID,rcv.toID);										// get the intend of the message

	// some debugs
	#ifdef AS_DBG																			// only if AS debug is set
	dbg << (char)bIntend << F("> ") << pHex(rcvBuf,rcvLen) << ' ' << pTime << '\n';
	#endif
	
	#ifdef AS_DBG_EX																		// only if extended AS debug is set
	explainMessage(rcvBuf);
	#endif 
	
	// filter out unknown or not for us
	if ((bIntend == 'l') || (bIntend == 'u')) {												// not for us, or sender unknown
		rcv.mLen = 0;																		// clear receive buffer
		return;
	}

	// filter out repeated messages
	if ((rcv.mFlg.RPTED) && (last_rCnt == rcv.mCnt)) {										// check if message was already received
		#ifdef AS_DBG																		// only if AS debug is set
		dbg << F("  repeated message\n");
		#endif

		rcv.mLen = 0;																		// clear receive buffer
		return;																				// wait for next message
	}
	last_rCnt = rcv.mCnt;
		
	// check which type of message was received
	if         ((rcv.mTyp == 0x01) && (rcv.by11 == 0x01)) {			// CONFIG_PEER_ADD
		// description --------------------------------------------------------
		//                                  Cnl      PeerID    PeerCnl_A  PeerCnl_B
		// l> 10 55 A0 01 63 19 63 01 02 04 01   01  1F A6 5C  06         05
		// do something with the information ----------------------------------

		// first call remPeer to avoid doubles
		uint8_t ret = ee.addPeer(rcv.by10,rcvBuf+12);										// send to addPeer function
		
		// let module registrations know of the change

		if ((ret) && (ackRq)) sendACK();													// send appropriate answer
		else if (ackRq) sendNACK();
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x02)) {			// CONFIG_PEER_REMOVE
		// description --------------------------------------------------------
		//                                  Cnl      PeerID    PeerCnl_A  PeerCnl_B
		// l> 10 55 A0 01 63 19 63 01 02 04 01   02  1F A6 5C  06         05
		// do something with the information ----------------------------------
		
		uint8_t ret = ee.remPeer(rcv.by10,rcvBuf+12);										// call the remPeer function
		if (ackRq) sendACK();																// send appropriate answer
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x03)) {			// CONFIG_PEER_LIST_REQ
		// description --------------------------------------------------------
		//                                  Cnl
		// l> 0B 05 A0 01 63 19 63 01 02 04 01  03
		// do something with the information ----------------------------------
		
		slcList.totSlc = ee.countPeerSlc(rcv.by10);											// how many slices are need
		slcList.mCnt = rcv.mCnt;															// remember the message count
		memcpy(slcList.toID,rcv.reID,3);
		slcList.cnl = rcv.by10;																// send input to the send peer function
		slcList.peer = 1;																	// set the type of answer
		slcList.active = 1;																	// start the send function
		// answer will send from sendSlcList(void)
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x04)) {			// CONFIG_PARAM_REQ
		// description --------------------------------------------------------
		//                                  Cnl    PeerID    PeerCnl  ParmLst
		// l> 10 04 A0 01 63 19 63 01 02 04 01  04 00 00 00  00       01
		// do something with the information ----------------------------------

		slcList.idx = ee.getIdxByPeer(rcv.by10, rcvBuf+12);									// fill struct
		slcList.totSlc = ee.countRegListSlc(rcv.by10, rcvBuf[16]);							// how many slices are need
		slcList.mCnt = rcv.mCnt;															// remember the message count
		memcpy(slcList.toID,rcv.reID,3);
		slcList.cnl = rcv.by10;																// send input to the send peer function
		slcList.lst = rcvBuf[16];															// send input to the send peer function
		slcList.reg2 = 1;																	// set the type of answer
		
		if ((slcList.idx != 0xff) && (slcList.totSlc > 0)) slcList.active = 1;				// only send register content if something is to send															// start the send function
		else memset((void*)&slcList,0,10);													// otherwise empty variable
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x05)) {			// CONFIG_START
		// description --------------------------------------------------------
		//                                  Cnl    PeerID    PeerCnl  ParmLst
		// l> 10 01 A0 01 63 19 63 01 02 04 00  05 00 00 00  00       00
		// do something with the information ----------------------------------

		cnfFlag.idx = ee.getIdxByPeer(rcv.by10, rcvBuf+12);									// fill structure to remember where to write
		cnfFlag.cnl = rcv.by10;
		cnfFlag.lst = rcvBuf[16];
		if (cnfFlag.idx != 0xff) {
			cnfFlag.active = 1;																// set active if there is no error on index
			// set message id flag to config in send module
		}
		
		if (ackRq) sendACK();																// send appropriate answer
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x06)) {			// CONFIG_END
		// description --------------------------------------------------------
		//                                  Cnl
		// l> 0B 01 A0 01 63 19 63 01 02 04 00  06
		// do something with the information ----------------------------------

		cnfFlag.active = 0;																	// set inactive
		// remove message id flag to config in send module
		
		if (ackRq) sendACK();																// send appropriate answer
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x07)) {			// CONFIG_WRITE_INDEX
		// sample needed

		
	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x08)) {			// CONFIG_WRITE_INDEX
		// description --------------------------------------------------------
		//                                  Cnl    Data
		// l> 13 02 A0 01 63 19 63 01 02 04 00  08 02 01 0A 63 0B 19 0C 63
		// do something with the information ----------------------------------

		if ((cnfFlag.active) && (cnfFlag.cnl == rcv.by10)) {								// check if we are in config mode and if the channel fit
			ee.setListArray(cnfFlag.cnl, cnfFlag.lst, cnfFlag.idx, rcvLen-11, rcvBuf+12);	// write the string to eeprom
		}
		// reload master id while cnl was 0 and lst was 0
		if (ackRq) sendACK();																// send appropriate answer
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x09)) {			// CONFIG_SERIAL_REQ
		// description --------------------------------------------------------
		//                                  
		// l> 0B 77 A0 01 63 19 63 01 02 04 00 09
		// do something with the information ----------------------------------
		sendINFO_SERIAL();																	// jump to create the answer
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x0A)) {			// PAIR_SERIAL
		// description --------------------------------------------------------
		//
		// b> 15 93 B4 01 63 19 63 00 00 00 01 0A 4B 45 51 30 32 33 37 33 39 36
		// do something with the information ----------------------------------
		if (cmpAry(rcvBuf+12,HMSR,10)) sendDEVICE_INFO();									// compare serial and send device info
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x01) && (rcv.by11 == 0x0E)) {			// CONFIG_STATUS_REQUEST
		//CHANNEL => "0,2", } },


	} else if  ((rcv.mTyp == 0x02) && (rcv.by10 == 0x00)) {			// ACK
		// description --------------------------------------------------------
		// 
		// l> 0A 05 80 02 63 19 63 01 02 04 00
		// do something with the information ----------------------------------
		
		if ((sndStc.active) && (rcv.mCnt == sndStc.mCnt)) sndStc.cntr = 0xff;				// was an ACK to an active message
		//dbg << "act:" << sndStc.active << " rC:" << rcv.mCnt << " sC:" << sndStc.mCnt << " cntr:" << sndStc.cntr << '\n';
		// --------------------------------------------------------------------
		
	} else if  ((rcv.mTyp == 0x02) && (rcv.by10 == 0x01)) {			// ACK_STATUS
		// description --------------------------------------------------------
		// <- 0B 08 B4 40 23 70 D8 1F B7 4A 02 08
		//                                      cnl stat DUL RSSI
		// l> 0E 08 80 02 1F B7 4A 23 70 D8 01  01  C8   80  27
		// do something with the information ----------------------------------
		// DUL = UP 10, DOWN 20, LOWBAT 80
		
		if ((sndStc.active) && (rcv.mCnt == sndStc.mCnt)) sndStc.cntr = 0xff;				// was an ACK to an active message
		// --------------------------------------------------------------------

	} else if  ((rcv.mTyp == 0x02) && (rcv.by10 == 0x02)) {			// ACK2 - smokeDetector pairing only?


	} else if  ((rcv.mTyp == 0x02) && (rcv.by10 == 0x04)) {			// ACK-proc - connected to AES??
		//Para1          => "02,4",
		//Para2          => "06,4",
		//Para3          => "10,4",
		//Para4          => "14,2",}}, # remote?

		
	} else if  ((rcv.mTyp == 0x02) && (rcv.by10 == 0x80)) {			// NACK
		// for test
		static uint8_t x2[2];
		x2[0] = 0x02;
		x2[1] += 1;
		
		sendREMOTE(1,x2);
		
	} else if  ((rcv.mTyp == 0x02) && (rcv.by10 == 0x84)) {			// NACK_TARGET_INVALID


	} else if  (rcv.mTyp == 0x12) {									// HAVE_DATA

		
	} else if  (rcv.mTyp == 0x3E) {									// SWITCH
		//DST      => "00,6",
		//UNKNOWN  => "06,2",
		//CHANNEL  => "08,2",
		//COUNTER  => "10,2", } },


	} else if  (rcv.mTyp == 0x3F) {									// TimeStamp
		//UNKNOWN  => "00,4",
		//TIME     => "04,2", } },


	} else if  (rcv.mTyp == 0x40) {									// REMOTE
		//BUTTON   => '00,2,$val=(hex($val)&0x3F)',
		//LONG     => '00,2,$val=(hex($val)&0x40)?1:0',
		//LOWBAT   => '00,2,$val=(hex($val)&0x80)?1:0',
		//COUNTER  => "02,2", } },


	} else if  (rcv.mTyp == 0x41) {									// Sensor_event
		//BUTTON   => '00,2,$val=(hex($val)&0x3F)',
		//LONG     => '00,2,$val=(hex($val)&0x40)?1:0',
		//LOWBAT   => '00,2,$val=(hex($val)&0x80)?1:0',
		//NBR      => '02,2,$val=(hex($val))',
		//VALUE    => '04,2,$val=(hex($val))',} },


	} else if  (rcv.mTyp == 0x53) {									// SensorData
		//CMD => "00,2",
		//Fld1=> "02,2",
		//Val1=> '04,4,$val=(hex($val))',
		//Fld2=> "08,2",
		//Val2=> '10,4,$val=(hex($val))',
		//Fld3=> "14,2",
		//Val3=> '16,4,$val=(hex($val))',
		//Fld4=> "20,2",
		//Val4=> '24,4,$val=(hex($val))'} },


	} else if  (rcv.mTyp == 0x58) {									// ClimateEvent
		//CMD      => "00,2",
		//ValvePos => '02,2,$val=(hex($val))', } },
		
		
	} else if  (rcv.mTyp == 0x59) {									// setTeamTemp
		//CMD      => "00,2",
		//desTemp  => '02,2,$val=((hex($val)>>2) /2)',
		//mode     => '02,2,$val=(hex($val) & 0x3)',} },


	} else if  (rcv.mTyp == 0x70) {									// WeatherEvent
		//TEMP     => '00,4,$val=((hex($val)&0x3FFF)/10)*((hex($val)&0x4000)?-1:1)',
		//HUM      => '04,2,$val=(hex($val))', } },


	}

	rcv.mLen = 0;																			// message progressed, nothing do to any more
}

// - send functions --------------------------------
void AS::sendDEVICE_INFO(void) {
	// description --------------------------------------------------------
	//                 reID      toID      fw  type   serial                         class  pCnlA  pCnlB  unknown
	// l> 1A 94 84 00  1F B7 4A  01 02 04  15  00 6C  4B 45 51 30 32 33 37 33 39 36  10     41     01     00
	// do something with the information ----------------------------------

	snd.mLen = 0x1a;

	if ((rcv.mTyp == 0x01) && (rcv.by11 == 0x0A)) snd.mCnt = rcv.mCnt;														// send counter - is it an answer or a initial message
	else snd.mCnt = sndCnt++;

	snd.mFlg.RPTEN = 1; snd.mFlg.CFG = 1;
	snd.mTyp = 0x00;
	memcpy(snd.reID,HMID,3);
	memcpy(snd.toID,MAID,3);

	memcpy_P(sndBuf+10,devDef.devIdnt,3);
	memcpy(sndBuf+13,HMSR,10);
	memcpy_P(sndBuf+23,devDef.devIdnt+3,4);
	sndStc.active = 1;																		// fire the message
}
void AS::sendACK(void) {
	// description --------------------------------------------------------
	//                reID      toID      ACK      
	// l> 0A 24 80 02 1F B7 4A  63 19 63  00
	// do something with the information ----------------------------------

	snd.mLen = 0x0a;
	snd.mCnt = rcv.mCnt;
	snd.mFlg.RPTEN = 1;
	snd.mTyp = 0x02;
	memcpy(snd.reID,HMID,3);
	memcpy(snd.toID,rcv.reID,3);
	snd.by10 = 0x00;
	sndStc.active = 1;																		// fire the message
}
void AS::sendACK_STATUS(void) {
	//"02;p01=01"   => { txt => "ACK_STATUS",  params => {
	//CHANNEL        => "02,2",
	//STATUS         => "04,2",
	//DOWN           => '06,02,$val=(hex($val)&0x20)?1:0',
	//UP             => '06,02,$val=(hex($val)&0x10)?1:0',
	//LOWBAT         => '06,02,$val=(hex($val)&0x80)?1:0',
	//RSSI           => '08,02,$val=(-1)*(hex($val))', }},
}
void AS::sendNACK(void) {
	// description --------------------------------------------------------
	//                reID      toID      NACK
	// l> 0A 24 80 02 1F B7 4A  63 19 63  80
	// do something with the information ----------------------------------

	snd.mLen = 0x0a;
	snd.mCnt = rcv.mCnt;
	snd.mFlg.RPTEN = 1;
	snd.mTyp = 0x02;
	memcpy(snd.reID,HMID,3);
	memcpy(snd.toID,rcv.reID,3);
	snd.by10 = 0x80;
	sndStc.active = 1;																		// fire the message
}
void AS::sendNACK_TARGET_INVALID(void) {
	// description --------------------------------------------------------
	//                reID      toID      ACK
	// l> 0A 24 80 02 1F B7 4A  63 19 63  84
	// do something with the information ----------------------------------

	snd.mLen = 0x0a;
	snd.mCnt = rcv.mCnt;
	snd.mFlg.RPTEN = 1;
	snd.mTyp = 0x02;
	memcpy(snd.reID,HMID,3);
	memcpy(snd.toID,rcv.reID,3);
	snd.by10 = 0x84;
	sndStc.active = 1;																		// fire the message
}
void AS::sendINFO_SERIAL(void) {
	// description --------------------------------------------------------
	// l> 0B 77 A0 01 63 19 63 1E 7A AD 00 09
	//                reID      toID     by10  serial
	// l> 14 77 80 10 1E 7A AD  63 19 63 00    4A 45 51 30 37 33 31 39 30 35
	// do something with the information ----------------------------------

	snd.mLen = 0x14;
	snd.mCnt = rcv.mCnt;
	snd.mFlg.RPTEN = 1;
	snd.mTyp = 0x10;
	memcpy(snd.reID,HMID,3);
	memcpy(snd.toID,rcv.reID,3);
	snd.by10 = 0x00;
	memcpy(sndBuf+11,HMSR,10);
	sndStc.active = 1;																		// fire the message
}
void AS::sendINFO_PEER_LIST(uint8_t len) {
	// description --------------------------------------------------------
	// l> 0B 44 A0 01 63 19 63 1F B7 4A 01 03
	//                reID      toID     by10  peer1        peer2
	// l> 1A 44 A0 10 1F B7 4A 63 19 63  01    22 66 08 02  22 66 08 01  22 66 08 04  22 66 08 03
	//
	// l> 0A 44 80 02 63 19 63 1F B7 4A 00
	// l> 1A 45 A0 10 1F B7 4A 63 19 63 01 24 88 2D 01 24 88 2D 02 24 88 2D 03 24 88 2D 04
	// l> 0A 45 80 02 63 19 63 1F B7 4A 00
	// l> 1A 46 A0 10 1F B7 4A 63 19 63 01 23 70 D8 02 23 70 D8 01 23 70 D8 04 23 70 D8 03
	// l> 0A 46 80 02 63 19 63 1F B7 4A 00
	// l> 1A 47 A0 10 1F B7 4A 63 19 63 01 23 70 D8 06 23 70 D8 05 22 6C 12 02 22 6C 12 01
	// l> 0A 47 80 02 63 19 63 1F B7 4A 00
	// l> 1A 48 A0 10 1F B7 4A 63 19 63 01 23 70 EC 02 23 70 EC 01 23 70 EC 04 23 70 EC 03
	// l> 0A 48 80 02 63 19 63 1F B7 4A 00
	// l> 16 49 A0 10 1F B7 4A 63 19 63 01 23 70 EC 06 23 70 EC 05 00 00 00 00
	// l> 0A 49 80 02 63 19 63 1F B7 4A 00
	// do something with the information ----------------------------------

	snd.mLen = 10+len;
	snd.mCnt = slcList.mCnt++;
	snd.mFlg.RPTEN = 1; snd.mFlg.BIDI = 1;
	snd.mTyp = 0x10;
	memcpy(snd.reID,HMID,3);
	memcpy(snd.toID,slcList.toID,3);
	snd.by10 = 0x01; //slcList.cnl;
	sndStc.active = 1;																		// fire the message
}
void AS::sendINFO_PARAM_RESPONSE_PAIRS(uint8_t len) {
	// description --------------------------------------------------------
	// l> 10 79 B0 01 63 19 63 01 02 04 00 04 00 00 00 00 00
	//                reID      toID      by10  reg  data  reg  data
	// l> 16 79 A0 10 01 02 04  63 19 63  02    02   01    05   40 0A 63 0B 19 0C 63 12 69
	//
	// l> 0A 79 80 02 63 19 63 01 02 04 00
	// l> 0C 7A A0 10 01 02 04 63 19 63 02 00 00
	// l> 0A 7A 80 02 63 19 63 01 02 04 00

	snd.mLen = 10+len;
	snd.mCnt = slcList.mCnt++;
	snd.mFlg.RPTEN = 1; snd.mFlg.BIDI = 1;
	snd.mTyp = 0x10;
	memcpy(snd.reID,HMID,3);
	memcpy(snd.toID,slcList.toID,3);
	snd.by10 = 0x02; //slcList.cnl;
	sndStc.active = 1;																		// fire the message
}
void AS::sendINFO_PARAM_RESPONSE_SEQ(uint8_t len) {
	// description --------------------------------------------------------
	// l> 10 90 A0 01 63 19 63 01 02 04 01 04 24 88 2D 03 03
	//                reID      toID      by10  Offset  Data
	// l> 16 90 A0 10 01 02 04  63 19 63  03    02      00 00 32 64 00 FF 00 FF 01 13 33
	//
	// l> 0A 90 80 02 63 19 63 01 02 04 00
	// l> 16 91 A0 10 01 02 04 63 19 63 03 82 00 00 32 64 00 FF 00 FF 21 13 33
	// l> 0A 91 80 02 63 19 63 01 02 04 00
	// l> 0C 92 A0 10 01 02 04 63 19 63 03 00 00
	// l> 0A 92 80 02 63 19 63 01 02 04 00


}
void AS::sendINFO_PARAMETER_CHANGE(void) {
	//"10;p01=04"   => { txt => "INFO_PARAMETER_CHANGE", params => {
	//CHANNEL => "2,2",
	//PEER    => '4,8,$val=CUL_HM_id2Name($val)',
	//PARAM_LIST => "12,2",
	//DATA => '14,,$val =~ s/(..)(..)/ $1:$2/g', } },
}
void AS::sendINFO_ACTUATOR_STATUS(void) {
	//"10;p01=06"   => { txt => "INFO_ACTUATOR_STATUS", params => {
	//CHANNEL => "2,2",
	//STATUS  => '4,2',
	//UNKNOWN => "6,2",
	//RSSI    => '08,02,$val=(-1)*(hex($val))' } },
}
void AS::sendINFO_TEMP(void) {
	//"10;p01=0A"   => { txt => "INFO_TEMP", params => {
	//SET     => '2,4,$val=(hex($val)>>10)&0x3F',
	//ACT     => '2,4,$val=hex($val)&0x3FF',
	//ERR     => "6,2",
	//VALVE   => "6,2",
	//MODE    => "6,2" } },
}
void AS::sendHAVE_DATA(void) {
	//"12"          => { txt => "HAVE_DATA"},
}
void AS::sendSWITCH(void) {
	//"3E"          => { txt => "SWITCH"      , params => {
	//DST      => "00,6",
	//UNKNOWN  => "06,2",
	//CHANNEL  => "08,2",
	//COUNTER  => "10,2", } },
}
void AS::sendTimeStamp(void) {
	//"3F"          => { txt => "TimeStamp"   , params => {
	//UNKNOWN  => "00,4",
	//TIME     => "04,2", } },
}
void AS::sendREMOTE(uint8_t cnl, uint8_t *pL) {
	// description --------------------------------------------------------
	//                 reID      toID      BLL Cnt
	// l> 0B 0A A4 40  23 70 EC  1E 7A AD  02  01
	// l> 0F 0A 80 02 1E 7A AD 23 70 EC 01 01 14 10 32 A4
	// l> 0B 0B B0 40 23 70 EC 1F B7 4A 02 01
	// l> 0E 0B 80 02 1F B7 4A 23 70 EC 01 01 C8 80 21
	// l> 0F 0B A4 10 1E 7A AD 63 19 63 06 01 C8 00 80 C8
	// l> 0A 0B 80 02 63 19 63 1E 7A AD 00
	// BUTTON = bit 0 - 5
	// LONG   = bit 6
	// LOWBAT = bit 7

	peerMsg.pL = pL;
	peerMsg.lenPL = 2;
	peerMsg.cnl = cnl;
	peerMsg.mFlg.RPTEN = 1; 
	peerMsg.mFlg.BIDI = 1;	// if BLL is set as long message, then no repeat is necessary
	peerMsg.mFlg.Burst = 1;
	peerMsg.mFlg.CFG = 1;
	peerMsg.mTyp = 0x40;
	peerMsg.active = 1;

}
void AS::sendSensor_event(uint8_t cnl, uint8_t *pL) {
	// description --------------------------------------------------------
	//                 reID      toID      BLL  Cnt  Val
	// l> 0C 0A A4 41  23 70 EC  1E 7A AD  02   01   200
	//"41"          => { txt => "Sensor_event", params => {
	// BUTTON = bit 0 - 5
	// LONG   = bit 6
	// LOWBAT = bit 7
}
void AS::sendSensorData(void) {
	//"53"          => { txt => "SensorData"  , params => {
	//CMD => "00,2",
	//Fld1=> "02,2",
	//Val1=> '04,4,$val=(hex($val))',
	//Fld2=> "08,2",
	//Val2=> '10,4,$val=(hex($val))',
	//Fld3=> "14,2",
	//Val3=> '16,4,$val=(hex($val))',
	//Fld4=> "20,2",
	//Val4=> '24,4,$val=(hex($val))'} },
}
void AS::sendClimateEvent(void) {
	//"58"          => { txt => "ClimateEvent", params => {
	//CMD      => "00,2",
	//ValvePos => '02,2,$val=(hex($val))', } },
}
void AS::sendSetTeamTemp(void) {
	//"59"          => { txt => "setTeamTemp" , params => {
	//CMD      => "00,2",
	//desTemp  => '02,2,$val=((hex($val)>>2) /2)',
	//mode     => '02,2,$val=(hex($val) & 0x3)',} },
}
void AS::sendWeatherEvent(void) {
	//"70"          => { txt => "WeatherEvent", params => {
	//TEMP     => '00,4,$val=((hex($val)&0x3FFF)/10)*((hex($val)&0x4000)?-1:1)',
	//HUM      => '04,2,$val=(hex($val))', } },
}	

// - homematic specific functions ------------------
void AS::decode(uint8_t *buf) {
	uint8_t prev = buf[1];
	buf[1] = (~buf[1]) ^ 0x89;

	uint8_t i, t;
	for (i=2; i<buf[0]; i++) {
		t = buf[i];
		buf[i] = (prev + 0xdc) ^ buf[i];
		prev = t;
	}

	buf[i] ^= buf[2];
}
void AS::encode(uint8_t *buf) {

	buf[1] = (~buf[1]) ^ 0x89;
	uint8_t buf2 = buf[2];
	uint8_t prev = buf[1];

	uint8_t i;
	for (i=2; i<buf[0]; i++) {
		prev = (prev + 0xdc) ^ buf[i];
		buf[i] = prev;
	}

	buf[i] ^= buf2;
}
void AS::explainMessage(uint8_t *buf) {
	dbg << F("   ");																		// save some byte and send 3 blanks once, instead of having it in every if
	
	if        ((buf[3] == 0x00)) {
		dbg << F("DEVICE_INFO; fw: ") << pHex((buf+10),1) << F(", type: ") << pHex((buf+11),2) << F(", serial: ") << pHex((buf+13),10) << '\n';
		dbg << F("              , class: ") << pHexB(buf[23]) << F(", pCnlA: ") << pHexB(buf[24]) << F(", pCnlB: ") << pHexB(buf[25]) << F(", na: ") << pHexB(buf[26]);

	} else if ((buf[3] == 0x01) && (buf[11] == 0x01)) {
		dbg << F("CONFIG_PEER_ADD; cnl: ") << pHexB(buf[10]) << F(", peer: ") << pHex((buf+12),3) << F(", pCnlA: ") << pHexB(buf[15]) << F(", pCnlB: ") << pHexB(buf[16]);

	} else if ((buf[3] == 0x01) && (buf[11] == 0x02)) {
		dbg << F("CONFIG_PEER_REMOVE; cnl: ") << pHexB(buf[10]) << F(", peer: ") << pHex((buf+12),3) << F(", pCnlA: ") << pHexB(buf[15]) << F(", pCnlB: ") << pHexB(buf[16]);

	} else if ((buf[3] == 0x01) && (buf[11] == 0x03)) {
		dbg << F("CONFIG_PEER_LIST_REQ; cnl: ") << pHexB(buf[10]);

	} else if ((buf[3] == 0x01) && (buf[11] == 0x04)) {
		dbg << F("CONFIG_PARAM_REQ; cnl: ") << pHexB(buf[10]) << F(", peer: ") << pHex((buf+12),3) << F(", pCnl: ") << pHexB(buf[15]) << F(", lst: ") << pHexB(buf[16]);

	} else if ((buf[3] == 0x01) && (buf[11] == 0x05)) {
		dbg << F("CONFIG_START; cnl: ") << pHexB(buf[10]) << F(", peer: ") << pHex((buf+12),3) << F(", pCnl: ") << pHexB(buf[15]) << F(", lst: ") << pHexB(buf[16]);

	} else if ((buf[3] == 0x01) && (buf[11] == 0x06)) {
		dbg << F("CONFIG_END; cnl: ") << pHexB(buf[10]);

	} else if ((buf[3] == 0x01) && (buf[11] == 0x08)) {
		dbg << F("CONFIG_WRITE_INDEX; cnl: ") << pHexB(buf[10]) << F(", data: ") << pHex((buf+12),(buf[0]-11));

	} else if ((buf[3] == 0x01) && (buf[11] == 0x09)) {
		dbg << F("CONFIG_SERIAL_REQ");
		
	} else if ((buf[3] == 0x01) && (buf[11] == 0x0A)) {
		dbg << F("PAIR_SERIAL, serial: ") << pHex((buf+12),10);

	} else if ((buf[3] == 0x01) && (buf[11] == 0x0E)) {
		dbg << F("CONFIG_STATUS_REQUEST, cnl: ") << pHexB(buf[10]);

	} else if ((buf[3] == 0x02) && (buf[11] == 0x00)) {
		if (buf[0] == 0x0A) dbg << F("ACK");
		else dbg << F("ACK; data: ") << pHex((buf+11),buf[0]-10);

	} else if ((buf[3] == 0x02) && (buf[11] == 0x01)) {
		dbg << F("ACK_STATUS; cnl: ") << pHexB(buf[11]) << F(", status: ") << pHexB(buf[12]) << F(", down/up/loBat: ") << pHexB(buf[13]);
		if (buf[0] > 13) dbg << F(", rssi: ") << pHexB(buf[14]);

	} else if ((buf[3] == 0x02) && (buf[11] == 0x02)) {
		dbg << F("ACK2");
		
	} else if ((buf[3] == 0x02) && (buf[11] == 0x04)) {
		dbg << F("ACK_PROC; para1: ") << pHex((buf+11),2) << F(", para2: ") << pHex((buf+13),2) << F(", para3: ") << pHex((buf+15),2) << F(", para4: ") << pHexB(buf[17]);

	} else if ((buf[3] == 0x02) && (buf[11] == 0x80)) {
		dbg << F("NACK");

	} else if ((buf[3] == 0x02) && (buf[11] == 0x84)) {
		dbg << F("NACK_TARGET_INVALID");
		
	} else if ((buf[3] == 0x03)) {
		dbg << F("AES_REPLY; data: ") << pHex((buf+10),buf[0]-9);
		
	} else if ((buf[3] == 0x04) && (buf[10] == 0x01)) {
		dbg << F("TOpHMLAN:SEND_AES_CODE; cnl: ") << pHexB(buf[11]);

	} else if ((buf[3] == 0x04)) {
		dbg << F("TO_ACTOR:SEND_AES_CODE; code: ") << pHexB(buf[11]);
		
	} else if ((buf[3] == 0x10) && (buf[10] == 0x00)) {
		dbg << F("INFO_SERIAL; serial: ") << pHex((buf+11),10);

	} else if ((buf[3] == 0x10) && (buf[10] == 0x01)) {
		dbg << F("INFO_PEER_LIST; peer1: ") << pHex((buf+11),4);
		if (buf[0] >= 19) dbg << F(", peer2: ") << pHex((buf+15),4);
		if (buf[0] >= 23) dbg << F(", peer3: ") << pHex((buf+19),4);
		if (buf[0] >= 27) dbg << F(", peer4: ") << pHex((buf+23),4);

	} else if ((buf[3] == 0x10) && (buf[10] == 0x02)) {
		dbg << F("INFO_PARAM_RESPONSE_PAIRS; data: ") << pHex((buf+11),buf[0]-10);

	} else if ((buf[3] == 0x10) && (buf[10] == 0x03)) {
		dbg << F("INFO_PARAM_RESPONSE_SEQ; offset: ") << pHexB(buf[11]) << F(", data: ") << pHex((buf+12),buf[0]-11);

	} else if ((buf[3] == 0x10) && (buf[10] == 0x04)) {
		dbg << F("INFO_PARAMETER_CHANGE; cnl: ") << pHexB(buf[11]) << F(", peer: ") << pHex((buf+12),4) << F(", pLst: ") << pHexB(buf[16]) << F(", data: ") << pHex((buf+17),buf[0]-16);

	} else if ((buf[3] == 0x10) && (buf[10] == 0x06)) {
		dbg << F("INFO_ACTUATOR_STATUS; cnl: ") << pHexB(buf[11]) << F(", status: ") << pHexB(buf[12]) << F(", na: ") << pHexB(buf[13]);
		if (buf[0] > 13) dbg << F(", rssi: ") << pHexB(buf[14]);
		
	} else if ((buf[3] == 0x11) && (buf[10] == 0x02)) {
		dbg << F("SET; cnl: ") << pHexB(buf[11]) << F(", value: ") << pHexB(buf[12]) << F(", rampTime: ") << pHex((buf+13),2) << F(", duration: ") << pHex((buf+15),2);

	} else if ((buf[3] == 0x11) && (buf[10] == 0x03)) {
		dbg << F("STOP_CHANGE; cnl: ") << pHexB(buf[11]);

	} else if ((buf[3] == 0x11) && (buf[10] == 0x04) && (buf[11] == 0x00)) {
		dbg << F("RESET");

	} else if ((buf[3] == 0x11) && (buf[10] == 0x80)) {
		dbg << F("LED; cnl: ") << pHexB(buf[11]) << F(", color: ") << pHexB(buf[12]);

	} else if ((buf[3] == 0x11) && (buf[10] == 0x81) && (buf[11] == 0x00)) {
		dbg << F("LED_ALL; Led1To16: ") << pHex((buf+12),4);
		
	} else if ((buf[3] == 0x11) && (buf[10] == 0x81)) {
		dbg << F("LED; cnl: ") << pHexB(buf[11]) << F(", time: ") << pHexB(buf[12]) << F(", speed: ") << pHexB(buf[13]);
		
	} else if ((buf[3] == 0x11) && (buf[10] == 0x82)) {
		dbg << F("SLEEPMODE; cnl: ") << pHexB(buf[11]) << F(", mode: ") << pHexB(buf[12]);
		
	} else if ((buf[3] == 0x12)) {
		dbg << F("HAVE_DATA");
		
	} else if ((buf[3] == 0x3E)) {
		dbg << F("SWITCH; dst: ") << pHex((buf+10),3) << F(", na: ") << pHexB(buf[13]) << F(", cnl: ") << pHexB(buf[14]) << F(", counter: ") << pHexB(buf[15]);
		
	} else if ((buf[3] == 0x3F)) {
		dbg << F("TIMESTAMP; na: ") << pHex((buf+10),2) << F(", time: ") << pHex((buf+12),2);
		
	} else if ((buf[3] == 0x40)) {
		dbg << F("REMOTE; button: ") << pHexB((buf[10] & 0x3F)) << F(", long: ") << (buf[10] & 0x40 ? 1:0) << F(", lowBatt: ") << (buf[10] & 0x80 ? 1:0) << F(", counter: ") << pHexB(buf[11]);
		
	} else if ((buf[3] == 0x41)) {
		dbg << F("SENSOR_EVENT; button: ") <<pHexB((buf[10] & 0x3F)) << F(", long: ") << (buf[10] & 0x40 ? 1:0) << F(", lowBatt: ") << (buf[10] & 0x80 ? 1:0) << F(", value: ") << pHexB(buf[11]) << F(", next: ") << pHexB(buf[12]);
		
	} else if ((buf[3] == 0x53)) {
		dbg << F("SENSOR_DATA; cmd: ") << pHexB(buf[10]) << F(", fld1: ") << pHexB(buf[11]) << F(", val1: ") << pHex((buf+12),2) << F(", fld2: ") << pHexB(buf[14]) << F(", val2: ") << pHex((buf+15),2) << F(", fld3: ") << pHexB(buf[17]) << F(", val3: ") << pHex((buf+18),2) << F(", fld4: ") << pHexB(buf[20]) << F(", val4: ") << pHex((buf+21),2);
		
	} else if ((buf[3] == 0x58)) {
		dbg << F("CLIMATE_EVENT; cmd: ") << pHexB(buf[10]) << F(", valvePos: ") << pHexB(buf[11]);
		
	} else if ((buf[3] == 0x70)) {
		dbg << F("WEATHER_EVENT; temp: ") << pHex((buf+10),2) << F(", hum: ") << pHexB(buf[12]);

	} else {
		dbg << F("Unknown Message, please report!");
	}
	dbg << F("\n\n");
}

// - some helpers ----------------------------------

AS hm;




// public:		//---------------------------------------------------------------------------------------------------------
uint8_t  waitTimer::done(void) {
	// todo - check if nexTime is near overflow and we have some delay, so getMillis() goes over 0
	// to get the correct timer result
	if (!armed) return 1;
	if ( nexTime > getMillis() ) return 0;
	armed = 0;
	return 1;
}
void     waitTimer::set(uint32_t ms) {
	armed = ms?1:0;
	if (armed) nexTime = getMillis() + ms -1;
}

