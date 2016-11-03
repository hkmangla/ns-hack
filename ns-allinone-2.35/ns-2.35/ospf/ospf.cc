/*
 * ospf.cc
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

#include "ospf.h"

// a global variable
OspfMessageCenter OspfMessageCenter::msgctr_;

int OspfRouting::msgSizes[OSPF_MESSAGE_TYPES];
static class initRouting {
public:
		initRouting(){
		OspfRouting::msgSizes[OSPF_MSG_HELLO] = OSPF_HELLO_MESSAGE_SIZE;
		OspfRouting::msgSizes[OSPF_MSG_DD] =  OSPF_DD_MESSAGE_SIZE;
		OspfRouting::msgSizes[OSPF_MSG_REQUEST] = OSPF_REQUEST_MESSAGE_SIZE;
		OspfRouting::msgSizes[OSPF_MSG_UPDATE] = OSPF_UPDATE_MESSAGE_SIZE;
		OspfRouting::msgSizes[OSPF_MSG_ACK] = OSPF_ACK_MESSAGE_SIZE;
	}	
} ospfRoutingInitializer;


static void ls_error(char* msg) 
{ 
	fprintf(stderr, "%s\n", msg);
	abort();
}

//**********************************************************************************
// OspfPaths methods
//**********************************************************************************

//**********************************************************************************
// insertPath method: insert a new path into OspfPaths 
//**********************************************************************************

OspfPaths::iterator OspfPaths::insertPath(int destId, int mtid, int cost, int nextHop) 
{
	iterator itr = OspfMTPathsMap::find(destId);
	// if new path, insert it and return iterator
	if (itr == end())
		return insertPathNoChecking(destId,mtid, cost, nextHop);
	
	OspfMTRouterPathsMap* pEPM=&(*itr).second;
	
	OspfEqualPaths * pEP = pEPM->findPtr(mtid);
	//if new mtid, insert it and return iterator
	if (pEP == NULL) {
		return insertPathNoChecking(destId,mtid, cost, nextHop);
	}

	// if the old path is better, ignore it, return end() 
	// to flag the error
	if (pEP->cost < cost)
		return end(); 

	// else if the new path is better, erase the old ones and save the new one
	LsNodeIdList & nhList = pEP->nextHopList;
	if (pEP->cost > cost) {
		pEP->cost = cost;
		nhList.erase(nhList.begin(), nhList.end());
		nhList.push_back(nextHop);
		return itr;
	}
	// else the old path is the same cost, check for duplicate
	for (LsNodeIdList::iterator itrList = nhList.begin();
	     itrList != nhList.end(); itrList++)
		// if duplicate found, return 0 to flag the error 
		if ((*itrList) == nextHop)
			return end(); 
	// else, the new path is installed indeed, the total number of nextHops
	// is returned. 
	nhList.push_back(nextHop);
	return itr;
}

//**********************************************************************************
// insertNextHopList method: insert a next hop list into OspfPaths
//**********************************************************************************

OspfPaths::iterator 
OspfPaths::insertNextHopList(int destId, int cost, int mtid,
			   const LsNodeIdList& nextHopList) 
{
	iterator itr = OspfMTPathsMap::find(destId);
	// if new path, insert it and return iterator 
	if (itr == end()) {
		OspfEqualPaths ep(cost, nextHopList);
		OspfMTRouterPathsMap  pEPM;
		itr = insert(destId,pEPM);
		(& (* itr).second)->insert(mtid,ep);
		itr=find(destId);
		
		return itr;
	}
	// else if new mtid, insert it and return iterator
	OspfEqualPaths* ptrOldEp = (&(*itr).second)->findPtr(mtid);
	// if the old path is better, ignore it, return end() to flag error
	if ( ptrOldEp  == NULL) {
		OspfEqualPaths ep(cost, nextHopList);
		(& (* itr).second)->insert(mtid,ep);
		itr = find(destId);
		return itr;
	}
	
	// else get the old ep
	if (ptrOldEp->cost < cost)
		return end();
	// else if the new path is better, replace the old one with the new one
	if (ptrOldEp->cost > cost) {
		ptrOldEp->cost = cost;
		ptrOldEp->nextHopList = nextHopList ; // copy
		return itr;
	}
	// equal cost: append the new next hops with checking for duplicates
	ptrOldEp->appendNextHopList(nextHopList);
	return itr;
}


//**********************************************************************************
// OspfPathsTentative methods
//**********************************************************************************

//**********************************************************************************
// popShortestPath method: get and remove min path 
//**********************************************************************************


OspfPath OspfPathsTentative::popShortestPath(int mtid) 
{
	findMinEqualPaths(mtid);
	OspfPath path;
	if (empty() || (minCostIterator == end()))
		return path; // an invalid one

	OspfEqualPaths* ep=(*minCostIterator).second.findPtr(mtid);

	LsNodeIdList& nhList = ep->nextHopList;

	if (nhList.empty() && (findMinEqualPaths(mtid) == end()))
		return path;
	path.desId_ = (*minCostIterator).first;
	path.cost_=ep->cost;
	path.mtId_=mtid;

	// the first 'nextHop'
	path.nextHop_ = nhList.front();
	nhList.pop_front();

	// if this pops out the last nextHop in the EqualPaths, find a new one.
	if (nhList.empty()) {
		erase(minCostIterator);
		findMinEqualPaths(mtid);
	}

	return path;
}

//**********************************************************************************
// findMinEqualPaths method: get less cost equal paths 
//**********************************************************************************

OspfPathsTentative::iterator OspfPathsTentative::findMinEqualPaths(int mtid)
{
	minCost = LS_MAX_COST + 1; 
	minCostIterator = end();
	for (iterator itr = begin(); itr != end(); itr++){
		
		OspfEqualPaths* ep=(*itr).second.findPtr(mtid);
		 
		if (ep!=NULL) {
		  if ((minCost > ep->cost) && 
		    !ep->nextHopList.empty()) {
			minCost = ep->cost;
			minCostIterator = itr;
		  }
                }
	}         
	return minCostIterator;
}

//**********************************************************************************
// OspfTopoMap methods
//**********************************************************************************

//**********************************************************************************
// insertLinkState method: insert a link state advertisement into the database 
//**********************************************************************************
OspfLinkStateList* OspfTopoMap::insertLinkState (int nodeId, OspfLinkState& link_state)
{	
	
	OspfLinkStateList* lsp = OspfLinkStateMap::findPtr(nodeId);
	if (lsp != NULL) {
		// there's a node with other linkState, not checking if there's
		// duplicate
		lsp->push_back(link_state);
		return lsp;
	}

	// else new node
	OspfLinkStateList lsl; // an empty one
	iterator itr = insert(nodeId,lsl);
	if (itr != end()){
		// successful
		(*itr).second.push_back(link_state);
		return &((*itr).second);
	}
	// else something is wrong
	ls_error("OspfTopoMap::insertLinkState failed\n"); // debug
	return (OspfLinkStateList *) NULL;


}

//**********************************************************************************
// update method: update the database if necessary and return true if something is 
//changed 
//**********************************************************************************

bool OspfTopoMap::update(int nodeId, 
		       const OspfLinkStateList& linkStateAdvList)
{
	printf("OspfTopoMap update\n");
	OspfLinkStateList * LSLAdvptr = findPtr (nodeId);

	if (LSLAdvptr == NULL) {
		insert(nodeId, linkStateAdvList);
		return true;
	}

	//get the first advertising store in the db: in this implementation is the one used
	OspfLinkState lsadold = LSLAdvptr->front();
	RouterLinkStateList lslold= (*lsadold.RouterLinkStateListPtr_);
	printf("OLD\n");
	for (RouterLinkStateList::iterator itrList =lslold.begin();
            itrList != lslold.end(); itrList++){

		printf("Nodo vecino: %d ",(&(*itrList))->Link_ID_);
		for (MTLinkList::iterator itrList2 = (&(*itrList))->MTLinkList_.begin();
             itrList2 != (&(*itrList))->MTLinkList_.end(); itrList2++){
		
		printf("Mtid: %d Coste: %d\n",(&(*itrList2))->mtId_,(&(*itrList2))->metric_);

		}	
	}	
	
	OspfLinkState lsad = linkStateAdvList.front();
	RouterLinkStateList* lslPtr_= lsad.RouterLinkStateListPtr_;
	printf("NEW\n");
	for (RouterLinkStateList::iterator itrList =lslPtr_->begin();
            itrList != lslPtr_->end(); itrList++){

		printf("Nodo vecino: %d ",(&(*itrList))->Link_ID_);
		for (MTLinkList::iterator itrList2 = (&(*itrList))->MTLinkList_.begin();
             itrList2 != (&(*itrList))->MTLinkList_.end(); itrList2++){
		
		printf("Mtid: %d Coste: %d\n",(&(*itrList2))->mtId_,(&(*itrList2))->metric_);

		}	
	}	
	
	bool retCode = false;
	
	RouterLinkStateList::iterator itrOld;
	for (RouterLinkStateList::iterator itrNew = lslPtr_->begin();
	     itrNew != lslPtr_->end(); itrNew++ ) {
  
		for (itrOld = lslold.begin();
		     itrOld != lslold.end(); itrOld++) {
			printf("OLD metric %d\n",(*itrOld).MT0_metric_);
			printf("NEW metric %d\n",(*itrNew).MT0_metric_);
			if((*itrOld).state_==LS_STATUS_DOWN)
				continue;

			if ((*itrNew).Link_ID_ == (*itrOld).Link_ID_) {
				
				// old link state found
				if ((*itrNew).MT0_metric_!= (*itrOld).MT0_metric_) {
					(*itrOld).MT0_metric_ = (*itrNew).MT0_metric_;
					retCode = true;
						
					int nmtids= (*itrNew).Num_MT_;
					for (int i=0; i<=nmtids; i++) {
						for (MTLinkList::iterator 
						itrListNew =(&(*itrNew))->MTLinkList_.begin();
              					itrListNew != (&(*itrNew))->MTLinkList_.end();
						itrListNew++){
						
						for (MTLinkList::iterator itrListOld = 
						(&(*itrOld))->MTLinkList_.begin();
              					itrListOld != (&(*itrOld))->MTLinkList_.end();
						itrListOld++){
														
	        					if((&(*itrListNew))->mtId_==(&(*itrListOld))
										  ->mtId_) {
		   					(*itrListOld).metric_ = (*itrListNew).metric_;	
							break;
							} //fi

						   }//for MTLinkList Old	

	      					}// for MTLinkList New
					
					}// for nmtids
			
				    }// if mt0metric
				
				break; // no need to search for more old link state;
			} // end if old link state found
		} // end for old link states
		if (itrOld == lslPtr_->end()) {
			// no old link found
			lslPtr_->push_back(*itrNew);
			retCode = true;
		}
	}// end for new link states 

	delLSA (nodeId,lsadold.ls_hdr_.LS_type_,
					lsadold.ls_hdr_.LS_ID_,
					lsadold.ls_hdr_.advertising_router_);
	//update LSage and LS sequence number 
	lsadold.ls_hdr_.LSage_=0;
	lsadold.ls_hdr_.LS_sequence_num_+=1;
	printf("seq number %f\n",lsadold.ls_hdr_.LS_sequence_num_);
	lsadold.RouterLinkStateListPtr_=new RouterLinkStateList(lslold);
	// insert the updated link state advertisement into the data base
	insertLinkState(nodeId,lsadold);
	
		
	return retCode;
}


//**********************************************************************************
// updateRoutingTable method: returns true if it is necessary to calculate the 
// routing table and false in other case	
//**********************************************************************************

bool OspfTopoMap::updateRoutingTable (int nodeId, OspfLinkState& linkStateAdv){


	OspfLinkStateList * LSLAdvptr = findPtr (nodeId);

	if (LSLAdvptr == NULL) {
		//the LSA doesn't exist in the database
		return true;
	}
			
	bool retCode = false;
	//new LSA
	LinkStateHeader lshdrnew=linkStateAdv.ls_hdr_;

	//get the LSA copy in the database
	OspfLinkState lsadold=getLSA (lshdrnew.LS_type_,lshdrnew.LS_ID_,
	lshdrnew.advertising_router_);
		
	//get LSA header in the database	
	LinkStateHeader lshdrold=lsadold.ls_hdr_;

	//compare LSA headers
	//options field
	if((lshdrold.options_.bit_T!=lshdrnew.options_.bit_T)||
	   (lshdrold.options_.bit_E!=lshdrnew.options_.bit_E))
		return true;
	//LSage field
	if(((lshdrold.LSage_=MAX_AGE)&&(lshdrnew.LSage_!=MAX_AGE))
	||((lshdrnew.LSage_=MAX_AGE)&&(lshdrold.LSage_!=MAX_AGE)))
		return true;

	//Length field
	if(((lshdrold.length_=MAX_AGE)&&(lshdrnew.length_!=MAX_AGE))
	||((lshdrnew.length_=MAX_AGE)&&(lshdrold.length_!=MAX_AGE)))
		return true;

	//compare LSA contents
	RouterLinkStateList* lsloldPtr_= lsadold.RouterLinkStateListPtr_;
	RouterLinkStateList* lslPtr_= linkStateAdv.RouterLinkStateListPtr_;

		
	RouterLinkStateList::iterator itrOld;
	for (RouterLinkStateList::iterator itrNew = lslPtr_->begin();
	     itrNew != lslPtr_->end(); itrNew++ ) {
  
		for (itrOld = lsloldPtr_->begin();
		     itrOld != lsloldPtr_->end(); itrOld++) {

			if ((*itrNew).Link_ID_ == (*itrOld).Link_ID_) {
				
				// old link state found
				if ((*itrNew).MT0_metric_!= (*itrOld).MT0_metric_) {
					(*itrOld).MT0_metric_ = (*itrNew).MT0_metric_;
					return true;
								
				    }// if mt0metric
				
				break; // no need to search for more old link state;
			} // end if old link state found
		} // end for old link states
		if (itrOld == lslPtr_->end()) 
			// no old link found
			return true;
	}// end for new link states 
	
	return retCode;
}

//***********************************************************************************
// getLSList method: list all the link state advertisements stored in the database. 
//***********************************************************************************

OspfLinkStateList OspfTopoMap:: getLSList () {

	OspfLinkStateList lsl;
	
	for (OspfLinkStateMap::iterator itrMap = begin();
            itrMap != end(); itrMap++) {
		for (OspfLinkStateList::iterator itrList = (*itrMap).second.begin();
	            itrList!=(*itrMap).second.end(); itrList++) {
			lsl.push_back(*itrList);	
			
		}
	}

	return lsl;
}


//**********************************************************************************
// lookupLSA method: return NO_EXIST if the LSA does not exist, EQUALS if two 
// instances are equal, OLDER if the LSA received is older than the database copy, 
// and NEWER if is newer. 
//**********************************************************************************

retCode_t OspfTopoMap::lookupLSA (LinkStateHeader hdr){

	retCode_t retCode=NO_EXIST;
	LinkStateHeader lshdr;

	for (OspfLinkStateMap::iterator itrMap = begin();
            itrMap != end(); itrMap++) {
		for (OspfLinkStateList::iterator itrList = (*itrMap).second.begin();
	            itrList!=(*itrMap).second.end(); itrList++) {

			lshdr=(*itrList).ls_hdr_;
			if((hdr.LS_type_==lshdr.LS_type_)&&
			   (hdr.LS_ID_==lshdr.LS_ID_)&&
			   (hdr.advertising_router_==lshdr.advertising_router_)){
			   //exists LSA in the database
				printf("LSage LSA %f\n",hdr.LSage_);
				printf("LSage %f\n",lshdr.LSage_);
				printf("LSseq LSA %f\n",hdr.LS_sequence_num_);
				printf("LSseq bd %f\n",lshdr.LS_sequence_num_);
				
			
			   	if(hdr.LS_sequence_num_<lshdr.LS_sequence_num_){
					//LSA received is older
					printf("LSA received is older\n");
					return OLDER;
				}
				
				if(hdr.LS_sequence_num_>lshdr.LS_sequence_num_){
					//LSA received is newer
					printf("LSA received is newer\n");
					return NEWER;
				}
				if(hdr.LS_sequence_num_==lshdr.LS_sequence_num_){
		
					if(hdr.LSage_==MAX_AGE)
						return NEWER;
					if(lshdr.LSage_==MAX_AGE)
						return OLDER;
					if((lshdr.LSage_-hdr.LSage_)>MAX_AGE_DIFF){
						
						if(hdr.LSage_<lshdr.LSage_)
							return NEWER;
						if(hdr.LSage_>lshdr.LSage_)
							return OLDER;
					}
						
					return EQUALS;				
				}


							
			} 	 	
		}
	}

	return retCode;
}


//**********************************************************************************
// lookupLSA method: return true if the LSA exists and false if not 
//**********************************************************************************

bool OspfTopoMap::lookupLSA (LS_type_t ls_type, u_int32_t ls_id, u_int32_t advertising_router){

	LinkStateHeader lshdr;
	for (OspfLinkStateMap::iterator itrMap = begin();
            itrMap != end(); itrMap++) {
		for (OspfLinkStateList::iterator itrList = (*itrMap).second.begin();
	            itrList!=(*itrMap).second.end(); itrList++) {

			lshdr=(*itrList).ls_hdr_;
			if((ls_type==lshdr.LS_type_)&&
			   (ls_id==lshdr.LS_ID_)&&
			   (advertising_router==lshdr.advertising_router_))
			   //exists LSA in the database
			   		return true;
			
			}
		}
	
	return false;
}


//**********************************************************************************
// getLSA method: get the link state advertisment indexed by the parameters
//**********************************************************************************

OspfLinkState OspfTopoMap::getLSA (LS_type_t ls_type, u_int32_t ls_id, u_int32_t advertising_router){

	LinkStateHeader lshdr;
	for (OspfLinkStateMap::iterator itrMap = begin();
            itrMap != end(); itrMap++) {
		for (OspfLinkStateList::iterator itrList = (*itrMap).second.begin();
	            itrList!=(*itrMap).second.end(); itrList++) {

			lshdr=(*itrList).ls_hdr_;
			if((ls_type==lshdr.LS_type_)&&
			   (ls_id==lshdr.LS_ID_)&&
			   (advertising_router==lshdr.advertising_router_)){
			   //exists LSA in the database
				return (*itrList);
		 }//end if	
	     }
	}
	
	return (OspfLinkState ());
}


//**********************************************************************************
// incLSage method: increments LSA's LSage field for nodeId
//**********************************************************************************

void OspfTopoMap::incLSage (int nodeId){
 	
	//OspfLinkStateMap::iterator itr= find(nodeId);
	OspfLinkStateList * ptrLSLAd = findPtr(nodeId);

	//get the first advertising: in this implementation is the one used
	if (ptrLSLAd==NULL)
		ls_error("OspfTopoMap::incLSage.\n");
		

	for (OspfLinkStateList::iterator itrList = ptrLSLAd->begin();
	            itrList!=ptrLSLAd->end(); itrList++) {
			printf("LSage: %f\n",(*itrList).ls_hdr_.LSage_);
			(*itrList).ls_hdr_.LSage_+=1;
			printf("LSage: %f\n",(*itrList).ls_hdr_.LSage_);
			return;
	}

	
}


//**********************************************************************************
// delLSA method: delete the LSA from the database 
//**********************************************************************************

void OspfTopoMap::delLSA (int nodeId, LS_type_t ls_type, u_int32_t ls_id, u_int32_t advertising_router){


	OspfLinkStateList* lsl = OspfLinkStateMap::findPtr(nodeId);
	LinkStateHeader lshdr;
	
	if (lsl!=NULL) {
		for (OspfLinkStateList::iterator itrList = lsl->begin();
	            itrList!=lsl->end(); itrList++) {

			lshdr=(*itrList).ls_hdr_;
			if((ls_type==lshdr.LS_type_)&&
			   (ls_id==lshdr.LS_ID_)&&
			   (advertising_router==lshdr.advertising_router_)){
			   //exists LSA in the database: remove it
				lsl->erase(itrList);
				return;
			}
			   		
			
			}
		}
	
}



//**********************************************************************************
// OspfMessageCenter methods
//**********************************************************************************


void OspfMessageCenter::init() 
{
	// only when nothing is provided by tcl code
	max_size = 300;
}

//*********************************************************************************
// newMessage method: create a new message of the type indicated as parameter
//**********************************************************************************

OspfMessage* OspfMessageCenter::newMessage (int senderNodeId, Ospf_message_type_t type)
{
	// check if max_message_number is invalid, call init ()
	if (max_size == 0)
		init(); 

	message_storage* storagePtr;
	u_int32_t currentId;
	
	//check the type to use a different storage
	switch (type) {
	case OSPF_MSG_INVALID:
		ls_error("OSPF_MSG_INVALID "
			 "OspfMessageCenter::newMessage.\n");
		break;
	case OSPF_MSG_HELLO:
		storagePtr = & hello_messages;
		if(current_hello_id==OSPF_WRAPAROUND_THRESHOLD)
			current_hello_id=LS_INVALID_MESSAGE_ID;	
		current_hello_id+=1;
		printf("current_hello_id:%d\n",current_hello_id);
		currentId = current_hello_id;
		break;

	case OSPF_MSG_DD:
		storagePtr = & dd_messages;
		if(current_dd_id==OSPF_WRAPAROUND_THRESHOLD)
			current_dd_id=LS_INVALID_MESSAGE_ID;	
		current_dd_id+=1;
		printf("current_dd_id:%d\n",current_dd_id);
		currentId = current_dd_id;
		break;

	case OSPF_MSG_REQUEST:
		storagePtr = & request_messages;
		if(current_request_id==OSPF_WRAPAROUND_THRESHOLD)
			current_request_id=LS_INVALID_MESSAGE_ID;	
		current_request_id+=1;
		printf("current_request_id:%d\n",current_request_id);
		currentId = current_request_id;
		break;

	case OSPF_MSG_UPDATE:
		storagePtr = & update_messages;
		if(current_update_id==OSPF_WRAPAROUND_THRESHOLD)
			current_update_id=LS_INVALID_MESSAGE_ID;	
		current_update_id+=1;
		printf("current_update_id:%d\n",current_update_id);
		currentId = current_update_id;
		break;

	case OSPF_MSG_ACK:
		storagePtr = & ack_messages;
		if(current_ack_id==OSPF_WRAPAROUND_THRESHOLD)
			current_ack_id=LS_INVALID_MESSAGE_ID;	
		current_ack_id+=1;
		printf("current_ack_id:%d\n",current_ack_id);
		currentId = current_ack_id;
		break;

	default:
		// nothing, just to avoid compiler warning
		break;
	}
	
	// if max_size reached, recycle
	if (storagePtr->size() >= max_size)
		storagePtr->erase(storagePtr->begin());
	
	OspfMessage msg;
	OspfMessage* msgPtr;
	msg.messageId_=(u_int32_t)currentId;
	msg.originNodeId_=senderNodeId;
	
	//insert the new message if could
	message_storage::iterator itr = 
		storagePtr->insert(currentId,msg); 
				   	
	if (itr == storagePtr->end())
		ls_error("Can't insert new message in "
			 "LsMessageCenter::newMessage.\n");
	else
		msgPtr = &((*itr).second);

	//return the message address
	return msgPtr;
}


//*********************************************************************************
// deleteMessage method: delete the message of the type indicated as parameter
//**********************************************************************************

bool OspfMessageCenter::deleteMessage(u_int32_t msgId, Ospf_message_type_t type)
{
	
	message_storage* storagePtr;
	switch (type) {
	case OSPF_MSG_INVALID:
		ls_error("OSPF_MSG_INVALID "
			 "OspfMessageCenter::deleteMessage.\n");
		break;
	case OSPF_MSG_HELLO:
		storagePtr = &hello_messages;
		break;

	case OSPF_MSG_DD:
		storagePtr = &dd_messages;
		break;

	case OSPF_MSG_REQUEST:
		storagePtr = & request_messages;
		break;

	case OSPF_MSG_UPDATE:
		storagePtr = & update_messages;
		break;

	case OSPF_MSG_ACK:
		storagePtr = & ack_messages;
		break;

	default:
		// nothing, just to avoid compiler warning
		break;
	}

	message_storage::iterator itr = storagePtr->find (msgId); 
	
	if (itr == storagePtr->end())
		return false;
	storagePtr->erase(itr);
		return true;

}


//*********************************************************************************
// retrieveMessagePtr method: retreive message of the type indicated as parameter
//**********************************************************************************

OspfMessage* OspfMessageCenter::retrieveMessagePtr(u_int32_t msgId, Ospf_message_type_t type)
{
	OspfMessage * ospfmsgPtr;
	
	switch (type) {
	case OSPF_MSG_INVALID:
		ls_error("OSPF_MSG_INVALID "
			 "OspfMessageCenter::retrieveMessage.\n");
		break;
	case OSPF_MSG_HELLO:
		ospfmsgPtr=hello_messages.findPtr(msgId);
		break;

	case OSPF_MSG_DD:
		ospfmsgPtr=dd_messages.findPtr(msgId);
		break;

	case OSPF_MSG_REQUEST:
		ospfmsgPtr=request_messages.findPtr(msgId);
		break;

	case OSPF_MSG_UPDATE:
		ospfmsgPtr=update_messages.findPtr(msgId);
		break;

	case OSPF_MSG_ACK:
		ospfmsgPtr=ack_messages.findPtr(msgId);
		break;

	default:
		ospfmsgPtr=NULL; // nothing, just to avoid compiler warning
		break;
	}	
	
	
	return ospfmsgPtr;
}


//**********************************************************************************
// OspfRetransmissionManager methods
//**********************************************************************************

//**********************************************************************************
// initTimeout method: gets the delay time compute for each neighbour 
//**********************************************************************************

void OspfRetransmissionManager::initTimeout(LsDelayMap * delayMapPtr) 
{
	if (delayMapPtr == NULL)
		return;
	
	for (LsDelayMap::iterator itr = delayMapPtr->begin();
	     itr != delayMapPtr->end(); itr++)
		// timeout is LS_TIMEOUT_FACTOR*one-way-delay estimate
		insert((*itr).first, 
		       OspfUnackPeer(this, (*itr).first, 
				   OSPF_TIMEOUT_FACTOR*(*itr).second));
}


//**********************************************************************************
// messageOut method: called by OspfRouting when a message is sent out
//**********************************************************************************

int OspfRetransmissionManager::messageOut(int peerId, const OspfMessage& msg,
					Ospf_message_type_t type)
{ 
	OspfUnackPeer* peerPtr = findPtr(peerId);
	
	if (peerPtr == NULL) {
		iterator itr = insert(peerId, OspfUnackPeer(this, peerId));
		if (itr == end()) { 
			ls_error ("Can't insert."); 
		}
		peerPtr = &((*itr).second);
	}
	
	switch (type) {
	case OSPF_MSG_DD:
	{
			printf("Message out: Peerid: %d send dd\n",peerId);
			peerPtr->DdSeq_.msgId_=msg.messageId_;
			peerPtr->DdSeq_.seq_=msg.DDPacketPtr_->DDSeqNumber_;
			printf("Seq %f\n",peerPtr->DdSeq_.seq_);
			
	 		// reschedule timer to allow account for this latest message
			printf("RxmTimeout:%f\n",peerPtr->rtxTimeout_);
			peerPtr->ackTimer_.resched(peerPtr->rtxTimeout_);
			
	}
			break;
		
	case OSPF_MSG_REQUEST:
	{
			printf("Message out: Peerid: %d send request\n",peerId);
			peerPtr->ReqSeq_=msg.messageId_;
			// reschedule timer to allow account for this latest message
			peerPtr->ackTimer_.resched(peerPtr->rtxTimeout_);
	}
	    		break;

	case OSPF_MSG_UPDATE:
	{
			printf("Message out: Peerid: %d send update\n",peerId);
			u_int32_t* msgId = peerPtr->UpdateMap_.findPtr(msg.originNodeId_);
			if (msgId == NULL){
				peerPtr->UpdateMap_.insert(msg.originNodeId_,msg.messageId_);
			}
			else {
				*msgId= msg.messageId_;
			}
			// reschedule timer to allow account for this latest message
			peerPtr->updateTimer_.resched(peerPtr->rtxTimeout_);
	}			
			break;

	case OSPF_MSG_INVALID:
	case OSPF_MSG_HELLO:
	case OSPF_MSG_ACK:
	default:
		// nothing, just to avoid compiler warning
		break;
	}
 
	
	return 0;
}


//**********************************************************************************
// messageIn: Called by OspfRouting,  when a message that serves as ack is received
//**********************************************************************************

int OspfRetransmissionManager::messageIn(int peerId, const OspfMessage& msg,Ospf_message_type_t type)
{
	
	OspfUnackPeer* peerPtr = findPtr(peerId);
	if (peerPtr == NULL) {
		printf("perrPtr NULL\n");
		// no pending ack for this neighbor 
		return 0;
	}

	int retCode=1; // 1--> ack incorrect: the message must be in the messageCenter
		       // 0--> ack correct: the message should be removed from the messageCenter
  
	LsMap<int, u_int32_t>::iterator itr;

	switch (type) {
	case OSPF_MSG_DD:
			printf("Message DD %d in: Peerid: %d\n",msg.messageId_,peerId);
			printf("seq store %f seq pkt %f\n",peerPtr->DdSeq_.seq_,
			msg.DDPacketPtr_->DDSeqNumber_);
			printf("Message id %d\n",peerPtr->DdSeq_.msgId_);
			
			if (peerPtr->DdSeq_.seq_ == msg.DDPacketPtr_->DDSeqNumber_){
			// We've got the right ack, so erase the unack record
			printf("dd received ok\n");
			peerPtr->DdSeq_.msgId_= LS_INVALID_MESSAGE_ID;
			peerPtr->DdSeq_.seq_= LS_INVALID_MESSAGE_ID;
			retCode=0;
			}

		if ((peerPtr->DdSeq_.seq_ == LS_INVALID_MESSAGE_ID) &&
	    	(peerPtr->DdSeq_.msgId_ == LS_INVALID_MESSAGE_ID)&&	
	    	(peerPtr->ReqSeq_== LS_INVALID_MESSAGE_ID)){
		// No more pending ack, cancel timer
		  peerPtr->ackTimer_.cancel();
		}
			
			break;

	case OSPF_MSG_UPDATE:
			printf("Message update in: Peerid: %d\n",peerId);
			peerPtr->ReqSeq_=LS_INVALID_MESSAGE_ID;
			retCode=0;

		if ((peerPtr->DdSeq_.seq_ == LS_INVALID_MESSAGE_ID) &&
	    	(peerPtr->DdSeq_.msgId_ == LS_INVALID_MESSAGE_ID)&&	
	    	(peerPtr->ReqSeq_== LS_INVALID_MESSAGE_ID)){
		// No more pending ack, cancel timer
		peerPtr->ackTimer_.cancel();
		}
	    		break;
	
	case OSPF_MSG_ACK:
			printf("Message ack in: Peerid: %d\n",peerId);
			itr = peerPtr->UpdateMap_.find(msg.originNodeId_);
	
			for (LsMap<int, u_int32_t>::iterator itr = peerPtr->UpdateMap_.begin();
			itr != peerPtr->UpdateMap_.end(); ++itr) {
				peerPtr->UpdateMap_.erase(itr);
			}

			if(peerPtr->UpdateMap_.empty()){
				printf("cancel update timer\n");	
				peerPtr->updateTimer_.cancel();
			}
			break;
	
	default:
		break;
	}
	

	return retCode;
}


//**********************************************************************************
// cancelTimer: cancel ackTimer
//**********************************************************************************

void OspfRetransmissionManager::cancelTimer (int nbrId) 
{
	OspfUnackPeer* peerPtr = findPtr(nbrId);
	if (peerPtr == NULL) 
		return;
	
	peerPtr->DdSeq_.seq_ = LS_INVALID_MESSAGE_ID;
	peerPtr->DdSeq_.msgId_ = LS_INVALID_MESSAGE_ID;
	peerPtr->ReqSeq_= LS_INVALID_MESSAGE_ID;	 
	peerPtr->UpdateMap_.eraseAll();
	if(peerPtr->UpdateMap_.empty())
		printf("UpdateMap empty\n");	
	peerPtr->ackTimer_.force_cancel();
	peerPtr->updateTimer_.force_cancel();
	
}


//**********************************************************************************
// resendMessages: resend all unack messages to the peer peerId
//**********************************************************************************

int OspfRetransmissionManager::resendMessages (int peerId) 
{	
	int nodo=OspfRouting_.getnodeid();
	printf("[%f] NODO: %d\n",NOW,nodo);
	printf("PeerId: %d RESEND MESSAGE\n",peerId);
	bool updTimer=false;
	
	
	OspfUnackPeer* peerPtr = findPtr (peerId);
	if (peerPtr == NULL) 
		// That's funny, We should never get in here
		ls_error ("Wait a minute, nothing to send for this neighbor");
	
	// resend DD map
	if (peerPtr->DdSeq_.seq_ != LS_INVALID_MESSAGE_ID){
	
	//get the ospf header	
	hdr_Ospf hdrOspf =OspfRouting_.getOspfHeader (OSPF_MSG_DD,peerPtr->DdSeq_.msgId_);
	OspfRouting_.resendMessage(peerId,hdrOspf);
	
	// reschedule retransmit timer
	peerPtr->ackTimer_.resched(peerPtr->rtxTimeout_);
	return 0;
	}
  	
	//resend request
	if (peerPtr->ReqSeq_!= LS_INVALID_MESSAGE_ID){
	printf("resend Request\n");
		
	//get the ospf header	
	hdr_Ospf hdrOspf =OspfRouting_.getOspfHeader (OSPF_MSG_REQUEST,peerPtr->ReqSeq_);
	OspfRouting_.resendMessage(peerId,hdrOspf);
	
	// reschedule retransmit timer
	peerPtr->ackTimer_.resched(peerPtr->rtxTimeout_);
	return 0;
	}
  		
	//resend update
	// resend all other unack'ed LSA
	for (LsMap<int, u_int32_t>::iterator itr = peerPtr->UpdateMap_.begin();
		
	     itr != peerPtr->UpdateMap_.end(); ++itr) {
		hdr_Ospf hdrOspf =OspfRouting_.getOspfHeader (OSPF_MSG_UPDATE,(*itr).second);
		printf("resend update \n");
		OspfRouting_.resendMessage(peerId,hdrOspf);
		updTimer=true;
	}
	
	// reschedule retransmit timer if needed
	if(updTimer){
		
		peerPtr->updateTimer_.resched(peerPtr->rtxTimeout_);
	}
	
	return 0;
}
 

 

//**********************************************************************************
// OspfInactivityManager methods
//**********************************************************************************

//**********************************************************************************
// initTimeout method: set the delay time for each neighbour 
//**********************************************************************************

void OspfInactivityManager::initTimeout(LsNodeIdList* peerIdListPtr, double router_dead_interval) 
{
	if (peerIdListPtr== NULL)
		return; 

	for (LsNodeIdList::iterator itr = peerIdListPtr->begin();
	     itr != peerIdListPtr->end(); itr++)
		
		insert(*itr,OspfHelloPeer(this,*itr,router_dead_interval));
}


//**********************************************************************************
// messageOut method: called by OspfRouting when a hello message is sent out
//**********************************************************************************

int OspfInactivityManager::messageOut(int peerId, Ospf_message_type_t type)
{ 
	OspfHelloPeer* peerPtr = findPtr(peerId);

	//init the timer the first time
	if (peerPtr == NULL) {
		
		iterator itr = insert(peerId, OspfHelloPeer(this, peerId));
		if (itr == end()) { 
			ls_error ("Can't insert."); 
		}
		peerPtr = &((*itr).second);
	
	
	printf("OspfInactivityTimer MessageOut peerId %d\n",peerId);
	switch (type) {
	case OSPF_MSG_HELLO:
			peerPtr->inactivityTimer_.resched(peerPtr->routerDeadInterval_);
			
		break;
		
	default:
		// nothing, just to avoid compiler warning
		break;
		}
 	}
	return 0;
}


//**********************************************************************************
// messageIn: Called by OspfRouting,  when a hello message is received
//**********************************************************************************

int OspfInactivityManager::messageIn(int peerId, Ospf_message_type_t type)
{
	
	OspfHelloPeer* peerPtr = findPtr(peerId);
	if (peerPtr == NULL) {
		printf("perrPtr NULL\n");
		return 0;
	}
	
	printf("OspfInactivityTimer Messagein peerId %d\n",peerId);

	switch (type) {
	case OSPF_MSG_HELLO:	
			peerPtr->inactivityTimer_.resched(peerPtr->routerDeadInterval_);
			
			break;
	default:
		break;
	}
	
	return 0;
}


//**********************************************************************************
// cancelTimer: cancel ackTimer
//**********************************************************************************

void OspfInactivityManager::cancelTimer (int nbrId) 
{
	OspfHelloPeer* peerPtr = findPtr(nbrId);

	if (peerPtr == NULL)
		return;
	
	
	peerPtr->inactivityTimer_.force_cancel();
	
}


//**********************************************************************************
// setNeighbourDown: call to inactivityTimer event
//**********************************************************************************

int OspfInactivityManager::setNeighbourDown (int peerId) 
{
	int nodo=OspfRouting_.getnodeid();
	
	printf("NODO: %d\n",nodo);
	printf("PeerId: %d EXPIRE INACTIVITY TIMER\n",peerId);
	
	//the inactivity timer for peerId is fired
	OspfRouting_.inactivityTimerEvent(peerId);
	
	return 0;
}
 


//**********************************************************************************
// LsAgingManager  methods
//**********************************************************************************

//**********************************************************************************
// initTimeout method: set the delay time for the node 
//**********************************************************************************

void LsAgingManager ::initTimeout(int nodeId) 
{
		
	insert(nodeId,LSAPeer(this,nodeId,1));
}


//**********************************************************************************
// createLSA method: called by OspfRouting when a LSA is create or inserted in the
// database
//**********************************************************************************

int LsAgingManager ::createLSA (int peerId)
{ 
	LSAPeer* peerPtr = findPtr(peerId);
	
	if (peerPtr == NULL) {
		iterator itr = insert(peerId, LSAPeer(this, peerId));
		if (itr == end()) { 
			ls_error ("Can't insert."); 
		}
		peerPtr = &((*itr).second);
	}
	//schedule the timer
	peerPtr->LsTimer_.resched(peerPtr->LsAgingInterval_);
			 	
	return 0;
}

//**********************************************************************************
// cancelTimer method: cancel LsAging Timer
//**********************************************************************************

void LsAgingManager::cancelTimer (int nbrId) 
{
	LSAPeer* peerPtr = findPtr(nbrId);
	if (peerPtr == NULL) 
		return;

	peerPtr->LsTimer_.force_cancel();
	
}


//**********************************************************************************
// ageLSA method: do the LSA aging
//**********************************************************************************

int LsAgingManager::ageLSA (int nodeId) 
{
	int nodo=OspfRouting_.getnodeid();

	printf("[%f]Nodo:%d peerId: %d EXPIRE LSAGING TIMER\n",NOW,nodo,nodeId);
	
	
	LSAPeer* peerPtr = findPtr(nodeId);
	if (peerPtr == NULL) 
		return 0;
	
	//the lsAging timer for peerId is fired
	if(OspfRouting_.ageLSA (nodeId)){
		//reschedule the timer
		peerPtr->LsTimer_.resched(peerPtr->LsAgingInterval_);
	}	
	return 0;
}
 

//**********************************************************************************
// OspfNeighbourMap methods 
//**********************************************************************************

//**********************************************************************************
// initNeighbour method: initialize the structure 
//**********************************************************************************

void OspfNeighbourMap:: initNeighbour (LsNodeIdList* peerIdListPtr)
{
	OspfNeighbour ndata;
	
	for (LsNodeIdList::iterator itrList = peerIdListPtr->begin();
            itrList != peerIdListPtr->end(); itrList++){
		
		insert(*itrList,ndata);
	}

}


//**********************************************************************************
// isState method: returns true if the neighbour nbId has the same state passed as 
// parameter or false in other case 
//**********************************************************************************

bool OspfNeighbourMap::isState (int nbId,StateN_type_t state)
{
	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::isState?: nbPtr null\n");
	
	if (nbPtr->state_==state) 
		return true;
	
	return false;
}


//**********************************************************************************
// setState method: set the state of the neighbour nbId to newState 
//**********************************************************************************

void OspfNeighbourMap::setState (int nbId, StateN_type_t newState)

{
	
	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setState: nbPtr null\n");
	
	nbPtr->state_=newState; 
}

//**********************************************************************************
// getState method: returns neighbour nbId state 
//**********************************************************************************

StateN_type_t OspfNeighbourMap::getState (int nbId)
{

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::isState?: nbPtr null\n");
	
	return (nbPtr->state_); 
		
}


//**********************************************************************************
// setNeighbourId method: set the neighbour id of the neighbour nbId 
//**********************************************************************************

void OspfNeighbourMap::setNeighbourId (int nbId)
{
	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setNeighbourId: nbPtr null\n");
	
	nbPtr->neighbourId_=nbId; 
	
}


//**********************************************************************************
// getNeighbourId method: returns neighbour nbId 
//**********************************************************************************

int OspfNeighbourMap::getNeighbourId (int nbId)
{
	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::getNeighbourId: nbPtr null\n");
	
	return (nbPtr->neighbourId_); 
	
}


//**********************************************************************************
// setOptions method: set neighbour nbId options 
//**********************************************************************************

void OspfNeighbourMap::setOptions(options_t op,int nbId)
{
	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setOptions: nbPtr null\n");
	
	nbPtr->options_=op; 
	
}


//**********************************************************************************
// getOptions method: returns neighbour nbId options 
//**********************************************************************************

options_t OspfNeighbourMap::getOptions(int nbId)
{
	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::getOptions: nbPtr null\n");
	
	return(nbPtr->options_); 
	
}
  
//**********************************************************************************
// setDDseq method:  set neighbour nbId DDseq to seq
//**********************************************************************************

void OspfNeighbourMap::setDDseq (int nbId, double seq){
	
	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setDDseq: nbPtr null\n");
	
	nbPtr->ddSeq_=seq; 
}
  
//**********************************************************************************
// incDDseq method:  increment neighbour nbId DDseq
//**********************************************************************************

void OspfNeighbourMap::incDDseq (int nbId){

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setDDseq: nbPtr null\n");
	
	if(nbPtr->ddSeq_==LS_INVALID_MESSAGE_ID) {
		//the first time set a new value never seen before (time of simulation)
		nbPtr->ddSeq_=NOW;
	} 

	nbPtr->ddSeq_+=1;
	printf("DDseq %f\n",nbPtr->ddSeq_);
}

//**********************************************************************************
// getDDseq method: returns neighbour nbId DDseq 
//**********************************************************************************

double OspfNeighbourMap::getDDseq(int nbId){

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::getDDseq: nbPtr null\n");

	return (nbPtr->ddSeq_);
}
  
//**********************************************************************************
// setMaster method: set neighbour nbId masterSlave bit to master 
//**********************************************************************************

void OspfNeighbourMap::setMaster (int nbId){

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setMaster: nbPtr null\n");
	
	//set the master/slave bit to Master
	nbPtr->masterSlave_=1;
}

//**********************************************************************************
// setSlave method: set neighbour nbId masterSlave bit to slave 
//**********************************************************************************

void OspfNeighbourMap::setSlave (int nbId) {

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setMaster: nbPtr null\n");
	
	//set the master/slave bit to Slave
	nbPtr->masterSlave_=0;


}

//**********************************************************************************
// getMasterSlave method: returns neighbour nbId masterSlave bit 
//**********************************************************************************

char OspfNeighbourMap::getMasterSlave (int nbId){

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setMaster: nbPtr null\n");
	
	return(nbPtr->masterSlave_);	
}


//**********************************************************************************
// delAllLists method: delete all neighbour's lists 
//**********************************************************************************

void OspfNeighbourMap::delAllLists (int nbId){

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::delallLists: nbPtr null\n");

	
	if(!nbPtr->lsHeaderList_.empty())
		nbPtr->lsHeaderList_.eraseAll();

	if(nbPtr->lsRetransmissionList_.empty()) {
	nbPtr->lsRetransmissionList_.eraseAll();
	}

	if(!nbPtr->lsRequestList_.empty()) {
	nbPtr->lsRequestList_.eraseAll();
	}

}


//**********************************************************************************
// setLinkStateHeaderList method: set the link state header list in the neighbour
// structure for the neighbour nbId
//**********************************************************************************

void OspfNeighbourMap::setLinkStateHeaderList (int nbId,LinkStateHeaderList& lshdrl){

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setLinkStateHeaderList: nbPtr null\n");
	nbPtr->lsHeaderList_=lshdrl;
}

//**********************************************************************************
// getLinkStateHeaderList method: returns the link state header list in the neighbour
// structure for the neighbour nbId
//**********************************************************************************

LinkStateHeaderList OspfNeighbourMap::getLinkStateHeaderList (int nbId){

	OspfNeighbour* nbPtr=findPtr(nbId);
	
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::getLinkStateHeaderList: nbPtr null\n");
	
	return nbPtr->lsHeaderList_;
}


//**********************************************************************************
// setLinkStateRetransList method: set the link state summary list in the neighbour
// structure for the neighbour nbId
//**********************************************************************************

void OspfNeighbourMap::setLinkStateRetransList (int nbId,OspfLinkStateList& lsl) {
	
	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setLinkStateRetransList: nbPtr null\n");
	
	nbPtr->lsRetransmissionList_=lsl;
}


//**********************************************************************************
// getLinkStateRetransList method: returns  the link state summary list in the neighbour
// structure for the neighbour nbId
//**********************************************************************************

OspfLinkStateList OspfNeighbourMap::getLinkStateRetransList (int nbId){
	
	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setLinkStateRetransList: nbPtr null\n");
	
	return (nbPtr->lsRetransmissionList_);
}

//**********************************************************************************
// lookupLSARetrans method: returns true if the LSA is in the retransmission list
// and false in other case
//**********************************************************************************

bool OspfNeighbourMap::lookupLSARetrans (int nbId,LinkStateHeader& lshdr){

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::lookupLSARetrans: nbPtr null\n");

	for(OspfLinkStateList::iterator itr=nbPtr->lsRetransmissionList_.begin();
	itr!=nbPtr->lsRetransmissionList_.end();itr++) {
		if((lshdr.LS_type_==(*itr).ls_hdr_.LS_type_)
		&&(lshdr.LS_ID_==(*itr).ls_hdr_.LS_ID_)
		&&(lshdr.advertising_router_==(*itr).ls_hdr_.advertising_router_)){
			return true;
		}		
	}
	return false;
}


//**********************************************************************************
// delLSARetrans method: remove LSA from Retransmission list
//**********************************************************************************

bool OspfNeighbourMap::delLSARetrans (int nbId, LinkStateHeader& lshdr){
OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::delLSARetrans: nbPtr null\n");

	if(!nbPtr->lsRetransmissionList_.empty()){
	
	for(OspfLinkStateList::iterator itr=nbPtr->lsRetransmissionList_.begin();
	itr!=nbPtr->lsRetransmissionList_.end();itr++) {
		if((lshdr.LS_type_==(*itr).ls_hdr_.LS_type_)
		&&(lshdr.LS_ID_==(*itr).ls_hdr_.LS_ID_)
		&&(lshdr.advertising_router_==(*itr).ls_hdr_.advertising_router_)){
			nbPtr->lsRetransmissionList_.erase(itr);	
			return true;		
		}		
	}
	
	}

	return false;
}

//**********************************************************************************
// addLSARetrans method: add LSA to Retransmission list
//**********************************************************************************

void OspfNeighbourMap::addLSARetrans (int nbId, OspfLinkState& ls){


	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::addLSARetrans: nbPtr null\n");

	nbPtr->lsRetransmissionList_.push_back(ls);

}

//**********************************************************************************
// setRequestList method: set the link state request list in the neighbour
// structure for the neighbour nbId
//**********************************************************************************

void OspfNeighbourMap::setRequestList (int nbId, LinkStateRecordIdList& lsr) {

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::setRequestList: nbPtr null\n");
	
	nbPtr->lsRequestList_=lsr;
}


//**********************************************************************************
// getRequestList method: returns the link state request list in the neighbour
// structure for the neighbour nbId
//**********************************************************************************

LinkStateRecordIdList OspfNeighbourMap::getRequestList (int nbId){

	OspfNeighbour* nbPtr=findPtr(nbId);
	
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::getRequestList: nbPtr null\n");
	
	return nbPtr->lsRequestList_;
}


//**********************************************************************************
// delLSARequest method: returns true if the LSA is in the request list
// and false in other case
//**********************************************************************************

void OspfNeighbourMap::delLSARequest (int nbId, LinkStateHeader& lshdr){

	OspfNeighbour* nbPtr=findPtr(nbId);

	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::delLSARetrans: nbPtr null\n");

		
	if(!nbPtr->lsRequestList_.empty()){
		
		for(LinkStateRecordIdList::iterator itr=nbPtr->lsRequestList_.begin();
		itr!=nbPtr->lsRequestList_.end();itr++) {
				
			if((lshdr.LS_type_==(*itr).type_)
			&&(lshdr.LS_ID_==(*itr).lsID_)
			&&(lshdr.advertising_router_==(*itr).advertisingRouter_)){
				nbPtr->lsRequestList_.erase(itr);
				return;
					
			}		
		}
	}

}


//**********************************************************************************
// lookupLSAReq method: remove LSA from Request list
//**********************************************************************************

bool OspfNeighbourMap::lookupLSAReq (int nbId, LinkStateHeader& lshdr){

	OspfNeighbour* nbPtr=findPtr(nbId);
	if (nbPtr==NULL)
		ls_error("OspfNeighbourMap::lookupLSAReq: nbPtr null\n");

	for(LinkStateRecordIdList::iterator itr=nbPtr->lsRequestList_.begin();
	itr!=nbPtr->lsRequestList_.end();itr++) {
		if((lshdr.LS_type_==(*itr).type_)
		&&(lshdr.LS_ID_==(*itr).lsID_)
		&&(lshdr.advertising_router_==(*itr).advertisingRouter_)){
			return true;
		}		
	}
	return false;

}

//**********************************************************************************
// OspfRouting methods
//**********************************************************************************

//**********************************************************************************
// init method: initialize the routing structure
//**********************************************************************************

bool OspfRouting::init(OspfNode * nodePtr) 
{
	
	if (nodePtr == NULL) 
		return false;
	
	myNodePtr_ = nodePtr;
	myNodeId_ = myNodePtr_->getNodeId();

	//my peers
	peerIdListPtr_ = myNodePtr_->getPeerIdListPtr();
	//my link states
	linkStateListPtr_ = myNodePtr_->getLinkStateListPtr();
	//hello interval: defined in tcl
	helloInterval_=myNodePtr_->getHelloInterval();
	//router dead interval: defined in tcl
	routerDeadInterval_=myNodePtr_->getRouterDeadInterval();
	//create my own link state
	OspfLinkState ls;
	ls.ls_hdr_.LSage_=0;
	ls.ls_hdr_.options_=options_t(1,0);
	ls.ls_hdr_.LS_type_=LS_ROUTER;
	ls.ls_hdr_.LS_ID_=myNodeId_;
	ls.ls_hdr_.LS_sequence_num_=0;
	ls.ls_hdr_.advertising_router_=myNodeId_;	
	ls.RouterLinkStateListPtr_=linkStateListPtr_;
	
	linkStateDatabase_.setNodeId(myNodeId_);
	//at first, the topo data base only has my own links' states
	if (linkStateListPtr_ != NULL) 
		linkStateListAdvPtr_=linkStateDatabase_.insertLinkState(myNodeId_,ls);
	
	
	//initialize delay map:
	delayMapPtr_ = myNodePtr_->getDelayMapPtr();
	if (delayMapPtr_ != NULL) {
		//initialize ack timer length
		AckManager_.initTimeout(delayMapPtr_);
	 }
	
	//initialize neighbour's structure
	neighbourData_.initNeighbour (peerIdListPtr_);
	//initialize inactivity timer length
	InactivityManager_.initTimeout(peerIdListPtr_,routerDeadInterval_);
	//initialize lsAging timer
	AgingManager_.initTimeout(myNodeId_);
	//active lsAging timer
	AgingManager_.createLSA (myNodeId_);
			
	return true;
}  

//**********************************************************************************
// linkStateChanged method: check and update the link state database  
//**********************************************************************************

void OspfRouting::linkStateChanged () 
{
	if (linkStateListPtr_ == NULL)
		ls_error("OspfRouting::linkStateChanged: linkStateListPtr null\n");
   	
	OspfLinkStateList* oldLsAdvPtr = linkStateDatabase_.findPtr(myNodeId_);
	
	if (oldLsAdvPtr == NULL) 
		// Should never happen,something's wrong, we didn't 
		// initialize properly
		ls_error("OpfRouting::linkStateChanged: oldLsAdvPtr null!!\n");

	OspfLinkState lsad = oldLsAdvPtr->front();
	
	//get the new linkStateList
	linkStateListPtr_=myNodePtr_->getLinkStateListPtr();
	
	printf("LINKSTATECHANGED\n");
	OspfLinkState ls;
	ls.ls_hdr_.LSage_=0;
	ls.ls_hdr_.options_=options_t(1,0);
	ls.ls_hdr_.LS_type_=LS_ROUTER;
	ls.ls_hdr_.LS_ID_=myNodeId_;
	ls.ls_hdr_.LS_sequence_num_=lsad.ls_hdr_.LS_sequence_num_;
	ls.ls_hdr_.advertising_router_=myNodeId_;	
	ls.RouterLinkStateListPtr_=linkStateListPtr_;
	//erase my old link state advertisement
	linkStateListAdvPtr_->eraseAll();
	linkStateListAdvPtr_->push_back(ls);
	
	
	// if there's any changes, compute new routes and send link states
	linkStateDatabase_.update(myNodeId_, *linkStateListAdvPtr_);
	 computeRoutes(); //a changed has happened: compute routes
	//send LSA changed
	sendLSAToAdyacents(LS_ROUTER ,myNodeId_,myNodeId_,myNodeId_);

		
}

//**********************************************************************************
// interfaceChanged method: update the active interfaces list  
//**********************************************************************************

void OspfRouting::interfaceChanged (){

	peerIdListPtr_ = myNodePtr_->getPeerIdListPtr();
}


//**********************************************************************************
// sendHellos method: send hello packets to every peer node  
//**********************************************************************************

bool OspfRouting::sendHellos() 
{
	
	if (myNodePtr_ == NULL)
		return false;
	if ((peerIdListPtr_ == NULL) || peerIdListPtr_->empty())
		return false;

	printf("[%f] SEND HELLOS:Node:%d\n",NOW,myNodeId_);
	
	//create and store a new hello message
	OspfMessage* msgPtr = msgctr().newMessage(myNodeId_, OSPF_MSG_HELLO);
	
	if (msgPtr == NULL) 
		return false; // can't get new message
		
	u_int32_t msgId = msgPtr->messageId_;
	HelloPacket hellopkt = getHelloPkt();
	
	HelloPacket* packetPtr = new HelloPacket(hellopkt);
	if (packetPtr == NULL) {
		ls_error ("Can't get new hello packet, in OspfRouting::sendHellos\n");
		// can't get new link state list
		msgctr().deleteMessage(msgId,OSPF_MSG_HELLO);
		return false;
	}

	msgPtr->HelloPacketPtr_ = packetPtr;

	
	printf("Send hellos: LISTA VECINOS Nodo %d:\n",myNodeId_);
	for (LsNodeIdList::iterator itrList = hellopkt.neighbourListPtr_->begin();
	     itrList != hellopkt.neighbourListPtr_->end(); itrList++) {
		printf("Nodo vecino: %d\n",*itrList);
	}
	
	//get the ospf header	
	hdr_Ospf hdrOspf =getOspfHeader (OSPF_MSG_HELLO,msgId);
	
	for (LsNodeIdList::iterator itr = peerIdListPtr_->begin();
	     itr != peerIdListPtr_->end(); itr++) {

		if(neighbourData_.isState((*itr),DOWN)) {
			neighbourData_.setState((*itr),ATTEMPT);
			InactivityManager_.messageOut((*itr),hdrOspf.type_);
		        
		}
		
		  myNodePtr_->sendMessage((*itr), hdrOspf);	
		
	}

	return true;
}


//**********************************************************************************
// sendDD method: send dd packets to neihgbourId  
//**********************************************************************************

void OspfRouting::sendDD (int neighbourId){

	printf("[%f] SEND DD Node:%d\n",NOW,myNodeId_);
	
	//create and store a new dd message
	OspfMessage* msgPtr = msgctr().newMessage(myNodeId_, OSPF_MSG_DD);
	
	if (msgPtr == NULL) 
		return; // can't get new message
		
	u_int32_t msgId = msgPtr->messageId_;
	DDPacket ddpkt = getDdPkt(neighbourId);
	

	DDPacket* packetPtr = new DDPacket(ddpkt);
	if (packetPtr == NULL) {
		ls_error ("Can't get new dd packet, in OspfRouting::sendDD\n");
		// can't get new link state list
		msgctr().deleteMessage(msgId,OSPF_MSG_DD);
		return;
	}
	
	msgPtr->DDPacketPtr_ = packetPtr;

	if(ddpkt.lsHeaderListPtr_!=NULL){
	//printf("Send DD: Ls sequence number DEL PAQUETE enviado por  Nodo %d:\n",myNodeId_);
	//printf("Ls secuence number: %f \n",ddpkt.DDSeqNumber_);
	for (LinkStateHeaderList::iterator itrList = ddpkt.lsHeaderListPtr_->begin();
	     itrList != ddpkt.lsHeaderListPtr_->end(); itrList++) {
		printf("Ls advertising: %d \n",(*itrList).advertising_router_);
	}
	}
	
	//get the ospf header	
	hdr_Ospf hdrOspf =getOspfHeader (OSPF_MSG_DD,msgId);
	if((neighbourData_.getMasterSlave (neighbourId))&&
	(!neighbourData_.isState(neighbourId,EX_START))){
 	//I'm the master and my neighbour is not EX_START: active ack retransmission timer for the
	// neighbour
	
	AckManager_.messageOut(neighbourId,*msgPtr,hdrOspf.type_);
	}

	//send DD message
	myNodePtr_->sendMessage(neighbourId, hdrOspf);	
	

}


//**********************************************************************************
// sendRequest method: send request packets to neihgbourId  
//**********************************************************************************

void OspfRouting::sendRequest (int neighbourId) {

	printf("[%f] SEND Request Node:%d\n",NOW,myNodeId_);
	
	//create and store a new request message
	OspfMessage* msgPtr = msgctr().newMessage(myNodeId_, OSPF_MSG_REQUEST);
	
	if (msgPtr == NULL) 
		return; // can't get new message
		
	u_int32_t msgId = msgPtr->messageId_;
	RequestPacket reqpkt = getRequestPkt(neighbourId);
	

	RequestPacket* packetPtr = new RequestPacket(reqpkt);
	if (packetPtr == NULL) {
		ls_error ("Can't get new request packet, in OspfRouting::sendRequest\n");
		// can't get new link state list
		msgctr().deleteMessage(msgId,OSPF_MSG_REQUEST);
		return;
	}

        //
	/*if(reqpkt.linkStateRecordIdListPtr_!=NULL){
	printf("Send Request: enviado por nodo:%d:\n",myNodeId_);
	for (LinkStateRecordIdList::iterator itrList = reqpkt.linkStateRecordIdListPtr_->begin();
	     itrList != reqpkt.linkStateRecordIdListPtr_->end(); itrList++) {
		printf("Ls type:%d Lsid:%d advertising router:%d \n",(*itrList).type_,(*itrList).lsID_,
		(*itrList).advertisingRouter_);
		}
	}*/
	//

	msgPtr->RequestPacketPtr_ = packetPtr;
	hdr_Ospf hdrOspf =getOspfHeader (OSPF_MSG_REQUEST,msgId);
	
	//Active ack retransmission timer for the neighbour
	AckManager_.messageOut(neighbourId,*msgPtr,hdrOspf.type_);
	

	//send REQUEST message
	myNodePtr_->sendMessage(neighbourId, hdrOspf);	
}


