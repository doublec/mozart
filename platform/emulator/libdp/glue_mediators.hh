/*
 *  Authors:
 *    Erik Klintskog, 2002
 * 
 *  Contributors:
 *    Raphael Collet (raph@info.ucl.ac.be)
 *    Boriss Mejias (bmc@info.ucl.ac.be)
 * 
 *  Copyright:
 *    Erik Klintskog, 2002
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

#ifndef __GLUE_MEDIATORS_HH
#define __GLUE_MEDIATORS_HH

#ifdef INTERFACE  
#pragma interface
#endif

#include "dss_object.hh"
#include "tagged.hh"
#include "engine_interface.hh"
#include "glue_buffer.hh"
#include "glue_faults.hh"

/*
  Mediators interface Oz entities to DSS abstract entities.  Several
  aspects of distribution are implemented by mediators:

  - Operations: An operation on a distributed Oz entity may require an
  operation on its abstract entity.  The entity's mediator gives
  access to its abstract entity.  The mediator also implements
  callbacks for remote operations, and the needs of the entity's
  distribution protocol.

  - Globalization/Localization: The mediator is responsible for
  creating the abstract entity, and for localizing the Oz entity.  It
  takes part in garbage collection, and makes the local and
  distributed GC cooperate.  It also stores the entity's annotation,
  which parameterizes its globalization.

  - Faults: Abstract operations report distribution failures to their
  mediator.  The mediator adapts the Oz entity's semantics to the
  given failure, and maintains a stream describing the entity's fault
  state.

  A mediator always has references to its Oz entity, and to its
  corresponding abstract entity (when distributed).  The abstract
  entity also refers to the mediator.  An Oz entity normally has a
  pointer to its mediator.  But this might not be the case, for
  instance when the mediator only stores the entity's annotation (see
  detached mediators below).

  Now a bit of terminology:

  - A mediator is ACTIVE when it refers to a valid language entity.
  It becomes PASSIVE whenever this entity disappears.  In our case
  this only happens when variables are bound.  For the sake of
  consistency, passive mediators must be attached (see below).

  - A mediator is ATTACHED when the emulator has a direct pointer to
  it.  Otherwise is is DETACHED, and should be looked up in the
  mediator table (using the entity's taggedref as a key).  Attached
  mediators are not found by looking up the mediator table.

  Note. Insertion of the mediator in the mediator table is automatic
  upon creation.  Connection to the abstract entity is also automatic
  upon globalization (not yet, still buggy).

 */



// the types of entities handled by the glue.  Those tags are also
// used as marshaling tags.
enum GlueTag {
  GLUE_NONE = 0,       // 
  GLUE_LAZYCOPY,       // immutables
  GLUE_UNUSABLE,
  GLUE_VARIABLE,       // transients
  GLUE_READONLY,
  GLUE_PORT,           // asynchronuous mutables
  GLUE_CELL,           // mutables
  GLUE_LOCK,
  GLUE_OBJECT,
  GLUE_ARRAY,
  GLUE_DICTIONARY,
  GLUE_THREAD,
  GLUE_LAST            // must be last
};

// attached or detached
#define DETACHED 0
#define ATTACHED 1



// Mediator is the abstract class for all mediators
class Mediator {
  friend class MediatorTable;
protected:
  bool            active:1;          // TRUE if it is active
  bool            attached:1;        // TRUE if it is attached
  bool            collected:1;       // TRUE if it has been collected
  DSS_GC          dss_gc_status:2;   // status of dss gc
  GlueTag         type:5;            // type of entity
  GlueFaultState  faultState:2;      // current fault state

  Annotation      annotation;        // the entity's annotation

  int             id;

  TaggedRef       entity;      // references to engine and dss entities
  TaggedRef       faultStream;
  TaggedRef       faultCtlVar;
  AbstractEntity* absEntity;

  Mediator*       next;           // for mediator table

public:
  /*************** constructor/destructor ***************/
  Mediator(TaggedRef entity, GlueTag type, bool attached);
  virtual ~Mediator();

  /*************** active/passive, attached/detached ***************/
  bool isActive() { return active; }
  void makePassive();

  bool isAttached() { return attached; }
  void setAttached(bool a) { attached = a; }

  /*************** set/get entity/abstract entity ***************/
  TaggedRef getEntity() { return entity; }
  AbstractEntity *getAbstractEntity() { return absEntity; }
  void setAbstractEntity(AbstractEntity *a);
  CoordinatorAssistantInterface* getCoordinatorAssistant();

  /*************** annotate/globalize/localize ***************/
  GlueTag getType() { return type; }

  Annotation getAnnotation() { return annotation; }
  void setAnnotation(const Annotation& a) { annotation = a; }
  void completeAnnotation();

  virtual void globalize() = 0;     // create abstract entity
  virtual void localize() = 0;      // try to localize entity

  /*************** garbage collection ***************/
  bool isCollected() { return collected; }
  void gCollect();            // collect mediator (idempotent)
  void checkGCollect();       // collect if entity marked
  void resetGCStatus();       // reset the gc status
  DSS_GC getDssGCStatus();    // ask and return dss gc status

