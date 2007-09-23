/*  
 *  Authors:
 *    Zacharias El Banna, 2002 (zeb@sics.se)
 * 
 *  Contributors:
 *    optional, Contributor's name (Contributor's email address)
 * 
 *  Copyright:
 *    Zacharias El Banna, 2002
 * 
 *  Last change:
 *    $Date$ by $Author$
 *    $Revision$
 * 
 *  This file is part of Mozart, an implementation 
 *  of Oz 3:
 *     http://www.mozart-oz.org
 * 
 *  See the file "LICENSE" or
 *     http://www.mozart-oz.org/LICENSE.html
 *  for information on usage and redistribution 
 *  of this file, and for a DISCLAIMER OF ALL 
 *  WARRANTIES.
 *
 */
 
#if defined(INTERFACE)
#pragma implementation "coordinator.hh"
#endif

#include "dssBase.hh"
#include "msl_serialize.hh"
#include "coordinator.hh"
#include "coordinator_stationary.hh"
#include "coordinator_fwdchain.hh"
#include "coordinator_mobile.hh"
#include "protocols.hh"


namespace _dss_internal{ //Start namespace

#ifdef DEBUG_CHECK
  int Coordinator::a_allocated=0;
  int Proxy::a_allocated=0;
#endif



  // *****************************************************************
  //
  // The AS-Node
  //

  AS_Node::~AS_Node(){}

  AS_Node::AS_Node(const AccessArchitecture& a, DSS_Environment* const env):
    NetIdNode(),
    DSS_Environment_Base(env),
    a_aa(a){
    // Created Resolver base node
  }
    
  AS_Node::AS_Node(NetIdentity ni, const AccessArchitecture& a,
		   DSS_Environment* const env):
    NetIdNode(ni),
    DSS_Environment_Base(env),
    a_aa(a){
    // Created Resolver base node
  }
  
  ::MsgContainer *
  AS_Node::m_createASMsg(const MessageType& mt){
    ::MsgContainer *msgC = m_getEnvironment()->a_msgnLayer->createAppSendMsgContainer();
    msgC->pushIntVal(mt); 
    gf_pushNetIdentity(msgC, m_getNetId());
    return msgC;
  }
  // ****************************** Coordinator ***********************************'
  
  Coordinator::Coordinator(const AccessArchitecture& a,
			   ProtocolManager* const p, DSS_Environment* const env):AS_Node(a,env), a_proxy(NULL), a_prot(p){
    DebugCode(a_allocated++);
    m_getEnvironment()->a_coordinatorTable->m_add(this);
  }


  Coordinator::Coordinator(NetIdentity ni, const AccessArchitecture& a,
			   ProtocolManager* const p, DSS_Environment* const env):AS_Node(ni, a,env), a_proxy(NULL), a_prot(p){
    DebugCode(a_allocated++);
    m_getEnvironment()->a_coordinatorTable->m_insert(this);
  }
  

  // Access structures has to delete the ref by themselves  
  Coordinator::~Coordinator(){
    DebugCode(a_allocated--);
    m_getEnvironment()->a_coordinatorTable->m_del(this); //free self and..
    delete a_prot;
  }
  
  
  AOcallback
  Coordinator::m_doe(const AbsOp& aop, DssThreadId* thid, DssOperationId* oId, PstInContainerInterface* builder, PstOutContainerInterface*& ans){
    return a_proxy->m_doe(aop, thid, oId,  builder, ans);
  }
  
  ::PstOutContainerInterface* 
  Coordinator::retrieveEntityState(){
    return a_proxy->retrieveEntityState();
  }
  
  ::PstOutContainerInterface* 
  Coordinator::deinstallEntityState(){
    return a_proxy->deinstallEntityState();
  }
  
  void 
  Coordinator::installEntityState(PstInContainerInterface* builder){
    a_proxy->installEntityState(builder); 
  }

  // ******************* Failure handlers ************************
  void
  Coordinator::m_siteStateChange(DSite *, const DSiteState&){
    // currently the coordinator does not bother...
  }

