/*
 *  Main authors:
 *     Raphael Collet <raph@info.ucl.ac.be>
 *     Gustavo Gutierrez <ggutierrez@cic.puj.edu.co>
 *     Alberto Delgado <adelgado@cic.puj.edu.co>
 *
 *  Contributing authors:
 *     
 *
 *  Copyright:
 *     Gustavo Gutierrez, 2006
 *     Alberto Delgado, 2006
 *
 *  Last modified:
 *     $Date$
 *     $Revision$
 *
 *  This file is part of Mozart, an implementation 
 *  of Oz 3:
 *     http://www.mozart-oz.org
 *
 *  See the file "LICENSE" for information on usage and
 *  redistribution of this file, and for a
 *     DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef __GECODE_VAR_HH__
#define __GECODE_VAR_HH__



#include "var_ext.hh"
#include "var_base.hh"
#include "susplist.hh"
#include "unify.hh"
#include "GeSpace.hh"

enum GeVarType {T_GeIntVar, T_GeSetVar, T_GeBoolVar};

/**
   \brief This class is intended to provide access without template
   arguments to methods/functions used by some core and constraint
   system independent functions such as trailing and unification.

   TODO: As GeVar has no longer template arguments, all these ethods
   should be there.
*/
class GeVarBase: public ExtVar {
protected:
  GeVarBase(Board *b, GeVarBase& gv) 
    :  hasDomRefl(gv.hasDomRefl), unifyC(gv.unifyC), ExtVar(b) {}

public:
  GeVarBase(Board *b) : hasDomRefl(false), unifyC(0), ExtVar(b) {}
  
  /**
     \brief Number of propagators associated with a variable. This
     method is used to test for space stability when speculating on
     variables.
  */
  virtual int degree(void) = 0;
  virtual int getIndex(void) = 0;
  virtual void setIndex(int) = 0;
  virtual Gecode::VarImpBase* clone(void) = 0;

  virtual bool hasSameDomain(TaggedRef) = 0;

  virtual int varprops(void) = 0;

  /// \name Reflection mechanisms
  //@{
protected:
  
  /**
     \brief Test whether the variable has a domain reflection
     propagator.  This propagator reflects any domain change in
     mozart. It is useful when a variable is browsed or inspected. It
     is also needed to wake up the supervisor thread if variable is
     not local to the space.
  */
  bool hasDomRefl;
  /**
     \brief Counter for the number of unifications in which this
     variable is involved.
  */
  unsigned int unifyC;

public:
  /** 
      \brief Tests whether this GeVar represents an assigned variable.
  */
  bool hasDomReflector(void) {return hasDomRefl; }
  void markDomReflected(void) { hasDomRefl = true; }

  void unmarkDomReflected(void) { hasDomRefl = false; }

  /**
     \brief Ensures the existence of a dom reflection propagator on
     this variable. A propagator that reflects *ANY* change in the
     domain's variable to mozart. Creates it if needed. This
     propagator is used when speculating (Supervisor Thread) to ensure
     the suspensions of the variable are kicked off.

     Note: This method is virtual because the propagator must reflect
     any changes in the variable domain. At this time gecode does not
     provide a generic propagation condition to represent it for any
     constraint variable. Every specialization of this class should
     provide a method that calls postDomReflector (GeVar.icc) with the
     appropriate template arguments.
  */
  virtual void ensureDomReflection(void) = 0;


  int getUnifyC(void) { return unifyC; }

  virtual OZ_Term getVal(void) = 0;
  //@}
  
  /// \name Variable speculation
  //@{
  /**
     \brief This functions returns a tagged reference to a clone of
     the variable v.
  */
  virtual TaggedRef clone(TaggedRef v) = 0;
  //@}  
};

/** 
 * \brief Abstract class for Oz variables that interface Gecode
 * variables inside a GenericSpace. VarImp is the corresponding
 * Variable implementation (e.g. IntVarImp, SetVarImp) and the
 * propagation condition \a pc is needed to create a domain reflection
 * propagator for this variable.
 */
class GeVar : public GeVarBase {
private:
  GeVarType type;    /// Type of variable (e.g IntVar, SetVar, etc)
protected:
  int index;        /// The index inside the corresponding GenericSpace

  /// Copy constructor
  // TODO: be careful with the use of getBoardInternal as this board
  // migth be merged (see ::merge())
  GeVar(GeVar& gv) : 
    GeVarBase(extVar2Var(&gv)->getBoardInternal()), type(gv.type), 
    index(gv.index)
  {
    // ensure a valid type of variable.
    Assert(type >= T_GeIntVar && type <= T_GeBoolVar);
  }

