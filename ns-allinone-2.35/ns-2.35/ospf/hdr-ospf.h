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
// Módulo en el que se define la estructura de datos que representa la cabecera de los paquetes OSPF.
//****************************************************************************************************

#ifndef ns_ospf_hdr_h
#define ns_ospf_hdr_h

#include "config.h"
#include "packet.h"
#include "address.h"
#include "ip.h"
#include "utils.h"


//**********************************************************************************
// Common packet's header 
//**********************************************************************************

struct hdr_Ospf {
	int version_; // version number (2)
	Ospf_message_type_t type_; // OSPF packet type
	u_int16_t packet_len_; // packet length in bytes
	u_int32_t	router_id_; // router ip address
	u_int32_t	area_id_; // area identifier 	

	int msgId_; 	// message identifier (added to simplify simulation)

	static int offset_; // needed by PacketHeaderManager
	inline static int& offset() { return offset_; }
	inline static hdr_Ospf* access(const Packet* p) {
		return (hdr_Ospf*) p->access(offset_);
	}

	// Access methods
	int& version() { return (version_);}
	Ospf_message_type_t& type() { return (type_);}
	u_int16_t& packet_len() { return (packet_len_);} 
	u_int32_t& router_id() { return (router_id_);}
	u_int32_t& area_id() { return (area_id_);}
	int& msgId() {return (msgId_);}

};

#endif