  void  
  Coordinator::m_undeliveredCoordMsg(DSite* dest, MessageType mtt,MsgContainer* msg){
    delete msg; 
  }
  void  
  Coordinator::m_undeliveredProxyMsg(DSite* dest, MessageType mtt,MsgContainer* msg){
    delete msg; 
  }
  void  
  Coordinator::m_noCoordAtDest(DSite* sender, MessageType mtt, MsgContainer* msg){
    delete msg; 
  }
  void  
  Coordinator::m_noProxyAtDest(DSite* sender, MessageType mtt, MsgContainer* msg){
    delete msg; 
  }



  /****************************** Proxy ******************************/
  
  Proxy::Proxy(NetIdentity ni, const AccessArchitecture& a,
	       ProtocolProxy* const prot, DSS_Environment* const env) :
    AS_Node(ni,a,env), a_ps(PROXY_STATUS_UNSET),
    a_currentFS(FS_ALL_OK), a_registeredFS(0),
    a_prot(prot), a_remoteRef(NULL),
    a_coordinator(NULL), a_abstractEntity(NULL)
  {
    DebugCode(a_allocated++);
    m_getEnvironment()->a_proxyTable->m_insert(this);
  }

  // update the fault state; fs must be nonzero, but may contain a
  // partial fault state, like a protocol failure only, for instance.
  void 
  Proxy::updateFaultState(FaultState fs) {
    Assert(fs);
    // first complete the missing parts
    if ((fs & FS_AA_MASK) == 0)   fs |= getFaultState() & FS_AA_MASK;
    if ((fs & FS_PROT_MASK) == 0) fs |= getFaultState() & FS_PROT_MASK;
    if (fs != a_currentFS) {
      // this is a real change, make the update
      a_currentFS = fs;
      // Notify the glue interface if required
      if (a_abstractEntity && (fs & getRegisteredFS()))
	a_abstractEntity->reportFaultState(fs & getRegisteredFS());
    }
  }
  
  // Access structures has to delete the ref by themselves
  Proxy::~Proxy(){
    DebugCode(a_allocated--);
    m_getEnvironment()->a_proxyTable->m_remove(this); //free self    
  }

  // return the parameters (protocol name, access architecture, and
  // reference consistency protocols) of this proxy
  void Proxy::getParameters(ProtocolName &pn,
			    AccessArchitecture &aa, RCalg &rc) const {
    pn = m_getProtocol()->getProtocolName();
    aa = getAccessArchitecture();
    rc = m_getReference()->m_getAlgorithms();
  }
  

  AOcallback
  Proxy::m_doe(const AbsOp& aop, DssThreadId* thid, DssOperationId* oId,
	       PstInContainerInterface* builder,
	       PstOutContainerInterface*& ans)
  {
    return applyAbstractOperation(a_abstractEntity, aop,
				  thid, oId, builder, ans);
  }
  
  ::PstOutContainerInterface* 
  Proxy::retrieveEntityState(){
    return a_abstractEntity->retrieveEntityRepresentation(); 
  }

  ::PstOutContainerInterface* 
  Proxy::deinstallEntityState(){
    return a_abstractEntity->deinstallEntityRepresentation(); 
  }
  
  void 
  Proxy::installEntityState(PstInContainerInterface* builder){
    a_abstractEntity->installEntityRepresentation(builder); 
  }
  
  
  bool
  Proxy::clearWeakRoot(){
    return a_prot->clearWeakRoot();
  }
  
