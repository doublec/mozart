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

  ------------------------------------------------------------------------
*/

#ifndef __FDHOOK__H__
#define __FDHOOK__H__

#ifdef INTERFACE
#pragma interface
#endif

#include "types.hh"

#include "runtime.hh"

inline 
Bool isUnifyCurrentPropagator () {
  Assert (am.currentThread()->isPropagator ());
  return (am.currentThread()->isUnifyThread ());
}

#endif
