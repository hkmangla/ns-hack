/*
 * ospf.h
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
 Módulo en el que se crean las estructuras y procedimientos necesarios para la 
 simulación del  protocolo OSPF. Contiene: definiciones de las estructuras del 
 protocolo: paquetes OSPF, registros de estado de enlace, base de datos de estado de 
 enlace, tabla de routing, manejador de ack's y 
 retransmisiones, manejador de paquetes hello, definición de estructuras de almacenamiento 
 de mensajes. Muchas de estas estructuras ya existían en el modulo linkstate, y han 
 sido redefinidas para adecuarlas a este protocolo de estado de enlace especifico.
************************************************************************************/

#ifndef ns_ospf_h
#define ns_ospf_h

#include "linkstate/ls.h"
#include "hdr-ospf.h"
#include "utils.h"

//**********************************************************************************
// LinkStateHeader: representing the link state advertisement header
//**********************************************************************************

struct LinkStateHeader {

// public data
	double 	LSage_;	//link state advertisement age
	options_t options_; 
	LS_type_t	LS_type_; //advertisement type
	u_int32_t	LS_ID_; // advertisement identification
	u_int32_t	advertising_router_; // OSPF advertising router identification; 
	double	        LS_sequence_num_;   
	u_int16_t	length_;  

//public methods
	LinkStateHeader() : LSage_(LS_BIG_NUMBER), options_(options_t(0,0)), LS_type_(LS_INVALID),
	LS_ID_(LS_INVALID_NODE_ID), advertising_router_(LS_INVALID_NODE_ID),LS_sequence_num_(0),length_(0) {}

	LinkStateHeader(double lsage, options_t op, LS_type_t ls_type, u_int32_t ls_id, u_int32_t 		adv_router,double seq_num, u_int16_t len): LSage_(lsage), options_(op),LS_type_(ls_type), LS_ID_(ls_id), advertising_router_(adv_router),
	LS_sequence_num_(seq_num),length_(len) {}

	void init (double lsage, options_t op, LS_type_t ls_type, u_int32_t ls_id, u_int32_t adv_router,double seq_num, u_int16_t len){

	  LSage_= lsage;
	  options_= op;
 	  LS_type_= ls_type;
	  LS_ID_= ls_id;
	  advertising_router_= adv_router;
	  LS_sequence_num_= seq_num;
          length_= len;

	}

};

//**********************************************************************************
// MTLink: representing MultiTopology id and metric attached to a link
//**********************************************************************************

struct MTLink {

//public data
	int	mtId_; // MultiTopology id
	int 	metric_; // metric used for routing

//public methods
	MTLink (): mtId_(0), metric_(0) {}
	MTLink (int MtId, int m): mtId_(MtId), metric_(m) {}
	void init (int MtId, int m){
	mtId_= MtId;
	metric_= m;
	}
};

//**********************************************************************************
// MTLinkList: List of MTLink attached to a link
//**********************************************************************************

typedef LsList<MTLink> MTLinkList;


//**********************************************************************************
// RouterLinkState: representing a router link state advertisement
//**********************************************************************************

struct RouterLinkState {

//public data
	ls_status_t	state_; // UP or DOWN
	
	u_int32_t	Link_ID_; // neighbour IP address
	u_int32_t 	Link_data_; // depend on the link's type field
	RouterLink_type_t Type_; //description of the router link
	int 	Num_MT_; //number of different MT id's .
	int 	MT0_metric_; //cost of using this router link for MT 0
	MTLinkList MTLinkList_; //MT id's and metrics attached to the link

//public methods
	RouterLinkState (): state_(LS_STATUS_UP), Link_ID_(LS_INVALID_NODE_ID), Link_data_(LS_INVALID_NODE_ID), Type_(routerLink_type_default), Num_MT_(0), MT0_metric_(0) {}

	RouterLinkState (ls_status_t st,u_int32_t link_id, u_int32_t link_data, RouterLink_type_t type, int nmt, int  mt0_m): state_(st),Link_ID_(link_id), Link_data_(link_data), Type_(type), Num_MT_(nmt), MT0_metric_(mt0_m) {}
	
	void init (ls_status_t st, u_int32_t link_id, u_int32_t link_data, RouterLink_type_t type, int nmt, int mt0_m){
	state_=st;
	Link_ID_=link_id;
	Link_data_=link_data;
	Type_=type;
	Num_MT_=nmt;
	MT0_metric_=mt0_m;
	}	
};


//**********************************************************************************
// RouterLinkStateList: representing the router links state
//**********************************************************************************

typedef LsList<RouterLinkState> RouterLinkStateList;

//****************************************************************************
// RouterLinksState: representing a router links state advertisment 
//****************************************************************************

struct RouterLinksState {
	
//public data
	VEB_t VEB_; // V=virtual, E=external, B=border 
	int links_; //number of router links described by this advertisment
	RouterLinkStateList RouterLinkStateList_;

//public methods
	RouterLinksState ():VEB_(VEB_t(0,0,0)),links_(0){}
	RouterLinksState (VEB_t veb, int links): VEB_(veb),links_(links){}
	void init (VEB_t veb, int links){
        VEB_=veb;
	links_=links;
        }

};

//**********************************************************************************
// NetworkLinksState: representing a network links state advertisment
//**********************************************************************************