  void incUnifyC(void) { unifyC++; }

public:
 
  /// \name Constructor
  //@{
  /** 
   * \brief Creates a new Oz variable that references a Gecode variable
   * 
   * @param h The corresponding GeSpace for the variable  
   * @param n The index inside the corresponding GenericSpace
   */
  GeVar(int n, GeVarType t)
    : GeVarBase(oz_currentBoard()), type(t), index(n) 
  {
    Assert(type >= T_GeIntVar && type <= T_GeBoolVar);
  }
  //@}
  
  /// \name Mozart ExtVar stuff  
  //@{
  ExtVarType getIdV() { return OZ_EVAR_GEVAR;}
  virtual ExtVar* gCollectV() = 0;
  virtual ExtVar* sCloneV() = 0;
  
  virtual void gCollectRecurseV() { }
  virtual void sCloneRecurseV() { }
  virtual OZ_Term statusV() = 0;
  virtual VarStatus checkStatusV() {
    return EVAR_STATUS_KINDED;
  }

  int getIndex(void) {return index;}

  void setIndex(int idx) {index = idx;}

  /// Returns the GenericSpace associated with this variable.
  // TODO: be careful with the use of getBoardInternal. (::merge())
  GenericSpace * getGSpace(void) {
    GenericSpace *gs =  
      extVar2Var(this)->getBoardInternal()->getGenericSpace(true);
    Assert(gs);
    return gs;
  }

  /**
     \brief Returns the number of foreign propagators associated with
     a variable. The extra 1 is because of every variable has a val
     reflector associated with the variable.
  */
  virtual int varprops(void) { return hasDomRefl+unifyC+1; }
  

  /// \name Variable Display
  //@{
private:
  /**
     \brief Puts in out a textual representation of the variable.
  */
  virtual void toStream(ostream &out) = 0;
public:
  /**
     \brief Called from Inspector to display variables. This method is
     also called by Show but it does not needs reflection because of
     its nature. 

     TODO: find a solution to not put a domainReflector propagator
     when show is being used
  */
  void printStreamV(ostream &out,int depth);
  //@}

  virtual void disposeV() {
    disposeS();     // free susplist
    // here we can free the memory taken by the variable itself (if we
    // really want to)
  }
  //@}

  /** 
      \brief Add a suspendable to the variable suspension list.  By
      default this function returns PROCEED for an ExtVar. However,
      for constraint variables it should return SUSPEND as buil-tin
      mozart constraints variables (i.e FD, FS). Returning PROCCED
      will lead to an infinite execution of BIwait built-in when it is
      used.
  */ 
  OZ_Return addSuspV(TaggedRef*, Suspendable* susp) {
    extVar2Var(this)->addSuspSVar(susp);
    return SUSPEND;
  }
  
  GeVarType getType(void) { return type; }

  // Method needed to clone pointed gecode variables.
  virtual Gecode::VarImpBase* clone(void) = 0;
 
  virtual bool In(TaggedRef x) = 0;


  virtual bool hasSameDomain(TaggedRef) = 0;

 
  /// \name Unification and Binding.
  //@{
  /**
     \brief Unify lPtr with rPtr.
  */
  OZ_Return unifyV(TaggedRef* lPtr, TaggedRef* rPtr);
  
  /**
     \brief Test whether x and y have an empty intersection.  This
     method is called when unifying two variables and the flag
     am.inEqEq is on. If intersection is empty then there is enough
     information to make the test == fail.
  */
  virtual bool IsEmptyInter(TaggedRef *x, TaggedRef *y) = 0;
  
  /**
     \brief Unification in gecode is ensured by means of an "equality"
     propagator. Equality must be defined according with the
     constraint system. For finite domains, the intersection between
     domains is enough.  This method have to post a propagator in the
     gecode space that only subsumes when both variables get
     determined. It needs to ensure completeness, otherwise
     unification may work unexpectedly.
  */
  virtual void propagator(GenericSpace *s, GeVar* x,  GeVar* y) = 0;
  
  virtual TaggedRef newVar() = 0; 
  virtual bool intersect(TaggedRef x) = 0; 

  
  OZ_Return bindV(TaggedRef*, TaggedRef);
  virtual Bool validV(TaggedRef v) = 0;
  
  virtual Gecode::ModEvent bind(GenericSpace *s, GeVar *v, OZ_Term val) = 0;

  void test();

  //@}

  /// \name Reflection mechanisms
  //@{
public:
  /** 
      \brief Tests whether this GeVar represents an assigned variable.
  */
  virtual bool assigned(void) = 0;
  