//**********************************************************************************
// sendUpdate method: send update packet to neihgbourId  
//**********************************************************************************

void OspfRouting::sendUpdate(int originNodeId, int neighbourId,UpdatePacket& updpkt){

	printf("[%f] SEND Update Node:%d\n",NOW,myNodeId_);
	
	//create and store a new update message
	OspfMessage* msgPtr = msgctr().newMessage(originNodeId, OSPF_MSG_UPDATE);
	
	if (msgPtr == NULL) 
		return; // can't get new message
		
	u_int32_t msgId = msgPtr->messageId_;
	
	UpdatePacket* packetPtr = new UpdatePacket(updpkt);
	if (packetPtr == NULL) {
		ls_error ("Can't get new request packet, in OspfRouting::sendUpdate\n");
		// can't get new link state list
		msgctr().deleteMessage(msgId,OSPF_MSG_UPDATE);
		return;
	}

        //PRUEBA
	/*
	if(updpkt.LsListAdvertPtr_!=NULL){
	printf("Send Update: enviado por nodo:%d:\n",myNodeId_);
	printf("Numero de advertisement:%d:\n",updpkt.numberAdvert_);

	for (OspfLinkStateList::iterator itrList = updpkt.LsListAdvertPtr_->begin();
	     itrList != updpkt.LsListAdvertPtr_->end(); itrList++) {
		printf("advertising router:%d \n",(*itrList).ls_hdr_.advertising_router_);
		printf("lsage: %f\n",(*itrList).ls_hdr_.LSage_);
			
		for (RouterLinkStateList::iterator itrList2 = (*itrList).RouterLinkStateListPtr_->begin();
	     	itrList2 != (*itrList).RouterLinkStateListPtr_->end(); itrList2++) {
		printf("Link id:%d \n",(*itrList2).Link_ID_);
		printf("State: %d\n",(*itrList2).state_);

			for (MTLinkList::iterator itrList3 = (*itrList2).MTLinkList_.begin();
             itrList3 != (*itrList2).MTLinkList_.end(); itrList3++){
		
		printf("Mtid: %d Coste: %d\n",(*itrList3).mtId_,(*itrList3).metric_);
			}
		}

		}
	}*/
	//

	msgPtr->UpdatePacketPtr_ = packetPtr;
	hdr_Ospf hdrOspf =getOspfHeader (OSPF_MSG_UPDATE,msgId);

	//Active ack retransmission timer for the neighbour
	AckManager_.messageOut(neighbourId,*msgPtr,hdrOspf.type_);
	

	//send UPDATE message
	myNodePtr_->sendMessage(neighbourId, hdrOspf);	
	
}