struct NetworkLinksState {

//public data
	u_int32_t network_mask_; 
	LsNodeIdList attachedRouterList_; 

//public methods
	NetworkLinksState (): network_mask_(0){}
	NetworkLinksState (u_int32_t network_mask): network_mask_(network_mask){}

	void init (u_int32_t network_mask) {
	//ls_hdr_.LS_type_=LS_NETWORK;
	network_mask_=network_mask;
	}
};


//**********************************************************************************
// SummaryLinkState: representing a summary link state advertisment
//**********************************************************************************

struct SummaryLinkState{

//public data

	u_int32_t network_mask_; 
	MTLinkList MTLinkList_;//MT id's and metrics attached to the link

//public methods
	SummaryLinkState (): network_mask_(0){}
	SummaryLinkState (u_int32_t network_mask): network_mask_(network_mask){}

	void init (u_int32_t network_mask) {
	network_mask_=network_mask;
	}

};

//**********************************************************************************
// OspfLinkState
//**********************************************************************************

struct OspfLinkState {
//public data
LinkStateHeader ls_hdr_; // link state header

	OspfLinkState():ls_hdr_(){}

 union {
	RouterLinkStateList* RouterLinkStateListPtr_;
	SummaryLinkState* SummaryLinkState_;
	NetworkLinksState* NetworkLinksState_;
  	};
};

//**********************************************************************************
// OspfLinkStateList: representing a ospf link state list
//**********************************************************************************

typedef LsList<OspfLinkState> OspfLinkStateList;

//**********************************************************************************
// OspfTopoMap: the Link State Database, the representation of the
//  topology within the protocol 
//**********************************************************************************

typedef LsMap<int, OspfLinkStateList> OspfLinkStateMap;

class OspfTopoMap : public OspfLinkStateMap{
public:

//public methods
	
	//constructor
	OspfTopoMap() : OspfLinkStateMap() {}
  
	// map operation
	iterator begin() { return OspfLinkStateMap::begin();}
	iterator end() { return OspfLinkStateMap::end();}
	
	
	// insert the LSA
	OspfLinkStateList* insertLinkState (int nodeId, OspfLinkState& link_state);
	
	// update the database and returns true if there's change
	bool update(int nodeId, const OspfLinkStateList& linkStateAdvList);

	//get the content of THE link state database
	OspfLinkStateList getLSList ();

	//delete the LSA
	void delLSA (int nodeId, LS_type_t ls_type, u_int32_t ls_id, u_int32_t advertising_router);

	
	//get the link state advertisment indexed by the parameters
	OspfLinkState getLSA (LS_type_t ls_type, u_int32_t ls_id, u_int32_t advertising_router);
	
	// return NO_EXIST if the LSA does not exist, EQUALS if the two instances are 
	// equal, OLDER if the LSA received is older than the database copy, and NEWER if 
	// it is newer 
	retCode_t lookupLSA (LinkStateHeader hdr);
	
	// reloaded funtion: called by OspfRouting when receive a request packet
	bool lookupLSA (LS_type_t ls_type, u_int32_t ls_id, u_int32_t advertising_router); 
	
	//increments LSA's LSage field for nodeId if possible 
	void incLSage (int nodeId);

	//returns true if it is necessary to calculate the routing table and false in other case
	bool updateRoutingTable (int nodeId, OspfLinkState& linkStateAdv);
	
	//   friend ostream & operator << ( ostream & os, LsTopoMap & x) ;
	void setNodeId(int id) { myNodeId_ = id ;}

private:
//private data
	int myNodeId_; // for update()
};


typedef OspfTopoMap OspfTopology;
typedef OspfTopoMap* OspfTopoMapPtr;

//**********************************************************************************
// OspfPath: representing ospf path data
//**********************************************************************************

struct OspfPath {

	int desId_; // destination id
	int cost_; //path cost
	int nextHop_; // next hop in the path
	int mtId_; // multiTopology id

	// constructors
	OspfPath() : desId_ (LS_INVALID_NODE_ID) {}
	OspfPath(int dest, int c, int nh, int Mtid)
		: desId_ (dest), cost_(c), nextHop_(nh),mtId_(Mtid) {}
	
	bool isValid() { 
		return ((desId_ != LS_INVALID_NODE_ID) && 
			(cost_ != LS_INVALID_COST) && 
			(nextHop_ != LS_INVALID_COST) &&
			(mtId_!= LS_INVALID_COST));
	}

};


//**********************************************************************************
// OspfEqualPaths: A struct with the cost and a list of multiple next hops used by 
// OspfPaths. 
//**********************************************************************************

struct OspfEqualPaths {
public:
	int  cost; //path cost
	LsNodeIdList nextHopList; // next hop list

	// constructors
	OspfEqualPaths() : cost(LS_INVALID_COST) {}
	OspfEqualPaths(int c, int nh) : cost(c), nextHopList() {
		nextHopList.push_back(nh);
	}
	OspfEqualPaths(const OspfPath & path) : cost (path.cost_), nextHopList() {
		nextHopList.push_back(path.nextHop_);
	}
	OspfEqualPaths(int c, const LsNodeIdList & nhList) 
		: cost(c), nextHopList(nhList) {}
  
