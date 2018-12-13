/*
 *  Authors:
 *    Kostja Popow (popow@ps.uni-sb.de)
 *    Michael Mehl (mehl@dfki.de)
 *    Christian Schulte <schulte@ps.uni-sb.de>
 * 
 *  Contributors:
 * 
 *  Copyright:
 *    Kostja Popow, 1997-1999
 *    Michael Mehl, 1997-1999
 *    Christian Schulte, 1997-1999
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

#ifndef __BOARDH
#define __BOARDH

// propagation stack for propagators
#define PROPAGATION_STACK

#ifdef INTERFACE
#pragma interface
#endif

#include "thr_class.hh"
#include "distributor.hh"
#include "suspendable.hh"
#include "susp_queue.hh"
#include "pointer-marks.hh"

#define GETBOARD(v) ((v)->getBoardInternal()->derefBoard())

//
enum BoardTags {
  BoTag_Root       = 0x1,
  BoTag_Failed     = 0x2,
  BoTag_Committed  = 0x4,
  BoTag_Mark       = 0x8,
  // 'BoTag_EvenGC' has to be equal to the GCStep's EvenGCStep;
  BoTag_EvenGC     = 0x10,
  BoTag_MAX_UNUSED = 0x20
};
#define BoardTagMask	0x1f
#define BoardTagBits	5

// Remaining bits are used for the numbering of copying steps between
// GC steps.  I (kost@) consider it pretty safe, since on an X bit
// machine it is fairly difficult to make 2 power (X-BoardTagBits)
// copies without a GC step :-)

//
class Board {
  friend int engine(Bool init);
public:
  NO_DEFAULT_CONSTRUCTORS(Board);
  Board(void);
  Board(Board *b);

  USEHEAPMEMORY;

  //
  // Home and parent access
  //
private:
  Board * parent;
  int     flags;

public:  
  //
  // State of the space
  //
  int isRoot(void) {
    return (flags & BoTag_Root);
  }
  int isFailed(void) {
    return (flags & BoTag_Failed);
  }
  int isCommitted(void) {
    return (flags & BoTag_Committed);
  }

  void setCommitted(Board * b) {
    parent = b;
    flags |= BoTag_Committed;
  }
  void setFailed(void) {
    flags |= BoTag_Failed;
  }
  void setParent(Board * p) {
    parent = p;
  }

  //
  // Mark needed during cloning
  //
  int hasMark(void) {
    return flags & BoTag_Mark;
  }
  void setMark(void) {
    flags |= BoTag_Mark;
  }
  void unsetMark(void) {
    flags &= ~BoTag_Mark;
  }

  //
  // GC&copying numbering;
  Bool isEqGCStep(int gct) {
    return ((flags & BoTag_EvenGC) == gct);
  }
  void nextGCStep() {
    flags ^= BoTag_EvenGC;
  }
private:
  void setGCStep(int gct) {
    flags |= gct;
  }
public:
  //
  int getCopyStep() {
    Assert(BoTag_MAX_UNUSED == 0x1<<BoardTagBits);
    return (flags >> BoardTagBits);
  }
  void resetCopyStep() {
    flags &= (BoTag_MAX_UNUSED - 1);
  }
  void setCopyStep(int cs) {
    Assert(BoTag_MAX_UNUSED == 0x1<<BoardTagBits);
    flags = (cs << BoardTagBits) | (flags & BoardTagMask);
  }

  //
  // Parent access
  //
  Board* getParentInternal(void) {
    return parent;
  }
  Board* derefBoard(void) {
    Board *bb;
    for (bb=this; bb->isCommitted(); bb=bb->getParentInternal()) {}
    return bb;
  }
  Board* getParent(void) {
    Assert(!isCommitted());
    return getParentInternal()->derefBoard();
  }
  int isAlive(void) {
    for (Board * s = this; !s->isRoot() ; s=s->getParent())
      if (s->isFailed())
	return NO;
    return OK;
  }
  int isAdmissible(void);

  //
  // Garbage collection and copying
  //
public:
  Bool cacIsMarked(void) {
    return IsMarkedPointer(parent,1);
  }
  Board * cacGetFwd(void) {
    Assert(cacIsMarked());
    return (Board *) UnMarkPointer(parent,1);
  }
  uintptr_t ** cacGetMarkField(void) {
    return (uintptr_t **) &parent;
  }
  void cacMark(Board * fwd) {
    Assert(!cacIsMarked());
    parent = (Board *) MarkPointer(fwd,1);
  }

  Bool    cacIsAlive(void);

  Board * sCloneBoard(void);
  Board * sCloneBoardDo(void);
  void    sCloneRecurse(void);
  void    sCloneMark(Board *);

  Board * gCollectBoard(void);
  Board * gCollectBoardDo(void);
  void    gCollectRecurse(void);
  void    gCollectMark(Board *);

  void unsetGlobalMarks(void);
  void setGlobalMarks(void);
  
  //
  // Local suspendable counter
  //
private:
  int suspCount;

public:  
  void incSuspCount(int n=1) {
    Assert(!isCommitted() && !isFailed());
    suspCount += n;
    Assert(suspCount >= 0);
  }
  void decSuspCount(void) {
    Assert(!isCommitted() && !isFailed());
    Assert(suspCount > 0);
    suspCount--;
  }
  int getSuspCount(void) {
    Assert(!isFailed() && suspCount >= 0);
    return suspCount;
  }
  Bool isStable(void);
  Bool isBlocked(void);
  void checkStability(void);
  void checkExtSuspension(Suspendable *);
  
  //
  // Cascaded runnable thread counter
  //
private:
  int crt;
public:
  int hasRunnableThreads(void) {
    return (crt > 0);
  }
  void incRunnableThreads(void);
  void decRunnableThreads(void);

  //
  // All taggedrefs in a row for garbage collection
  //
private:
  TaggedRef script, status, rootVar, optVar;

  //
  // Script and script installation
  //
private:
  OZ_Return installScript(Bool isMerging);

  Bool installDown(Board *);

  void setScript(TaggedRef s) {
    script = s;
  }

public:
  Bool install(void);

  //
  // Suspension list
  //
private:
  SuspList  *suspList;

public:
  void addSuspension(Suspendable *); 
  SuspList * getSuspList(void) { 
    return suspList; 
  }
  void setSuspList(SuspList *sl) { 
    suspList=sl; 
  }
  Bool isEmptySuspList() { 
    return suspList==0; 
  }
  void clearSuspList(Suspendable *);

  //
  // Propagation queue
  //

private:
#ifdef PROPAGATION_STACK
  SuspStack lpq;
#else
  SuspQueue lpq;
#endif
  static Board * board_served;
  
  void wakeServeLPQ(void);
  void killServeLPQ(void);

public:
  void addToLPQ(Propagator * p) {
    Assert(!isCommitted() && p->getBoardInternal()->derefBoard() == this);
    if (lpq.isEmpty())
      wakeServeLPQ();
    lpq.enqueue(p);
  }
  OZ_Return scheduleLPQ(void);


  //
  // nonmonotonic propagators
  //

private:
  OrderedSuspList * nonMonoSuspList;

public:
  void setNonMono(OrderedSuspList * l) { 
    nonMonoSuspList = l; 
  }
  OrderedSuspList *getNonMono() { 
    return nonMonoSuspList; 
  }
  void addToNonMono(Propagator *);
  void scheduleNonMono(void);

  //
  // distributors
  //
private:
  Distributor * dist;

public:
  Distributor * getDistributor(void) {
    return dist;
  }
  void setDistributor(Distributor * d) {
    dist = d;
  }
  

  //
  // Operations
  //
  Board * clone(void);
  void fail(void);
  OZ_Return commit(TaggedRef,int);
  OZ_Return commit(TaggedRef,int,int);
  void inject(TaggedRef, int arity = 1);
  OZ_Return merge(Board *, Bool);

  //
  // Status variable
  //
public:
  TaggedRef getStatus() { 
    return status; 
  }
  void setStatus(TaggedRef v) { 
    status = v; 
  }
  void bindStatus(TaggedRef t);

  void clearStatus(void);
  void patchAltStatus(int i);

  TaggedRef genSucceeded(Bool);
  TaggedRef genAlt(int);
  TaggedRef genFailed(void);
  TaggedRef genSuspended(TaggedRef);

  //
  // Root variable
  //
public:
  TaggedRef getRootVar() {
    return makeTaggedRef(&rootVar);
  }

  //
  // The template for optimized variables. OptVar"s are local to a
  // space and cannot have suspensions. All OptVar"s in a space share
  // one single var body, whose "master copy" is stored in the space:
  TaggedRef getOptVar() {
    return (optVar);
  }

  // 
  // Installation control
  //
private:
  static Bool _ignoreWakeUp;

public:
  static Bool mustIgnoreWakeUp(void) {
    return Board::_ignoreWakeUp;
  }
  static void ignoreWakeUp(Bool iwu) {
    Board::_ignoreWakeUp = iwu;
  }

  //
  // Checks
  //
  void checkSituatedness(TaggedRef*,TaggedRef*,TaggedRef*);

  //
  // Misc
  //
  
  OZPRINTLONG;

#ifdef DEBUG_CHECK
  void printTree();
#endif

  //
  // Trailing comparison
  //

#ifdef CS_PROFILE
public:
  int32 * orig_start;
  int32 * copy_start;
  int     copy_size;

  TaggedRef getCloneDiff(void);
#endif


};


#ifndef OUTLINE
#include "board.icc"
#endif

#endif