//**********************************************************************************
// sendAck method: send acknowledge packet to neihgbourId  
//**********************************************************************************

void OspfRouting::sendAck (int neighbourId, int originNodeIdAck, LinkStateHeaderList& lshdrl){

	printf("[%f] SEND Ack Node:%d\n",NOW,myNodeId_);
	
	//create and store a new dd message
	OspfMessage* msgPtr = msgctr().newMessage(originNodeIdAck, OSPF_MSG_ACK);
	
	if (msgPtr == NULL) 
		return; // can't get new message
	
	AckPacket ackpkt;	
	u_int32_t msgId = msgPtr->messageId_;
	
	LinkStateHeaderList* lshdrlPtr= new LinkStateHeaderList (lshdrl);
	ackpkt.lsHeaderListPtr_=lshdrlPtr;
	AckPacket* packetPtr = new AckPacket(ackpkt);
	if (packetPtr == NULL) {
		ls_error ("Can't get new request packet, in OspfRouting::sendUpdate\n");
		// can't get new link state list
		msgctr().deleteMessage(msgId,OSPF_MSG_ACK);
		return;
	}

	msgPtr->AckPacketPtr_ = packetPtr;
	hdr_Ospf hdrOspf =getOspfHeader (OSPF_MSG_ACK,msgId);

	
	//send UPDATE message
	myNodePtr_->sendMessage(neighbourId, hdrOspf);	
}