	OspfEqualPaths& operator = (const OspfEqualPaths & x ) {
		cost = x.cost;
		nextHopList = x.nextHopList;
		return *this;
	}

	// copy 
	OspfEqualPaths& copy(const OspfEqualPaths & x) { return operator = (x) ;}

	// appendNextHopList 
	int appendNextHopList(const LsNodeIdList & nhl) {
		return nextHopList.appendUnique (nhl);
	}
};

//**********************************************************************************
// OspfMTRouterPathsMap: store paths for each mt id.
//**********************************************************************************

typedef LsMap<int,OspfEqualPaths > OspfMTRouterPathsMap;

//**********************************************************************************
// OspfMTPathsMap: store paths for each node and for each mt id
//**********************************************************************************

typedef LsMap<int,OspfMTRouterPathsMap> OspfMTPathsMap;


//**********************************************************************************
//  OspfPaths -- enhanced OspfMTPathsMap, used in OspfRouting
//**********************************************************************************
class OspfPaths : public OspfMTPathsMap {
public:
	//publid methods
	OspfPaths(): OspfMTPathsMap() {}
  
	// -- map operations 
	iterator begin() { return OspfMTPathsMap::begin();}
	iterator end() { return OspfMTPathsMap::end();}

	// -- specical methods that make easier computeRoutes of OspfRouting
	
	// lookup the path's cost for the destId and Mtid
	int lookupCost(int destId, int Mtid) {
		OspfMTRouterPathsMap * pEPM = findPtr (destId);
	if (pEPM == NULL ) return LS_MAX_COST + 1;
	else {	
		OspfEqualPaths * pEP = pEPM->findPtr(Mtid);
	
	if ( pEP == NULL ) return LS_MAX_COST + 1;
		 else
		return pEP->cost;
	     }		
	}

	// lookup next hop list for the destId and Mtid
	LsNodeIdList* lookupNextHopListPtr(int destId,int Mtid) {
		OspfMTRouterPathsMap * pEPM = findPtr (destId);
		if (pEPM == NULL) 
			return (LsNodeIdList *) NULL;
		else {
		OspfEqualPaths * pEP = pEPM->findPtr(Mtid);
		if (pEP == NULL ) 
			return (LsNodeIdList *) NULL;
		 else
		return &(pEP->nextHopList);
		}
	}
  
	// insert Path without checking validity
	iterator insertPathNoChecking(int destId, int Mtid, int cost, int nextHop) {
		
		OspfMTRouterPathsMap * pEPM = findPtr (destId);
		iterator itr;
		if (pEPM == NULL) {

		  OspfEqualPaths ep(cost, nextHop);
 		  OspfMTRouterPathsMap  pEPM;
		  itr = insert(destId,pEPM);
		  (& (* itr).second)->insert(Mtid,ep);
		  itr=find(destId);
		  return itr; // for clarity
		  }
		
		//else
		OspfEqualPaths * pEP = pEPM->findPtr(Mtid);
		if (pEP == NULL ) {
		  OspfEqualPaths ep(cost, nextHop);
		  pEPM->insert(Mtid,ep);
		  itr=find(destId);
		  return itr;	  
		  }
		
 		return itr;
	}     

	// insert Path without checking validity
	iterator insertPathNoChecking (const OspfPath & path) {
		return insertPathNoChecking(path.desId_, path.mtId_,path.cost_, 
					    path.nextHop_);
	}

	// insert Path  checking validity. Returns end() if error, else returns iterator
	iterator insertPath(int destId, int mtid, int cost, int nextHop);
	// insert Path  checking validity. Returns end() if error, else returns iterator
	iterator insertPath(const OspfPath& path) {
		return insertPath(path.desId_, path.mtId_,path.cost_, path.nextHop_);
	}
	
	// insert next hop list for destId and mtid
	iterator insertNextHopList(int destId, int cost, int mtid,
				   const LsNodeIdList& nextHopList);
};

//**********************************************************************************
//  OspfPathsTentative:  Used in OspfRouting, remembers min cost and location 
//**********************************************************************************

class OspfPathsTentative : public OspfPaths {
public:
	//public methods
	OspfPathsTentative() : OspfPaths(), minCost(LS_MAX_COST+1) {
		minCostIterator = end();
	}
  
	// combining get and remove min path
	OspfPath popShortestPath(int mtid);
  
private:
	//private data
	int minCost; // remembers the min cost
	iterator minCostIterator; // remembers where it's stored
	//private methods
	// returns the iterator for the less cost path for mtid
	iterator findMinEqualPaths(int mtid); 
};

//******************************************************************************
// HelloPacket: Packets sent within the Hello protocol
//****************************************************************************** 

struct HelloPacket {
//public data

	u_int32_t networkMask_; // network mask associated with this interface
	double helloInterval_; // number of seconds between this router's Hello packets
	options_t options_; //optional captabilites supported by the router
	double deadInterval_; // number of seconds before declaring a silent router down
	LsNodeIdList* neighbourListPtr_; // Router IDs of each router from whom valid Hello packets 
				      // have been seen in the last deadInterval seconds.
//public methods

	HelloPacket (): networkMask_(0),options_(options_t(0,0)){}

	HelloPacket (u_int32_t network_mask, double hello_interval, options_t opt,double 		dead_interval):
	networkMask_(network_mask),helloInterval_(hello_interval),
	options_(opt),deadInterval_(dead_interval){}
 
