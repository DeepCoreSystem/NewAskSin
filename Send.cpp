//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <trilu@gmx.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- AskSin send function ---------------------------------------------------------------------------------------------
//- with a lot of support from martin876 at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

#define SN_DBG
#include "Send.h"
#include "AS.h"

#define sndLen       this->buf[0]+1
#define reqACK       this->mBdy.mFlg.BIDI													// check if an ACK is requested

waitTimer sndTmr;																			// send timer functionality

// public:		//---------------------------------------------------------------------------------------------------------
SN::SN() {
} 

void SN::init(AS *ptrMain) {
	#ifdef SN_DBG																			// only if ee debug is set
	dbgStart();																				// serial setup
	dbg << F("SN.\n");																		// ...and some information
	#endif

	pHM = ptrMain;
}
void SN::poll(void) {
	#define maxRetries    3
	#define maxTime       300
	
	// set right amount of retries
	if (!this->maxRetr) {																	// first time run, check message type and set retries
		if (reqACK) this->maxRetr = maxRetries;												// if BIDI is set, we have three retries
		else this->maxRetr = 1;
	}
	
	// send something while timer is not busy with waiting for an answer and max tries are not done 
	if ((this->retrCnt < this->maxRetr) && (sndTmr.done() )) {								// not all sends done and timing is OK

		// some sanity
		this->mBdy.mFlg.RPTEN = 1;
		this->timeOut = 0;																	// not timed out because just started
		this->lastMsgCnt = this->mBdy.mCnt;													// copy the message count to identify the ACK
		this->retrCnt++;																	// increase counter while send out

		// check if we should send an internal message
		if (cmpAry(this->mBdy.toID,HMID,3)) {												// message is addressed to us
			memcpy(pHM->rv.buf, this->buf, sndLen);											// copy send buffer to received buffer
			this->retrCnt = 0xff;															// ACK not required, because internal
						
			#ifdef SN_DBG																	// only if AS debug is set
			dbg << F("<i ");
			#endif

		} else {																			// send it external
			uint8_t tBurst = this->mBdy.mFlg.BURST;											// get burst flag, while string will get encoded
			pHM->encode(this->buf);															// encode the string
			_disableGDO0Int
			pHM->cc.sndData(this->buf,tBurst);												// send to communication module
			_enableGDO0Int
			pHM->decode(this->buf);															// decode the string, so it is readable next time
			
			if (reqACK) sndTmr.set(maxTime);												// set the time out for the message
			
			#ifdef SN_DBG																	// only if AS debug is set
			dbg << F("<- ");
			#endif

		}

		#ifdef SN_DBG																		// only if AS debug is set
		dbg << pHex(this->buf,sndLen) << ' ' << pTime << '\n';
		#endif

	} else if (this->retrCnt == 0xff) {														// answer was received, clean up the structure
		this->timeOut = 0;
		this->retrCnt = 0;
		this->maxRetr = 0;
		this->active = 0;
		sndTmr.set(0);

	} else if ((this->retrCnt >= this->maxRetr) && (sndTmr.done() )) {						// max retries achieved, but seems to have no answer
		this->retrCnt = 0;
		this->maxRetr = 0;
		this->active = 0;
		if (!reqACK) return;
		
		this->timeOut = 1;																	// set the time out only while an ACK or answer was requested

		#ifdef SN_DBG																		// only if AS debug is set
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