//**********************************************************************************
// getHelloPkt method: get the especific hello packet's fields 
//**********************************************************************************

HelloPacket OspfRouting:: getHelloPkt()
{
	HelloPacket pkt;
	pkt.networkMask_=NETWORK_MASK_DEFAULT;
	pkt.helloInterval_=helloInterval_;
	pkt.options_= options_t(1,0); // set tos routing capability
	pkt.deadInterval_=routerDeadInterval_;
	pkt.neighbourListPtr_=&neighbourIdList; 
	return pkt;
	
}


//**********************************************************************************
// getDdPkt method: get the especific dd packet's fields for neighbour nbId 
//**********************************************************************************

DDPacket OspfRouting::getDdPkt(int nbId) {

	DDPacket pkt;
	StateN_type_t state= neighbourData_.getState (nbId);
	
	if(state==EX_START) {
	// get an empty DDpacket with I,M,MS bits activados
	pkt.options_=neighbourData_.getOptions (nbId);
	char bitMS=neighbourData_.getMasterSlave (nbId);
	pkt.IMMSbits_=IMMS_t(1,1,bitMS);
	pkt.DDSeqNumber_=neighbourData_.getDDseq (nbId);
	pkt.lsHeaderListPtr_=NULL;
	
        }
	if(state==EXCHANGE) {
	printf("getddpkt EXCHANGE\n");
	pkt.options_=neighbourData_.getOptions (nbId);
	char bitMS=neighbourData_.getMasterSlave (nbId);
	pkt.IMMSbits_=IMMS_t(0,0,bitMS);
	pkt.DDSeqNumber_=neighbourData_.getDDseq (nbId);
	LinkStateHeaderList lshrdl = neighbourData_.getLinkStateHeaderList (nbId);
		
	LinkStateHeaderList* lshdrptr = new LinkStateHeaderList(lshrdl);
	pkt.lsHeaderListPtr_=lshdrptr;
	
	}

	return pkt;
}