	void init (u_int32_t network_mask, double hello_interval, options_t opt,double 		dead_interval){

	networkMask_=network_mask;
	helloInterval_=hello_interval;
	options_=opt;
	deadInterval_=dead_interval;
	}

};		
	

//******************************************************************************
// Link State Header List
//****************************************************************************** 
typedef LsList<LinkStateHeader> LinkStateHeaderList;


//******************************************************************************
// Database Description Packet: Packet sent within exchange protocol
//****************************************************************************** 

struct DDPacket {
//public data

	options_t options_; //optional captabilites supported by the router
	IMMS_t IMMSbits_; // I=initialize M=more MS=master/slave
	double DDSeqNumber_; // sequence number
	LinkStateHeaderList* lsHeaderListPtr_; // link state advertisment header list

//public methods
	 
	DDPacket():options_(options_t(0,0)),IMMSbits_(IMMS_t(0,0,0)),DDSeqNumber_(LS_INVALID_NODE_ID){}

	DDPacket (options_t opt,IMMS_t imms_bits, double seq_num):options_(opt),IMMSbits_(imms_bits),DDSeqNumber_(seq_num){}
 
	void init (options_t opt, IMMS_t imms_bits, double seq_num){

	options_=opt;
	IMMSbits_=imms_bits;
	DDSeqNumber_=seq_num;
	}

};		

//******************************************************************************
// Link State Record Identifier
//****************************************************************************** 

struct LinkStateRecordId {
	
	LS_type_t type_; // record type
	u_int32_t lsID_; //link state record identifier
	u_int32_t advertisingRouter_; //advertising router identifier
LinkStateRecordId (LS_type_t type,u_int32_t lsId, u_int32_t advRouter):type_(type),lsID_(lsId),advertisingRouter_(advRouter){}
 
};


//******************************************************************************
// Link State Record Identifier List
//****************************************************************************** 

typedef LsList<LinkStateRecordId> LinkStateRecordIdList;
 
//******************************************************************************
// Request Packet: Packet sent within exchange protocol
//****************************************************************************** 

struct RequestPacket {
//public data
	LinkStateRecordIdList* linkStateRecordIdListPtr_;
	
//public methods
	 
	void init (void){
	}
};		

//******************************************************************************
// Update Packet: Packet sent within exchange and flooding protocol
//****************************************************************************** 

struct UpdatePacket {
//public data

	int numberAdvert_; //The number of link state advertisements 
	OspfLinkStateList* LsListAdvertPtr_; // link state advertisment list
	
//public methods
	 
	UpdatePacket(): numberAdvert_(0), LsListAdvertPtr_(NULL){}
	UpdatePacket(int num_advert): numberAdvert_(num_advert){}
	void init (int num_advert){
	numberAdvert_=num_advert;	
	}

};		

//******************************************************************************
// Ack Packet: Packet sent to ack update packets
//****************************************************************************** 


struct AckPacket {
//public data

	LinkStateHeaderList* lsHeaderListPtr_; // link state advertisment header list
	
//public methods
	 void init (void){
	}

};		



//******************************************************************************
// OspfMessage : store the message's information 
//****************************************************************************** 

struct OspfMessage {

//public data

u_int32_t messageId_; // ospf message id 
int originNodeId_; // originator node id: used in acknowledging

	OspfMessage() : messageId_(LS_INVALID_NODE_ID)	{}

	union {
		HelloPacket* HelloPacketPtr_;
		DDPacket* DDPacketPtr_;
                UpdatePacket* UpdatePacketPtr_; 
                RequestPacket* RequestPacketPtr_;
		AckPacket* AckPacketPtr_;	
	};

};

//******************************************************************************
// OspfMessageCenter:  Global storage of Message's for retrieval
//****************************************************************************** 

class OspfMessageCenter {
public:
	typedef LsMap <u_int32_t, OspfMessage> baseMap;
	// constructor
	OspfMessageCenter () 
		: current_hello_id(LS_INVALID_MESSAGE_ID), 
		current_dd_id(LS_INVALID_MESSAGE_ID),
		current_request_id(LS_INVALID_MESSAGE_ID),
		current_update_id(LS_INVALID_MESSAGE_ID),
		current_ack_id(LS_INVALID_MESSAGE_ID) {}
  	
	// set the max size of the structure taking into account the number of nodes of the topology
	void setNodeNumber (int number_of_nodes) {
		max_size = number_of_nodes * LS_MESSAGE_CENTER_SIZE_FACTOR;
	}

	// create and returns a new message
	OspfMessage* newMessage (int senderNodeId, Ospf_message_type_t type);
	
	u_int32_t duplicateMessage( u_int32_t msgId) {
		return msgId;
	}

	u_int32_t duplicateMessage(const OspfMessage& msg) {
		return duplicateMessage(msg.messageId_);
	}

	// deletes the message and returns true if the deletion is done and false in other case
	bool deleteMessage(u_int32_t msgId, Ospf_message_type_t type);

	//Returns the ospf message taking into account the type and the id of the 
	//message 
	OspfMessage* retrieveMessagePtr(u_int32_t msgId, Ospf_message_type_t type);
	