  /**
     \brief Ensures the existence of a ValRelection propagator on this
     variable and creates it if needed.
  */
  void ensureValReflection(void);
 
  //@}
};

/**
   \brief Tests if v contains a Gecode Variale.
*/
inline
bool oz_isGeVar(OzVariable *v) {
  return v->getType() != OZ_VAR_EXT ?  false :
    var2ExtVar(v)->getIdV() == OZ_EVAR_GEVAR;
}

/**
   \brief Tests if the OZ_Term t represents a Gecode variable.
*/
inline 
bool oz_isGeVar(OZ_Term t) {
  OZ_Term dt = OZ_deref(t);
  return oz_isExtVar(dt) && oz_getExtVar(dt)->getIdV() == OZ_EVAR_GEVAR;
}

inline 
GeVarBase* get_GeVar(OZ_Term v) {
  OZ_Term ref = OZ_deref(v);
  Assert(oz_isGeVar(ref));
  ExtVar *ev = oz_getExtVar(ref);
  return static_cast<GeVarBase*>(ev);
}

/**
   \brief This Gecode propagator reflects a Gecode variable assignment
   inside Mozart. It wakes up upon determination, and binds the
   corresnding OzVariable. This propagator is posted via the template
   function postValReflector (see GeVar.icc)
*/
template <class View0>
class ValReflector :
  public Gecode::UnaryPropagator<View0, Gecode::PC_GEN_ASSIGNED>
{
protected:
  int index;
  using Gecode::UnaryPropagator<View0, Gecode::PC_GEN_ASSIGNED>::x0;
public:
  ValReflector(GenericSpace* s, View0 v, int idx) :
    Gecode::UnaryPropagator<View0, Gecode::PC_GEN_ASSIGNED>(s, v),
    index(idx) {}

  ValReflector(GenericSpace* s, bool share, ValReflector& p) :
    Gecode::UnaryPropagator<View0, Gecode::PC_GEN_ASSIGNED>(s, share, p),
    index(p.index) {}

  virtual Gecode::Actor* copy(Gecode::Space* s, bool share) {
    return new (s) ValReflector(static_cast<GenericSpace*>(s), share, *this);
  }
  
  static Gecode::Support::Symbol ati(void) {
    return Gecode::Reflection::mangle<View0>("ValReflector");
  }

  virtual Gecode::Reflection::ActorSpec
  spec(const Gecode::Space* home, Gecode::Reflection::VarMap& m) const {
    
    OZ_Term ov = static_cast<const GenericSpace*>(home)->getVarRef2(index);
    
    static char address[2 * sizeof(OZ_Term) + 1]; 
    sprintf(address,"%x",ov);
   
    Gecode::Reflection::ActorSpec spec(ati());
    spec << x0.spec(home, m);
    spec <<  Gecode::Reflection::Arg::newString(address);

    return spec;
  }

  static void
  post(Gecode::Space* home, Gecode::Reflection::VarMap& vars, 
       const Gecode::Reflection::ActorSpec& spec) {
    
    GenericSpace *gs = static_cast<GenericSpace *>(home);
    
    spec.checkArity(2);
    View0 x(home, vars, spec[0]);
    const char * add = spec[1]->toString();
    
    OZ_Term ov;
    sscanf(add,"%x",&ov);

    Assert(oz_isGeVar(ov));
    GeVarBase *var = get_GeVar(ov);
    
    int index = var->getIndex();
    Board *ib = extVar2Var(get_GeVar(ov))->getBoardInternal()->derefBoard();
    if (ib != oz_currentBoard()) {
      //printf("VR First time recreating variable%d\n", ib == oz_currentBoard());
      fflush(stdout);
           
      /*
	Patch ov to contain the index of the new created variable.
      */
      int new_index = gs->getVarsSize();
      var->setIndex(new_index);
    
      /*
	Add a pointer to the Variable implementation to the array of
	constraint variables in the space.
      */
      int i = gs->newVar(static_cast<Gecode::VarImpBase*>(x.var()), ov);
      Assert(new_index == i);
      index = new_index;

      // Change the internal board of the OZ_Term representing the
      // variable
      extVar2Var(get_GeVar(ov))->setHome(oz_currentBoard());
    }
    (void) new (home) ValReflector<View0>(gs,x,index);
  }
  
  virtual OZ_Term getVarRef(GenericSpace* s) {
    return s->getVarRef(index); 
  }

  Gecode::ExecStatus propagate(Gecode::Space* s, Gecode::ModEventDelta){
    
    OZ_Term ref = getVarRef(static_cast<GenericSpace*>(s));

    if (!oz_isGeVar(ref))
      return Gecode::ES_SUBSUMED(this,sizeof(*this));
    
    GenericSpace *gs = static_cast<GenericSpace*>(s);
    GeVarBase *gv = get_GeVar(ref);
    OZ_Term val = gv->getVal();
    
    DEREF(ref,refPtr);
    oz_bindLocalVar(tagged2Var(ref), refPtr, val);

    if(gv->hasDomReflector()) {
      gs->decForeignProps();
    }
    gs->incDetermined();
    gs->decUnifyProps(gv->getUnifyC());
   
    return Gecode::ES_SUBSUMED(this,sizeof(*this));
  } 

};