//**********************************************************************************
// getRequestPkt method: get the especific request packet's fields for neighbour nbId 
//**********************************************************************************

RequestPacket OspfRouting::getRequestPkt (int nbId) {

	RequestPacket pkt;
	StateN_type_t state= neighbourData_.getState (nbId);

	if((state==EXCHANGE)||(state==LOADING)){
	//get the  link state advertisment to request

	LinkStateRecordIdList lsr= neighbourData_.getRequestList (nbId);
	LinkStateRecordIdList* lsrptr = new LinkStateRecordIdList(lsr);
	pkt.linkStateRecordIdListPtr_=lsrptr;
			
	}
	
	else	//We should never get in here
	     ls_error ("No packet to request\n");
	
	return pkt;

}

//**********************************************************************************
// getOspfHeader method: create an ospf header for a new packet of the type 
// indicated as parameter  
//**********************************************************************************

hdr_Ospf OspfRouting::getOspfHeader ( Ospf_message_type_t type, int msgId){

	hdr_Ospf hdr;
	hdr.version_=2;
	hdr.type_=type;
	hdr.packet_len_=getLength(type);
	hdr.router_id_=myNodeId_;
	hdr.msgId_=msgId;
	return hdr;
}

//**********************************************************************************
// getLength method: returns packet's length taking into account the type  
//**********************************************************************************

int OspfRouting::getLength (Ospf_message_type_t type) {
	
	int len;
switch (type) {

	case OSPF_MSG_INVALID:
		ls_error("OSPF_MSG_INVALID "
			 "OspfRouting::getLength.\n");
		break;
	case OSPF_MSG_HELLO:
		len= OSPF_HEADER_SIZE+OSPF_HELLO_MESSAGE_SIZE;
		break;

	case OSPF_MSG_DD:
		len= OSPF_HEADER_SIZE+OSPF_DD_MESSAGE_SIZE;
		break;

	case OSPF_MSG_REQUEST:
		len= OSPF_HEADER_SIZE+OSPF_REQUEST_MESSAGE_SIZE;
		break;

	case OSPF_MSG_UPDATE:
		len= OSPF_HEADER_SIZE+OSPF_UPDATE_MESSAGE_SIZE;
		break;

	case OSPF_MSG_ACK:
		len= OSPF_HEADER_SIZE+OSPF_ACK_MESSAGE_SIZE;
		break;

	default: 
		len=OSPF_DEFAULT_MESSAGE_SIZE;
		break;
	}
	
	return len;
}


//**********************************************************************************
// receiveMessage method: returns true if there's a need to re-compute routes  
//**********************************************************************************

bool OspfRouting::receiveMessage (int senderId, u_int32_t msgId,Ospf_message_type_t type)
{
	if (senderId == LS_INVALID_NODE_ID)
		return false;

	OspfMessage* msgPtr = msgctr().retrieveMessagePtr(msgId,type);
	
	if (msgPtr == NULL)
		return false;
	
	bool retCode = false;
	
	
	// A switch statement to see the type.
	// and handle differently
	switch (type){
	case OSPF_MSG_HELLO:
		retCode = receiveHello(senderId,msgPtr);
		break;
	
	case OSPF_MSG_DD:
		retCode = receiveDD (senderId, msgPtr);
		break;
	case OSPF_MSG_REQUEST: 
		retCode = receiveRequest (senderId, msgPtr);
		break;
	case OSPF_MSG_UPDATE:
		retCode = receiveUpdate (senderId, msgPtr); 
		break;
	case OSPF_MSG_ACK: 
		receiveAck(senderId, msgPtr);
		msgctr().deleteMessage(msgId,OSPF_MSG_ACK);
		break;
	default:
		break;
	}
	return retCode;
}

//**********************************************************************************
// receiveHello method: returns true if there's a need to re-compute routes  
//**********************************************************************************

bool OspfRouting::receiveHello (int neighbourId, OspfMessage* msgPtr)
{
	
	printf(" [%f] Node: %d RECEIVE Hello from node:%d\n",NOW,myNodeId_,neighbourId);
	
	if (msgPtr == NULL)
		return false;
	
	bool retCode = false;
		
	
	// check routerDeadInterval and HelloInterval values
	 double hello_interval=msgPtr->HelloPacketPtr_->helloInterval_;
	 double router_dead_interval=msgPtr->HelloPacketPtr_->deadInterval_;
	if((hello_interval!=helloInterval_)||(router_dead_interval!=routerDeadInterval_))
		return false;
	
	//STORE NEIGHBOUR's DATA
	//set neighbourid
	neighbourData_.setNeighbourId(neighbourId);
	//set options
	options_t op=msgPtr->HelloPacketPtr_->options_;
	neighbourData_.setOptions(op,neighbourId);
	op=neighbourData_.getOptions(neighbourId);
	printf("OPTIONS NEIGHBOUR T:%d E:%d\n",op.bit_T,op.bit_E); 
 
		
	//add neighbour to the neighbours list if necessary
	addNeighbourToList(neighbourId);
	
	printf("LISTA VECINOS DEL NODO:%d:\n",myNodeId_);
		for (LsNodeIdList::iterator itrList = neighbourIdList.begin();
	     	itrList != neighbourIdList.end(); itrList++) {
		printf("Nodo vecino: %d\n",*itrList);
		}

	// change the Router LSA state to up
	printf("NEIGHBOURID :%d\n",neighbourId);
	changeRouterLSAState(myNodeId_,neighbourId,LS_STATUS_UP);
	
	//GENERATING EVENTS
	//hello received event
	retCode=helloReceivedEvent(neighbourId);
 	
	//check if 2-way conectivity
	if(isTwoWayConectivity(msgPtr)) {

	//we are simulating point to point networks so the router seen is already adyacent
	//add the new adyacent router to the adyacentsIdList_
		addAdyacentToList(neighbourId);
		
		printf("LISTA ADYACENTES DEL NODO:%d\n",myNodeId_);
		for (LsNodeIdList::iterator itrList = adyacentsIdList.begin();
	     		itrList != adyacentsIdList.end(); itrList++) {
			printf("Nodo vecino: %d\n",*itrList);
			
		}
	// 2way conectivity event
		retCode=twoWayReceivedEvent (neighbourId);

	}
	//there's not 2 way conectivity
	else 
		//1-wayReceived event  
		retCode=oneWayReceivedEvent (neighbourId);
	
	return retCode;
}

//**********************************************************************************
// receiveDD method: returns true if there's a need to re-compute routes  
//**********************************************************************************


bool OspfRouting::receiveDD (int neighbourId, OspfMessage* msgPtr){
	
	
	printf("RECEIVE DD from node:%d\n",neighbourId);
	
	if (msgPtr == NULL)
		return false;
		
	bool retCode = false;
		
	if((neighbourData_.getMasterSlave(neighbourId))
	&&(!neighbourData_.isState(neighbourId,EX_START))){
 	//I'm the master:
		
		AckManager_.messageIn(neighbourId,*msgPtr,OSPF_MSG_DD);
	}

	StateN_type_t state= neighbourData_.getState (neighbourId);
	
	if ((state==ATTEMPT)||(state==DOWN)) {
		
		return false;
	}

	if (state==INIT) {
	
	// 2way conectivity event
	retCode=twoWayReceivedEvent (neighbourId);
	//add neighbour to adyacent list
	addAdyacentToList (neighbourId);
	state=neighbourData_.getState (neighbourId);
	}
	
	if(state==TWO_WAY){
		
		return false;	
			
	}
	
	if(state==EX_START){
			
		if(rolesDistributed(neighbourId,msgPtr)){
			return NegDoneEvent(neighbourId);		
		}
		
		return false;
	}
	
	if(state==EXCHANGE) {
		printf("EXCHANGE\n");
						
		if(isSeqNumberMismatch (neighbourId,msgPtr)){
			printf("Seq Number Mismatch\n");
			
			retCode=SeqNumberMismatchEvent (neighbourId);
			return retCode;
		}

		if(isPacketDuplicated (neighbourId,msgPtr)){
		//deprecate the packet or resend
			printf("Packet duplicated\n");
			return managePacketDuplicated(neighbourId,msgPtr);
		}

		if(isNextPacketSeq(neighbourId,msgPtr))
			return processPacket(neighbourId,msgPtr);
				
	}
	
	//else: SeqNumberMismatchEvent
		return SeqNumberMismatchEvent (neighbourId);
	
	return retCode;	
}

//**********************************************************************************
// receiveRequest method: returns true if there's a need to re-compute routes  
//**********************************************************************************

bool OspfRouting::receiveRequest (int neighbourId, OspfMessage* msgPtr){
	
	printf("Node: %d RECEIVE REQUEST from node:%d\n",myNodeId_,neighbourId);
	if (msgPtr == NULL)
		return false;
		
	bool retCode = false;
	StateN_type_t state= neighbourData_.getState (neighbourId);
	OspfLinkState ls;
	OspfLinkStateList lsl;
	UpdatePacket updpkt;
	int numadv=0;

	if((state==EXCHANGE)||(state==LOADING)||(state==FULL)){
		RequestPacket* reqpkt=msgPtr->RequestPacketPtr_;
		 
		for (LinkStateRecordIdList::iterator itrList = reqpkt->linkStateRecordIdListPtr_->begin();
	     itrList != reqpkt->linkStateRecordIdListPtr_->end(); itrList++) {
		printf("Ls type:%d Lsid:%d advertising router:%d \n",(*itrList).type_,(*itrList).lsID_,
		(*itrList).advertisingRouter_);
			//for each link state record Id
			if(linkStateDatabase_.lookupLSA((*itrList).type_,(*itrList).lsID_,
			(*itrList).advertisingRouter_)) {
				//exists the link state advertisement in the database
			 	ls=linkStateDatabase_.getLSA((*itrList).type_,(*itrList).lsID_,
				(*itrList).advertisingRouter_);
				//get the link state advertisement
				lsl.push_back(ls);
				//push into link state list
				neighbourData_.addLSARetrans (neighbourId,ls);
				//add new LSA to retransmission list
				
				
			}
			else {
				printf("ERROR:El LSA no esta en la base de datos\n");
			        return BadLSReqEvent(neighbourId);	
				
			}
	
		  	numadv++;	
		}    
	
	updpkt.numberAdvert_=numadv; 
	OspfLinkStateList* ladvPtr_ = new OspfLinkStateList(lsl); 
	updpkt.LsListAdvertPtr_=ladvPtr_;
	sendUpdate(myNodeId_,neighbourId,updpkt);
	
	}

	else {
		//deprecate the packet
	}
	
	return retCode;
}


//**********************************************************************************
// receiveUpdate method: returns true if there's a need to re-compute routes  
//**********************************************************************************