	static OspfMessageCenter& instance() { 
		return msgctr_;
	}

private:
	static OspfMessageCenter msgctr_;	// Singleton class
	// current number id used for the differents packets.
	u_int32_t current_hello_id;
	u_int32_t current_dd_id ;
	u_int32_t current_request_id;
	u_int32_t current_update_id;
	u_int32_t current_ack_id;

	unsigned int max_size; // if size() greater than this number, erase begin().
	typedef LsMap <u_int32_t, OspfMessage> message_storage;
	// storage used for the differents packets
	message_storage hello_messages;
	message_storage dd_messages;
	message_storage request_messages;
	message_storage update_messages;
	message_storage ack_messages;

	void init();
};


//********************************************************************************
// OspfRetransTimer: representing the timer retrieval for dd and request packets
//********************************************************************************
 

class OspfRetransmissionManager;
class OspfRetransTimer : public TimerHandler {
public:
	OspfRetransTimer() {}
	OspfRetransTimer (OspfRetransmissionManager *amp , int nbrId)
		: ackManagerPtr_(amp), neighbourId_(nbrId) {}
	virtual void expire(Event *e);
protected:
	OspfRetransmissionManager* ackManagerPtr_;
	int neighbourId_;
};

//******************************************************************************
// OspfRetransUpdateTimer: representing the timer retrieval for updates 
//****************************************************************************** 

class OspfRetransUpdateTimer : public TimerHandler {
public:
	OspfRetransUpdateTimer() {}
	OspfRetransUpdateTimer (OspfRetransmissionManager *amp , int nbrId)
		: ackManagerPtr_(amp), neighbourId_(nbrId) {}
	virtual void expire(Event *e);
protected:
	OspfRetransmissionManager* ackManagerPtr_;
	int neighbourId_;
	
};

//******************************************************************************
// OspfIdSeq: representing the id and sequence number of a message 
//****************************************************************************** 

struct OspfIdSeq {
	u_int32_t msgId_; //message id
	double seq_; // sequence number
	OspfIdSeq():msgId_(LS_INVALID_MESSAGE_ID),seq_(LS_INVALID_MESSAGE_ID) {}
	OspfIdSeq(u_int32_t id, u_int32_t s) : msgId_(id), seq_(s) {}
};

//********************************************************************************
// OspfUnackPeer: used in ackManager to keep record a peer who still haven't ack 
// some of  its Update or DD packets
//********************************************************************************

struct OspfUnackPeer {
	double rtxTimeout_; // time out value for ackTimer
	OspfRetransTimer ackTimer_;
	OspfRetransUpdateTimer updateTimer_;
	OspfIdSeq DdSeq_;  
	u_int32_t  ReqSeq_; // Request message id
	LsMap<int, u_int32_t> UpdateMap_; //update messages
	 
	// constructor
	OspfUnackPeer() : DdSeq_(), ReqSeq_(LS_INVALID_MESSAGE_ID), UpdateMap_(){}
	OspfUnackPeer(OspfRetransmissionManager* amp, int nbrId, 
		    double timeout = RXMT_INTERVAL) : 
		rtxTimeout_(timeout), ackTimer_(amp, nbrId),updateTimer_(amp,nbrId), DdSeq_(),
		ReqSeq_(LS_INVALID_MESSAGE_ID), UpdateMap_(){}
};


//******************************************************************************
//  OspfRetransmissionManager -- handles retransmission,acknowledgement  
//******************************************************************************
class OspfRouting;
class OspfRetransmissionManager : public LsMap<int, OspfUnackPeer> {
public:
	OspfRetransmissionManager(OspfRouting& lsr) : OspfRouting_(lsr) {} 

	void initTimeout(LsDelayMap* delayMapPtr);
	void cancelTimer(int neighbourId);
	
	// Called by OspfRouting when a message is sent out 
	int messageOut(int peerId, const OspfMessage& msg,Ospf_message_type_t type);
  
	// Called by OspfRouting when an message is received
	int messageIn(int peerId,  const OspfMessage& msg,Ospf_message_type_t type);

	// Called by retransmit timer
	int resendMessages(int peerId);
	

private:
	// data
	OspfRouting& OspfRouting_;
};

inline void OspfRetransTimer::expire(Event *e) 
{ 
	ackManagerPtr_->resendMessages(neighbourId_); 
}


inline void OspfRetransUpdateTimer::expire(Event *e)
{
	ackManagerPtr_->resendMessages(neighbourId_);
}

//******************************************************************************
// OspfInactivityTimer: representing the inactivity timer used to detect
// routers down
//****************************************************************************** 

class OspfInactivityManager;
class OspfInactivityTimer : public TimerHandler {
public:
	OspfInactivityTimer() {}
	OspfInactivityTimer (OspfInactivityManager *amp , int nbrId)
		: inactivityManagerPtr_(amp), neighbourId_(nbrId) {}
	virtual void expire(Event *e);
protected:
	OspfInactivityManager* inactivityManagerPtr_;
	int neighbourId_;
};


//******************************************************************************
// OspfHelloPeer: used in inactivityManager to keep record a peer timer
//******************************************************************************

struct OspfHelloPeer {
	double routerDeadInterval_; // time out value for inactivity timer
	OspfInactivityTimer inactivityTimer_;
	
	 

