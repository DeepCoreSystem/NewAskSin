//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <trilu@gmx.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- AskSin power management function --------------------------------------------------------------------------------------
//- with a lot of support from martin876 at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

//#define PW_DBG
#include "Power.h"
#include "AS.h"

// private:		//---------------------------------------------------------------------------------------------------------
waitTimer pwrTmr;																			// power timer functionality


PW::PW() {
} 
void PW::init(AS *ptrMain) {
	#ifdef PW_DBG																			// only if ee debug is set
	dbgStart();																				// serial setup
	dbg << F("PW.\n");																		// ...and some information
	#endif

	pHM = ptrMain;																			// pointer to main class
	pwrMode = 0;																			// set default
}
void PW::setMode(uint8_t mode) {
	pwrMode = mode;

	#ifdef PW_DBG																			// only if pw debug is set
	dbg << F("PowerMode: ") << pwrMode << '\n';												// ...and some information
	#endif

	setSleepMode();
	stayAwake(2000);																		// startup means stay awake for next 20 seconds
}
void PW::stayAwake(uint16_t time) {
	if (time < pwrTmr.remain()) return;														// set new timeout only if we have to add something
	pwrTmr.set(time);
}
void PW::poll(void) {
	// check against active flag of various modules
	// on mode 0 there is nothing to do, maybe set idle mode to save some energy
	//
	// mode 1 means - check every 250ms if there is a transmition signal, if yes, wait
	// 50ms and check again - if it is still active, then wakeup the device for some time, 
	// if not, then sleep again
	//
	// mode 2 means - sleep for 250ms, wake up - check if something is to do, otherwise sleep again
	// communication module could stay idle, communication will start with transmition
	//
	// mode 3 means - sleep for 8000ms, wake up - check if something is to do, otherwise sleep again
	// communication module could stay idle, communication will start with transmition
	//
	// mode 4 means - sleep for ever until an interrupt get raised
	
	if (pwrMode == 0) return;																// no power savings, there for we can exit
	if (!pwrTmr.done()) return;																// timer active, jump out
	// some communication still active, jump out
	if ((pHM->sn.active) || (pHM->stcSlice.active) || (pHM->cFlag.active) || (pHM->pairActive)) return;
	
	#ifdef PW_DBG																			// only if pw debug is set
	dbg << '.';																				// ...and some information
	_delay_ms(1);
	uint32_t fTme = getMillis();
	#endif

	// check communication on power mode 1
	if (pwrMode == 1) {
		chkBurst();																			// rxtx check every 250ms, between deep sleep
		if (comStat == 1) {																	// communication is still active, means we have detected a burst, stay wake for some time																	
			stayAwake(500);
			return;
		}
	}

	// if we are here, we could go sleep. set cc module idle, switch off led's and sleep
	pHM->cc.setIdle();																		// set communication module to idle
	pHM->ld.set(nothing);																	// switch off all led's
	//pHM->ld.blinkRed();																	// we go sleeping

	cli();
	if ((pwrMode == 1) && (!chkCCBurst)) startWDG250ms();									// set respective watchdog time out
	if ((pwrMode == 1) && (chkCCBurst)) startWDG32ms();
	if (pwrMode == 2) startWDG250ms();
	if (pwrMode == 3) startWDG8000ms();
	sei();

	setSleep();																				// call sleep function in HAL
	// wake up will be here
	// ---------------------
	//
	if (pwrMode != 4) stopWDG();															// stop the watchdog
	stayAwake(6);																			// stay awake for a very short time to get things done
	
	#ifdef PW_DBG																			// only if pw debug is set
	dbg << ':';// << (getMillis() -fTme) << '\n';												// ...and some information
	#endif
		
}
void PW::chkBurst(void) {
	// only valid for power mode 1, we will check every 250ms if there is a burst signal
	// if there is one, we set the pwrTmr to stay awake for some time, otherwise we fall in sleep again
	// and switch communication to power down
	
	// well will check for burst twice - primary start every 250ms, if we detect a burst, we set the flag and check again in 50ms
	
	if (pHM->cc.detectBurst()) {															
		// burst detected, check if it was first or second time		
		if (chkCCBurst) {																	// it was second time
			chkCCBurst = 0;																	// reset the flag
			comStat = 1;																	// remember that communication module is still active

			#ifdef PW_DBG																	// only if pw debug is set
			dbg << '2';																		// ...and some information
			#endif
			
		} else {																			// it was first time
			chkCCBurst = 1;																	// set the flag
			//pHM->cc.setIdle();																// set communication module to idle
			comStat = 0;																	// remember that communication module is off

			#ifdef PW_DBG																	// only if pw debug is set
			dbg << '1';																		// ...and some information
			#endif
			
		}
		
	} else {
		if (chkCCBurst) {																	// secondary test was negative, reset the flag
			chkCCBurst = 0;																	// reset the flag

			#ifdef PW_DBG																	// only if pw debug is set
			dbg << '-';																		// ...and some information
			_delay_ms(1);
			#endif

		}
		// no burst detected, means we can go sleep again
		//pHM->cc.setIdle();																	// set communication module to idle
		comStat = 0;																		// remember that communication module is off
	}

}
