//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <trilu@gmx.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- AskSin battery status functions ---------------------------------------------------------------------------------------
//- with a lot of support from martin876 at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

#define BT_DBG
#include "Battery.h"
#include "AS.h"

waitTimer battTmr;																			// battery timer for duration check

// public:		//---------------------------------------------------------------------------------------------------------
void    BT::set(uint8_t mode, uint8_t tenthVolt, uint32_t duration) {
	bMode = mode;
	bDuration = duration;
	checkTenthVolt = tenthVolt;
}

// private:		//---------------------------------------------------------------------------------------------------------
BT::BT() {
} 
void    BT::init(AS *ptrMain) {
	
	#ifdef BT_DBG																			// only if ee debug is set
	dbgStart();																				// serial setup
	dbg << F("BT.\n");																		// ...and some information
	#endif

	pHM = ptrMain;
	bMode = 0;
	bDuration = 0;
}
void    BT::poll(void) {
	
	if (!battTmr.done() ) return;															// timer still running

	if (bMode == 0) {																		// nothing to do, step out
		return;

	} else  if (bMode == BATTERY_MODE_BANDGAP_MESSUREMENT) {								// measure internal
		measureTenthVolt = getBatteryVoltageInternal();

	} else  if (bMode == BATTERY_MODE_EXTERNAL_MESSUREMENT) {								// measure external
		measureTenthVolt = getBatteryVoltageExternal();
	}

	bState = (measureTenthVolt < checkTenthVolt) ? 1 : 0;									// set the battery status

	#ifdef BT_DBG																			// only if ee debug is set
	dbg << "cTV:" << checkTenthVolt << ", mTV:" << measureTenthVolt << " , s:" << bState << '\n';
	#endif

	battTmr.set(bDuration);																	// set next check time
}
uint8_t BT::getStatus(void) {
	return bState;
}