	// constructor
	OspfHelloPeer() {}
	OspfHelloPeer(OspfInactivityManager* amp, int nbrId, 
		    double timeout = RXMT_INTERVAL) : 
		routerDeadInterval_(timeout), inactivityTimer_(amp, nbrId){}
};


//******************************************************************************
//  OspfInactivityManager: handles inactivity timer  
//******************************************************************************

class OspfInactivityManager : public LsMap<int, OspfHelloPeer> {
public:
	OspfInactivityManager(OspfRouting& lsr) : OspfRouting_(lsr) {} 

	void initTimeout(LsNodeIdList* peerIdListPtr,double router_dead_interval);
	void cancelTimer(int neighbourId);
	
	// Called by OspfRouting when a message is sent out 
	int messageOut(int peerId, Ospf_message_type_t type);
  
	// Called by OspfRouting when an message is received
	int messageIn(int peerId, Ospf_message_type_t type);

	// Called by inactivity timer
	int setNeighbourDown(int peerId);
	

private:
	// data
	OspfRouting& OspfRouting_;
};

inline void OspfInactivityTimer::expire(Event *e) 
{ 
	inactivityManagerPtr_->setNeighbourDown(neighbourId_); 
}


//******************************************************************************
// LsAgingTimer: used to increment the LSAs LSage when they are store in the
// database
//****************************************************************************** 

class LsAgingManager;
class LsAgingTimer : public TimerHandler {
public:
	LsAgingTimer() {}
	LsAgingTimer (LsAgingManager *amp , int nbrId)
		: LsAgingManagerPtr_(amp), neighbourId_(nbrId) {}
	virtual void expire(Event *e);
protected:
	LsAgingManager* LsAgingManagerPtr_;
	int neighbourId_;
};


//******************************************************************************
// LSAPeer: used in LsAgingManager to keep record a peer LSA timer 
//******************************************************************************

struct LSAPeer {
	double LsAgingInterval_; // 1 second
	LsAgingTimer LsTimer_;
	 

	// constructor
	LSAPeer () {}
	LSAPeer (LsAgingManager* amp, int nbrId, 
		    double timeout = 1) : 
		LsAgingInterval_(timeout), LsTimer_(amp, nbrId){}
};


//******************************************************************************
//  LsAgingManager: handles LsAging timer  
//******************************************************************************

class LsAgingManager : public LsMap<int, LSAPeer> {
public:
	LsAgingManager (OspfRouting& lsr) : OspfRouting_(lsr) {} 

	void initTimeout (int nodeId);
	void cancelTimer(int neighbourId);
	
	// Called by OspfRouting when a LSA is created or inserted in the database 
	int createLSA (int peerId);
  
	// Called by LsAging timer
	int ageLSA(int peerId);
	

private:
	// data
	OspfRouting& OspfRouting_;
};

inline void LsAgingTimer::expire(Event *e) 
{ 
	LsAgingManagerPtr_->ageLSA (neighbourId_); 
}


//**********************************************************************************
// OspfNeighbour: representing a neighbour data structure 
//**********************************************************************************

struct OspfNeighbour {

//public data
StateN_type_t state_; 
char masterSlave_; // 1-->I'm the master  0--> I'm the slave 
double  ddSeq_;
int neighbourId_; // neighbour OSPF Router ID 
u_int32_t neighbourIpAddr_; // neighbour interface IP address
options_t options_; // neighbour optional captabilites
LinkStateHeaderList lsHeaderList_; //link states to send in DD packets to the neighbour
OspfLinkStateList lsRetransmissionList_; //link states to retransmit to the neighbour
LinkStateRecordIdList lsRequestList_; // link states to request to the neighbour

//public methods
 OspfNeighbour (): state_(DOWN), masterSlave_(0),ddSeq_(LS_INVALID_MESSAGE_ID),neighbourId_(LS_INVALID_NODE_ID),options_
	(options_t(0,0)),lsHeaderList_(),lsRetransmissionList_(),lsRequestList_(){}

 OspfNeighbour (StateN_type_t state,char MS, double ddseq, int nid, options_t op):state_(state),masterSlave_(MS),ddSeq_(ddseq),neighbourId_(nid),options_(op){} 	
	

};


//**********************************************************************************
// OspfNeighbourMap: representing neighbouring data structures 
//**********************************************************************************


class OspfNeighbourMap : public LsMap<int,OspfNeighbour> {
public:
	//public methods
	OspfNeighbourMap (){}
	//init Neighbour structure
	void initNeighbour (LsNodeIdList* peerIdListPtr);
	// check the state of neighbour nbId
	bool isState (int nbId,StateN_type_t state);
	// set the state of neighbour nbId to newState
	void setState (int nbId, StateN_type_t newState);
	// get the state of neighbour nbId
	StateN_type_t getState (int nbId);
	// set the OSPF router id to nbId
	void setNeighbourId (int nbId);
	// get the OSPF router id of nbId
	int getNeighbourId (int nbId);

	// set the options of neighbour nbId to options_t
	void setOptions(options_t op,int nbId);
	// get the options of neighbour nbId
	options_t getOptions(int nbId);
	
	// set the DD sequence number of neighbour nbId to seq
	void setDDseq (int nbId, double seq);	
	// increment the DD sequence number of neighbour nbId
	void incDDseq (int nbId);
	// get the DD sequence number of neighbour nbId
	double getDDseq (int nbId);
	