  bool
  Proxy::marshal(DssWriteBuffer *buf, const ProxyMarshalFlag& prf = PMF_ORDINARY){
    switch(prf){
    case PMF_ORDINARY:
    case PMF_PUSH: // ERIK: ZACHARIAS Should probably be handled outside of this. i.e in dss_accesbs
      if (m_getEnvironment()->m_getDestDSite() == NULL) {
	m_getEnvironment()->a_map->GL_warning("Called marshalProxy without destination");
	return false;
      };
      break;
    case PMF_FREE:
      dssLog(DLL_ALL,"PROXY (%p): Persistent (Free marshalling) proxy",this);
      m_makePersistent();
      break;
    default: Assert(0);
    }

    DSite* dest = m_getEnvironment()->a_msgnLayer->m_getDestDSite();
    
    // Bit saving schema:
    //        +-------------+-------------+-------------+-------------+
    // head = | access arch | proto name  |   ae name   | marshal flag|
    //        +-------------+-------------+-------------+-------------+
    Assert(AA_NBITS + PN_NBITS + AEN_NBITS + PMF_NBITS <= 16);
    unsigned int head = ((((m_getASname())
			   << PN_NBITS | m_getProtocol()->getProtocolName())
			  << AEN_NBITS | m_getAEname())
			 << PMF_NBITS | prf);
    gf_Marshal8bitInt(buf, head >> 8);
    gf_Marshal8bitInt(buf, head & 0xFF);        // 2 bytes

    gf_marshalNetIdentity(buf, m_getNetId());   // a DSite + a number
    m_getReferenceInfo(buf, dest);              // up to 48 (often ~10, WRC)

    // Returns true when the protocol is immediate, i.e., when the
    // whole node should be distributed.  The protocol using DKS
    // marshals a NetId + 3 numbers + a DSite.  The other protocols
    // marshal at most 1 byte.
    return m_getProtocol()->marshal_protocol_info(buf, dest);
  }

  int
  Proxy::sm_getMRsize() {   // incomplete!
    Assert(0);
    return 48 + 3 * sz_M8bitInt + sz_MNumberMax + 200;
    // Note: this is correct for all protocols except PN_DKSBROADCAST,
    // for which you must add 2 * <DSite size> + 4 * sz_MNumberMax.
  }


  void Proxy::setRegisteredFS(const FaultState& s){
    // For now, we don't automaticallyt connect when we 
    // have a failure detection need on a particular site.
    //    m_getGUIdSite()->m_connect();
    a_registeredFS = s;
  }


  // ******************* Failure handlers ************************
  void Proxy::m_siteStateChange(DSite *, const DSiteState&){
    // just ignore it.  (Don't tell anyone we do this!)
  }
  void Proxy::m_undeliveredCoordMsg(DSite* dest, MessageType mtt,MsgContainer* msg){
    delete msg; 
  }
  void Proxy::m_undeliveredProxyMsg(DSite* dest, MessageType mtt,MsgContainer* msg){
    delete msg; 
  }
  void Proxy::m_noCoordAtDest(DSite* sender, MessageType mtt, MsgContainer* msg){
    delete msg; 
  }
  void Proxy::m_noProxyAtDest(DSite* sender, MessageType mtt, MsgContainer* msg){
    delete msg; 
  }
  // ************* Communication ********************
  
  
  bool 
  Proxy:: m_sendToProxy(DSite* dest, ::MsgContainer* msg){
    return dest->m_sendMsg(msg); 
  }
  
  
  ::MsgContainer*
  Proxy::m_createCoordProtMsg(){
    return m_createASMsg(M_PROXY_COORD_PROTOCOL);
  }
  ::MsgContainer* 
  Proxy::m_createProxyProtMsg(){
    return m_createASMsg(M_PROXY_PROXY_PROTOCOL);
  }
  
  ::MsgContainer * 
  Proxy::m_createCoordRefMsg(){
    return m_createASMsg(M_PROXY_COORD_REF);
  }
  ::MsgContainer * 
  Proxy::m_createProxyRefMsg(){
    return m_createASMsg(M_PROXY_PROXY_REF);
  }
  
  


  // ***************************** CoordinatorTable ****************************
  // *
  // * - Only contains managers (and index), 
  // *   reference might be placed here later
  // *
  // ***********************************************************************

