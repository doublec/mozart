/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: mehl
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  interface of threads (both runnable and non-runnable);
  ------------------------------------------------------------------------
*/

#ifndef __THREADHH
#define __THREADHH


#ifdef INTERFACE
#pragma interface
#endif

#include "oz_cpi.hh"

//
//  On Sparc (v8), most efficient flags are (strictly) between 0x0 and 
// 0x1000. Flags up to 0x1000 and above 0x1000 should not be mixed, 
// because then three instructions are required (for testing, i mean);
enum ThreadFlag {
  //
  T_null   = 0x000000, // no flag is set;

  //
  T_dead   = 0x000001,   // the thread is dead;
  T_prop   = 0x000002,   // "propagated", i.e. the thread is runnable;

  //
  T_p_thr  = 0x000004,   // 'real' propagators by Tobias;
  T_stack  = 0x000008,   // it has an (allocated) stack;

  //
  T_hasjob = 0x000010,   // has more than one job on the stack;
  T_solve  = 0x000020,   // it was created in a search CS 
                           // (former notificationBoard);

  //
  T_ext    = 0x000040,   // an external suspension wrt current search
			   // problem;

  T_unif   = 0x000080,   // the thread is due to a (proper) unification 
                           // of fd variables; 
  T_loca   = 0x000100,   // all variables of this propagator are local;

  //
  T_tag    = 0x000200,   // a special stuff for fd 
			   // (Tobias, please comment?);
  T_ofs    = 0x000400,   // the OFS thread (needed for DynamicArity);

  T_ltq    = 0x000800,   // designates local thread queue

  // debugger
  T_G_trace= 0x010000,   // thread is being traced
  T_G_step = 0x020000,   // step mode turned on
  T_G_stop = 0x040000,   // no permission to run

  //
  T_max    = 0x800000      // MAXIMAL FLAG;
};

//  Types of a suspended thread;
//  ('S_RTHREAD' is a (generically) suspended thread);
#define  S_TYPE_MASK  (T_stack|T_p_thr)
#define  S_WAKEUP     T_null
#define  S_RTHREAD    T_stack
#define  S_PR_THR     T_p_thr


// 
//  Body of the runnable thread;
class RunnableThreadBody {
friend class AM;
friend class Thread;
private:
  TaskStack taskStack;
  union {
    Object *self;              /* the self object pointer     */
    RunnableThreadBody *next;  /* for linking in the freelist */
  } u;
  TaggedRef streamTail;    // the debug stream's tail

public:
  USEHEAPMEMORY;
  // 
  //  Note that 'RunnableThreadBody'"s
  // are allocated in pre-defined regions, and, therefore, 
  // their allocators may not be used.

  void reInit ();		// for the root thread only;

  //  gc methods;
  RunnableThreadBody(int sz) : taskStack(sz) { }
  RunnableThreadBody *gcRTBody();

  void setSelf(Object *o) { Assert(u.self==NULL); u.self = o; }
  void makeRunning();

  TaggedRef getStreamTail()        { return streamTail; }
  void setStreamTail(TaggedRef v)  { streamTail = v; }

  void pushTask(ProgramCounter pc,RefsArray y,RefsArray g,RefsArray x,int i)
  {
    taskStack.pushCont(pc,y,g,x,i);
  }
  void pushTask(SolveActor * sa)
  {
    taskStack.pushLTQ(sa);
  }
};

//
//  
union ThreadBodyItem {
  OZ_Propagator *propagator;
  RunnableThreadBody *threadBody;
};  


//
//  Flags & priority of a thread;
//
//  Every thread can be in the following states - 
// suspended, runnable, running and dead:
//
//  Moreover, only the following transactions are possible:
//
//                    .------------> dead <-----------.
//                    |                               |
//                    |                               |
//  (create) ---> suspended -----> runnable <----> running
//                    ^                               |
//                    |                               |
//                    `-------------------------------'
//
class Thread : public ConstTerm {
  friend void ConstTerm::gcConstRecurse(void);
private:
  //  Sparc, for instance, has a ldsb/stb instructions - 
  // so, this is exactly as efficient as just two integers;
  struct {
    int pri:    sizeof(char) * 8;
    int flags:  (sizeof(int) - sizeof(char)) * sizeof(char) * 8;
  } state;

  TaggedRef cell;
  ThreadBodyItem item;		// NULL if it's a deep 'unify' suspension;

  //  special allocator for thread's bodies;
  void freeThreadBody ();

  //
  void disposeThread ();
  
