//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <trilu@gmx.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
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
	uint8_t MAID[3];	// master id for communication
	
  protected:	//---------------------------------------------------------------------------------------------------------
  private:		//---------------------------------------------------------------------------------------------------------
	#define maxMsgLen 16																// define max message length in byte
	
  public:		//---------------------------------------------------------------------------------------------------------
	EE();																				// class constructor
	void    init(void);
	void    getMasterID(void);
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
	void    clearRegs(void);															// ok, clears register space

	uint8_t countRegListSlc(uint8_t cnl, uint8_t lst);									// ok, counts the slices for a complete regs transmition
	uint8_t getRegListSlc(uint8_t cnl, uint8_t lst, uint8_t idx, uint8_t slc, uint8_t *buf);// ok, generates answer to a channel/list request
	uint8_t getRegAddr(uint8_t cnl, uint8_t lst, uint8_t idx, uint8_t addr);			// ok, gets a single register value
	uint8_t setListArray(uint8_t cnl, uint8_t lst, uint8_t idx, uint8_t len, uint8_t *buf);// ok, set registers from a string

	//uint8_t getListForMsg3(uint8_t cnl, uint8_t lst, uint8_t *peer, uint8_t *buf);		
	//void    getCnlListByPeerIdx(uint8_t cnl, uint8_t peerIdx);							
	//void    setListFromModule(uint8_t cnl, uint8_t peerIdx, uint8_t *data, uint8_t len); 

  private:		//---------------------------------------------------------------------------------------------------------
	uint8_t getRegListIdx(uint8_t cnl, uint8_t lst);									// ok, returns the respective line of cnlTbl

}; 
extern EE::s_devDef devDef;																// initial register.h
extern uint8_t HMID[];
extern uint8_t HMSR[];

//- some helpers ----------------------------------------------------------------------------------------------------------
uint16_t crc16(uint16_t crc, uint8_t a);												// crc function


#endif

