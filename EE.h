//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- AskSin eeprom functions ---------------------------------------------------------------------------------------------
//- with a lot of support from martin876 at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef _EE_H
#define _EE_H

#include "HAL.h"

//- struct declaration ----------------------------------------------------------------------------------------------------
struct s_cnlTbl {		// channel table holds all information regarding channels and lists
	uint8_t cnl;
	uint8_t lst;
	uint8_t sIdx;
	uint8_t sLen;
	uint16_t pAddr;
};
struct s_peerTbl {		// peer table holds information were to find peers in eeprom
	uint8_t cnl;
	uint8_t pMax;
	uint16_t pAddr;
};
struct s_eepDefTbl {	// eeprom default table holds information which has to be added at boot time to the eeprom
	uint16_t addr;
	uint8_t len;
	uint8_t *contPtr;
};


//- class definition ------------------------------------------------------------------------------------------------------
class EE {
  public:		//---------------------------------------------------------------------------------------------------------
	struct s_devDef {	// device definition table
		const uint8_t   cnlNbr;															// number of channels
		const uint8_t   lstNbr;															// number of lists
		const uint8_t   *devIdnt;														// pointer to device identifier
		const uint8_t   *cnlAddr;														// pointer to slice addresses
		const s_cnlTbl  *cnlTbl;														// pointer to array of elements
		const s_peerTbl *peerTbl;														// pointer to peer table
	};
	struct s_eepDef {	// eeprom default definition table
		const uint8_t nbr;																// number of lines to add
		const s_eepDefTbl *eepDefTbl;													// pointer to defaults table
	};
	
	uint8_t HMID[3];	// own ID for communication
	uint8_t HMSR[10];	// serial ID for pairing, etc
	
  protected:	//---------------------------------------------------------------------------------------------------------
  private:		//---------------------------------------------------------------------------------------------------------
	#define maxMsgLen 16																// define max message length in byte
	
  public:		//---------------------------------------------------------------------------------------------------------
	EE();																				// class constructor
	void    init(void);
	void    testModul(void);															// prints register.h definition on console
	uint8_t isPairValid(uint8_t *pair);													// ok, check if a valid pair was given

	// peer functions
	void    clearPeers(void);															// ok, clears complete peer database
	uint8_t isPeerValid (uint8_t *peer);												// ok, checks if a valid peer was given

	uint8_t countFreeSlots(uint8_t cnl);												// ok, counts the free peer slots of a channel
	uint8_t getIdxByPeer(uint8_t cnl, uint8_t *peer);									// ok, find the index of the respective peer
	uint8_t getPeerByIdx(uint8_t cnl, uint8_t idx, uint8_t *peer);						// ok, returns the respective peer of the given index
	uint8_t addPeer(uint8_t cnl, uint8_t idx, uint8_t *peer);							// ok, writes a peer in the database on respective slot 
	uint8_t remPeer(uint8_t cnl, uint8_t idx);											// ok, writes a zero to the respective slot
	uint8_t countPeerSlc(uint8_t cnl);													// ok, count the slices for function getPeerListSlc
	uint8_t getPeerListSlc(uint8_t cnl, uint8_t slc, uint8_t *buf);						// ok, returns the whole peer database as a string	
	uint8_t getPeerSlots(uint8_t cnl);													// ok, returns max peers per channel
	
	// register functions
	void    clearRegs(void);

	uint8_t countRegListSlc(uint8_t cnl, uint8_t lst);
	uint8_t getRegListSlc(uint8_t cnl, uint8_t lst, uint8_t slc, uint8_t idx, uint8_t *buf);

	uint8_t getListForMsg3(uint8_t cnl, uint8_t lst, uint8_t *peer, uint8_t *buf);		
	uint8_t setListFromMsg(uint8_t cnl, uint8_t lst, uint8_t *peer, uint8_t len, uint8_t *buf); 
	uint8_t doesListExist(uint8_t cnl, uint8_t lst);									

	uint8_t getRegAddr(uint8_t cnl, uint8_t lst, uint8_t pIdx, uint8_t addr, uint8_t len, uint8_t *buf); 
	void    getCnlListByPeerIdx(uint8_t cnl, uint8_t peerIdx);							
	void    setListFromModule(uint8_t cnl, uint8_t peerIdx, uint8_t *data, uint8_t len); 
	

  private:		//---------------------------------------------------------------------------------------------------------


}; 
extern EE::s_devDef devDef;																// initial register.h
extern EE::s_eepDef eepDef;


//- some helpers ----------------------------------------------------------------------------------------------------------
uint16_t crc16(uint16_t crc, uint8_t a);												// crc function


#endif