	// set the bit masterSlave_ of neighbour nbId to master
	void setMaster (int nbId);
	// set the bit masterSlave_ of neighbour nbId to slave
	void setSlave (int nbId);
	// get the bit masterSlave of neighbour nbId
	char getMasterSlave (int nbId);

	// set linkStateHeaderList of neighbour nbId to lshdrl
	void setLinkStateHeaderList (int nbId,LinkStateHeaderList& lshdrl);
	// get the linkStateHeaderList of neighbour nbId
	LinkStateHeaderList getLinkStateHeaderList (int nbId);

	// set linkStateRetransList of neighbour nbId to lsl
	void setLinkStateRetransList (int nbId,OspfLinkStateList& lsl);
	// get linkStateRetransList of neighbour nbId
	OspfLinkStateList getLinkStateRetransList (int nbId);	
	// lookup the LSA in the linkStateRetransList of the neighbour nbId 
	// return true if exists and false in other case	
	bool lookupLSARetrans (int nbId,LinkStateHeader& lshdr);
	// delete the LSA in the linkStateRetransList of the neighbour nbId
	bool delLSARetrans (int nbId, LinkStateHeader& lshdr);
	// add the LSA ls to the linkStateRetransList of the neighbour nbId
	void addLSARetrans (int nbId, OspfLinkState& ls);

	// set the RequestList of neighbour nbId to lsr
	void setRequestList (int nbId, LinkStateRecordIdList& lsr);
	// get the RequestList of neighbour nbId
	LinkStateRecordIdList getRequestList (int nbId);
	// del the LSA in the RequestList of neighbour nbId
	void delLSARequest(int nbId, LinkStateHeader& lshdr);
	// lookup the LSA in the RequestList of the neighbour nbId 
	// return true if exists and false in other case
	bool lookupLSAReq (int nbId, LinkStateHeader& lshdr);
	
	// delete all list of neighbour nbId 
	void delAllLists (int nbId); 
};

//**********************************************************************************
//   OspfNode:  represents the node enviroment interface 
//   It serves as the interface between the Routing class and the actual 
//   simulation enviroment 
// rtProtoOSPF will derive from OspfNode as well as Agent
//**********************************************************************************

class OspfNode {
public:
	//constructor
        virtual ~OspfNode () {}
	
	virtual bool sendMessage(int destId, hdr_Ospf hdr) = 0;
	virtual void receiveMessage(int sender, u_int32_t msgId,Ospf_message_type_t type) = 0;
	virtual int getNodeId() = 0;
	virtual RouterLinkStateList* getLinkStateListPtr()=0;
	virtual LsNodeIdList* getPeerIdListPtr() = 0;
	virtual LsDelayMap* getDelayMapPtr() = 0;
	virtual int getNumMtIds() = 0;
	virtual double getHelloInterval() = 0;
	virtual double getRouterDeadInterval() = 0;
	virtual void routeChanged() = 0;
	virtual void intfChanged()= 0;

	
};

//******************************************************************************
//  OspfRouting: The implementation of the Ospf protocol
//******************************************************************************

class OspfRouting {
public:
	static int msgSizes[ OSPF_MESSAGE_TYPES ];
	friend class OspfRetransmissionManager;
	friend class OspfInactivityManager;
	friend class LsAgingManager;

	// constructor 
	OspfRouting() : myNodePtr_(NULL),  myNodeId_(LS_INVALID_NODE_ID),
		helloInterval_(LS_INVALID_NODE_ID), routerDeadInterval_(LS_INVALID_NODE_ID), 
		peerIdListPtr_(NULL), neighbourIdList(), adyacentsIdList(),
		linkStateListAdvPtr_(NULL),linkStateListPtr_(NULL),messageCenterPtr_(NULL),
		routingTablePtr_(NULL),	linkStateDatabase_(), AckManager_(*this),neighbourData_(), 			InactivityManager_(*this),AgingManager_(*this),
		delayMapPtr_(NULL){}
	
