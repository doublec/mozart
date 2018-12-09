/*
 *  Authors:
 *    Kostja Popow (popow@ps.uni-sb.de)
 *    Christian Schulte <schulte@ps.uni-sb.de>
 * 
 *  Copyright:
 *    Kostja Popov, 1999
 *    Christian Schulte, 1999
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

#ifndef __TRAILH
#define __TRAILH

#ifdef INTERFACE
#pragma interface
#endif

#include "base.hh"
#include "stack.hh"


enum TeType {
  Te_Mark     = 0,
  Te_Bind     = 1,
  Te_Variable = 2,
};

class Trail: public Stack {

public:
  Trail(void): Stack(DEFAULT_TRAIL_SIZE, Stack_WithMalloc) {
    Stack::push((StackEntry) Te_Mark);
  }
  

  /*
   * Tests
   *
   */

  TeType getTeType(void) {
    return (TeType) (int) (uintptr_t) Stack::topElem();
  }

  Bool isEmptyChunk() { 
    return getTeType() == Te_Mark;
  }


  /*
   * Pushing
   *
   */
  void test(void);

  void pushMark(void);

  void pushBind(TaggedRef *);

  void pushVariable(TaggedRef *);


  /*
   * Popping
   *
   */

  void popMark(void);

  void popBind(TaggedRef *&, TaggedRef &);

  void popVariable(TaggedRef *&, OzVariable *&);
  

  /*
   * Unwinding
   *
   */

  TaggedRef unwind(Board *);

  void unwindFailed(void);

  void unwindEqEq(void);

};

extern Trail trail;


#endif
