/*
 * rtProtoOSPF.cc
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


#include "config.h"
#ifdef HAVE_STL
#include "hdr-ospf.h"
#include "rtProtoOSPF.h"
#include "agent.h"
#include "string.h" // for strtok

// Helper classes
class LsTokenList : public LsList<char *> {
public:
	LsTokenList (char * str, const char * delim ) 
		: LsList<char*> () { 
		for ( char * token = strtok (str, delim);
		      token != NULL; token = strtok(NULL, delim) ) {
			push_back (token);
		}
	}
};
   
class LsIntList : public LsList<int> {
public:
	LsIntList (const char * str, const char * delim)
		: LsList<int> () {
		for ( char * token = strtok ((char *)str, delim);
		      token != NULL; token = strtok(NULL, delim) ) {
			push_back ( atoi(token) );
		}
	}
};

/***********************************************************************************
 Builder: initialize class atributes 
************************************************************************************/

rtProtoOSPF::rtProtoOSPF(): Agent(PT_RTPROTO_OSPF) {
OSPF_ready_=0;
bind("helloInterval",&helloInterval_);
bind("routerDeadInterval",&routerDeadInterval_);

} 


 
int rtProtoOSPF::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	  char ns_[8];
 	 tcl.evalf("Simulator instance");
  	strcpy(ns_,tcl.result());


	if (strcmp(argv[1], "initialize") == 0) {
		initialize ();
		return TCL_OK;
	}
	if (strcmp(argv[1], "setDelay" ) == 0 ) {
		if ( argc == 4) {
			int nbrId = atoi(argv[2]);
			double delay = strtod ( argv[3], NULL);
			setDelay(nbrId, delay);
			return TCL_OK;
		}
	}
	if (strcmp(argv[1], "setNodeNumber" ) == 0 ) {
		if ( argc == 3 ) {
			int number_of_nodes = atoi(argv[2]);
			OspfMessageCenter::instance().setNodeNumber(number_of_nodes);
		}
		return TCL_OK;
	}

	if (strcmp(argv[1], "computeRoutes") == 0) {
		computeRoutes();
		return TCL_OK;
	}

	if (strcmp(argv[1], "intfChanged") == 0) {
		intfChanged();
		return TCL_OK;
	}

	if (strcmp(argv[1], "lookup") == 0) {
		if (argc == 4) {
			int dst = atoi(argv[2]);
			int mtid = atoi(argv[3]);
			lookup (dst,mtid);
			/* use tcl.resultf () to return the lookup results */
			return TCL_OK;
		}
	}

	if (strcmp(argv[1], "sendHellos") == 0) {
		sendHellos ();	
		return TCL_OK;
	}

	if (strcmp(argv[1], "interfaceChanged") == 0) {
		interfaceChanged ();	
		return TCL_OK;
	}


	return Agent::command(argc, argv);
}


//***********************************************************************************
// initialize method: init class atributes 
//***********************************************************************************
void rtProtoOSPF::initialize() 
{
	
	Tcl & tcl = Tcl::instance();

	// call tcl get-node-id, atoi, set nodeId
	tcl.evalf("%s get-node-id", name());
	const char * resultString = tcl.result();
	nodeId_ = atoi(resultString);
 	
	// call tcl get-peers, strtok, set peerAddrMap, peerIdList;
	tcl.evalf("%s get-peers", name());
	resultString = tcl.result();
	
	int nodeId;
	ns_addr_t peer;
	

	// XXX no error checking for now yet. tcl MUST return pairs of numbers
	for ( LsIntList intList(resultString, " \t\n");
	      !intList.empty(); ) {
		nodeId = intList.front();
		intList.pop_front();
		// Agent.addr_
		peer.addr_ = intList.front();
		intList.pop_front();
		peer.port_ = intList.front();
		intList.pop_front();
		peerAddrMap_.insert(nodeId, peer);
		peerIdList_.push_back(nodeId);
	}
	
	//call tcl get-num-mtids
	tcl.evalf("%s get-num-mtids",name());
	resultString = tcl.result();
	numMtIds_ = atoi(resultString);
	
	int mtid; 
	int MT_metric_;
	RouterLinkState R;
	
	// call tcl get-links-status, strtok, set OspfLinkStateList_;
	tcl.evalf("%s get-links-status", name());
	resultString = tcl.result();

	for ( LsIntList intList2(resultString, " \t\n");
	      !intList2.empty(); ) {
		R.Link_ID_ = (u_int32_t) intList2.front();
		intList2.pop_front();
		R.Type_ = (RouterLink_type_t) intList2.front();
		intList2.pop_front();
		R.Num_MT_ = (int) intList2.front();
		intList2.pop_front();

 		if (!R.MTLinkList_.empty()) {
		R.MTLinkList_.erase (R.MTLinkList_.begin(),R.MTLinkList_.end());
		}

		// get the cost for each mtid
		for (int i=0; i<=R.Num_MT_; i++) {
			mtid= (int) intList2.front();
			intList2.pop_front();
			MT_metric_= (int) intList2.front();
			intList2.pop_front();	
				if (mtid==0) {
				R.MT0_metric_= MT_metric_;
				}
		   	R.MTLinkList_.push_back(MTLink (mtid,MT_metric_));
			}
		
   	   OspfLinkStateList_.push_back(R);
	}
	
	// call tcl get-delay-estimates
	tcl.evalf ("%s get-delay-estimates", name());
	
	// call routing.init(this); and computeRoutes
	routing_.init(this);
	routing_.computeRoutes();
	// debug
	tcl.evalf("%s set OSPF_ready", name());
	const char* token = strtok((char *)tcl.result(), " \t\n");
	if (token == NULL) 
		OSPF_ready_ = 0;
	else
		OSPF_ready_ = atoi(token); // buggy
}