  Bool wakeUpPropagator (Board *home, 
			 PropCaller calledBy = pc_propagator);
  Bool wakeUpBoard (Board *home);
  Bool wakeUpThread (Board *home);

  // 
  void setExtThreadOutlined (Board *varHome);
  //  it asserts that the suspended thread is 'external' (see beneath);
  void checkExtThreadOutlined ();

protected:

  //  Note that other flags are removed;
  void markDeadThread () { 
    state.flags = state.flags | T_dead;
  }
  //
  //  for 'Thread::print ()';
  int getFlags () { return (state.flags); }

public:
  Thread (int flags, int prio, Board *bb, TaggedRef val);

  USEHEAPMEMORY;
  OZPRINT;
  OZPRINTLONG;

  // 
  Thread *gcThread();
  Thread *gcDeadThread();
  void gcRecurse();

  void setBody(RunnableThreadBody *rb) { item.threadBody=rb; }
  void setInitialPropagator(OZ_Propagator *pro) { item.propagator=pro; }

  TaggedRef getValue() { return cell; }
  void setValue(TaggedRef v) { cell = v; }

  //  priority;
  int getPriority() { 
    Assert ((int)state.pri >= OZMIN_PRIORITY && state.pri <= OZMAX_PRIORITY);
    return (state.pri);
  }
  void setPriority (int newPri) { 
    Assert ((int)state.pri >= OZMIN_PRIORITY && state.pri <= OZMAX_PRIORITY);
    state.pri = newPri;
  }

  //
  void setHasJobs() { 
    Assert (isRunnable ());
    state.flags = state.flags | T_hasjob; 
  }
  void unsetHasJobs() {
    state.flags = state.flags & ~T_hasjob;
  }
  Bool hasJobs () { return (state.flags & T_hasjob); }

  /* check is thread has a stack */
  Bool isRThread() { return (state.flags & S_TYPE_MASK) == S_RTHREAD; }

  //  
  Bool isDeadThread () { return (state.flags & T_dead); }

  //  ... if not dead;
  Bool isSuspended () { 
    Assert (!(isDeadThread ()));
    return (!(state.flags & T_prop));
  }
  Bool isPropagated () { 
    Assert (!(isDeadThread ()));
    return (state.flags & T_prop);
  }
  //  'isRunnable' is just an alias for the 'isPropagated'; 
  Bool isRunnable () { 
    Assert (!(isDeadThread ()));
    return (state.flags & T_prop);
  }

  //  For reinitialisation; 
  void setRunnable () { 
    state.flags = (state.flags & ~T_dead) | T_prop;
  }

  Bool isInSolve () { 
    Assert (!(isDeadThread ()));
    return (state.flags & T_solve);
  }
  void setInSolve () { 
    Assert (isRunnable ());
    state.flags = state.flags | T_solve;
  }

  //  non-runnable threads;
  void markPropagated () {
    Assert (isSuspended () && !(isDeadThread ()));
    state.flags = state.flags | T_prop;
  }
  void unmarkPropagated () { 
    Assert (isPropagated () && !(isDeadThread ()));
    state.flags = state.flags & ~T_prop;
  }

  void setExtThread () { 
    Assert (isSuspended () && !(isDeadThread ()));
    state.flags = state.flags | T_ext;
  }
  Bool isExtThread () { 
    Assert (isRunnable ());
    return (state.flags & T_ext);
  }
  Bool wasExtThread () {
    Assert (isDeadThread ());	// already killed!
    return (state.flags & T_ext);
  }

  void markPropagatorThread () { 
    Assert (!(isDeadThread ()));
    state.flags = state.flags | T_p_thr;
  }
  void unmarkPropagatorThread () { 
    Assert (!(isDeadThread ()));
    state.flags = state.flags & ~T_p_thr;
  }
  Bool isPropagator () { 
    Assert (!(isDeadThread ()));
    return (state.flags & T_p_thr);
  }
  
  void headInitPropagator(void) {
    state.flags = T_p_thr | T_prop | T_unif;
  }

  // debugger
  Bool traceMode() {
    return (state.flags & T_G_trace);
  }
  Bool stepMode() {
    return (state.flags & T_G_step);
  }
  Bool stopped() {
    return (state.flags & T_G_stop);
  }

  void startTraceMode() {
    state.flags = state.flags | T_G_trace;
  }
  void stopTraceMode() {
    state.flags = state.flags & ~T_G_trace;
  }

  void startStepMode() {
    state.flags = state.flags | T_G_step;
  }
  void stopStepMode() {
    state.flags = state.flags & ~T_G_step;
  }
  