bool OspfRouting::receiveUpdate (int neighbourId, OspfMessage* msgPtr){

	printf("Node: %d RECEIVE UPDATE from node:%d\n",myNodeId_,neighbourId);
	if (msgPtr == NULL)
		return false;

	bool retCode=false;
	
	retCode_t res;
	
	UpdatePacket* updpkt= msgPtr->UpdatePacketPtr_;
	StateN_type_t state= neighbourData_.getState (neighbourId);
	int originNodeId=msgPtr->originNodeId_;
	if(state<EXCHANGE) {
		return false;
	}
	
	//for each LSA
	for (OspfLinkStateList::iterator itrList = updpkt->LsListAdvertPtr_->begin();
	     	itrList != updpkt->LsListAdvertPtr_->end(); itrList++) {
		if(!isLsTypeValid((*itrList).ls_hdr_.LS_type_)){
			//LStype is not valid: discard the LSA, get the next	
			continue;
		}
		//lookup the LSA in the database
		res=linkStateDatabase_.lookupLSA((*itrList).ls_hdr_);
		
		//PRUEBA
		/*printf("LS TYPE: %d\n",(*itrList).ls_hdr_.LS_type_);
		printf("LS ID: %d\n",(*itrList).ls_hdr_.LS_ID_);
		printf("advertising router: %d\n",(*itrList).ls_hdr_.advertising_router_);
	
	for (RouterLinkStateList::iterator itrList3 =(*itrList).RouterLinkStateListPtr_->begin();
            itrList3 != (*itrList).RouterLinkStateListPtr_->end(); itrList3++){

		printf("Nodo vecino: %d ",(&(*itrList3))->Link_ID_);

		printf("State %d\n",(*itrList3).state_);
		for (MTLinkList::iterator itrList2 = (&(*itrList3))->MTLinkList_.begin();
             itrList2 != (&(*itrList3))->MTLinkList_.end(); itrList2++){
		
		printf("Mtid: %d Coste: %d\n",(&(*itrList2))->mtId_,(&(*itrList2))->metric_);

		}	
	}*/	
		//	

		if(((*itrList).ls_hdr_.LSage_==MAX_AGE)&&(res==NO_EXIST)){
			printf("lsage=MAX AGE or no exist\n");
			//send ack:
			LinkStateHeaderList lshdrl;
			lshdrl.push_back((*itrList).ls_hdr_);
			sendAck(neighbourId,originNodeId,lshdrl);
			
			//remove outstanding request for equal o previous instances of LSA from
			// neighbour request list
			neighbourData_.delLSARequest(neighbourId,(*itrList).ls_hdr_);	
			if((state==EXCHANGE)||(state==LOADING)){
				//insert lsa in the database
				u_int32_t adv_router=(*itrList).ls_hdr_.advertising_router_;

				linkStateDatabase_.insertLinkState(adv_router,
				(*itrList));
	
				//active Lsaging timer
				AgingManager_.createLSA(adv_router);
				continue;
			}
			//else
			//deprecate the LSA

				continue;
		}
		//else
		//find the instance of LSA in the database
		
		LS_type_t lstypeN=(*itrList).ls_hdr_.LS_type_;
		u_int32_t lsidN=(*itrList).ls_hdr_.LS_ID_;
		u_int32_t adv_routerN=(*itrList).ls_hdr_.advertising_router_;
		//get LSA copy in the database
		OspfLinkState ls=linkStateDatabase_.getLSA (lstypeN,lsidN,adv_routerN);
			
		if((res==NO_EXIST)||(res==NEWER)){
			printf("No exist or newer\n");	
	
			// flood the new LSA out of router's interfaces
			sendFlooding(neighbourId,(*itrList));
			
			//install the new LSA
			if(res==NO_EXIST){
				printf(" NO EXIST\n");
				//insert the new LSA
				linkStateDatabase_.insertLinkState
				(adv_routerN,(*itrList));
				
				//activate Lsaging timer
				AgingManager_.createLSA(adv_routerN);
				
				// compute the routing table
				computeRoutes();
				retCode=true;

			}

			else {
			printf("EXIST\n");
			//the LSA exists in the database
			//check if routing table must be recalculated
			retCode=linkStateDatabase_.updateRoutingTable
			(adv_routerN,(*itrList));
			//remove LSA copy from the database
			linkStateDatabase_.delLSA(ls.ls_hdr_.advertising_router_,
						   ls.ls_hdr_.LS_type_,
					           ls.ls_hdr_.LS_ID_,
					           ls.ls_hdr_.advertising_router_);	
			
			
			//insert new LSA into the database
			linkStateDatabase_.insertLinkState(
			adv_routerN,(*itrList));
			//activate LSaging timer
			AgingManager_.createLSA(adv_routerN);

				if(retCode){
					//routing table must be recalculated
					computeRoutes();
				}

			}//end else NO EXIST
			
			//send ack to node sender
			LinkStateHeaderList lshdrl;
			lshdrl.push_back((*itrList).ls_hdr_);
			sendAck(neighbourId,originNodeId,lshdrl);
			
			//check if self-originated LSA
			//in this case, no special action is required
						
			continue;				
		}//no exist or newer
		
		//there is an instance in of the LSA on the sending
		// neighbor’s Link state request list
		if(neighbourData_.lookupLSAReq (neighbourId,
							(*itrList).ls_hdr_)){				
			//an error has occurred
			return BadLSReqEvent(neighbourId);
		}

		//LSA received = LSA in database
		if(res==EQUALS){
			
			printf("[%f] EQUALS\n",NOW);
			//PRUEBA
		/*	printf("RETRANSMISSION LIST\n");
			OspfLinkStateList lsl=neighbourData_.getLinkStateRetransList(neighbourId);
		
		for (OspfLinkStateList::iterator itrList4 = lsl.begin();
	     	itrList4 != lsl.end(); itrList4++) {
			printf("LS TYPE: %d\n",(*itrList4).ls_hdr_.LS_type_);
			printf("LS ID: %d\n",(*itrList4).ls_hdr_.LS_ID_);
			printf("advertising router: %d\n",(*itrList4).ls_hdr_.advertising_router_);
		}*/
		//
			if(neighbourData_.lookupLSARetrans (neighbourId,(*itrList).ls_hdr_)){
								
				//the LSA is consider as implicit ack
				bool del=neighbourData_.delLSARetrans (neighbourId,(*itrList).ls_hdr_);
				//OspfLinkStateList lsRetran=neighbourData_.getLinkStateRetransList 
				//(neighbourId);
				UpdatePacket* upd=msgPtr->UpdatePacketPtr_;
				AckPacket ack=UpdateToAck(*upd);
				(*msgPtr).AckPacketPtr_=&ack;
				
				OspfLinkStateList lsl=neighbourData_.getLinkStateRetransList(neighbourId);
		
				if((del)&&(lsl.empty())){
				//some item is removed from the retransmission list
				// and this list is empty
				AckManager_.messageIn(neighbourId,*msgPtr,OSPF_MSG_ACK);
				}					
			}
			//send ack to node sender
			LinkStateHeaderList lshdrl;
			lshdrl.push_back((*itrList).ls_hdr_);
			sendAck(neighbourId,originNodeId,lshdrl);
				
			continue;
		}

		// LSA received is older than LSA in the database
		printf("the LSA received is older than LSA in the database\n");
		
		if((ls.ls_hdr_.LSage_==MAX_AGE)&&
		(ls.ls_hdr_. LS_sequence_num_==OSPF_WRAPAROUND_THRESHOLD)){
			//discard LSA without acknowledging
						
			printf("discard LSA\n");
			continue;
		}
			
		//add new LSA to retransmission list
		neighbourData_.addLSARetrans (neighbourId,ls);

		//Send LSA in the database to the neighbour
		OspfLinkStateList lsl;
		UpdatePacket updpktN;
		//ls.ls_hdr_.LSage_=(*itrList).ls_hdr_.LSage_;
		//ls.ls_hdr_. LS_sequence_num_=(*itrList).ls_hdr_.LS_sequence_num_;
		lsl.push_back(ls);
		updpktN.numberAdvert_=1;		 
		OspfLinkStateList* ladvPtr_ = new OspfLinkStateList(lsl); 
		updpktN.LsListAdvertPtr_=ladvPtr_;
		sendUpdate(myNodeId_,neighbourId,updpktN);
		
	
	}//end for
	
	//check if there is some LSA to request
	LinkStateRecordIdList reql=neighbourData_.getRequestList(neighbourId); 
	if((reql.empty())&&(state==LOADING)){
			printf("Node: %d PeerId: %d\n",myNodeId_,neighbourId);
			AckManager_.messageIn(neighbourId,*msgPtr,OSPF_MSG_UPDATE);
			LoadingDoneEvent(neighbourId);
	}	
	
	
	return retCode;
	
}

//**********************************************************************************
// receiveAck method: returns true if there's a need to re-compute routes  
//**********************************************************************************

bool OspfRouting::receiveAck (int neighbourId, OspfMessage* msgPtr){

	printf("Node: %d RECEIVE ACK from node:%d\n",myNodeId_,neighbourId);
	if (msgPtr == NULL)
		return false;

	bool retCode=true;
	AckPacket* ackpkt= msgPtr->AckPacketPtr_;
	StateN_type_t state= neighbourData_.getState (neighbourId);
	
	if(state<EXCHANGE){
		return false;
	}
	//PRUEBA
	/*printf("RETRANSMISSION LIST\n");
	OspfLinkStateList lsl=neighbourData_.getLinkStateRetransList(neighbourId);
	for (OspfLinkStateList::iterator itrList = lsl.begin();
	     	itrList != lsl.end(); itrList++) {
		printf("LS TYPE: %d\n",(*itrList).ls_hdr_.LS_type_);
		printf("LS ID: %d\n",(*itrList).ls_hdr_.LS_ID_);
		printf("advertising router: %d\n",(*itrList).ls_hdr_.advertising_router_);
	}*/
	//
	printf("ACK PACKET \n");
	
	//for each ack
	for (LinkStateHeaderList::iterator itrList = ackpkt->lsHeaderListPtr_->begin();
	     	itrList != ackpkt->lsHeaderListPtr_->end(); itrList++) {
		//PRUEBA	
		/*printf("LS TYPE: %d\n",(*itrList).LS_type_);
		printf("LS ID: %d\n",(*itrList).LS_ID_);
		printf("advertising router: %d\n",(*itrList).advertising_router_);*/
		//
	
		if(! neighbourData_.lookupLSARetrans (neighbourId,(*itrList))){
			//no exist the lsa in link state retransmission list for neighbourId
			printf("No exist lsa in retransmission list\n");
			
			continue;
		}
		//else
		bool del=neighbourData_.delLSARetrans (neighbourId,(*itrList));
		OspfLinkStateList lsl=neighbourData_.getLinkStateRetransList(neighbourId);
		
		if((del)&&(lsl.empty())){			
			//some item is removed from the retransmission list
			// and the retransmission list is empty	
			AckManager_.messageIn(neighbourId,*msgPtr,OSPF_MSG_ACK);
		}
		continue;	
	}
	
	return retCode;
}

//**********************************************************************************
// UpdateToAck method: converts an Update packet into a Ack packet  
//**********************************************************************************

AckPacket OspfRouting::UpdateToAck (UpdatePacket& updpkt){
	
	AckPacket ackpkt;
	LinkStateHeaderList lsHeaderList;
	
	for (OspfLinkStateList::iterator itrList = updpkt.LsListAdvertPtr_->begin();
	     itrList != updpkt.LsListAdvertPtr_->end(); itrList++){
		lsHeaderList.push_back((*itrList).ls_hdr_);
	}

	ackpkt.lsHeaderListPtr_=&lsHeaderList;
	return ackpkt;
}
//**********************************************************************************
// addNeighbourToList method: add neighbour to neighbours list if necessary  
//**********************************************************************************

void OspfRouting::addNeighbourToList (int nbId)
{
	
	for (LsNodeIdList::iterator itrList = neighbourIdList.begin();
	     itrList != neighbourIdList.end(); itrList++) {
		if (*itrList==nbId){
			return;
		}
	}

	neighbourIdList.push_back(nbId);
}

//**********************************************************************************
// delNeighbourFromList method: delete neighbour from neighbours list if exists  
//**********************************************************************************

void OspfRouting::delNeighbourFromList (int nbId){

	for (LsNodeIdList::iterator itrList = neighbourIdList.begin();
	     itrList != neighbourIdList.end(); itrList++) {
		if (*itrList==nbId) {
			 neighbourIdList.erase(itrList);
			return;
		}
	}
	
}	

//**********************************************************************************
// addAdyacentToList method: add adyacency to adyacent neighbours list if necessary  
//**********************************************************************************

void OspfRouting::addAdyacentToList (int nbId){

	for (LsNodeIdList::iterator itrList = adyacentsIdList.begin();
	     itrList != adyacentsIdList.end(); itrList++) {
		if (*itrList==nbId){
			return;
		}
	}

	adyacentsIdList.push_back(nbId);

}

//**********************************************************************************
// delAdyacentFromList method: delete and adyacency from adyacent neighbours list 
// if exists  
//**********************************************************************************

void OspfRouting::delAdyacentFromList (int nbId) {

	for (LsNodeIdList::iterator itrList = adyacentsIdList.begin();
	     itrList != adyacentsIdList.end(); itrList++) {
		if (*itrList==nbId) {
			 adyacentsIdList.erase(itrList);
			return;
		}
	}
	
  printf("No existe el adyacente:%d\n",nbId);
}

//**********************************************************************************
// helloReceivedEvent method: representing hello received event  
//**********************************************************************************
bool OspfRouting::helloReceivedEvent (int nbId)
{
	printf("Hello Received Event from Node:%d\n",nbId);
	bool retCode = false;
	StateN_type_t state= neighbourData_.getState (nbId);
	
	//set the new neighbour state and reactive 
	if ((state==ATTEMPT)||(state==DOWN)) {
		printf("ATTEMPT or DOWN\n");
		neighbourData_.setState(nbId,INIT);
		InactivityManager_.messageIn(nbId, OSPF_MSG_HELLO);	
		retCode=false;
	}
	
	if(state>=INIT) {
		printf("INIT or grater\n");
		InactivityManager_.messageIn(nbId,OSPF_MSG_HELLO);
		retCode=false;	
	}

	return retCode;
}


//**********************************************************************************
// isTwoWayConectivity method: returns true if two way conectivity and false in
// other case  
//**********************************************************************************

bool OspfRouting::isTwoWayConectivity (OspfMessage* msgPtr)
{ 
	HelloPacket* helloPktPtr=msgPtr->HelloPacketPtr_;
	LsNodeIdList* nbIdListPtr=helloPktPtr->neighbourListPtr_;
	for (LsNodeIdList::iterator itrList = nbIdListPtr->begin();
	     itrList != nbIdListPtr->end(); itrList++) {
		if (*itrList==myNodeId_){
			printf("hay doble conectividad\n");
			return true;
		}
	}
	
	return false;
}


//**********************************************************************************
// twoWayReceivedEvent method: representing 2-way received event  
//**********************************************************************************

bool OspfRouting::twoWayReceivedEvent (int nbId)
{
	printf("2-WayReceived Event from Node:%d\n",nbId);
	StateN_type_t state= neighbourData_.getState (nbId);
	
	if(state==INIT) {
		printf("INIT\n");
		
		//set the new neighbour state
		neighbourData_.setState(nbId,EX_START);
		//increment DDsequence
		neighbourData_.incDDseq (nbId);
		//set bit MasterSlave to master
		neighbourData_.setMaster (nbId);
		//send DD packets
		sendDD(nbId);
		return false;	
	}

	if (state>=TWO_WAY) {
		printf("2-WAY or grater\n");
		return false;
	}
	
	return false;
	
}

//**********************************************************************************
// oneWayReceivedEvent method: representing 1-way received event  
//**********************************************************************************

bool OspfRouting::oneWayReceivedEvent (int nbId) {


	printf("1-WayReceived Event from Node:%d\n",nbId);
	StateN_type_t state= neighbourData_.getState (nbId);
	
	if(state>=TWO_WAY) {
		printf("TWO_WAY\n");
		//set the new neighbour state
		neighbourData_.setState(nbId,INIT);
		//delete all lists for this neighbour
		neighbourData_.delAllLists(nbId);
		//delete nbId from adyacentsIdList
		delAdyacentFromList (nbId);
		return false;	
	}

	return false;

}

//**********************************************************************************
// inactivityTimerEvent method: representing inactivityTimer event  
//**********************************************************************************

void OspfRouting::inactivityTimerEvent (int nbId) {

		
	printf("[%f] NODE: %d INACTIVITY TIMER EVENT\n",NOW,myNodeId_);
		
	//set the new neighbour state
	neighbourData_.setState(nbId,DOWN);
	//delete all lists for this neighbour
	neighbourData_.delAllLists(nbId);
	//set the node role to slave
	neighbourData_.setSlave (nbId);
	//init DDseq for nbId
	neighbourData_.setDDseq(nbId, LS_INVALID_MESSAGE_ID);
	
	//delete nbId from adyacentsIdList
	delAdyacentFromList (nbId);
	//delete nbId from neighbourIdList
	delNeighbourFromList (nbId);

	//Cancel the ack timer for nbId
	AckManager_.cancelTimer(nbId);

	//change the Router LSA state to down
	changeRouterLSAState (myNodeId_,nbId,LS_STATUS_DOWN);
	
	// recompute routing table
	computeRoutes();
	
	
}

//**********************************************************************************
// sendLSAToAdyacents method: sends the database copy of the LSA indexed by the 
// parameters to all adyacents neighbours  except to no_dest_node
//**********************************************************************************

