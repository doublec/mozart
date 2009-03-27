/*
 *  Main authors:
 *     Gustavo Gutierrez <ggutierrez@cic.puj.edu.co>
 *     Alberto Delgado <adelgado@cic.puj.edu.co> 
 *     Alejandro Arbelaez <aarbelaez@cic.puj.edu.co>
 *
 *  Contributing authors:
 *     Andres Felipe Barco <anfelbar@univalle.edu.co>
 *     Gustavo A. Gomez Farhat <gafarhat@univalle.edu.co>
 *
 *  Copyright:
 *    Alberto Delgado, 2006-2007
 *    Alejandro Arbelaez, 2006-2007
 *    Gustavo Gutierrez, 2006-2007
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


#ifndef __GEOZ_INT_VAR_BUILTINS_CC__
#define __GEOZ_INT_VAR_BUILTINS_CC__

#include "IntVarMacros.hh"

using namespace Gecode;
using namespace Gecode::Int;


/** 
 * \brief Creates a new IntVar variable 
 * 
 * @param 0 The space
 * @param 1 Domain description
 * @param 2 The new variable
 */
OZ_BI_define(new_intvar,1,1)
{
  OZ_declareDetTerm(0,arg);
  if(OZ_label(arg) == AtomCompl) {
    DECLARE_INT_SET(OZ_getArg(arg,0), dom);
    OZ_RETURN(new_GeIntVarCompl(dom));
  }
  DECLARE_INT_SET(arg,dom);   // the domain of the IntVar
  OZ_RETURN(new_GeIntVar(dom));
}
OZ_BI_end

/** 
 * \brief Test whether \a OZ_in(0) is a GeIntVar
 * 
 */
OZ_BI_define(intvar_is,1,1)
{
  OZ_RETURN_BOOL(OZ_isGeIntVar(OZ_in(0)));
}
OZ_BI_end


/** 
 * \brief Returns the Max number that gecode can representate
 * 
 * @param 0 Max integer in c++
 */

OZ_BI_define(int_sup,0,1)
{
  OZ_RETURN_INT(Int::Limits::max);
} 
OZ_BI_end

/** 
 * \brief Returns the Min number that gecode can representate
 * 
 * @param 0 Min integer in c++
 */

OZ_BI_define(int_inf,0,1)
{
  OZ_RETURN_INT(Int::Limits::min);
} 
OZ_BI_end


/** 
 * \brief Returns the minimum element in the domain
 * 
 * @param intvar_getMin 
 * @param 0 A reference to the variable 
 * @param 1 The minimum of the domain 
 */
OZ_BI_define(intvar_getMin,1,1)
{
  DeclareGeIntVar1(0,v);
  OZ_RETURN_INT(v.min());
}
OZ_BI_end

/** 
 * \brief Returns the maximum element in the domain
 * 
 * @param intvar_getMax 
 * @param 0 A reference to the variable 
 * @param 1 The maximum of the domain 
 */
OZ_BI_define(intvar_getMax,1,1)
{
  DeclareGeIntVar1(0,v);
  OZ_RETURN_INT(v.max());
}
OZ_BI_end

/** 
 * \brief Returns the size of the domain
 * 
 * @param OZ_in(0) A reference to the variable 
 * @param OZ_out(0) The domain size
 */
OZ_BI_define(intvar_getSize,1,1)
{
  DeclareGeIntVar1(0,v);
  OZ_RETURN_INT(v.size());
}
OZ_BI_end

/**
 *\brief Returns the oz domain \a OZ_in(0)
 * @param 0 A reference to the variable
 */
/* I have to think in a better way to do this method*/
OZ_BI_define(int_dom,1,1)
{
  if(!OZ_isGeIntVar(OZ_deref(OZ_in(0))))
    OZ_typeError(0,"IntVar");
  IntVar Tmp = get_IntVar(OZ_in(0));
  
  IntVarRanges TmpRange(Tmp);
  int TotalRangs = 0;
  
  for(;TmpRange();++TmpRange,TotalRangs++);
  
  OZ_Term TmpArray[TotalRangs];
  IntVarRanges TmpRange1(Tmp);

  for(int i=0;TmpRange1();++TmpRange1,i++)
    TmpArray[i] = OZ_mkTupleC("#",2,OZ_int(TmpRange1.min()),OZ_int(TmpRange1.max()));

  if(TotalRangs==1) OZ_RETURN(TmpArray[0]);
  OZ_Term DomList = OZ_toList(TotalRangs,TmpArray);
  OZ_RETURN(DomList);
}
OZ_BI_end


/**
 * \brief Returns the oz domain of \a OZ_in(0) in a ordered list of integers
 * @param 0 A reference to the variable
 * @param 1 List that represent the oz domain of the first parameter
 */