/*
  \brief This Gecode propagator reflects variable's domain changes to
  mozart.  It wakes up upon domain change, and kicks suspensions. This
  propagator is suscribed to the variable implementation with the high
  propagation condition available for that kind of
  variables. Hopefully this P.C will track any change in the domain of
  the variable.
*/
template <class View0, Gecode::PropCond pc>
class DomReflector :
  public Gecode::UnaryPropagator<View0, pc>
{
protected:
  int index;
   using Gecode::UnaryPropagator<View0,pc>::x0;
public:
  DomReflector(GenericSpace* s, View0 v, int idx) :
    Gecode::UnaryPropagator<View0, pc>(s, v), index(idx) { }

  DomReflector(GenericSpace* s, bool share, DomReflector& p) :
    Gecode::UnaryPropagator<View0, pc>(s, share, p), index(p.index) {}

  virtual Gecode::Actor* copy(Gecode::Space* s, bool share) {
    return new (s) DomReflector(static_cast<GenericSpace*>(s), share, *this);
  }

  static Gecode::Support::Symbol ati(void) {
    return Gecode::Reflection::mangle<View0>("DomReflector");
  }

  virtual Gecode::Reflection::ActorSpec
  spec(const Gecode::Space* home, Gecode::Reflection::VarMap& m) const {
    
    OZ_Term ov = static_cast<const GenericSpace*>(home)->getVarRef2(index);
    
    static char address[2 * sizeof(OZ_Term) + 1];
    sprintf(address,"%x",ov);
   
    Gecode::Reflection::ActorSpec spec(ati());
    spec << x0.spec(home, m);
    spec <<  Gecode::Reflection::Arg::newString(address);
    
    return spec;
  }

  static void
  post(Gecode::Space* home, Gecode::Reflection::VarMap& vars, 
       const Gecode::Reflection::ActorSpec& spec) {

    GenericSpace *gs = static_cast<GenericSpace *>(home);
    
    spec.checkArity(2);
    View0 x(home, vars, spec[0]);
    const char * add = spec[1]->toString();
    
    OZ_Term ov;
    sscanf(add,"%x",&ov);

    Assert(oz_isGeVar(ov));
    GeVarBase *var = get_GeVar(ov);

    int index = var->getIndex();
    Board *ib = extVar2Var(get_GeVar(ov))->getBoardInternal()->derefBoard();
    if (ib != oz_currentBoard()) {
      //printf("DR First time recreating variable%d\n", ib == oz_currentBoard());
      fflush(stdout);
           
      /*
	Patch ov to contain the index of the new created variable.
      */
      int new_index = gs->getVarsSize();
      var->setIndex(new_index);
    
      /*
	Add a pointer to the Variable implementation to the array of
	constraint variables in the space.
      */
      int i = gs->newVar(static_cast<Gecode::VarImpBase*>(x.var()), ov);
      Assert(new_index == i);
      index = new_index;

      // Change the internal board of the OZ_Term representing the
      // variable
      extVar2Var(get_GeVar(ov))->setHome(oz_currentBoard());
    }

    /*
      TODO: Reposition this comment!!
      Explanation: When this propagator is created, there are two
      possibilities: 1) A ValReflector propagator is already created
      by the unreflection mechanism of the space; and 2) It is created
      before the ValReflector corresponding to this variable. 

      What is well known is that when the unreflection process ends,
      for every variable the ValReflector propagator was executed and
      fixed the index inside the GeVar.

      Using the current index as the new inex is not a good idea
      since this propagator should be created before ValReflector. In
      that case, this method must be in charge to add the variable
      pointer and to update the reference in the generic space.
     */ 
    (void) new (home) DomReflector<View0,pc>(gs,x,index);
  }
  
  virtual OZ_Term getVarRef(GenericSpace* s) {return s->getVarRef(index); }

  // this propagator should never fail nor subsume
  Gecode::ExecStatus propagate(Gecode::Space* s, Gecode::ModEventDelta) {  
    OZ_Term ref = getVarRef(static_cast<GenericSpace*>(s));
   
    Assert(oz_isVarOrRef(ref));	
    
    if(oz_isGeVar(ref)) {
      OzVariable *var=extVar2Var(oz_getExtVar(oz_deref(ref)));
      if(var->getBoardInternal() != oz_currentBoard()) return Gecode::ES_FIX;
      SuspList **sl = var->getSuspListRef();
      if(oz_currentBoard()==var->getBoardInternal())
	oz_checkAnySuspensionList(sl, var->getBoardInternal(), pc_all);
    }

    return Gecode::ES_FIX;
  }
};