void OspfRouting::sendLSAToAdyacents(LS_type_t type ,int adv_router,int link_id, int no_dest_node){

	for (LsNodeIdList::iterator itrList =adyacentsIdList.begin();
	            itrList!= adyacentsIdList.end(); itrList++) {
		
	   if ((*itrList)==no_dest_node){
		continue;
	    }	
	    //get the linkstate advertisement whose link id= myNodeId
	    OspfLinkState ls=linkStateDatabase_.getLSA(LS_ROUTER,adv_router,link_id);
	    //add new LSA to retransmission list
	    neighbourData_.addLSARetrans ((*itrList),ls);
	    //increment LSage and sequence number
	    double* DelayPtr=delayMapPtr_->findPtr((*itrList));
	    double infTransDelay=*DelayPtr;
	    printf("LSAGE %f\n",ls.ls_hdr_.LSage_);
	    ls.ls_hdr_.LSage_+=infTransDelay;
	    printf("infTransDelay %f\n",infTransDelay);
	    printf("LSAGE %f\n",ls.ls_hdr_.LSage_);
	    //send update packet
	    UpdatePacket updpkt;
	    OspfLinkStateList lsl;
	    lsl.push_back(ls);
	    updpkt.numberAdvert_=1;		 
	    OspfLinkStateList* ladvPtr_ = new OspfLinkStateList(lsl); 
	    updpkt.LsListAdvertPtr_=ladvPtr_;
	    sendUpdate(myNodeId_,(*itrList),updpkt);
	}

}

//**********************************************************************************
// rolesDistributed method: returns true if roles master/slave have been 
// distributed and false in other case.  
//**********************************************************************************

bool OspfRouting::rolesDistributed(int nbId,OspfMessage* msgPtr){
		
	if(isSlave(nbId,msgPtr)||isMaster(nbId,msgPtr)){
		
		return true;
	}
	else
		return false;
}

//**********************************************************************************
// isSlave method: returns true if the router is the slave    
//**********************************************************************************

bool OspfRouting::isSlave(int nbId, OspfMessage* msgPtr){

	DDPacket* ddpkt=msgPtr->DDPacketPtr_;
	char bitI,bitM,bitMS;
	bitI=ddpkt->IMMSbits_.bit_I;
	bitM=ddpkt->IMMSbits_.bit_M;
	bitMS=ddpkt->IMMSbits_.bit_MS;
	LinkStateHeaderList* lsl=ddpkt->lsHeaderListPtr_;
	if((bitI==1)&&(bitM==1)&&(bitMS==1)&&(lsl==NULL)&&(nbId>myNodeId_)){
		neighbourData_.setSlave(nbId);
		neighbourData_.setDDseq(nbId,ddpkt->DDSeqNumber_);
		return true;
	}
	
	return false;
}

//**********************************************************************************
// isMaster method: returns true if the router is the master    
//**********************************************************************************

bool OspfRouting::isMaster(int nbId, OspfMessage* msgPtr){

	DDPacket* ddpkt=msgPtr->DDPacketPtr_;
	char bitI,bitMS;
	bitI=ddpkt->IMMSbits_.bit_I;
	bitMS=ddpkt->IMMSbits_.bit_MS;
	double ddseqpkt=ddpkt->DDSeqNumber_;
	double ddseqnb=neighbourData_.getDDseq(nbId);
		
	if((bitI==0)&&(bitMS==0)&&(nbId<myNodeId_)&&(ddseqpkt==ddseqnb)){
		neighbourData_.setMaster(nbId);
		neighbourData_.incDDseq (nbId);
		return true;
	}
	
	return false;
}


//**********************************************************************************
// NegDoneEvent method: representing NegotiationDone event  
//**********************************************************************************

bool OspfRouting::NegDoneEvent(int nbId){
	
	printf("NegDone Event from Node:%d\n",nbId);
	StateN_type_t state= neighbourData_.getState (nbId);
	

	if(state==EX_START) {
		printf("EX_START\n");
		//set the new neighbour state
		neighbourData_.setState(nbId,EXCHANGE);
		// list the contents of its link state database in the 
		//neighbor Database summary list
		OspfLinkStateList lsl;
		lsl=linkStateDatabase_.getLSList();
		OspfLinkStateList lslret;
		LinkStateHeaderList lshdrl; 
		
		
		for (OspfLinkStateList::iterator itrList =lsl.begin();
	            itrList!= lsl.end(); itrList++) {
			printf("Link id %d\n",(*itrList).ls_hdr_.LS_ID_);
				
			if((*itrList).ls_hdr_.LSage_==MAX_AGE){
				//add the ls advertisement to retransmission list
				lslret.push_back(*itrList);
			}
			else	{
				
				//add the ls header advertisment to summary list	
				lshdrl.push_back((*itrList).ls_hdr_);
			}
		}
		
		neighbourData_.setLinkStateHeaderList(nbId,lshdrl);
		neighbourData_.setLinkStateRetransList(nbId,lslret);
		sendDD(nbId);
			
	}

	return false;

	
}	

//**********************************************************************************
//  isSeqNumberMismatch method: returns false if every field match between the
// packet's fields and the values stored in the router, and false in other case  
//**********************************************************************************

bool OspfRouting::isSeqNumberMismatch (int neighbourId, OspfMessage* msgPtr){
	
	DDPacket* ddpktPtr=msgPtr->DDPacketPtr_;
	options_t op=neighbourData_.getOptions(neighbourId);
	char MS=neighbourData_.getMasterSlave(neighbourId);
	//if MSbit=1 in the packet, is because the sender neighbour is the master, so the MS bit
	//for that neighbour in the neighbour structure must be 0: I'm the slave.
	//
	
	//printf("isSeqNumberMismatch Nodo:%d Vecino:%d\n",myNodeId_,neighbourId);
	//printf("PACKETE: ms:%d\n, bit_t:%d, bit_e:%d, bit_I:%d\n",ddpktPtr->IMMSbits_.bit_MS,
	//ddpktPtr->options_.bit_T,ddpktPtr->options_.bit_E,ddpktPtr->IMMSbits_.bit_I);
	//printf("VECINO: ms:%d\n, bit_t:%d, bit_e:%d\n",MS,
	//op.bit_T,op.bit_E);
	
	
	if((ddpktPtr->IMMSbits_.bit_MS!=MS)&&
	(ddpktPtr->IMMSbits_.bit_I==0)){
		if((op.bit_T==ddpktPtr->options_.bit_T)&&(op.bit_E==ddpktPtr->options_.bit_E))
			return false;
		//else
			return true;
	}	
	return true;

}

//**********************************************************************************
// SeqNumberMismatchEvent: representing SeqNumberMismatch event  
//**********************************************************************************

bool OspfRouting::SeqNumberMismatchEvent (int nbId) {


	printf("SeqNumberMismatch Event from Node:%d\n",nbId);
	StateN_type_t state= neighbourData_.getState (nbId);
	
	
	if(state>=EXCHANGE) {
		printf("EXCHANGE or grater\n");
		//set the new neighbour state
		neighbourData_.setState(nbId,EX_START);
		//adjacency is torn down
		delAdyacentFromList (nbId);
		//cancel ack timer
		AckManager_.cancelTimer(nbId);
		//clear the lists:retransmission list, Database summary list and Link
                // state request list 
		neighbourData_.delAllLists(nbId);
		//increments the DD sequence number in the
                // neighbor data structure,
		neighbourData_.incDDseq(nbId);
		//declares itself master
		neighbourData_.setMaster(nbId);
                //starts sending Database Description Packets	
		sendDD(nbId);
		
	}
	
	return false;
}


//**********************************************************************************
//  isPacketDuplicated method: returns true if the packet arrived is duplicated
// and false in other case.  
//**********************************************************************************

bool OspfRouting::isPacketDuplicated (int nbId,OspfMessage* msgPtr){


	DDPacket* ddpktPtr=msgPtr->DDPacketPtr_;
	char MS=neighbourData_.getMasterSlave(nbId);
	double ddseq=neighbourData_.getDDseq (nbId);
	double ddseqpkt=ddpktPtr->DDSeqNumber_;


	printf("isPacketDuplicated Nodo:%d Vecino:%d\n",myNodeId_,nbId);
		
	//I'm the master
	if((MS==1)&&(ddseqpkt<ddseq)){
		return true;
	}
	//I'm the slave
		
	if((MS==0)&&(ddseqpkt==ddseq)){
		return true;
	}
	return false; 
}


//**********************************************************************************
//  managePacketDuplicated method: resend the packet if the router is the slave or
// deprecate it if the router is the master.  
//**********************************************************************************

bool OspfRouting::managePacketDuplicated(int neighbourId, OspfMessage* msgPtr){
	
	
	printf("managePacketDuplicated Nodo:%d Vecino:%d\n",myNodeId_,neighbourId);
		
	bool retCode=false;
	char MS=neighbourData_.getMasterSlave(neighbourId);
	//I'm the master
	if(MS==1){
		//deprecate the packet
		
	}
	//I'm the slave
	if(MS==0){
		
		//resend the old packet
		hdr_Ospf hdrOspf =getOspfHeader (OSPF_MSG_DD,msgPtr->messageId_);
		resendMessage(neighbourId,hdrOspf);
	}
	
	return retCode;	
}


//**********************************************************************************
//  isNextPacketSeq method: returns true if the packet arrived is the next in the
// sequence and false in other case.  
//**********************************************************************************

bool OspfRouting::isNextPacketSeq (int nbId,OspfMessage* msgPtr){

	DDPacket* ddpktPtr=msgPtr->DDPacketPtr_;
	char MS=neighbourData_.getMasterSlave(nbId);
	double ddseq=neighbourData_.getDDseq (nbId);
	double ddseqpkt=ddpktPtr->DDSeqNumber_;


	//I'm the master
	if((MS==1)&&(ddseqpkt==ddseq)){
		//
		//printf("Nodo actual master:%d, Nodo vecino:%d esclavo\n",myNodeId_,nbId); 
		//printf("PACKET: ddseq:%f\n",ddseqpkt);
		//printf("VECINO:%d ddseq:%f MS:%d\n",nbId,ddseq,MS);
		//
		return true;
	}
	//I'm the slave
	if((MS==0)&&(ddseqpkt>ddseq)){
		//
		//printf("Nodo actual esclavo:%d, Nodo vecino:%d master\n",myNodeId_,nbId); 
		//printf("PACKET: ddseq:%f\n",ddseqpkt);
		//printf("VECINO:%d ddseq:%f MS:%d\n",nbId,ddseq,MS);
		
		//
		return true;
	}
	return false; 
	
	
}

//**********************************************************************************
//  processPacket method: representing processing of the next sequence packet  
//**********************************************************************************

bool OspfRouting::processPacket(int neighbourId, OspfMessage* msgPtr) {
	
	printf("processPacket Nodo:%d Vecino:%d\n",myNodeId_,neighbourId);
	DDPacket* ddpktPtr=msgPtr->DDPacketPtr_; 
	LinkStateHeaderList* lslhdrPtr=ddpktPtr->lsHeaderListPtr_;
	bool retCode=false;
	LinkStateRecordIdList lsrecordl;
	retCode_t res;
	
	//I'm the slave: get link state to request
	for (LinkStateHeaderList::iterator itrList =lslhdrPtr->begin();
	            itrList!= lslhdrPtr->end(); itrList++) {
			
			//check the LStype for each LSA
			if(!isLsTypeValid((*itrList).LS_type_)){
				printf("El tipo LS type es incorrecto\n");
				return SeqNumberMismatchEvent(neighbourId);		
			}
			//else: Ls type valid
			// looks up the LSA in its database
			res=linkStateDatabase_.lookupLSA(*itrList);
			printf("RES %d\n",res);
			printf("LS ID %d\n",(*itrList).LS_ID_);
			if((res==NO_EXIST)||(res==NEWER)) {
					
			// it does not exist, or the LSA received is more recent
				LS_type_t type=(*itrList).LS_type_; // record type
				u_int32_t lsID=(*itrList).LS_ID_; //link state record id
				u_int32_t advertisingRouter=(*itrList).advertising_router_; //advertising 												router identifier
 				lsrecordl.push_back(LinkStateRecordId(type,lsID,advertisingRouter));
			}
			
		}
	

	if(!lsrecordl.empty()){
	neighbourData_.setRequestList(neighbourId,lsrecordl);
	}
	
	//if I'm the master
	if(neighbourData_.getMasterSlave(neighbourId)){
		printf("I'm the master\n");
		//increment DDsequence
		neighbourData_.incDDseq(neighbourId);
		if(ddpktPtr->IMMSbits_.bit_M==0){
			//the exchange has finished
			retCode=ExchangeDoneEvent(neighbourId);
			
		}
		else {
			sendDD(neighbourId);
		}
		
	}
	else {//I'm the slave
		printf("I'm the slave\n");
		//set DDsequence number to the DD sequence number appearing 
		//in the received packet
		neighbourData_.setDDseq(neighbourId,ddpktPtr->DDSeqNumber_); 
		//send the reply
		sendDD(neighbourId);
			
		if(ddpktPtr->IMMSbits_.bit_M==0){
			printf("bit M=0\n");
			//the exchange has finished
			retCode=ExchangeDoneEvent(neighbourId);
		}
	}
	
	return retCode;
}


//**********************************************************************************
//  isLsTypeValid method: returns true if the type is valid and false in other case
//**********************************************************************************

bool OspfRouting::isLsTypeValid (LS_type_t type){

	switch(type){
		case LS_ROUTER: return true;
		case LS_NETWORK: return true;
		case LS_SUMMARY: return true;
		default:	
			return false;
	};
}

//**********************************************************************************
// ExchangeDoneEvent: representing ExchangeDone event  
//**********************************************************************************

bool OspfRouting::ExchangeDoneEvent(int nbId){


	printf("ExchangeDone Event from Node:%d\n",nbId);
	StateN_type_t state= neighbourData_.getState (nbId);
	
	if(state==EXCHANGE) {
		// link state request list empty
		LinkStateRecordIdList reql=neighbourData_.getRequestList (nbId);
		if(reql.empty()){
		//set the new neighbour state: final state
		neighbourData_.setState(nbId,FULL);
		}
		else {
		//set the new neighbour state: LOADING
		neighbourData_.setState(nbId,LOADING);
		sendRequest(nbId);
		}	
	}
	
	return false;		
}


//**********************************************************************************
// BadLSReqEvent: representing BadLSReqEvent event  
//**********************************************************************************

bool OspfRouting::BadLSReqEvent(int nbId){

	printf("BadLSReqEvent Event from Node:%d\n",nbId);
	StateN_type_t state= neighbourData_.getState (nbId);
	
	if(state>=EXCHANGE) {
		printf("EXCHANGE or grater\n");
		//set the new neighbour state
		neighbourData_.setState(nbId,EX_START);
		//adjacency is torn down
		delAdyacentFromList (nbId);
		//clear the lists:retransmission list, Database summary list and Link
                // state request list 
		neighbourData_.delAllLists(nbId);
		//cancel ack timer
		AckManager_.cancelTimer(nbId);
				
		//increments the DD sequence number in the
                // neighbor data structure,
		neighbourData_.incDDseq(nbId);
		//declares itself master
		neighbourData_.setMaster(nbId);
                //starts sending Database Description Packets
		sendDD(nbId);
	}
	
	return false;
}


//**********************************************************************************
// ageLSA method: increments LSA LSage field
//**********************************************************************************

