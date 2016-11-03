/*
 * hdp-ospf.h
 * Copyright (C) 2006 by the University of Extremadura
 * This program is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version. This program is distributed in the 
 * hope that it will be useful, but WITHOUT ANY WARRANTY; without 
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR 
 * A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details. You should have received a copy of the GNU General 
 * Public License along with this program. If not, see <http://www.gnu.org/licenses/>. 
 */

// by Inés María Romero Dávila (inroda04@alumnos.unex.es)
// and Alfonso Gazo Cervero (agazo@unex.es)

//****************************************************************************************************
// Módulo en el que se definen las constantes y tipos.
//****************************************************************************************************

#ifndef ns_utils_h
#define ns_utils_h


//**********************************************************************************
// constants definitions
//**********************************************************************************

const int OSPF_MESSAGE_CENTER_SIZE_FACTOR = 4; // times the number of nodes 
// Sizes definition
const int OSPF_HEADER_SIZE= 24; // in bytes
const int OSPF_DEFAULT_MESSAGE_SIZE = 100; // in bytes
const int OSPF_HELLO_MESSAGE_SIZE = 100; // in bytes
const int OSPF_DD_MESSAGE_SIZE = 200; // (in bytes) Description Database packet
const int OSPF_REQUEST_MESSAGE_SIZE = 100; // in bytes
const int OSPF_UPDATE_MESSAGE_SIZE = 100; // in bytes
const int OSPF_ACK_MESSAGE_SIZE = 20; // in bytes
const int LINKSTATE_HEADER_SIZE = 20; // in bytes
const unsigned int OSPF_WRAPAROUND_THRESHOLD = 2147483646; // 2^31-2

const int OSPF_MESSAGE_TYPES = 6;

//Network mask definition
const int NETWORKA_MASK=((u_int32_t) 0xff000000);
const int NETWORKB_MASK=((u_int32_t) 0xffff0000);
const int NETWORKC_MASK=((u_int32_t) 0xffffff00);
const int NETWORK_MASK_DEFAULT=((u_int32_t) 0x00000000);

// Timers definition
const int MAX_AGE= 3600; // in seconds 3600
const int LS_REFRESH_TIME= 1800; // in seconds 1800 
const int MAX_AGE_DIFF= 900; // 900 in seconds
const double RXMT_INTERVAL= 0.002; // in seconds
const int OSPF_TIMEOUT_FACTOR = 3;   // x times of one-way total delay


//**********************************************************************************
// OSPF packets definition
//**********************************************************************************

enum Ospf_message_type_t { 
	OSPF_MSG_INVALID=0, 
	OSPF_MSG_HELLO=1, 	// Hello message
	OSPF_MSG_DD=2,		// Description DataBase message	
	OSPF_MSG_REQUEST=3,	// Request message
	OSPF_MSG_UPDATE=4,	// Update message
	OSPF_MSG_ACK=5,		// Update's ACK
};

//**********************************************************************************
// LS type definition
//**********************************************************************************

enum LS_type_t {
	LS_INVALID = 0,
	LS_ROUTER = 1, //router links
	LS_NETWORK = 2, // network links
	LS_SUMMARY = 3, // summary links
	LS_EXTERNAL = 4 // external links
};


//**********************************************************************************
// OPTIONS field definition
//**********************************************************************************
struct options_t {
	char bit_T;
	char bit_E;
	options_t(): bit_T(0),bit_E(0) {}
	options_t(char bitT, char bitE): bit_T(bitT), bit_E(bitE){}
};


//**********************************************************************************
// Router Link Types definition
//**********************************************************************************

enum RouterLink_type_t {

	routerLink_type_default=0,
	point_to_point=1,
	connection_to_transit_net=2,
	connection_to_stub_net=3,
	virtual_link=4
};

//**********************************************************************************
// VEB_t: representing bits V,E,B (not used in this implementation)
//**********************************************************************************
struct VEB_t{
	char bit_V;
	char bit_E;
	char bit_B;
	VEB_t():bit_V(0),bit_E(0),bit_B(0){}
	VEB_t(char bitV,char bitE,char bitB):bit_V(bitV),bit_E(bitE),bit_B(bitB){}
};

//**********************************************************************************
// retCode type definition
//**********************************************************************************

enum retCode_t {
	NO_EXIST = 0, //lsa doesn't exists in the database
	EQUALS = 1, // lsa is equal to the copy in the database
	OLDER = 2, // lsa is older than the copy in the database
	NEWER = 3 // lsa is newer than the copy in the database
	
};

//******************************************************************************
// TMMS_t: representig T,M,MS bits for OspfLinksState 
//****************************************************************************** 
struct IMMS_t {
	char bit_I; //Initialize bit
	char bit_M; // More bit
	char bit_MS; // Master Slave bit
	IMMS_t():bit_I(0),bit_M(0),bit_MS(0){}
	IMMS_t(char bitI,char bitM,char bitMS):bit_I(bitI),bit_M(bitM),bit_MS(bitMS){}
};



//**********************************************************************************
//  StateN type definition: representing neighbour's states
//**********************************************************************************

enum StateN_type_t {
	DOWN= 0,
	ATTEMPT = 1, 
	INIT = 2,
	TWO_WAY = 3, 
	EX_START = 4,
	EXCHANGE = 5,
	LOADING = 6,
	FULL = 7
};


#endif