  // Note: In order to specialize gCollect(), make it a virtual method.

  /*************** fault handling ***************/
  GlueFaultState getFaultState() { return faultState; }
  void           setFaultState(GlueFaultState fs);
  TaggedRef      getFaultStream();
  OZ_Return      suspendOnFault();     // suspend on control var

  void reportFS(const FaultState& fs);

  /*************** marshaling ***************/
  virtual void marshal(ByteBuffer *bs);

  /*************** debugging ***************/
  virtual char* getPrintType() = 0;
  void print();
};



// return the mediator of an entity (create one if necessary)
Mediator *glue_getMediator(TaggedRef entity);



// ConstMediator is an abstract class for mediators referring to a
// ConstTerm in the emulator.
class ConstMediator: public Mediator {
public: 
  ConstMediator(TaggedRef t, GlueTag type, bool attached);
  ConstTerm* getConst();
  virtual char *getPrintType();
  virtual void globalize();
  virtual void localize();
};



// mediators for Oz threads
class OzThreadMediator: public ConstMediator, public MutableMediatorInterface {
public:
  OzThreadMediator(TaggedRef t);
  OzThreadMediator(TaggedRef t, AbstractEntity *ae);
  
  virtual AOcallback callback_Write(DssThreadId* id_of_calling_thread,
				    DssOperationId* operation_id,
				    PstInContainerInterface* operation,
				    PstOutContainerInterface*& possible_answer);
  virtual AOcallback callback_Read(DssThreadId* id_of_calling_thread,
				   DssOperationId* operation_id,
				   PstInContainerInterface* operation,
				   PstOutContainerInterface*& possible_answer);
  virtual PstOutContainerInterface *retrieveEntityRepresentation();
  virtual void installEntityRepresentation(PstInContainerInterface*);  
  virtual char *getPrintType();
  virtual void reportFaultState(const FaultState& fs) { reportFS(fs); }
};


// mediators for "unusables".  An unusable is a remote representation
// of a site-specific entity.
class UnusableMediator: public Mediator, public ImmutableMediatorInterface {
public:
  UnusableMediator(TaggedRef t);
  UnusableMediator(TaggedRef t, AbstractEntity* ae);
  
  virtual AOcallback callback_Read(DssThreadId* id_of_calling_thread,
				   DssOperationId* operation_id,
				   PstInContainerInterface* operation,
				   PstOutContainerInterface*& possible_answer);
  virtual void globalize();
  virtual void localize();
  virtual PstOutContainerInterface *retrieveEntityRepresentation() { Assert(0); return NULL;}
  virtual void installEntityRepresentation(PstInContainerInterface*) { Assert(0);}
  virtual char *getPrintType();
  virtual void reportFaultState(const FaultState& fs) { reportFS(fs); }
};


// mediators for Oz ports
class PortMediator:
  public ConstMediator, public RelaxedMutableMediatorInterface {
public:
  PortMediator(TaggedRef t);
  PortMediator(TaggedRef t, AbstractEntity *ae);
  
  virtual AOcallback callback_Write(DssThreadId* id_of_calling_thread,
				    DssOperationId* operation_id,
				    PstInContainerInterface* operation);
  virtual AOcallback callback_Read(DssThreadId* id_of_calling_thread,
				   DssOperationId* operation_id,
				   PstInContainerInterface* operation,
				   PstOutContainerInterface*& possible_answer);
  virtual void globalize();
  virtual PstOutContainerInterface *retrieveEntityRepresentation() { Assert(0); return NULL;}
  virtual void installEntityRepresentation(PstInContainerInterface*) { Assert(0);} 
  virtual char *getPrintType();
  virtual void reportFaultState(const FaultState& fs) { reportFS(fs); }
}; 


// mediators for Oz cells
class CellMediator: public ConstMediator, public MutableMediatorInterface{
public:
  CellMediator(TaggedRef t);
  CellMediator(TaggedRef t, AbstractEntity *ae);
  
  virtual AOcallback callback_Write(DssThreadId* id_of_calling_thread,
				    DssOperationId* operation_id,
				    PstInContainerInterface* operation,
				    PstOutContainerInterface*& possible_answer);
  virtual AOcallback callback_Read(DssThreadId* id_of_calling_thread,
				   DssOperationId* operation_id,
				   PstInContainerInterface* operation,
				   PstOutContainerInterface*& possible_answer);
  virtual PstOutContainerInterface *retrieveEntityRepresentation();
  virtual PstOutContainerInterface *deinstallEntityRepresentation();
  virtual void installEntityRepresentation(PstInContainerInterface*); 
  virtual char *getPrintType();
  virtual void reportFaultState(const FaultState& fs) { reportFS(fs); }
}; 


// mediators for Oz locks
class LockMediator: public ConstMediator, public MutableMediatorInterface{
public:
  LockMediator(TaggedRef t);
  LockMediator(TaggedRef t, AbstractEntity *ae);
  
