/*
 *  Authors:
 *    Author's name (Author's email address)
 * 
 *  Contributors:
 *    optional, Contributor's name (Contributor's email address)
 * 
 *  Copyright:
 *    Organization or Person (Year(s))
 * 
 *  Last change:
 *    $Date$ by $Author$
 *    $Revision$
 * 
 *  This file is part of Mozart, an implementation 
 *  of Oz 3:
 *     $MOZARTURL$
 * 
 *  See the file "LICENSE" or
 *     $LICENSEURL$
 *  for information on usage and redistribution 
 *  of this file, and for a DISCLAIMER OF ALL 
 *  WARRANTIES.
 *
 */
/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: tmueller
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  ------------------------------------------------------------------------
*/

/* 
 * THIS FUNCTION HAS BEEN MOVED HERE SINCE GCC UNDER WIN32
 * COULD NOT COMPILE IT CORRECTLY
 */

#define VERBOSE(X) // X

#include <string.h>

#include "sum.hh"
#include "rel.hh"
#include "auxcomp.hh"
#include "arith.hh"

//=============================================================================

OZ_Return LinEqPropagator::propagate(void)
{
  OZ_DEBUGPRINTTHIS("in ");

  int &c = reg_c, &sz = reg_sz, * a = reg_a;

  simplify_on_equality();

  OZ_DEBUGPRINTTHIS("in (after simplify)");

  if (sz == 0) return c ? FAILED : PROCEED;
  if (sz == 2 && c == 0 && (a[0] == -a[1]))
    return replaceBy(reg_x[0], reg_x[1]);
  
  DECL_DYN_ARRAY(OZ_FDIntVar, x, sz);
  PropagatorController_VV P(sz, x);

  DECL_DYN_ARRAY(OZ_Boolean, flag_txl, sz);
  DECL_DYN_ARRAY(OZ_Boolean, flag_txu, sz);

  int i;
  for (i = sz; i--; ) {
    x[i].read(reg_x[i]);
#ifndef FLAGS
    flag_txl[i] = flag_txu[i] = OZ_TRUE;
#else
    flag_txl[i] = flag_txu[i] = OZ_FALSE;
#endif
  }

#ifdef FLAGS
  VERBOSE(cout << endl << "in: " << *this << endl <<flush);

  for (i = sz; i--; ) {
    if (_l[i] < x[i]->getMinElem()) {

      VERBOSE(cout << _l[i] << "<" << x[i]->getMinElem() << " " << flush);

      for (int j = sz; j--; ) {
	if (i != j) flag_txl[j] |= is_recalc_txl_lower(j, i, a);
	if (i != j) flag_txu[j] |= is_recalc_txu_lower(j, i, a); 
      }
    }
    
    if (_u[i] > x[i]->getMaxElem()) {

      VERBOSE(cout << _u[i] << ">" << x[i]->getMaxElem() << " " << flush);
      
      for (int j = sz; j--; ) {
	if (i != j) flag_txl[j] |= is_recalc_txl_upper(j, i, a);
	if (i != j) flag_txu[j] |= is_recalc_txu_upper(j, i, a);
      }
    }
  }
  if (0) { 
#else
  if (reg_sz > CACHESLOTSIZE + 2) { 
#endif
    // 
    // uncached part
    //

    int cache_slot_num = find_cache_slot_num(sz);
    DECL_DYN_ARRAY(int, cache_slot_from, cache_slot_num);
    DECL_DYN_ARRAY(int, cache_slot_to, cache_slot_num);
    DECL_DYN_ARRAY(double, tx_pos_cache, cache_slot_num);
    DECL_DYN_ARRAY(double, tx_neg_cache, cache_slot_num);
    
    init_cache_slot_index(sz, cache_slot_num, cache_slot_from, cache_slot_to);
    
    for (i = cache_slot_num; i--; ) 
      update_cache(i, a, x, cache_slot_from, cache_slot_to, 
		   tx_pos_cache, tx_neg_cache);
    
    OZ_Boolean repeat_outer;
    
    do {
      repeat_outer = OZ_FALSE;
      
      for (int slot = cache_slot_num; slot--; ) {
	int cache_to = cache_slot_to[slot];
	double tx_neg_cache_v = precalc_lin(cache_slot_num, slot, tx_neg_cache, c);
	double tx_pos_cache_v = precalc_lin(cache_slot_num, slot, tx_pos_cache, c);
      loop1:      
	for (i = cache_slot_from[slot]; i < cache_to; i += 1) {
	  //	  OZ_Boolean repeat_inner = OZ_FALSE;
	  
	  if (flag_txl[i]) {
	    int txl_i = calc_txl_lin(i, slot, a, x, 
				     cache_slot_from, cache_slot_to, 
				     tx_neg_cache_v, tx_pos_cache_v);
	    
	    if (txl_i > x[i]->getMinElem()) {
	      FailOnEmpty(*x[i] >= txl_i);
	      for (int j = sz; j--; ) {
		flag_txl[j] |= is_recalc_txl_lower(j, i, a);
		flag_txu[j] |= is_recalc_txu_lower(j, i, a); 
	      }
	      repeat_outer = OZ_TRUE;
	    }
	    flag_txl[i] = OZ_FALSE;
	  }
	  /*
	  if (repeat_inner) {
	    repeat_outer = OZ_TRUE;
	    //goto loop1;
	  }
	  */
	} // for
      loop2:      
	for (i = cache_slot_from[slot]; i < cache_to; i += 1) {
	  //	  OZ_Boolean repeat_inner = OZ_FALSE;
	  
	  if (flag_txu[i]) {
	    int txu_i = calc_txu_lin(i, slot, a, x, 
				     cache_slot_from, cache_slot_to, 
				     tx_pos_cache_v, tx_neg_cache_v);
	    
	    if (txu_i < x[i]->getMaxElem()) {
	      FailOnEmpty(*x[i] <= txu_i);
	      for (int j = sz; j--; ) {
		flag_txl[j] |= is_recalc_txl_upper(j, i, a);
		flag_txu[j] |= is_recalc_txu_upper(j, i, a);
	      }
	      repeat_outer = OZ_TRUE;
	    }
	    flag_txu[i] = OZ_FALSE;
	  }
	  /*
	  if (repeat_inner) {
	    repeat_outer = OZ_TRUE;
	    //goto loop2;
	  }
	  */
	} // for
	
	if (repeat_outer)
	  update_cache(slot, a, x, cache_slot_from, cache_slot_to, 
		       tx_pos_cache, tx_neg_cache);
	
      } // for (int slot = ...
    } while (repeat_outer);
  } else {
    // 
    // uncached part
    //

    OZ_Boolean repeat_outer;
    
    do {
      repeat_outer = OZ_FALSE;
      
    loop3:      
      for (i = sz; i--; ) {
	//	OZ_Boolean repeat_inner = OZ_FALSE;
	
	if (flag_txl[i]) {
#ifdef FLAGS
	  VERBOSE(cout << "-" << flush);
#endif
	  
	  int txl_i = calc_txl_lin(i, sz, a, x, c);
	  
	  if (txl_i > x[i]->getMinElem()) {
#ifdef FLAGS
	    VERBOSE(cout << "!" << flush);
#endif
	    FailOnEmpty(*x[i] >= txl_i);
	    for (int j = sz; j--; ) {
	      flag_txl[j] |= is_recalc_txl_lower(j, i, a);
	      flag_txu[j] |= is_recalc_txu_lower(j, i, a); 
	    }
	    repeat_outer = OZ_TRUE;
	  }
	  flag_txl[i] = OZ_FALSE;
	}
#ifdef FLAGS
	VERBOSE(else cout << "." << flush);
#endif
	/*
	if (repeat_inner) {
	  repeat_outer = OZ_TRUE;
	  //goto loop3;
	}
	*/
      } // for
    loop4:      
      for (i = sz; i--; ) {
	//OZ_Boolean repeat_inner = OZ_FALSE;
	
	if (flag_txu[i]) {
#ifdef FLAGS
	  VERBOSE(cout << "+" << flush);
#endif
	  int txu_i = calc_txu_lin(i, sz, a, x, c);
	  
	  if (txu_i < x[i]->getMaxElem()) {
#ifdef FLAGS
	    VERBOSE(cout << "!" << flush);
#endif
	    FailOnEmpty(*x[i] <= txu_i);
	    for (int j = sz; j--; ) {
	      flag_txl[j] |= is_recalc_txl_upper(j, i, a);
	      flag_txu[j] |= is_recalc_txu_upper(j, i, a);
	    }
	    repeat_outer = OZ_TRUE;
	  }
	  flag_txu[i] = OZ_FALSE;
	}
#ifdef FLAGS
	VERBOSE(else cout << "." << flush);
#endif
	/*
	  if (repeat_inner) {
	  repeat_outer = OZ_TRUE;
	  goto loop4;
	  }
	  */
      } // for 
#ifdef FLAGS
      VERBOSE(cout << "   " << flush);
#endif

    } while (repeat_outer);
  }
  OZ_DEBUGPRINTTHIS("out ");

#ifdef FLAGS
      VERBOSE(cout << endl << flush);
      for (i = reg_sz; i--; ) {
	_l[i] = x[i]->getMinElem();
	_u[i] = x[i]->getMaxElem();
      }
  VERBOSE(cout << "out: " << *this << endl <<flush);
#endif
  
  return P.leave();
  
failure:
  OZ_DEBUGPRINT(("fail"));

#ifdef FLAGS
  VERBOSE(cout << "fail: " << *this << endl <<flush);
#endif
  
  return P.fail();
}