  void 
  CoordinatorTable::m_gcResources(){
    dssLog(DLL_BEHAVIOR,"******************** COORDINATOR TABLE: GC Resources (%d) ********************\n");
    for (NetIdNode *n = m_getNext(NULL) ; n;) {
      Coordinator *c = static_cast<Coordinator*>(n);
      n = m_getNext(n);
      // n points on the next element; this allows to delete c now
      if (c->m_getProxy() == NULL &&
	  c->m_getDssDGCStatus() == DSS_GC_LOCALIZE) {
	// Ok this one is a "single" manager, check it
	dssLog(DLL_DEBUG,"COORDINATOR TABLE: removing coordinator %p", c);
	delete c;     // c removes itself from the table!
      } else {
	c->m_makeGCpreps();
      }
    }
  }
  
  void
  CoordinatorTable::m_siteStateChange(DSite *s, const DSiteState& newState)
  {
    for(NetIdNode *n = m_getNext(NULL) ; n != NULL; n = m_getNext(n)) {
      static_cast<Coordinator *>(n)->m_siteStateChange(s,newState);
    }
  }
  
#ifdef DSS_LOG
  void
  CoordinatorTable::log_print_content(){
    dssLog(DLL_PRINT,"************************* COORDINATOR TABLE ************************");
    for(NetIdNode *n = m_getNext(NULL) ; n != NULL; n = m_getNext(n)) {
      dssLog(DLL_PRINT,"%p %s",n,static_cast<Coordinator *>(n)->m_stringrep());
    }
    dssLog(DLL_PRINT,"********************** COORDINATOR TABLE - DONE ********************");
  }
#endif
  
  // ***************************** ProxyTable *************************************
  // *
  // * - Gc is a single step and realized in proxy entries
  // *
  // ******************************************************************************
  
  void
  ProxyTable::m_siteStateChange(DSite *s, const DSiteState& newState){
    for(NetIdNode *n = m_getNext(NULL) ; n != NULL; n = m_getNext(n)) {
      static_cast<Proxy *>(n)->m_siteStateChange(s,newState); 
    }
  }
  
  
  void
  ProxyTable::m_gcResources(){
    dssLog(DLL_BEHAVIOR,"******************** PROXY TABLE - GC Resources () *********************");
    for(NetIdNode *n = m_getNext(NULL) ; n != NULL; n = m_getNext(n)) {
      Proxy *pe = static_cast<Proxy *>(n);
#ifdef DEBUG_CHECK
      dssLog(DLL_DEBUG,"PROXY %p %s",pe,pe->m_stringrep());
#else
      dssLog(DLL_DEBUG,"PROXY %p",pe);
#endif
      pe->m_getGUIdSite()->m_makeGCpreps();
      pe->m_makeGCpreps();
    }
  }
  
#ifdef DSS_LOG
  void
  ProxyTable::log_print_content(){
    for(NetIdNode *n = m_getNext(NULL) ; n != NULL; n = m_getNext(n)) {
      dssLog(DLL_PRINT,"************************** PROXY TABLE () *************************");
      dssLog(DLL_PRINT,"%p %s",n,static_cast<Proxy *>(n)->m_stringrep());
    }
    dssLog(DLL_PRINT,"*********************** PROXY TABLE - DONE *********************");
  }
#endif


  
  
  Proxy* gf_createCoordinationProxy(AccessArchitecture type,
				    NetIdentity ni,
				    ProtocolProxy *prox, 
				    DSS_Environment* env){
    switch(type){
    case AA_STATIONARY_MANAGER: return new ProxyStationary(ni, prox, env);
    case AA_MIGRATORY_MANAGER:  return new ProxyFwdChain(ni, prox, env);
    case AA_MOBILE_COORDINATOR: return new ProxyMobile(ni, prox, env);
    default: Assert(0); 
    }
    return NULL;
  }
  
  Coordinator *gf_createCoordinator(AccessArchitecture type,
				    ProtocolManager *pman,
				    RCalg GC_annot,
				    DSS_Environment *env){
    switch(type){
    case AA_STATIONARY_MANAGER: return new CoordinatorStationary(pman,GC_annot,env);
    case AA_MIGRATORY_MANAGER:  return new CoordinatorFwdChain(pman,GC_annot, env);
    case AA_MOBILE_COORDINATOR: return new CoordinatorMobile(pman, GC_annot, env); 
    default:  Assert(0); 
    }
    return NULL;
  }


} //End namespace