#include "GeVar.icc"

inline
void checkGlobalVar(OZ_Term v) {
  //- printf("Called checkGlobalVar\n");fflush(stdout);
  Assert(oz_isGeVar(v));
  ExtVar *ev = oz_getExtVar(oz_deref(v));
  if (!oz_isLocalVar(ev)) {
    //- printf("CheckGlobalVar found non local reference\n");fflush(stdout);
    /* This is tha cease when speculating on var v (which contains
       glv).  being v the global var and nlv the new local var
    */

    TaggedRef nlv =  static_cast<GeVarBase*>(ev)->clone(v);
    ExtVar *varTmp = var2ExtVar(tagged2Var(oz_deref(nlv)));
    GeVarBase *gvar = static_cast<GeVarBase*>(varTmp);
    
    //put v [v] in the trail
    TaggedRef nlvAux = oz_deref(nlv);

    Assert(oz_isVar(nlvAux));
    oz_unify(v,nlv);
  }
}

template <class VarImp, Gecode::PropCond pc>
inline
GeVar* get_GeVar(OZ_Term v, bool cgv = true) {
  if (cgv) checkGlobalVar(v);
  OZ_Term ref = OZ_deref(v);
  Assert(oz_isGeVar(ref));
  ExtVar *ev = oz_getExtVar(ref);
  return static_cast<GeVar*>(ev);
}


/*
  Some useful macros used by each constraint system to declare
  variables from OZ_Term's
*/
/* trm is or will be bind to a constraint variable. This macro is used
   to wait for the variable if it is not defined yet (e.g. a free
   var).
*/
#define declareTerm(trm,varName)		\
  TaggedRef varName = (trm);			\
  {						\
    DEREF(varName,varName_ptr);			\
    Assert(!oz_isRef(varName));			\
    if (oz_isFree(varName)) {			\
      oz_suspendOn(makeTaggedRef(varName_ptr));	\
    }}

/**
   Waits until variable at position pos is defined and then declares
   it as an OZ_Term
*/
#define declareInTerm(pos,varName) declareTerm(OZ_in(pos),varName)

/* 
   Some useful macros to declare VarArgs from OZ_terms (list, records, tuples)
   type must be IntVarArgs or BoolVarargs. op is the name of a function, macro to
   declare one variable of the corresponding array type.
*/

#define DECLARE_VARARGS(tIn,array,sp,type,opDecl)			\
  int __x##tIn = 0;							\
  {									\
    declareInTerm(tIn,t);						\
    __x##tIn =  OZ_isLiteral(t) ? 0 : OZ_isCons(t) ? OZ_length(t) : OZ_width(t); \
  }									\
  type array(__x##tIn);							\
  {									\
    int sz;								\
    OZ_Term t = OZ_deref(OZ_in(tIn));					\
    if(OZ_isLiteral(t)) {						\
      sz=0;								\
      Gecode::type _array(sz);						\
      array=_array;							\
    }									\
    else if(OZ_isCons(t)) {						\
      sz = OZ_length(t);						\
      Gecode::type _array(sz);						\
      for(int i=0; OZ_isCons(t); t=OZ_tail(t),i++){			\
	opDecl(OZ_deref(OZ_head(t)),_array,i,sp);			\
      }									\
      array=_array;							\
    }									\
    else if(OZ_isTuple(t)) {						\
      sz=OZ_width(t);							\
      Gecode::type _array(sz);						\
      for(int i=0;i<sz;i++) {						\
	opDecl(OZ_getArg(t,i),_array,i,sp);				\
      }									\
      array=_array;							\
    }									\
    else {								\
      Assert(OZ_isRecord(t));						\
      OZ_Term al = OZ_arityList(t);					\
      sz = OZ_width(t);							\
      Gecode::type _array(sz);						\
      for(int i=0; OZ_isCons(al); al=OZ_tail(al),i++) {			\
	opDecl(OZ_subtree(t,OZ_head(al)),_array,i,sp);			\
      }									\
      array=_array;							\
    }									\
  }

#endif