//***********************************************************************************
// intfChanged method: called when a link cost has changed 
//***********************************************************************************

void rtProtoOSPF::intfChanged ()
{
	// XXX redudant, to be changed later
	Tcl & tcl = Tcl::instance();
	// call tcl get-links-status, strtok, set linkStateList;
	
	
	// destroy the old link states
	OspfLinkStateList_.eraseAll();
	// XXX no error checking for now. tcl MUST return triplets of numbers
  	
	int mtid; 
	int MT_metric_;
	RouterLinkState R;
	
	// call tcl get-links-status, strtok, set OspfLinkStateList_;
	tcl.evalf("%s get-links-status", name());
	const char * resultString = tcl.result();
	printf("get-link-status %s\n",resultString);
	
	for ( LsIntList intList2(resultString, " \t\n");
	      !intList2.empty(); ) {
		R.Link_ID_ = (u_int32_t) intList2.front();
		intList2.pop_front();
		R.Type_ = (RouterLink_type_t) intList2.front();
		intList2.pop_front();
		R.Num_MT_ = (int) intList2.front();
		intList2.pop_front();

 		if (!R.MTLinkList_.empty()) {
		R.MTLinkList_.erase (R.MTLinkList_.begin(),R.MTLinkList_.end());
		}

		// get the cost for each mtid
		for (int i=0; i<=R.Num_MT_; i++) {
			mtid= (int) intList2.front();
			intList2.pop_front();
			MT_metric_= (int) intList2.front();
			intList2.pop_front();	
				if (mtid==0) {
				R.MT0_metric_= MT_metric_;
				}
		   	R.MTLinkList_.push_back(MTLink (mtid,MT_metric_));
			}
		
   	   OspfLinkStateList_.push_back(R);
	}
	
	// call routing_'s LinkStateChanged() 
	//   for now, don't compute routes yet (?)
	routing_.linkStateChanged();
}

//***********************************************************************************
// interfaceChanged method: called when a link state has changed 
//***********************************************************************************

void rtProtoOSPF:: interfaceChanged() {

	//get tcl instance
	Tcl & tcl = Tcl::instance();

	// destroy the old active interfaces
	peerIdList_.eraseAll();
	// call tcl get-peers, strtok, set peerAddrMap, peerIdList;
	tcl.evalf("%s get-peers", name());
	const char * resultString = tcl.result();
	
	int nodeId;
	ns_addr_t peer;

	// XXX no error checking for now yet. tcl MUST return pairs of numbers
	for ( LsIntList intList(resultString, " \t\n");
	      !intList.empty(); ) {
		nodeId = intList.front();
		intList.pop_front();
		// Agent.addr_
		peer.addr_ = intList.front();
		intList.pop_front();
		peer.port_ = intList.front();
		intList.pop_front();
		peerAddrMap_.insert(nodeId, peer);
		peerIdList_.push_back(nodeId);
	}

	routing_.interfaceChanged();
	
}


void rtProtoOSPF::recv(Packet* p, Handler*)
{   
	hdr_Ospf* rh = hdr_Ospf::access(p);
	hdr_ip* ih = hdr_ip::access(p);
	// -- OSPF stuffs --


	printf("Destination Node  %d:receive message\n",nodeId_);
	Tcl& tcl = Tcl::instance();
	char ns_[8];
 	tcl.evalf("Simulator instance");
  	strcpy(ns_,tcl.result());

	switch(rh->type()) {
		case OSPF_MSG_HELLO:
			tcl.evalf("%s trace-annotate \"Agente OSPF %d.Reciviendo un HELLO de %d msgid %d\"",ns_,nodeId_,ih->src(),rh->msgId());			
			break;
		case OSPF_MSG_DD:
			tcl.evalf("%s trace-annotate \"Agente OSPF %d.Reciviendo un DD de %d msgid %d\"",ns_,nodeId_,ih->src(),rh->msgId());
			break;
		case OSPF_MSG_UPDATE:
			tcl.evalf("%s trace-annotate \"Agente OSPF %d.Reciviendo un UPDATE de %d msgid %d\"",ns_,
                  nodeId_,ih->src(),rh->msgId());
			break;
		case OSPF_MSG_REQUEST:
			tcl.evalf("%s trace-annotate \"Agente OSPF %d.Reciviendo un REQUEST de %d msgid %d\"",ns_,nodeId_,ih->src(),rh->msgId());
			break;
		case OSPF_MSG_ACK:
			tcl.evalf("%s trace-annotate \"Agente OSPF %d.Reciviendo un ACK de %d msgid %d\"",
			ns_,nodeId_,ih->src(),rh->msgId());
		default:
			break;
		};

		receiveMessage(findPeerNodeId(ih->src()), rh->msgId(),rh->type());

	Packet::free(p);
}