  void cont() {
    state.flags = state.flags & ~T_G_stop;
  }
  void stop() {
    state.flags = state.flags | T_G_stop;
  }


  int getThrType () { return (state.flags & S_TYPE_MASK); }

  // 
  //  Convert a thread of any type to a 'wakeup' thread (without a stack).
  //  That's used in GC because thread with a dead home board
  // might not dissappear during GC, but moved to a first alive board,
  // and killed in the emulator.
  void setWakeUpTypeGC () {
    state.flags = (state.flags & ~S_TYPE_MASK) | S_WAKEUP;
  }

  void markUnifyThread () { 
    Assert ((isPropagator ()) && !(isDeadThread ()));
    state.flags = state.flags | T_unif;
  }
  void unmarkUnifyThread () { 
    Assert ((isPropagator ()) && !(isDeadThread ()));
    state.flags = state.flags & ~T_unif;
  }
  Bool isUnifyThread () {
    Assert ((isPropagator ()) && !(isDeadThread ()));
    return (state.flags & T_unif);
  }

  void setOFSThread () {
    Assert ((isPropagator ()) && !(isDeadThread ()));
    state.flags = state.flags | T_ofs;
  }
  Bool isOFSThread () {
    Assert (!(isDeadThread ()));
    return (state.flags & T_ofs);
  }

  // the following six member functions operate on dead threads too
  void markLocalThread () {
    if (isDeadThread()) return;
    state.flags = state.flags | T_loca;
  }
  void unmarkLocalThread () {
    state.flags = state.flags & ~T_loca;
  }
  Bool isLocalThread () {
    return (state.flags & T_loca);
  }

  void markTagged () { 
    if (isDeadThread ()) return;
    state.flags = state.flags | T_tag;
  }
  void unmarkTagged () { 
    state.flags = state.flags & ~T_tag;
  }
  Bool isTagged () { 
    return (state.flags & T_tag);
  }
  // -----
  //  stack;
  Bool hasStack () { 
    Assert (!(isDeadThread ()));
    return (state.flags & T_stack);
  }
  Bool hadStack () { 
    Assert (isDeadThread ());
    return (state.flags & T_stack);
  }
  void setHasStack () { 
    Assert (isRunnable ());
    Assert (!(isPropagator ()));
    state.flags = state.flags | T_stack; 
  }

  //  General;
  //  Zeroth: constructors for various cases;
  void reInit (int prio, Board *home);  // for the root thread only;

  //  Second - get/set the home board;
  Board *getBoardFast ();
  Board *getBoard () { return (Board *) getPtr(); }
  void setBoard (Board *bp) { setPtr(bp); }
  void setSelf(Object *o);

  TaggedRef getStreamTail();
  void setStreamTail(TaggedRef v);

  OZ_Propagator * swapPropagator(OZ_Propagator * p) {
    OZ_Propagator * r = item.propagator;
    item.propagator = p;
    return r;
  }

  void setPropagator(OZ_Propagator * p) {
    Assert(isPropagator());
    delete item.propagator; 
    item.propagator = p;
  }

  OZ_Return runPropagator(void) {
    Assert(isPropagator());
    ozstat.propagatorsInvoked.incf();
    extern char * ctHeap, * ctHeapTop;
    ctHeap = ctHeapTop;
    extern int staticSpawnVarsNumber, staticSuspendVarsNumber;
    staticSpawnVarsNumber = 0, staticSuspendVarsNumber = 0;
    return item.propagator->run();
  }
  OZ_Propagator * getPropagator(void) {
    Assert(isPropagator());
    return item.propagator;
  }
  
  //  Third - transactions between states;
  //  These methods are specialised because the type of suspended thread
  // is known statically;
  void suspThreadToRunnable ();
  void wakeupToRunnable ();
  void propagatorToRunnable ();

  Bool terminate();
  void propagatorToNormal();

  // 
  //  Note that the stack is allocated now in "lazy fashion", i.e. 
  // that it is created only when a thread is getting "running"; 
  //  This can be simply switched off when the stack is allocated 
  // in '<something>ToRunnable ()'; 'makeRunning ()' is getting 
  // empty in this case;
  void makeRunning ();

