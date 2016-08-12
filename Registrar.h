//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <trilu@gmx.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- AskSin registrar functions --------------------------------------------------------------------------------------------
//- with a lot of support from martin876 at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef _RG_H
#define _RG_H
#include "HAL.h"


/**
* @brief Delegation for channel module registrar.
*        There is no way to store class memeber pointers easily in c++,
*        therefore we use a delegation class.
* You will find more infos here: http://blog.coldflake.com/posts/C++-delegates-on-steroids/
*
*/
template<typename return_type, typename... params>
class Delegate {

	typedef return_type(*Type)(void* callee, params...);

public:	 //---------------------------------------------------------------------------------------------------------
	Delegate(void* callee = NULL, Type function = NULL) : fpCallee(callee), fpCallbackFunction(function) {}
	template <class T, return_type(T::*TMethod)(params...)>
	static Delegate from_function(T* callee) {
		Delegate d(callee, &methodCaller<T, TMethod>);
		return d;
	}
	return_type operator()(params... xs) const {
		return (*fpCallbackFunction)(fpCallee, xs...);
	}

private: //---------------------------------------------------------------------------------------------------------
	void* fpCallee;
	Type fpCallbackFunction;
	template <class T, return_type(T::*TMethod)(params...)>
	static return_type methodCaller(void* callee, params... xs) {
		T* p = static_cast<T*>(callee);
		return (p->*TMethod)(xs...);
	}
};

typedef Delegate<void, uint8_t, uint8_t, uint8_t, uint8_t*, uint8_t> myDelegate;



/**
* @brief Registrar class - this is all about an struct array to store information of
*        the registered channel modules. Holds a pointer for list 1 and list 3 or 4
*        but also a pointer to call the hmEventCol funktion of the respective channel module.
*
*  The struct array is sized by the amount of channels, channel 0 has an own slot to register
*  a conf button module in the future.
*
*/
class RG {
public:	//---------------------------------------------------------------------------------------------------------

	/**
	* @brief Struct to hold information to handle channel modules
	*
	* @param cnl      Channel where the module is registered to
	* @param lst      Module has a list3 or list 4
	* @param msgCnt   Channel message counter
	* @param *lstCnl  Pointer to list0/1 in the registered channel module
	* @param *lstPeer Pointer to list3/4
	* @param mDlgt    Delegate to the module function
	*/
	struct s_modTable {
		uint8_t cnl;																		// channel where the module is registered to
		uint8_t lst;																		// module has a list3 or list 4
		uint8_t msgCnt;																		// channel message counter
		uint8_t *lstCnl;																	// pointer to list0/1
		uint8_t *lstPeer;																	// pointer to list3/4
		/** @brief Delegate to the channel module function
		* (by3, by10, by11, *buf, len)
		* 0,0,0,NULL,0  = poll()
		*/
		myDelegate mDlgt;																	
	};

	RG() {}																					// constructor
	// todo: -- depreciated, clean up user channel modules 
	void regUserModuleInAS(uint8_t cnl, uint8_t lst, myDelegate delegate, uint8_t *mainList, uint8_t *peerList);
	void poll(void);																		// polls regulary through the channel modules
	
};

extern const uint8_t cnl_max;																// get access to cnl_max in register.h
extern RG::s_modTable modTbl[];																// initial register.h

#endif