//***********************************************************************************
// receiveMessage method: called by node when a message is received 
//***********************************************************************************

void rtProtoOSPF::receiveMessage(int sender, u_int32_t msgId,Ospf_message_type_t type)
{

	if (routing_.receiveMessage(sender, msgId,type)){
	
	  installRoutes();
	}
}

//***********************************************************************************
// sendMessage method: called by node when a message is sent 
//***********************************************************************************

bool rtProtoOSPF::sendMessage(int destId, hdr_Ospf hdr) 
{
	printf("sendMessage: Destination Node %d\n",destId);
	
	Tcl& tcl = Tcl::instance();
	char ns_[8];
 	tcl.evalf("Simulator instance");
  	strcpy(ns_,tcl.result());

	switch(hdr.type_) {
		case OSPF_MSG_HELLO:
			tcl.evalf("%s hello-colour", name());
			tcl.evalf("%s trace-annotate \"Agente OSPF %d.Enviando un HELLO a %d msgid %d\"",
			ns_,nodeId_,destId,hdr.msgId_);			
			break;
		case OSPF_MSG_DD:
			tcl.evalf("%s dd-colour", name());
			tcl.evalf("%s trace-annotate \"Agente OSPF %d.Enviando un DD a %d msgid %d\"",ns_,
                  nodeId_,destId,hdr.msgId_);
			break;
		case OSPF_MSG_UPDATE:
			tcl.evalf("%s update-colour", name());
		tcl.evalf("%s trace-annotate \"Agente OSPF %d.Enviando un UPDATE a %d msgid %d\"",ns_,
                  nodeId_,destId,hdr.msgId_);
			break;
		case OSPF_MSG_REQUEST:
			tcl.evalf("%s request-colour", name());
			tcl.evalf("%s trace-annotate \"Agente OSPF %d.Enviando un REQUEST a %d msgid %d\"",
			ns_,nodeId_,destId,hdr.msgId_);
			break;
		case OSPF_MSG_ACK:
			tcl.evalf("%s ack-colour", name());
			tcl.evalf("%s trace-annotate \"Agente OSPF %d.Enviando un ACK a %d msgid %d\"",
			ns_,nodeId_,destId,hdr.msgId_);
		default:
			break;
		};

	ns_addr_t* agentAddrPtr = peerAddrMap_.findPtr(destId);
	if (agentAddrPtr == NULL) 
		return false;
	dst_ = *agentAddrPtr;
	size_ = hdr.packet_len_;
	
	
	Packet* p = Agent::allocpkt();
	hdr_Ospf *rh = hdr_Ospf::access(p);
	rh->version() = hdr.version_;
	rh->type() = hdr.type_;
	rh->packet_len() = hdr.packet_len_;
	rh->router_id() = hdr.router_id_;
	rh->msgId() = hdr.msgId_;

	
	target_->recv(p);
	return true;
}

//***********************************************************************************
// lookup method: called when looking for the path of destId and mtid 
//***********************************************************************************

void rtProtoOSPF::lookup(int destId, int mtid) 
{
	
	// Call routing_'s lookup
	
	OspfEqualPaths* EPptr = routing_.lookup(destId,mtid);
		
	// then use tcl.resultf() to return the results
	if (EPptr == NULL) {
		
		Tcl::instance().resultf( "%s",  "");
		return;
	}
	char resultBuf[64]; // XXX buggy;
	sprintf(resultBuf, "%d" , EPptr->cost);
	
	char tmpBuf[16]; // XXX
 
	for (LsNodeIdList::iterator itr = (EPptr->nextHopList).begin();
	     itr != (EPptr->nextHopList).end(); itr++) {
		sprintf(tmpBuf, " %d", (*itr) );		
		strcat (resultBuf, tmpBuf); // strcat (dest, src);
	}

	Tcl::instance().resultf("%s", resultBuf);
}

 
//***********************************************************************************
// findPeerNodeId method: find the int value mapped by the agentAddr 
//***********************************************************************************

int rtProtoOSPF::findPeerNodeId (ns_addr_t agentAddr) 
{
	// because the agentAddr is the value, not the key of the map
	for (PeerAddrMap::iterator itr = peerAddrMap_.begin();
	     itr != peerAddrMap_.end(); itr++) {
		if ((*itr).second.isEqual (agentAddr)) {
			return (*itr).first;
		}
	}
	return LS_INVALID_NODE_ID;
}


#endif // HAVE_STL