OZ_BI_define(int_domList,1,1)
{
  if(!OZ_isGeIntVar(OZ_deref(OZ_in(0))))
    OZ_typeError(0,"IntVar");
  IntVar Tmp = get_IntVar(OZ_in(0));
  IntVarRanges TmpRange(Tmp);
  OZ_Term TmpArray[Tmp.size()];
  int i = 0;
  for(;TmpRange();++TmpRange)
    for(int j=TmpRange.min();j<=TmpRange.max();j++,i++)
      TmpArray[i] = OZ_int(j);
  OZ_Term DomList = OZ_toList(Tmp.size(),TmpArray);
  OZ_RETURN(DomList);
}
OZ_BI_end

/**
 * \brief Returns the next integer that \a OZ_in(1) in the GeIntVar \a OZ_in(0)
 * @param 0 A reference to the variable
 * @param 1 integer
 */

OZ_BI_define(int_nextLarger,2,1)
{
  if(!OZ_isGeIntVar(OZ_deref(OZ_in(0))))
    OZ_typeError(0,"IntVar");
  int Val = OZ_intToC(OZ_in(1));
  IntVar Tmp = get_IntVar(OZ_in(0));
  IntVarRanges TmpRange(Tmp);

  for(;TmpRange(); ++TmpRange) {
    if(TmpRange.min() <= Val && TmpRange.max() > Val)
      OZ_RETURN_INT(Val+1);
    if(TmpRange.min() > Val)
      OZ_RETURN_INT(TmpRange.min());
  }
  return OZ_typeError(0,"The domain does not have a next larger value input");

} 
OZ_BI_end

/**
 * \brief Returns the small integer that \a OZ_in(1) in the GeIntVar \a OZ_in(0)
 * @param 0 A reference to the variable
 * @param 1 integer
 */

OZ_BI_define(int_nextSmaller,2,1)
{
  if(!OZ_isGeIntVar(OZ_deref(OZ_in(0))))
    OZ_typeError(0,"IntVar");
  int Val = OZ_intToC(OZ_in(1));
  IntVar Tmp = get_IntVar(OZ_in(0));
  IntVarRanges TmpRange(Tmp);
  int Min = Gecode::Int::Limits::max;
  if(Tmp.min() >= Val)
    return OZ_typeError(0,"Input value is smaller that domain of input variable");
  for(;TmpRange(); ++TmpRange) {
    if(TmpRange.min() >= Val)
      OZ_RETURN_INT(Min);
    if(TmpRange.min() < Val && TmpRange.max() >= Val)
      OZ_RETURN_INT(Val-1);
    if(TmpRange.max() < Val)
      Min = TmpRange.max();
  }
  Assert(false);
} 
OZ_BI_end


/** 
 * \brief Returns the median of the domain
 * 
 * @param OZ_in(0) A reference to the variable 
 * @param OZ_out(0) The median
 */
OZ_BI_define(intvar_getMed,1,1)
{
  DeclareGeIntVar1(0,v);
  OZ_RETURN_INT(v.med());
}
OZ_BI_end


/** 
 * \brief Returns the width of the domain
 * 
 * @param OZ_in(0) A reference to the variable 
 * @param OZ_out(0) The width
 */
OZ_BI_define(intvar_getWidth,1,1)
{
  DeclareGeIntVar1(0,v);
  OZ_RETURN_INT(v.width());
}
OZ_BI_end

/** 
 * \brief Returns the Regret Min of the domain
 * 
 * @param OZ_in(0) A reference to the variable 
 * @param OZ_out(0) The Regret Min
 */
OZ_BI_define(intvar_getRegretMin,1,1)
{
  DeclareGeIntVar1(0,v);
  OZ_RETURN_INT(IntView(v).regret_min());
}
OZ_BI_end

/** 
 * \brief Returns the Regret Max of the domain
 * 
 * @param OZ_in(0) A reference to the variable 
 * @param OZ_out(0) The Regret Max
 */
OZ_BI_define(intvar_getRegretMax,1,1)
{
  DeclareGeIntVar1(0,v);
  OZ_RETURN_INT(IntView(v).regret_max());
}
OZ_BI_end

/** 
 * \brief Returns the number of propagators associated with this variable
 * 
 * @param OZ_in(0) A reference to the variable 
 * @param OZ_out(0) Number of associated propagators
 */
OZ_BI_define(intvar_propSusp,1,1)
{
  DeclareGeIntVar1(0,v);
  GeVarBase *gv = get_GeVar(OZ_in(0));
  OZ_RETURN_INT(v.degree()-gv->varprops());
}
OZ_BI_end

/**
 * \brief the same that FD.watch.min in mozart
 * @param 0 A reference to the variable
 * @param 1 A reference to the variable (BoolVar)
 * @param 2 Integer
 */

#endif