	// distructor
	~OspfRouting() {
		if (routingTablePtr_ != NULL)
			delete routingTablePtr_;
	}
	// initialize the structure
	bool init(OspfNode* nodePtr);
	// compute the routing table
	void computeRoutes() {
	        if (routingTablePtr_ != NULL)
	                delete routingTablePtr_;
	        routingTablePtr_ = _computeRoutes();
	}
	// returns the path for destId and Mtid
	OspfEqualPaths* lookup(int destId,int Mtid) {		
		if (routingTablePtr_!= NULL){		
		OspfMTRouterPathsMap* M;
		M=routingTablePtr_->findPtr(destId);
		  if (M!=NULL) {		   
		    OspfEqualPaths* P;
		    P = M->findPtr(Mtid);
		    return P;
		  }
		}
	  return (OspfEqualPaths *)NULL;
	        
	}
	// send hellos packets to all active interfaces
	bool sendHellos();
	// called by node when one of the link's cost has changed
	void linkStateChanged();
	// called by node when one of the interfaces' state has changed
	void interfaceChanged ();
	// called by node when messages arrive
	bool receiveMessage(int senderId , u_int32_t msgId,Ospf_message_type_t type);
	//increments LSA Lsage field for peer peerId
	int ageLSA (int peerId);
	// returns node id
	int getnodeid();
	

private:
	// private data
	OspfNode * myNodePtr_; // where I am residing in
	int myNodeId_; // who am I
	double helloInterval_;  
	double routerDeadInterval_;
	LsNodeIdList* peerIdListPtr_; // my ospf peers used to send hello messages
	LsNodeIdList neighbourIdList; //my neighbours seen in hello protocol
	LsNodeIdList adyacentsIdList; //my adyacents neighbour's (2-way conectivity)
	OspfLinkStateList* linkStateListAdvPtr_; // My link state advertisment 
	RouterLinkStateList* linkStateListPtr_; // My Router links
	OspfMessageCenter* messageCenterPtr_; // points to static messageCenter
	OspfPaths* routingTablePtr_; // the routing table
	OspfTopoMap linkStateDatabase_; // database topology
	OspfRetransmissionManager AckManager_; // RxmTimeout handler 
	OspfNeighbourMap neighbourData_; // my neighbours' data structure
	OspfInactivityManager InactivityManager_; // RouterDeadInterval handler
	LsAgingManager AgingManager_; // LSA aging timer handler
	LsDelayMap* delayMapPtr_; //my neighbours delays


private:
	// private methods
	// get OspfMessageCenter instance
	OspfMessageCenter& msgctr() { return OspfMessageCenter::instance(); }
	// compute routing table
	OspfPaths* _computeRoutes();
	// get the hello packet 
	HelloPacket getHelloPkt();
	// get the Ospf Header
	hdr_Ospf getOspfHeader ( Ospf_message_type_t type,int msgId);
	// get the packet length taking into account the type
	int getLength (Ospf_message_type_t type); 
	// get the Dd packet for the neighbour nbId
	DDPacket getDdPkt(int nbId);
	// get the Request packet for the neighbour nbId
	RequestPacket getRequestPkt (int nbId);

	// add neighbour nbId to the neighbourIdList
	void addNeighbourToList (int nbId);
	// del neighbour nbId from the neighbourIdList
	void delNeighbourFromList (int nbId);
	// add neighbour nbId to the adyacentsIdList
	void addAdyacentToList (int nbId);
	// del neighbour nbId from adyacentsIdList 
	void delAdyacentFromList (int nbId);

	// check if there is two way conectivity between the neighbour
	// from where the message is received and me
	bool isTwoWayConectivity (OspfMessage* msgPtr);
	// check if the roles master and slave have been distributed in a correct way
	bool rolesDistributed(int nbId,OspfMessage* msgPtr);		
	// check if I am the master
	bool isSlave(int nbId, OspfMessage* msgPtr);
	// check if I am the slave
	bool isMaster(int nbId, OspfMessage* msgPtr);
	// check if there has been a sequence number mismatch
	bool isSeqNumberMismatch (int nbId, OspfMessage* msgPtr);
	// check if the packet received is duplicated
	bool isPacketDuplicated (int nbId,OspfMessage* msgPtr);
	// treat the packet duplicated
	bool managePacketDuplicated(int nbId, OspfMessage* msgPtr);
	// check if the packet received is the next in the sequence
	bool isNextPacketSeq(int neighbourId,OspfMessage* msgPtr);
	// process the packet received
	bool processPacket(int neighbourId, OspfMessage* msgPtr);
	// check if the type is valid
	bool isLsTypeValid (LS_type_t type);
	// convets an update packet into an ack packet. Used for acknowledging.
	AckPacket UpdateToAck (UpdatePacket& updpkt);
	// change the Router LSA's state to st
	void changeRouterLSAState (int adv_router, int link_id, ls_status_t st);
	// send the LSA to all adyacents nodes
	void sendLSAToAdyacents(LS_type_t type, int adv_router, int link_id, int no_dest_node);
		
	// receive ospf messages
	bool receiveHello (int neighbourId, OspfMessage* msgPrt);
	bool receiveDD (int neighbourId, OspfMessage* msgPtr);
	bool receiveUpdate (int neighbourId, OspfMessage* msgPtr);
	bool receiveRequest (int neighbourId, OspfMessage* msgPtr);
	bool receiveAck (int neighbourId, OspfMessage* msgPtr);
	

	// send ospf messages
	void sendDD (int neighbourId);
	void sendRequest(int neighbourId);
	void sendUpdate(int originNodeId,int neighbourId,UpdatePacket& updpkt);
	void sendAck (int nbrId,int originNodeIdAck, LinkStateHeaderList& lshdrl);
	void resendMessage (int destId, hdr_Ospf hdr) {
		myNodePtr_->sendMessage(destId,hdr);
	}
	//next step in the flooding procedure
	void sendFlooding (int senderId,OspfLinkState& linkState);

	
	//neighbour state machine events
	bool helloReceivedEvent (int nbId);
	bool twoWayReceivedEvent (int nbId);
	bool oneWayReceivedEvent (int nbId);
	void inactivityTimerEvent (int nbId);	
	bool NegDoneEvent(int nbId);	
	bool SeqNumberMismatchEvent (int nbId);
	bool ExchangeDoneEvent(int nbId);
	bool BadLSReqEvent(int nbId);
	void LoadingDoneEvent (int nbId);

};

#endif