int OspfRouting::ageLSA (int peerId){

	//get the LSA
	OspfLinkState ls=linkStateDatabase_.getLSA(LS_ROUTER,peerId,peerId);
	
	//reflooding self-originated LSA if LSage field reaches LSRefreshTime
	if((ls.ls_hdr_.LSage_==LS_REFRESH_TIME)&&(peerId==myNodeId_)){
		printf("[%f] ls refresh time myNodeId= %d\n",NOW,myNodeId_);
		
		//create new instance 
		OspfLinkState ls;
		ls.ls_hdr_.LSage_=0;
		ls.ls_hdr_.options_=options_t(1,0);
		ls.ls_hdr_.LS_type_=LS_ROUTER;
		ls.ls_hdr_.LS_ID_=myNodeId_;
		ls.ls_hdr_.LS_sequence_num_=ls.ls_hdr_.LS_sequence_num_;
		ls.ls_hdr_.advertising_router_=myNodeId_;
		
		//get the first advertising: in this implementation is the one used
		OspfLinkState lsad = linkStateListAdvPtr_->front();
		linkStateListPtr_= lsad.RouterLinkStateListPtr_;	
		ls.RouterLinkStateListPtr_=linkStateListPtr_;
	
	//update database
	
		linkStateDatabase_.delLSA (myNodeId_,ls.ls_hdr_.LS_type_,
					ls.ls_hdr_.LS_ID_,
					ls.ls_hdr_.advertising_router_);
		
		linkStateListAdvPtr_=linkStateDatabase_.insertLinkState(myNodeId_,ls);
		linkStateListPtr_=linkStateListAdvPtr_->front().RouterLinkStateListPtr_;
	
	        //send the LSA in an update packet to all adyacents neighbours
		sendFlooding(myNodeId_,ls);
		
	return 1;
	}

	//check if it is necessary to flush the LSA from the routing domain		
	if(ls.ls_hdr_.LSage_==MAX_AGE){
		
		for (LsNodeIdList::iterator itrList =neighbourIdList.begin();
	            itrList!= neighbourIdList.end(); itrList++) {
			//for each neighbour
			StateN_type_t state=neighbourData_.getState ((*itrList)); 
			if((!neighbourData_.lookupLSARetrans ((*itrList),ls.ls_hdr_))
			&&(state!=EXCHANGE)&&(state!=LOADING)){
				continue;
			}
			else
			 	return 0;
		}	
	 
	//remove max age LSA	
	linkStateDatabase_.delLSA (peerId,ls.ls_hdr_.LS_type_,
					ls.ls_hdr_.LS_ID_,
					ls.ls_hdr_.advertising_router_);
	 return 0;	
	}
	else {

	//call to OspfTopoMap method: incLSage
	linkStateDatabase_.incLSage (peerId);
	return 1;
	} 
	
}

//**********************************************************************************
// LoadingDoneEvent: representing LoadingDone Event event  
//**********************************************************************************

void OspfRouting::LoadingDoneEvent (int nbId){

	printf("LoadingDoneEvent Event from Node:%d\n",nbId);
	StateN_type_t state= neighbourData_.getState (nbId);
	
	if(state==LOADING) {
	//set the new neighbour state: final state
		neighbourData_.setState(nbId,FULL);
	
	}

}

//**********************************************************************************
// sendFlooding method: representing the flood of LSA to all adyacencies  
//**********************************************************************************
void OspfRouting::sendFlooding (int senderId,OspfLinkState& linkState){

	printf("Node:%d sendFlooding\n",myNodeId_);
	//for each adyacency
	
	double lsage_ini=linkState.ls_hdr_.LSage_;
	//double seqnum_ini=linkState.ls_hdr_.LS_sequence_num_;

	for (LsNodeIdList::iterator itrList =adyacentsIdList.begin();
	            itrList!= adyacentsIdList.end(); itrList++) {

	StateN_type_t state= neighbourData_.getState ((*itrList));
		
		if(state<EXCHANGE){
			//this neighbour doesn't  participate in the flooding
			//get the next neighbour
			continue;
		}
		
		if (senderId==(*itrList)){
			//the LSA was received from this neighbour
			printf("neighbourId=sender\n");
		
			if(neighbourData_.lookupLSAReq((*itrList),linkState.ls_hdr_)){
			 //the two LSA copies are the same instance
				//remove LSA from request list
				neighbourData_.delLSARequest((*itrList),linkState.ls_hdr_);
			}
			//get the next neighbour
			continue;
		}

		if((state==EXCHANGE)||(state==LOADING)){
			printf("neighbourId %d state %d\n",(*itrList),state);
			if(neighbourData_.lookupLSAReq((*itrList),linkState.ls_hdr_)){
			 //the two LSA copies are the same instance
				//remove LSA from request list
				neighbourData_.delLSARequest((*itrList),linkState.ls_hdr_);
				//get the next neighbour
				continue;	
			}
		}
		//at this point: we are not positive that the neighbour
		//has up-to-date instance of LSA.
		
		//add new LSA to retransmission list
		neighbourData_.addLSARetrans ((*itrList),linkState);
		
		//PRUEBA
		/*printf("RETRANSMISSION LIST\n");
		OspfLinkStateList lslp=neighbourData_.getLinkStateRetransList((*itrList));
		
		for (OspfLinkStateList::iterator itrList4 = lslp.begin();
	     	itrList4 != lslp.end(); itrList4++) {
			printf("LS TYPE: %d\n",(*itrList4).ls_hdr_.LS_type_);
			printf("LS ID: %d\n",(*itrList4).ls_hdr_.LS_ID_);
			printf("advertising router: %d\n",(*itrList4).ls_hdr_.advertising_router_);
		}*/
		//

		//send update packet
		UpdatePacket updpkt;
		OspfLinkStateList lsl;
		 
		//get originator node
		int origNode=linkState.ls_hdr_.advertising_router_;
		//getUpdatePkt((*itrList),linkstate)=updpkt
		//LSage must be incremented by InfTransDelay
		
		double* DelayPtr=delayMapPtr_->findPtr((*itrList));
		double infTransDelay=*DelayPtr;
		linkState.ls_hdr_.LSage_=lsage_ini+infTransDelay;
		printf("infTransDelay %f\n",infTransDelay);
		printf("LSAGE %f\n",linkState.ls_hdr_.LSage_);
		//linkState.ls_hdr_. LS_sequence_num_=seqnum_ini+1;
		lsl.push_back(linkState);
		updpkt.numberAdvert_=1;		 
		OspfLinkStateList* ladvPtr_ = new OspfLinkStateList(lsl); 
		updpkt.LsListAdvertPtr_=ladvPtr_;
		sendUpdate(origNode,(*itrList),updpkt);
		
	}//for adyacents

	
}

//**********************************************************************************
// changeRouterLSAState method: changes the router LSA state  
//**********************************************************************************


void OspfRouting::changeRouterLSAState (int adv_router, int link_id, ls_status_t st){

	bool change=false;
	// update my linkstate database
	//get the linkstate advertisement whose link id= myNodeId
	OspfLinkState ls=linkStateDatabase_.getLSA(LS_ROUTER,adv_router,adv_router);
	
	if((int)ls.ls_hdr_.advertising_router_!=LS_INVALID_NODE_ID) {
	
	printf(" link id %d advrouter %d \n",ls.ls_hdr_.LS_ID_,ls.ls_hdr_.advertising_router_);
		
	RouterLinkStateList routerlsl=(*ls.RouterLinkStateListPtr_);
		
	for (RouterLinkStateList::iterator itrList = routerlsl.begin();
            itrList != routerlsl.end(); itrList++){
		if((int)(*itrList).Link_ID_==link_id){
			
			if((*itrList).state_!=st){
					printf("state!=st\n");
					(*itrList).state_=st;
					change=true;
					break;
			}
		}
	 }		
	
	if(change){
	//the database has changed
	
	//remove link sate advertisement from the database
	linkStateDatabase_.delLSA (myNodeId_,ls.ls_hdr_.LS_type_,
					ls.ls_hdr_.LS_ID_,
					ls.ls_hdr_.advertising_router_);
	//update LSage and LS sequence number 
	ls.ls_hdr_.LSage_=0;
	ls.ls_hdr_.LS_sequence_num_+=1;
	printf("seq number %f\n",ls.ls_hdr_.LS_sequence_num_);
	ls.RouterLinkStateListPtr_=new RouterLinkStateList(routerlsl);
	// insert the updated link state advertisement into the data base
	linkStateListAdvPtr_=linkStateDatabase_.insertLinkState(myNodeId_,ls);
	linkStateListPtr_=linkStateListAdvPtr_->front().RouterLinkStateListPtr_;
	
		if(st==LS_STATUS_UP){
			//active the ls aging timer
			AgingManager_.createLSA(link_id);
	
		}
		else{	
		//LS_STATUS_DOWN
		//cancel lsAging timer
			AgingManager_.cancelTimer(link_id);
		
		}
	//recompute routes
	computeRoutes();
	//send the database copy LSA in an update packet to all adyacents neighbours
	// except to link_id
	sendLSAToAdyacents(LS_ROUTER,myNodeId_,myNodeId_,link_id);

	}// if change
    }
}

//**********************************************************************************
// _computeRoutes method: private method called by computeRoutes  
//**********************************************************************************

OspfPaths* OspfRouting::_computeRoutes () 
{
	printf("Compute routes\n");
	OspfPathsTentative* pTentativePaths = new OspfPathsTentative();
	OspfPaths* pPaths = new OspfPaths() ; // to be returned; 
 	int nmtids = myNodePtr_->getNumMtIds();
	int path_cost;
	// step 1. put myself in path for each mtid
	for (int i=0; i<=nmtids;i++) {
	  pPaths->insertPathNoChecking(myNodeId_,i,0,myNodeId_);
	}
	
	//PRUEBA
	/*for (OspfLinkStateMap::iterator itrMap = linkStateDatabase_.begin();
            itrMap != linkStateDatabase_.end(); itrMap++) {
		for (OspfLinkStateList::iterator itrList = (*itrMap).second.begin();
	            itrList!=(*itrMap).second.end(); itrList++) {			
			
			printf("Node %d LS_sequence_num_: %f\n",myNodeId_,
			(*itrList).ls_hdr_.LS_sequence_num_);
			printf("Advertising router: %d\n",(*itrMap).first);
			
	for (RouterLinkStateList::iterator itrList2 = (*itrList).RouterLinkStateListPtr_->begin();
            itrList2 != (*itrList).RouterLinkStateListPtr_->end(); itrList2++){

		printf("Nodo vecino: %d ",(&(*itrList2))->Link_ID_);
		printf("State: %d\n",(*itrList2).state_);
		for (MTLinkList::iterator itrList3 = (&(*itrList2))->MTLinkList_.begin();
             itrList3 != (&(*itrList2))->MTLinkList_.end(); itrList3++){
		
		printf("Mtid: %d Coste: %d\n",(&(*itrList3))->mtId_,(&(*itrList3))->metric_);

		}	
	      }
			
	   }
	}*/
	//

	for (int i=0; i<=nmtids;i++){ // calculate the routes for each Mtid

	OspfLinkStateList * ptrLSLAd = linkStateDatabase_.findPtr(myNodeId_);
	
	if(ptrLSLAd->empty())
		return pPaths;

	//get the first advertising: in this implementation is the one used
	OspfLinkState lsad = ptrLSLAd->front();

	//LSA whose LSage field is equal to MaxAge, is not used in computing routing table
	if(lsad.ls_hdr_.LSage_==MAX_AGE)
		return pPaths;

	RouterLinkStateList* lslPtr_= lsad.RouterLinkStateListPtr_;
	int newNodeId = myNodeId_;
	bool done = false;

	  while (!done) {
	    
	    printf(" myNodeId: %d\n",newNodeId);
	    // Step 2. for the new node just put in path
	    // find the next hop to the new node
	    LsNodeIdList nhl;
	    LsNodeIdList *nhlp = &nhl; // nextHopeList pointer
    	    
	    if (newNodeId != myNodeId_) {
						
			// if not looking at my own links, find the next hop 
			// to new node for the mti
			nhlp = pPaths->lookupNextHopListPtr(newNodeId,i);
			if (nhlp == NULL)
				ls_error("computeRoutes: nhlp == NULL \n");
	    }

   	    // for each of it's links
 	    for (RouterLinkStateList::iterator itrList = lslPtr_->begin();
              itrList != lslPtr_->end(); itrList++){

	      printf("Nodo vecino: %d\n",(*itrList).Link_ID_);
	      int dest=	(*itrList).Link_ID_;

	      if((*itrList).state_==LS_STATUS_DOWN){
		  continue;
	      }		
	      //look for the cost attach to the mtid=i
	      for (MTLinkList::iterator itrList2 = (&(*itrList))->MTLinkList_.begin();
              itrList2 != (&(*itrList))->MTLinkList_.end(); itrList2++){
		
	        if((&(*itrList2))->mtId_==i) {
		   path_cost = (*itrList2).metric_+
			pPaths->lookupCost(newNodeId,i);	
		printf("Mtid: %d Coste: %d\n",i,path_cost);
		 break;
		} //fi
	      } //for mtids
			
		
 	     if (pPaths->lookupCost(dest,i) < path_cost){
		printf("better path already in paths\n");
		// better path already in paths, 
		// move on to next link
		continue;
		}
		

	     else {
		 // else we have a new or equally good path, 
		 // OspfPathsTentative::insertPath(...) will 
		 // take care of checking if the new path is
		 // a better or equally good one, etc.
		  LsNodeIdList nextHopList;
		    if (newNodeId == myNodeId_) {
		      // destination is directly connected, 
		      // nextHop is itself
		      printf("newNodeId==myNodeId\n");	
		      nextHopList.push_back(dest);
		      nhlp = &nextHopList;
		      
		    }
		
                  pTentativePaths->insertNextHopList(dest,						   					path_cost,i, *nhlp);
		  } //else
	   }// for -RouterLinks
  
	    done = true;
           	
		// if tentatives empty, terminate;
		while (!pTentativePaths->empty()) {
			// else pop shortest path  from tentatives
			OspfPath sp = pTentativePaths->popShortestPath(i);
			if (!sp.isValid())
				ls_error (" popShortestPath() failed\n");
			// install the newly found shortest path among 
			// tentatives
			printf("Destino:%d, mtid:%d, coste:%d nextHop:%d\n",
			sp.desId_,sp.mtId_,sp.cost_,sp.nextHop_);
			pPaths->insertPath(sp);
			newNodeId = sp.desId_;
			ptrLSLAd = linkStateDatabase_.findPtr(newNodeId);
			
			if (ptrLSLAd!=NULL) {
				printf("ptrLSAd !=empty\n");
				//if we have the link state for the new node
				// break out of inner do loop to continue 
				// computing routes
				//get the first advertising: in this implementation is the one used
				//OspfLinkState lsad = ptrLSLAd->front();
				lsad = ptrLSLAd->front();
				
				//LSA whose LSage field is equal to MaxAge, is not used in computing 					routing table
				if(lsad.ls_hdr_.LSage_==MAX_AGE){
					continue;
				}
				//RouterLinkStateList* lslPtr_= lsad.RouterLinkStateListPtr_;
				lslPtr_= lsad.RouterLinkStateListPtr_;
				done = false;
				break;
			} 
			// else  we don't have linkstate for this new node, 
		// we need to keep popping shortest paths

		} //while
	   }//while done    
	 } //for -mtids	
	
	//PRUEBA
	printf("CONTENIDO TABLA ROUITNG\n");
	printf("Nodo actual: %d\n",myNodeId_);
	
	for (OspfMTPathsMap::iterator itrMap = pPaths->begin();
            itrMap != pPaths->end(); itrMap++) {
 	    printf("Nodo destino: %d\n",(*itrMap).first);	
	    OspfMTRouterPathsMap * pEPM =&(*itrMap).second;
		for (OspfMTRouterPathsMap::iterator itrMap2 = pEPM->begin();
	            itrMap2!= pEPM->end(); itrMap2++) {
			printf("Mtid: %d\n",(*itrMap2).first);
			printf("Coste: %d\n",(*itrMap2).second.cost);
			LsNodeIdList* nextHopListPtr_=&((*itrMap2).second.nextHopList);
			for (LsNodeIdList::iterator itrList = nextHopListPtr_->begin();
	            	itrList!= nextHopListPtr_->end(); itrList++) {
			printf("Next hop %d\n",*itrList);
			}
			
		}
	    		
	}

	
	//is neccesary to re-calculate routes in tcl
	myNodePtr_-> routeChanged();
	
	return pPaths;
}


int OspfRouting::getnodeid()
{
	return myNodeId_;
}

#endif