  // 
  //  Note: killing the suspended thread *might not* make any
  // actor reducible OR reducibility must be tested somewhere else !!!
  //
  //  Invariant: 
  // There can be no threads which are suspended not in its 
  // "proper" home, i.e. not in the comp. space where it is started;
  // 
  //  Note that this is true for wakeups, continuations etc. anyway, 
  // *and* this is also true for threads suspended in the sequential 
  // mode! The point is that whenever a thread tries to suspend in a 
  // deep guard, a new (local) thread is created which carries the 
  // rest of the guard (Hi, Michael!);
  //
  //  Note also that these methods don't decrement suspCounters, 
  // thread counters or whatever because their home board might 
  // not exist at all - such things should be done outside!
  void disposeSuspendedThread ();
  //
  //  It marks the thread as dead and disposes it;
  void disposeRunnableThread ();

  //  
  //  Mark a suspended thread as an 'external' one, 
  // and increcement thread counters in solve actor(s) above 
  // the current board;
  void updateExtThread (Board *varHome);
  //  Check all the solve actors above for stabily 
  // (and, of course, wake them up if needed);
  void checkExtThread ();

  //
  //  Runnable (running) threads;
  Bool discardLocalTasks();
  // 
  //  The iterative procedure which cleans up all the tasks 
  // up to the 'current' board, and sets the 'current' to the 
  // thread's board;
  void cleanUp (Board *current);

  //
  Bool isBelowFailed (Board *top);

  //
  void pushTask(SolveActor * sa) { item.threadBody->pushTask(sa); }
  void pushDebug (OzDebug *d);
  void pushCall (TaggedRef pred, RefsArray  x, int n); 
  void pushCatch(TaggedRef pred) {
    item.threadBody->taskStack.pushCatch(pred);
  }
  TaggedRef findCatch(TaggedRef &traceback) {
    return item.threadBody->taskStack.findCatch(traceback);
  }
  void pushJob();
  void pushSetCaa(AskActor *aa);
  void pushSelf(Object *obj);
  void pushSetModeTop();
  void pushLocal();
  void pushCFunCont (OZ_CFun f, RefsArray  x, int n, Bool copyF);
  void pushCont (ProgramCounter pc, 
		 RefsArray y, RefsArray g, RefsArray x, int n);
  void pushCont (Continuation *cont);
  //
  Bool isEmpty(); 

  //
  void printTaskStack (ProgramCounter pc, 
		       Bool verbose = NO, int depth = 10000);

  TaggedRef dbgGetTaskStack(ProgramCounter pc, int depth = 10000);

  // 
  //  Gets the size of the stack segment with the top-level job, 
  // and *virtually* removes it -- i.e. moves the 'tos' downwards, 
  // while preserving the data. It means in particular, 
  // that it must be followed by the 'getJob ()'!
  int getSeqSize ();
  //  
  //  'getJob' yields a *suspended* thread!
  Thread *getJob ();
  //
  DebugCode (Bool hasJobDebug ();)

  //
  TaskStack *getTaskStackRef ();
  TaskStackEntry *getTop ();
  void setTop (TaskStackEntry *newTos);
  TaskStackEntry pop ();

  /*
   *  propagators special;
   *
   */
  DebugCode 
  (void removePropagator () { 
    Assert (isPropagator ());
    item.propagator = (OZ_Propagator *) NULL;
  })

  //  
  //  (re-)Suspend a propagator again; (was: 'reviveCurrentTaskSusp');
  //  It does not take into account 'solve threads', i.e. it must 
  // be done externally - if needed;
  void suspendPropagator ();
  void scheduledPropagator();

  //
  //  Terminate a propagator thread which is (still) marked as propagated
  // (was: 'killPropagatedCurrentTaskSusp' with some variations);
  //
  //  This might be used only from the local propagation queue,
  // because it doesn't check for entaiment, stability, etc. 
  // Moreover, such threads are NOT counted in solve actors
  // and are not marked as "inSolve" EVEN in the "running" state!
  //
  //  Philosophy (am i right, Tobias?):
  // When some propagator returns 'PROCEED' and still has the 
  // 'propagated' flag set, then it's done.
  void closeDonePropagator ();

  void closeDonePropagatorCD ();
  void closeDonePropagatorThreadCD ();

  // wake up cconts and board conts
  Bool wakeUp (Board *home, PropCaller calledBy);

};


//
const size_t threadBodySize = sizeof (RunnableThreadBody);

inline 
Bool isThread(TaggedRef term)
{
  return isConst(term) && tagged2Const(term)->getType() == Co_Thread;
}

inline
Thread *tagged2Thread(TaggedRef term)
{
  Assert(isThread(term));
  return (Thread *) tagged2Const(term);
}

#ifndef OUTLINE
#include "thread.icc"
#else
#define inline
#endif

#endif
