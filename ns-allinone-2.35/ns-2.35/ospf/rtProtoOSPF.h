/*
 * rtProtoOSPF.h
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

/***********************************************************************************
 Módulo en el que se define el agente para protocolo OSPF
************************************************************************************/


#ifndef ns_rtprotoospf_h
#define ns_rtprotoospf_h

#include "packet.h"
#include "agent.h"
#include "ip.h"
#include "ospf.h"
#include "hdr-ospf.h"
#include "utils.h"


extern OspfMessageCenter messageCenter;



//**********************************************************************************
// rtProtoOSPF: OSPF Agent class definition  
//**********************************************************************************

class rtProtoOSPF : public Agent , public OspfNode {
public:
	// constructor
	rtProtoOSPF();
	int command(int argc, const char*const* argv);
        void recv(Packet* p, Handler*);

	

protected:
	// protected methods
	
	// set routing_ structure
	void initialize(); 		
	// set delayMap structure	
	void setDelay(int nbrId, double delay) {
		delayMap_.insert(nbrId, delay);
	}
	// call to computeRoutes in routing_ structure
	void computeRoutes() { routing_.computeRoutes(); }
	// called by tcl ospf agent when a link cost has changed
	void intfChanged();
	// called by tcl ospf agent to look for the path to reach to destinationNodeId for mtid
	void lookup(int destinationNodeId, int mtid);
	// called by tcl ospf agent when sending hello packets
	void sendHellos() { routing_.sendHellos(); }
	// called by tcl ospf agent when a link state has changed
	void interfaceChanged ();	

public:
	// public methods
	
	bool sendMessage(int destId, hdr_Ospf hdr);
	void receiveMessage(int sender, u_int32_t msgId,Ospf_message_type_t type);
	int getNodeId() { return nodeId_; }
	RouterLinkStateList* getLinkStateListPtr()  { return &OspfLinkStateList_; }
	LsNodeIdList* getPeerIdListPtr() { return &peerIdList_; }
	LsDelayMap* getDelayMapPtr() { 
		return delayMap_.empty() ? (LsDelayMap *)NULL : &delayMap_;
	}
	int getNumMtIds() { return numMtIds_;}
	double getHelloInterval() { return helloInterval_;}
	double getRouterDeadInterval() { return routerDeadInterval_;}
	
	void installRoutes() {
		Tcl::instance().evalf("%s route-changed", name());
	}
	// called by node when is necessary to re-calculate routing table in tcl
	void routeChanged() { installRoutes();}
	

private:
	// private data
	typedef LsMap<int, ns_addr_t> PeerAddrMap; 
	// peer address map
	PeerAddrMap peerAddrMap_;
	// node id	 
	int nodeId_;
	// ospf link state
	RouterLinkStateList OspfLinkStateList_;
	// peer Id list
	LsNodeIdList peerIdList_;
	// to differentiate fake and real OSPF. Needed in recv and sendMessage; 
	int OSPF_ready_;
	// The length of time, in seconds, between the Hello Packets the router sends on the interface
	double helloInterval_; 
	// After ceasing to hear a router's Hello Packets, the number of seconds before its neighbors 	//declare the router down
	double routerDeadInterval_; 
	//store the estimated one-way total delay for each neighbor, in second
	LsDelayMap delayMap_;
	// Ospf ruting protocol instance
	OspfRouting routing_;
	//store the number of Multitopology identifiers defined
	int numMtIds_;

	// private methods
	int findPeerNodeId(ns_addr_t agentAddr);

};// ns_rtprotols_h


//**********************************************************************************
// Linkage class definition
//**********************************************************************************


static class rtProtoOSPFclass : public TclClass {
public:
	rtProtoOSPFclass() : TclClass("Agent/rtProto/OSPF") {}
	TclObject* create(int, const char*const*) {
		return (new rtProtoOSPF());
	}
} class_rtProtoOSPF;
    



#endif // ns_rtprotoospf_h