  virtual AOcallback callback_Write(DssThreadId* id_of_calling_thread,
				    DssOperationId* operation_id,
				    PstInContainerInterface* operation,
				    PstOutContainerInterface*& possible_answer);
  virtual AOcallback callback_Read(DssThreadId* id_of_calling_thread,
				   DssOperationId* operation_id,
				   PstInContainerInterface* operation,
				   PstOutContainerInterface*& possible_answer);
  virtual PstOutContainerInterface *retrieveEntityRepresentation();  
  virtual void installEntityRepresentation(PstInContainerInterface*);  
  virtual char *getPrintType();
  virtual void reportFaultState(const FaultState& fs) { reportFS(fs); }
}; 


// mediators for immutable values that are replicated lazily
class LazyVarMediator: public Mediator {
public:
  LazyVarMediator(TaggedRef t, AbstractEntity *ae);
  virtual void globalize();
  virtual void localize();
  virtual char *getPrintType();
}; 


// mediators for Oz arrays
class ArrayMediator: public ConstMediator, public MutableMediatorInterface{
public:
  ArrayMediator(TaggedRef t);
  ArrayMediator(TaggedRef t, AbstractEntity *ae);
  
  virtual AOcallback callback_Write(DssThreadId* id_of_calling_thread,
				    DssOperationId* operation_id,
				    PstInContainerInterface* operation,
				    PstOutContainerInterface*& possible_answer);
  virtual AOcallback callback_Read(DssThreadId* id_of_calling_thread,
				   DssOperationId* operation_id,
				   PstInContainerInterface* operation,
				   PstOutContainerInterface*& possible_answer);
  virtual PstOutContainerInterface *retrieveEntityRepresentation() ;
  virtual PstOutContainerInterface *deinstallEntityRepresentation() ;
  virtual void installEntityRepresentation(PstInContainerInterface*) ;
  virtual void marshal(ByteBuffer *bs);
  virtual char *getPrintType();
  virtual void reportFaultState(const FaultState& fs) { reportFS(fs); }
}; 


// mediators for Oz arrays
class DictionaryMediator: public ConstMediator, public MutableMediatorInterface{
public:
  DictionaryMediator(TaggedRef t);
  DictionaryMediator(TaggedRef t, AbstractEntity *ae);
  
  virtual AOcallback callback_Write(DssThreadId* id_of_calling_thread,
				    DssOperationId* operation_id,
				    PstInContainerInterface* operation,
				    PstOutContainerInterface*& possible_answer);
  virtual AOcallback callback_Read(DssThreadId* id_of_calling_thread,
				   DssOperationId* operation_id,
				   PstInContainerInterface* operation,
				   PstOutContainerInterface*& possible_answer);
  virtual PstOutContainerInterface *retrieveEntityRepresentation() ;
  virtual PstOutContainerInterface *deinstallEntityRepresentation() ;
  virtual void installEntityRepresentation(PstInContainerInterface*) ;
  virtual char *getPrintType();
  virtual void reportFaultState(const FaultState& fs) { reportFS(fs); }
}; 


// mediators for Oz objects
class ObjectMediator: public ConstMediator, public MutableMediatorInterface{
public:
  ObjectMediator(TaggedRef t);
  ObjectMediator(TaggedRef t, AbstractEntity *ae);
  
  virtual AOcallback callback_Write(DssThreadId* id_of_calling_thread,
				    DssOperationId* operation_id,
				    PstInContainerInterface* operation,
				    PstOutContainerInterface*& possible_answer);
  virtual AOcallback callback_Read(DssThreadId* id_of_calling_thread,
				   DssOperationId* operation_id,
				   PstInContainerInterface* operation,
				   PstOutContainerInterface*& possible_answer);
  virtual void globalize();
  virtual PstOutContainerInterface *retrieveEntityRepresentation();
  virtual void installEntityRepresentation(PstInContainerInterface*);
  virtual void marshal(ByteBuffer *bs);
  virtual char *getPrintType();
  virtual void reportFaultState(const FaultState& fs) { reportFS(fs); }
};


// mediators for Oz variables
class OzVariableMediator: public Mediator, public MonotonicMediatorInterface {
public:
  OzVariableMediator(TaggedRef t);
  OzVariableMediator(TaggedRef t, AbstractEntity *ae);
  
  virtual void globalize();
  virtual void localize();
  virtual char *getPrintType();

  virtual AOcallback callback_Bind(DssOperationId *id,
				   PstInContainerInterface* operation); 
  virtual AOcallback callback_Append(DssOperationId *id,
				     PstInContainerInterface* operation);
  virtual AOcallback callback_Changes(DssOperationId* id,
				      PstOutContainerInterface*& answer);
  virtual PstOutContainerInterface *retrieveEntityRepresentation();
  virtual void installEntityRepresentation(PstInContainerInterface*);
  virtual void reportFaultState(const FaultState& fs) { reportFS(fs); }
}; 

// Note.  Abstract entities of patched variables are always kept
// alive, even when the variable is bound.  The DistributedVarPatches
// do collect their mediators, which forces them to keep the abstract
// entity alive.

#endif
