/* -*- C++ -*-
 *  Authors:
 *    Per Brand (perbrand@sics.se)
 *    Konstantin Popov <kost@sics.se>
 *    Michael Mehl (mehl@dfki.de)
 *    Ralf Scheidhauer (Ralf.Scheidhauer@ps.uni-sb.de)
 *    Erik Klintskog (erik@sics.se) 
 * 
 *  Contributors:
 *    Andreas Sundstroem <andreas@sics.se>
 * 
 *  Copyright:
 *    Per Brand, 1998
 *    Konstantin Popov, 1998-2000
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
#pragma implementation "dpMarshaler.hh"
#endif

#include "base.hh"
#include "dpBase.hh"
#include "perdio.hh"
#include "msgType.hh"
#include "table.hh"
#include "dpMarshaler.hh"
#include "dpInterface.hh"
#include "var.hh"
#include "var_obj.hh"
#include "var_readonly.hh"
#include "var_class.hh"
#include "gname.hh"
#include "state.hh"
#include "port.hh"
#include "dpResource.hh"
#include "boot-manager.hh"

//
// #define DBG_TRACE

//
#if defined(DBG_TRACE)
static char dbgname[128];
static FILE *dbgout = (FILE *) 0;

#define DBGINIT()						\
  if (dbgout == (FILE *) 0) {					\
    sprintf(dbgname, "/tmp/dpm-dbg-%d.txt", osgetpid());       	\
    dbgout = fopen(dbgname, "a+");				\
  }

#define DBG_TRACE_CODE(C) C
#else
#define DBG_TRACE_CODE(C)
#endif

// The count of marshaled diffs should be done for the distributed
// messages and not for other marshaled structures. 
// Erik
inline
void marshalDIFcounted(MarshalerBuffer *bs, MarshalTag tag)
 {
  dif_counter[tag].send();
  marshalDIF(bs,tag);
}


/**********************************************************************/
/*  basic borrow, owner */
/**********************************************************************/

//
static inline 
void marshalOwnHead(MarshalerBuffer *bs, OB_TIndex i)
{
  PD((MARSHAL_CT,"OwnHead"));
  bs->put(DIF_SITE_SENDER);
  OwnerEntry *oe = ownerIndex2ownerEntry(i);
  marshalNumber(bs, oe->getExtOTI());
  bs->put((BYTE) ENTITY_NORMAL);
  marshalCredit(bs, oe->getCreditBig());
}

//
static inline 
void saveMarshalOwnHead(OB_TIndex oti, RRinstance *&c)
{
  c = ownerIndex2ownerEntry(oti)->getCreditBig();
}

//
static inline 
void marshalOwnHeadSaved(MarshalerBuffer *bs, OB_TIndex oti, RRinstance *c)
{
  PD((MARSHAL_CT,"OwnHead"));
  bs->put(DIF_SITE_SENDER);
  OwnerEntry *oe = ownerIndex2ownerEntry(oti);
  marshalNumber(bs, oe->getExtOTI());
  bs->put((BYTE) ENTITY_NORMAL);
  marshalCredit(bs, c);
}

//
static inline 
void discardOwnHeadSaved(OB_TIndex oti, RRinstance *c)
{
  ownerIndex2ownerEntry(oti)->mergeReference(c);
}

//
static inline
void marshalToOwner(MarshalerBuffer *bs, OB_TIndex bi)
{
  PD((MARSHAL,"toOwner"));
  BorrowEntry *b = borrowIndex2borrowEntry(bi); 
  Ext_OB_TIndex OTI = b->getExtOTI();
  marshalCreditToOwner(bs, b->getSmallReference(), OTI);
}

//
// 'saveMarshalToOwner'/'marshalToOwnerSaved' are complimentary. These
// are used for immediate exportation of variable proxies and
// marshaling corresponding "exported variable proxies" later.
static inline
void saveMarshalToOwner(OB_TIndex bi, Ext_OB_TIndex &oti, RRinstance *&c)
{
  PD((MARSHAL,"toOwner"));
  BorrowEntry *b = borrowIndex2borrowEntry(bi); 

  //
  oti = b->getExtOTI();
  c = b->getSmallReference();
}

//
static inline
void marshalToOwnerSaved(MarshalerBuffer *bs, RRinstance  *c, Ext_OB_TIndex oti)
{
  marshalCreditToOwner(bs, c, oti);
}

//
static inline
void marshalBorrowHead(MarshalerBuffer *bs, OB_TIndex bi, BYTE ec)
{
  PD((MARSHAL,"BorrowHead"));	
  BorrowEntry *b = borrowIndex2borrowEntry(bi);
  NetAddress *na = b->getNetAddress();
  na->site->marshalDSite(bs);
  marshalNumber(bs, Ext_OB_TIndex2Int(na->index));
  bs->put(ec);
  marshalCredit(bs, b->getBigReference());
}

//
static inline
void saveMarshalBorrowHead(OB_TIndex bi, DSite* &ms, Ext_OB_TIndex &oti,
			   RRinstance *&c)
{
  PD((MARSHAL,"BorrowHead"));

  BorrowEntry *b = borrowIndex2borrowEntry(bi);
  NetAddress *na = b->getNetAddress();

  //
  ms = na->site;
  oti = na->index;
  //
  c = b->getBigReference();
}

//
static inline
void marshalBorrowHeadSaved(MarshalerBuffer *bs, DSite *ms,
			    Ext_OB_TIndex oti, RRinstance *c, BYTE ec)
{
  ms->marshalDSite(bs);
  marshalNumber(bs, Ext_OB_TIndex2Int(oti));
  bs->put((BYTE) ec);
  //
  marshalCredit(bs, c);
}

//
// The problem with borrow entries is that they can go away.
static inline 
void discardBorrowHeadSaved(DSite *ms, Ext_OB_TIndex oti, RRinstance *credit)
{
  BorrowEntry *b = borrowTable->find(oti, ms);

  //
  if (b) {
    // still there - then just nail credits back;
    b->mergeReference(credit);
  } else {
    sendRRinstanceBack(ms,oti,credit);
  }
}


//
// Variables - immediate ones and patches;
//
extern Bool globalRedirectFlag;

//
MgrVarPatch::MgrVarPatch(OZ_Term locIn, OzValuePatch *nIn,
			 ManagerVar *mv, DSite *dest)
  : OzValuePatch(locIn, nIn), isMarshaled(NO)
{
  Assert(mv->getIdV() == OZ_EVAR_MANAGER);

  //
  oti = mv->getIndex();
  saveMarshalOwnHead(oti, remoteRef);
  if ((USE_ALT_VAR_PROTOCOL) && (globalRedirectFlag == AUT_REG)) {
    tag = mv->isReadOnly() ? DIF_READONLY_AUTO : DIF_VAR_AUTO;
    mv->registerSite(dest);
  } else {
    tag = mv->isReadOnly() ? DIF_READONLY : DIF_VAR;
  }
}

//
void MgrVarPatch::disposeV()
{
  if (!isMarshaled)
    discardOwnHeadSaved(oti, remoteRef);
  disposeOVP();
  DebugCode(isMarshaled = OK;);
  DebugCode(oti = (OB_TIndex) -1;);
  DebugCode(remoteRef = (RRinstance *) -1;);
  DebugCode(tag = (MarshalTag) -1;);
  oz_freeListDispose(extVar2Var(this), extVarSizeof(MgrVarPatch));
}

//
// An index, if any, is marshaled *afterwards*;
void MgrVarPatch::marshal(ByteBuffer *bs, Bool hasIndex)
{
  DebugCode(PD((MARSHAL,"var manager oti:%d", oti)););
  // Should not be seen again: 'processVar()' must recognize
  // co-references, and marshal a corresponding REF;
  Assert(isMarshaled == NO);
  //
  marshalDIFcounted(bs, (hasIndex ? defmap[tag] : tag));
  marshalOwnHeadSaved(bs, oti, remoteRef);
  isMarshaled = OK;
}

//
PxyVarPatch::PxyVarPatch(OZ_Term locIn, OzValuePatch *nIn,
			 ProxyVar *pv, DSite *dest)
  : OzValuePatch(locIn, nIn), isMarshaled(NO)
{
  Assert(pv->getIdV() == OZ_EVAR_PROXY);
  OB_TIndex bi = pv->getIndex();
  DebugCode(bti = bi;);

  //
  PD((MARSHAL,"var proxy bi: %d", bi));
  isReadOnly = pv->isReadOnly();
  ms = borrowTable->getOriginSite(bi);
  if (dest && ms == dest) {
    isToOwner = OK;
    saveMarshalToOwner(bi, oti, remoteRef);
  } else {
    isToOwner = NO;
    saveMarshalBorrowHead(bi, ms, oti, remoteRef);
    ec = pv->getInfo() ?
      (pv->getInfo()->getEntityCond()&PERM_FAIL) : ENTITY_NORMAL;
  }
}

//
void PxyVarPatch::disposeV()
{
  if (!isMarshaled)
    discardBorrowHeadSaved(ms, oti, remoteRef);
  disposeOVP();
  DebugCode(isMarshaled = OK;);
  DebugCode(oti = (Ext_OB_TIndex) -1;);
  DebugCode(remoteRef = (RRinstance *) -1;);
  DebugCode(ms = (DSite *) -1;);
  DebugCode(isReadOnly = isToOwner = -1;);
  DebugCode(ec = (BYTE) -1;);
  oz_freeListDispose(extVar2Var(this), extVarSizeof(PxyVarPatch));
}

//
// An index, if any, is marshaled *afterwards*;
void PxyVarPatch::marshal(ByteBuffer *bs, int hasIndex)
{
  Assert(isMarshaled == NO);
  isMarshaled = OK;
  //
  if (isToOwner) {
    marshalDIFcounted(bs, (hasIndex ? DIF_OWNER_DEF : DIF_OWNER));
    marshalToOwnerSaved(bs, remoteRef, oti);
  } else {
    marshalDIFcounted(bs, (isReadOnly ? 
			   (hasIndex ? DIF_READONLY_DEF : DIF_READONLY) :
			   (hasIndex ? DIF_VAR_DEF : DIF_VAR)));
    marshalBorrowHeadSaved(bs, ms, oti, remoteRef, ec);
  }
}

//
// An index, if any, is marshaled *afterwards*;
void ManagerVar::marshal(ByteBuffer *bs, Bool hasIndex)
{
  Assert(getIdV() == OZ_EVAR_MANAGER);
  OB_TIndex oti = getIndex();
  PD((MARSHAL,"var manager o:%d", oti));
  if ((USE_ALT_VAR_PROTOCOL) && (globalRedirectFlag == AUT_REG)) {
    MarshalTag tag = (isReadOnly() ? 
		      (hasIndex ? DIF_READONLY_AUTO_DEF : DIF_READONLY_AUTO) :
		      (hasIndex ? DIF_VAR_AUTO_DEF : DIF_VAR_AUTO));
    marshalDIFcounted(bs, tag);
    marshalOwnHead(bs, oti);
    registerSite(bs->getSite());
  } else {
    MarshalTag tag = (isReadOnly() ? 
		      (hasIndex ? DIF_READONLY_DEF : DIF_READONLY) :
		      (hasIndex ? DIF_VAR_DEF : DIF_VAR));
    marshalDIFcounted(bs, tag);
    marshalOwnHead(bs, oti);
  }
}

//
// An index, if any, is marshaled *afterwards*;
void ProxyVar::marshal(ByteBuffer *bs, Bool hasIndex)
{
  Assert(getIdV() == OZ_EVAR_PROXY);
  DSite *dest = bs->getSite();
  OB_TIndex bti = getIndex();
  PD((MARSHAL,"var proxy o:%d", bti));

  if (dest && dest == borrowTable->getOriginSite(bti)) {
    marshalDIFcounted(bs, (hasIndex ? DIF_OWNER_DEF : DIF_OWNER));
    marshalToOwner(bs, bti);
  } else {
    MarshalTag tag = (isReadOnly() ? 
		      (hasIndex ? DIF_READONLY_DEF : DIF_READONLY) :
		      (hasIndex ? DIF_VAR_DEF : DIF_VAR));
    BYTE ec = (getInfo() ?
	       (getInfo()->getEntityCond() & PERM_FAIL) : ENTITY_NORMAL);
    marshalDIFcounted(bs, tag);
    marshalBorrowHead(bs, bti, ec);
  }
}

//
// kost@ : both 'DIF_VAR_OBJECT' and 'DIF_STUB_OBJECT' (currently)
// have the "tertiary" representation (and, thus, are unmarshaled
// using 'unmarshalTertiary'). An index, if any, is marshaled
// *afterwards*;
void ObjectVar::marshal(ByteBuffer *bs, Bool hasCoRefsIndex)
{
  PD((MARSHAL,"var objectproxy"));
  GName *classgn = getGNameClass();

  //
  DSite* sd = bs->getSite();
  if (sd && borrowTable->getOriginSite(index) == sd) {
    marshalDIFcounted(bs, (hasCoRefsIndex ? DIF_OWNER_DEF : DIF_OWNER));
    marshalToOwner(bs, index);
  } else {
    Assert(gname);
    Assert(classgn);

    // ERIK, entity condition not yet taken care of
    marshalDIFcounted(bs, (hasCoRefsIndex ?
			   DIF_VAR_OBJECT_DEF : DIF_VAR_OBJECT));
    marshalBorrowHead(bs, index, ENTITY_NORMAL);
    marshalGName(bs, gname);
    marshalGName(bs, classgn);
  }
}

//
// 'dpMarshalLitCont' marshals a string (which is passed in as 'arg')
// as long as the buffer allows. If the string does not fit
// completely, then marshaling suspends. Note that the string fragment
// looks exactly as a string!
//
static void dpMarshalLitCont(GenTraverser *gt, GTAbstractEntity *arg);
//
static inline 
void dpMarshalString(ByteBuffer *bs, GenTraverser *gt,
		     DPMarshalerLitSusp *desc)
{
  const char *name = desc->getRemainingString();
  int nameSize = desc->getCurrentSize();
  int availSpace = bs->availableSpace() - MNumberMaxSize;
  Assert(availSpace >= 0);

  //
  int ms = min(nameSize, availSpace);
  marshalStringNum(bs, name, ms);
  // If it didn't fit completely, put the continuation back:
  if (ms < nameSize) {
    desc->incIndex(ms);
    gt->suspendAC(dpMarshalLitCont, desc);
  } else {
    delete desc;
  }
}

//
static
void dpMarshalLitCont(GenTraverser *gt, GTAbstractEntity *arg)
{
  Assert(arg->getType() == GT_LiteralSusp);
  ByteBuffer *bs = (ByteBuffer *) gt->getOpaque();
  // we should advance:
  Assert(bs->availableSpace() > 2*DIFMaxSize + MNumberMaxSize);

  //
  dif_counter[DIF_LIT_CONT].send();
  marshalDIFcounted(bs, DIF_LIT_CONT);
#if defined(DBG_TRACE)
  {
    DBGINIT();
    DPMarshalerLitSusp *desc = (DPMarshalerLitSusp *) arg;
    char buf[10];
    buf[0] = (char) 0;
    strncat(buf, desc->getRemainingString(),
	    min(10, desc->getCurrentSize()));
    fprintf(dbgout, "> tag: %s(%d) = %.10s\n",
	    dif_names[DIF_LIT_CONT].name, DIF_LIT_CONT, buf);
    fflush(dbgout);
  }
#endif
  dpMarshalString(bs, gt, (DPMarshalerLitSusp *) arg);
}

//
#define MarshalRHTentry(entity, index)					\
{									\
  Ext_OB_TIndex rhtIndex = RHT->find(entity);				\
  if (rhtIndex == (Ext_OB_TIndex) RESOURCE_NOT_IN_TABLE) {		\
    OwnerEntry *oe_manager;						\
    (void) ownerTable->newOwner(oe_manager);				\
    rhtIndex = oe_manager->getExtOTI();					\
    PD((GLOBALIZING,"GLOBALIZING Resource index:%d", rhtIndex));	\
    oe_manager->mkRef(entity);						\
    RHT->add(entity, rhtIndex);						\
  }									\
  marshalDIFcounted(bs, index ? DIF_RESOURCE_DEF : DIF_RESOURCE);	\
  marshalOwnHead(bs, OT->extOTI2ownerEntry(rhtIndex));			\
  if (index) marshalTermDef(bs, index);					\
}

//
#include "dpMarshalcode.cc"

//
// Marshaling of "hash table references" (for "match" instructions)
// into a limited space buffer is special: it can be partial, in which
// case marshaling of the code area suspends *at the instruction* that
// referenced that hash table (and not at the next one, as usual);
//
// It returns 'OK' when marshaling [of a hash table] was partial.
// In other words, 'OK' means that (a) marshaling of the code area has
// to suspend, and (b) it has to do so on the current instruction.
Bool dpMarshalHashTableRef(GenTraverser *gt,
			   DPMarshalerCodeAreaDescriptor *desc,
			   int start, IHashTable *table,
			   ByteBuffer *bs)
{
  // Make a worst-case estimation for the number of entries we can fit
  // into the remaining space in the in the buffer.
  const int nDone = desc->getHTNDone();
  // 'nDone' is non-zero when marshaling of a hash table is continued;
  const int tEntries = table->getEntries();  // Total Entries;
  Assert(tEntries > 0);
  const int rEntries = tEntries - nDone;     // Remaining Entries;
  const int wcFit =		// how many entries would fit;
    min((bs->availableSpace() - 4*MNumberMaxSize) / (4*MNumberMaxSize),
	rEntries);
  Assert(wcFit > 0);
  // Note: that field is present but not used by the pickler;
  marshalNumber(bs, wcFit);
#if defined(DBG_TRACE)
  DBGINIT();
  fprintf(dbgout,
	  ">  hash table=%p, nDone=%d, tEntries=%d, wcFit=%d\n",
	  table, nDone, tEntries, wcFit);
  fflush(dbgout);
#endif

  //
  int hti;
  if (nDone == 0) {
    marshalLabel(bs, start, table->lookupElse());
    marshalLabel(bs, start, table->lookupLTuple());
    marshalNumber(bs, tEntries);
    hti = table->getSize();
  } else {
    marshalLabel(bs, start, 0);
    marshalLabel(bs, start, 0);
    marshalNumber(bs, tEntries);
    hti = desc->getHTIndex();
  }    

  //
  int num = wcFit;
  // Tests in this order! 
  // If we suspend, we want to continue with the first unprocessed index;
  while (num > 0 && hti-- > 0) {
    if (table->entries[hti].val) {
      if (oz_isLiteral(table->entries[hti].val)) {
	if (table->entries[hti].sra == mkTupleWidth(0)) {
	  // That's a literal entry
	  marshalNumber(bs, ATOMTAG);
	  marshalLabel(bs, start, table->entries[hti].lbl);
	  gt->traverseOzValue(table->entries[hti].val);
	} else {
	  // That's a record entry
	  marshalNumber(bs, RECORDTAG);
	  marshalLabel(bs,start, table->entries[hti].lbl);
	  gt->traverseOzValue(table->entries[hti].val);
	  marshalRecordArity(gt, table->entries[hti].sra, bs);
	}
      } else {
	Assert(oz_isNumber(table->entries[hti].val));
	// That's a number entry
	marshalNumber(bs,NUMBERTAG);
	marshalLabel(bs, start, table->entries[hti].lbl);
	gt->traverseOzValue(table->entries[hti].val);
      }
      num--;
    }
  }

  //
  Assert(num == 0);
  // if we suspend, then there are some unprocessed indexes left:
  DebugCode(const Bool ret = (wcFit < rEntries) ? OK : NO;);
  Assert(ret == NO || hti > 0);
  // When we finish, the last index we checked is not necessarily 0;

  //
  if (wcFit < rEntries) {
    desc->setHTIndex(hti);
    desc->setHTNDone(nDone + wcFit);
    DebugCode(desc->setHTREntries(rEntries));
    DebugCode(desc->setHTable(table));
    return (OK);
  } else {
    DebugCode(desc->setHTIndex(-1));
    DebugCode(desc->setHTREntries(-1));
    DebugCode(desc->setHTable((IHashTable *) -1));
    desc->setHTNDone(0);
    return (NO);
  }
}

#if defined(DEBUG_CHECK)
void DPMarshaler2ndP::vHook()
{}
#endif

//
#define DPMARSHALERCLASS DPMarshaler1stP
#define VISITNODE VisitNodeM1stP
#include "dpMarshalerCode.cc"
#undef VISITNODE
#undef DPMARSHALERCLASS

//
#define DPMARSHALERCLASS DPMarshaler2ndP
#define VISITNODE VisitNodeM2ndP
#include "dpMarshalerCode.cc"
#undef VISITNODE
#undef DPMARSHALERCLASS

//
//
OZ_Term
unmarshalBorrow(MarshalerBuffer *bs,
		OB_Entry *&ob, OB_TIndex &bi, BYTE &ec)
{
  PD((UNMARSHAL,"Borrow"));
  DSite *sd = unmarshalDSite(bs);
  Ext_OB_TIndex si = MakeExt_OB_TIndex(ToPointer(unmarshalNumber(bs)));
  PD((UNMARSHAL,"borrow o:%d",si));
  
  if(sd==myDSite){ Assert(0);}
  
  BorrowEntry *b = borrowTable->find(si, sd);
  
  ec = bs->get();
  RRinstance* cred = unmarshalCredit(bs);    

  if (b!=NULL) {
    b->mergeReference(cred);
    ob = b;
    // Assert(b->getValue() != (OZ_Term) 0);
    return b->getValue();
  } else {
    bi = borrowTable->newBorrow(cred,sd,si);
    b=borrowIndex2borrowEntry(bi);
    ob=b;
    return 0;
  }

  ob=b;
  return 0;
}

//
// 'suspend' turns 'OK' if unmarshaling is not complete;
ProgramCounter
dpUnmarshalHashTableRef(Builder *b,
			ProgramCounter pc, MarshalerBuffer *bs,
			DPBuilderCodeAreaDescriptor *desc, Bool &suspend)
{
  //
  if (pc) {
    const int num = unmarshalNumber(bs);
    const int elseLabel = unmarshalNumber(bs);
    const int listLabel = unmarshalNumber(bs);
    const int nEntries = unmarshalNumber(bs);
    const int nDone = desc->getHTNDone();
    IHashTable *table;

    //
    if (nDone == 0) {
      // new entry;
      table = IHashTable::allocate(nEntries, elseLabel);
      if (listLabel)
	table->addLTuple(listLabel);
    } else {
      // continuation;
      Assert(elseLabel == 0 && listLabel == 0);
      table = (IHashTable *) getAdressArg(pc);
    }

    //
    for (int i = num; i--; ) {    
      const int termTag = unmarshalNumber(bs);
      const int label   = unmarshalNumber(bs);
      HashTableEntryDesc *desc = new HashTableEntryDesc(table, label);

      //
      switch (termTag) {
      case RECORDTAG:
	{
	  b->getOzValue(getHashTableRecordEntryLabelCA, desc);
	  //
	  RecordArityType at = unmarshalRecordArityType(bs);
	  if (at == RECORDARITY) {
	    b->getOzValue(saveRecordArityHashTableEntryCA, desc);
	  } else {
	    Assert(at == TUPLEWIDTH);
	    int width = unmarshalNumber(bs);
	    desc->setSRA(mkTupleWidth(width));
	  }
	  break;
	}

      case ATOMTAG:
	b->getOzValue(getHashTableAtomEntryLabelCA, desc);
	break;

      case NUMBERTAG:
	b->getOzValue(getHashTableNumEntryLabelCA, desc);
	break;

      default: Assert(0); break;
      }
    }

    //
    if (num + nDone < nEntries) {
      suspend = OK;		// even if it already was;
      desc->setHTNDone(num + nDone);
    } else {
      Assert(num + nDone == nEntries);
      desc->setHTNDone(0);
    }

    // 
    // Either a new one, or a continuation: just write it off;
    return (CodeArea::writeIHashTable(table, pc));
  } else {
    const int num = unmarshalNumber(bs);
    skipNumber(bs);		// elseLabel
    skipNumber(bs);		// listLabel
    const int nEntries = unmarshalNumber(bs);
    const int nDone = desc->getHTNDone();

    //
    for (int i = num; i--; ) {
      const int termTag = unmarshalNumber(bs);
      skipNumber(bs);		// label

      //
      switch (termTag) {
      case RECORDTAG:
	{
	  b->discardOzValue();
	  //
	  RecordArityType at = unmarshalRecordArityType(bs);
	  if (at == RECORDARITY)
	    b->discardOzValue();
	  else
	    skipNumber(bs);
	  break;
	}

      case ATOMTAG:
	b->discardOzValue();
	break;

      case NUMBERTAG:
	b->discardOzValue();
	break;

      default: Assert(0); break;
      }
    }

    //
    if (num + nDone < nEntries) {
      suspend = OK;		// even if it already was;
      desc->setHTNDone(num + nDone);
    } else {
      Assert(num + nDone == nEntries);
      desc->setHTNDone(0);
    }

    //
    return ((ProgramCounter) 0);
  }
}

//
// 
inline 
void VSnapshotBuilder::processSmallInt(OZ_Term siTerm) {}
inline 
void VSnapshotBuilder::processFloat(OZ_Term floatTerm) {}

inline 
void VSnapshotBuilder::processLiteral(OZ_Term litTerm)
{
  VisitNodeTrav(litTerm, vIT, return);
}
inline 
void VSnapshotBuilder::processBigInt(OZ_Term biTerm)
{
  VisitNodeTrav(biTerm, vIT, return);
}

inline 
void VSnapshotBuilder::processBuiltin(OZ_Term biTerm, ConstTerm *biConst)
{
  VisitNodeTrav(biTerm, vIT, return);
}
inline 
void VSnapshotBuilder::processExtension(OZ_Term et)
{
  VisitNodeTrav(et, vIT, return);
}

//
inline 
Bool VSnapshotBuilder::processObject(OZ_Term objTerm, ConstTerm *objConst)
{
  VisitNodeTrav(objTerm, vIT, return(TRUE));
  if (doToplevel) {
    doToplevel = FALSE;
    return (FALSE);
  } else {
    return (TRUE);
  }
}
inline 
void VSnapshotBuilder::processNoGood(OZ_Term resTerm)
{
  Assert(!oz_isVar(resTerm));
  VisitNodeTrav(resTerm, vIT, return);
}
inline 
void VSnapshotBuilder::processLock(OZ_Term lockTerm, Tertiary *tert)
{
  VisitNodeTrav(lockTerm, vIT, return);
}
inline 
Bool VSnapshotBuilder::processCell(OZ_Term cellTerm, Tertiary *tert)
{
  VisitNodeTrav(cellTerm, vIT, return(TRUE));
  return (TRUE);
}
inline 
void VSnapshotBuilder::processPort(OZ_Term portTerm, Tertiary *tert)
{
  VisitNodeTrav(portTerm, vIT, return);
}
inline 
void VSnapshotBuilder::processResource(OZ_Term rTerm, Tertiary *tert)
{
  VisitNodeTrav(rTerm, vIT, return);
}

//
inline 
void VSnapshotBuilder::processVar(OZ_Term v, OZ_Term *vRef)
{
  Assert(oz_isVar(v));
  OZ_Term vrt = makeTaggedRef(vRef);

  // Note: a variable is identified by its *location*;
  VisitNodeTrav(vrt, vIT, return);

  //
  // Now, construct a VAR patch for this variable (export it if needed);
  if (oz_isExtVar(v)) {
    ExtVarType evt = oz_getExtVar(v)->getIdV();
    switch (evt) {
    case OZ_EVAR_MANAGER:
      {
	// make&save an "exported" manager;
	ManagerVar *mvp = oz_getManagerVar(v);
	expVars = new MgrVarPatch(vrt, expVars, mvp, dest);
      }
      break;

    case OZ_EVAR_PROXY:
      {
	// make&save an "exported" proxy:
	ProxyVar *pvp = oz_getProxyVar(v);
	expVars = new PxyVarPatch(vrt, expVars, pvp, dest);
      }
      break;

    case OZ_EVAR_LAZY:
      // First, lazy variables are transparent for the programmer.
      // Secondly, the earlier an object gets over the network the
      // better for failure handling (aka "no failure - simple
      // failure handling!"). Altogether, delayed exportation is a
      // good thing here, so we ignore these vars;
      break;

    default:
      Assert(0);
      break;
    }
  } else if (oz_isFree(v) || oz_isReadOnly(v)) {
    Assert(perdioInitialized);

    //
    ManagerVar *mvp = globalizeFreeVariable(vRef);
    expVars = new MgrVarPatch(vrt, expVars, mvp, dest);
  } else { 
    //

    // Actualy, we should copy all these types of variables
    // (constraint stuff), and then marshal them as "nogood"s.  So,
    // just ignoring them is a limitation: the system behaves
    // differently when such variables are bound between logical
    // sending of a message and their marshaling.
  }
}

//
inline
Bool VSnapshotBuilder::processLTuple(OZ_Term ltupleTerm)
{
  VisitNodeTrav(ltupleTerm, vIT, return(TRUE));
  return (NO);
}
inline 
Bool VSnapshotBuilder::processSRecord(OZ_Term srecordTerm)
{
  VisitNodeTrav(srecordTerm, vIT, return(TRUE));
  return (NO);
}
inline 
Bool VSnapshotBuilder::processChunk(OZ_Term chunkTerm, ConstTerm *chunkConst)
{
  VisitNodeTrav(chunkTerm, vIT, return(TRUE));
  return (NO);
}

//
inline 
Bool VSnapshotBuilder::processFSETValue(OZ_Term fsetvalueTerm)
{
  return (NO);
}

//
inline Bool
VSnapshotBuilder::processDictionary(OZ_Term dictTerm, ConstTerm *dictConst)
{
  VisitNodeTrav(dictTerm, vIT, return(TRUE));
  OzDictionary *d = (OzDictionary *) dictConst;
  return (d->isSafeDict() ? NO : OK);
}

//
inline Bool
VSnapshotBuilder::processArray(OZ_Term arrayTerm, ConstTerm *arrayConst)
{
  VisitNodeTrav(arrayTerm, vIT, return(TRUE));
  return (OK);
}

//
inline Bool
VSnapshotBuilder::processClass(OZ_Term classTerm, ConstTerm *classConst)
{ 
  VisitNodeTrav(classTerm, vIT, return(TRUE));
  //
  ObjectClass *cl = (ObjectClass *) classConst;
  return (cl->isSited());
}

//
inline Bool
VSnapshotBuilder::processAbstraction(OZ_Term absTerm, ConstTerm *absConst)
{
  VisitNodeTrav(absTerm, vIT, return(TRUE));

  //
  Abstraction *pp = (Abstraction *) absConst;
  PrTabEntry *pred = pp->getPred();
  //
  if (pred->isSited()) {
    return (OK);		// done - a leaf;
  } else {
    ProgramCounter start = pp->getPC() - sizeOf(DEFINITION);
    XReg reg;
    int nxt, line, colum;
    TaggedRef file, predName;
    CodeArea::getDefinitionArgs(start, reg, nxt, file,
				line, colum, predName);

    //
    DPMarshalerCodeAreaDescriptor *desc = 
      new DPMarshalerCodeAreaDescriptor(start, start + nxt, 
					(AddressHashTableO1Reset *) 0);
    traverseBinary(traverseCode, desc);
    return (NO);
  }
  Assert(0);
}

//
inline 
void VSnapshotBuilder::processSync() {}

//
#define	TRAVERSERCLASS	VSnapshotBuilder
#include "gentraverserLoop.cc"
#undef	TRAVERSERCLASS

//
// Not in use since we take a snapshot of a value ahead of
// marshaling now;

static void contProcNOOP(GenTraverser *gt, GTAbstractEntity *cont)
{}

//
void VSnapshotBuilder::copyStack(DPMarshaler *dpm)
{
  StackEntry *copyTop = dpm->getTop();
  StackEntry *copyBottom = dpm->getBottom();
  StackEntry *top;
  int entries;

  //
  entries = copyTop - copyBottom;
  top = extendBy(entries);
  setTop(top);

  //
  while (copyTop > copyBottom) {
    OZ_Term& t = (OZ_Term&) *(--copyTop);
    OZ_Term tc = t;
    DEREF(tc, tPtr);

    //
    *(--top) = (StackEntry) t;

    //
    if (oz_isMark(tc)) {
      switch (tc) {
      case taggedBATask:
	{
	  StackEntry arg = *(--copyTop);
	  TraverserBinaryAreaProcessor proc =
	    (TraverserBinaryAreaProcessor) *(--copyTop);
	  Assert(proc == dpMarshalCode);

	  //
	  DPMarshalerCodeAreaDescriptor *desc =
	    (DPMarshalerCodeAreaDescriptor *) arg;
	  DPMarshalerCodeAreaDescriptor *descCopy;
	  if (desc)
	    descCopy = new DPMarshalerCodeAreaDescriptor(*desc);
	  else
	    descCopy = (DPMarshalerCodeAreaDescriptor *) 0;
	  *(--top) = (StackEntry) descCopy;
	  *(--top) = (StackEntry) traverseCode;
	}
	break;

      case taggedSyncTask:
	break;

      case taggedContTask:
	{
	  *(--top) = *(--copyTop);
	  TraverserContProcessor proc = 
	    (TraverserContProcessor) *(--copyTop);
	  // marshaling continuations are dropped:
	  *(--top) = (StackEntry) contProcNOOP;
	}
	break;
      }
    }      
  }
}


//
//
VSnapshotBuilder vsb;

/* *********************************************************************/
/*   interface to Oz-core                                  */
/* *********************************************************************/

//
// An index, if any, is marshaled *after* 'marshalTertiary()';
void marshalTertiary(ByteBuffer *bs, 
		     Tertiary *t, Bool hasIndex, MarshalTag tag)
{
#if defined(DBG_TRACE)
  DBGINIT();
  fprintf(dbgout, "> tag: %s(%d) = %s\n",
	  dif_names[tag].name, tag, toC(makeTaggedConst(t)));
  fflush(dbgout);
#endif
  PD((MARSHAL,"Tert"));
  switch(t->getTertType()){
  case Te_Local:
    globalizeTert(t);
    // no break here!
  case Te_Manager:
    {
      PD((MARSHAL_CT,"manager"));
      OB_TIndex OTI = MakeOB_TIndex(t->getTertPointer());
      marshalDIFcounted(bs, (hasIndex ? defmap[tag] : tag));
      marshalOwnHead(bs, OTI);
    }
    break;

  case Te_Frame:
  case Te_Proxy:
    {
      PD((MARSHAL,"proxy"));
      OB_TIndex BTI = MakeOB_TIndex(t->getTertPointer());
      DSite* sd=bs->getSite();
      if (sd && (sd == borrowTable->getOriginSite(BTI))) {
	marshalDIFcounted(bs, (hasIndex ? DIF_OWNER_DEF : DIF_OWNER));
	marshalToOwner(bs, BTI);

	//
      } else {
	EntityInfo *ei = t->getInfo(); 
	BYTE ec = (ei?(ei->getEntityCond() & (PERM_FAIL)):ENTITY_NORMAL);
	marshalDIFcounted(bs, (hasIndex ? defmap[tag] : tag));
	marshalBorrowHead(bs, BTI, ec);
      }
      break;
    }
  default:
    Assert(0);
  }
}


static 
char const *tagToComment(MarshalTag tag)
{
  switch(tag){
  case DIF_PORT:
  case DIF_PORT_DEF:
    return "port";
  case DIF_CELL:
  case DIF_CELL_DEF:
    return "cell";
  case DIF_LOCK:
  case DIF_LOCK_DEF:
    return "lock";
  case DIF_OBJECT:
  case DIF_OBJECT_DEF:
  case DIF_VAR_OBJECT:
  case DIF_VAR_OBJECT_DEF:
  case DIF_STUB_OBJECT:
  case DIF_STUB_OBJECT_DEF:
    return "object";
  case DIF_RESOURCE:
  case DIF_RESOURCE_DEF:
    return "resource";
  default:
    Assert(0);
    return "";
}}

OZ_Term
unmarshalTertiary(MarshalerBuffer *bs, MarshalTag tag)
{
  OB_Entry* ob;
  OB_TIndex bi;
  BYTE ec; 

  OZ_Term val = unmarshalBorrow(bs, ob, bi, ec);

  if (val) {
    PD((UNMARSHAL,"%s hit b:%d",tagToComment(tag),bi));
    switch (tag) {
    case DIF_RESOURCE:
    case DIF_RESOURCE_DEF:
    case DIF_PORT:
    case DIF_PORT_DEF:
    case DIF_CELL:
    case DIF_CELL_DEF:
    case DIF_LOCK:
    case DIF_LOCK_DEF:
      break;

    case DIF_STUB_OBJECT:
    case DIF_STUB_OBJECT_DEF:
    case DIF_VAR_OBJECT:
    case DIF_VAR_OBJECT_DEF:
      TaggedRef obj;
      TaggedRef clas;
      (void) unmarshalGName(&obj, bs);
      (void) unmarshalGName(&clas, bs);
      break;
    default:         
      Assert(0);
    }

    // If the entity had a failed condition, propagate it.
    if (ec & PERM_FAIL){
      // As failed entities are propageted as Resources, the 
      // type of the marshaled entity can differ from the 
      // type of the borrowtable entity 
      // i.e. A failed variable, the table contains a variable 
      // but the unmarshaled entity is a tertiary(resource).
      if(ob->isTertiary())
	deferProxyTertProbeFault(ob->getTertiary(),PROBE_PERM);
      else
	deferProxyVarProbeFault(val,PROBE_PERM);
    }
    return (val);
  }

  PD((UNMARSHAL,"%s miss b:%d",tagToComment(tag),bi));  
  Tertiary *tert;

  switch (tag) { 
  case DIF_RESOURCE:
  case DIF_RESOURCE_DEF:
    tert = new DistResource(bi);
    break;  
  case DIF_PORT:
  case DIF_PORT_DEF:
    tert = new PortProxy(bi);        
    break;
  case DIF_CELL:
  case DIF_CELL_DEF:
    tert = new CellProxy(bi); 
    break;
  case DIF_LOCK:
  case DIF_LOCK_DEF:
    tert = new LockProxy(bi); 
    break;

  case DIF_VAR_OBJECT:
  case DIF_VAR_OBJECT_DEF:
  case DIF_STUB_OBJECT:
  case DIF_STUB_OBJECT_DEF:
    {
      OZ_Term obj;
      OZ_Term clas;
      OZ_Term val;
      GName *gnobj = unmarshalGName(&obj, bs);
      GName *gnclass = unmarshalGName(&clas, bs);
      if(!gnobj) {	
//  	printf("Had Object %d:%d flags:%d\n",
//  	       ((BorrowEntry *)ob)->getNetAddress()->index,
//  	       ((BorrowEntry *)ob)->getNetAddress()->site->getTimeStamp()->pid,
//  	       ((BorrowEntry *)ob)->getFlags());
	if(!(borrowTable->maybeFreeBorrowEntry(bi))){
	  ob->mkRef(obj,ob->getFlags());
//    	  printf("indx:%d %xd\n",((BorrowEntry *)ob)->getNetAddress()->index,
//    	  	 ((BorrowEntry *)ob)->getNetAddress()->site);
	}
	return (obj);
      }

      //      
      if (gnclass) {
	// A new proxy for the class (borrow index is not there since
	// we do not run the lazy class protocol here);
	clas = newClassProxy(MakeOB_TIndex(-1), gnclass);
	addGName(gnclass, clas);
      } else {
	// There are two cases: we've got either the class or its
	// proxy. In either case, it's used for the new object proxy:
      }

      //
      val = newObjectProxy(bi, gnobj, clas);
      addGName(gnobj, val);
      ob->changeToVar(val);

      //
      return (val);
    }
  default:         
    Assert(0);
  }
  val=makeTaggedConst(tert);
  ob->changeToTertiary(tert); 
  
  // If the entity carries failure info react to it
  // othervise check the local environment. The present 
  // representation of the entities site might have information
  // about the failure state.
  
  if (ec & PERM_FAIL)
    {  
      deferProxyTertProbeFault(tert,PROBE_PERM);
    }
  else
    {
      switch(((BorrowEntry*)ob)->getSite()->siteStatus()){
      case SITE_OK:{
	break;}
      case SITE_PERM:{
	deferProxyTertProbeFault(tert,PROBE_PERM);
	break;}
      case SITE_TEMP:{
	deferProxyTertProbeFault(tert,PROBE_TEMP);
	break;}
      default:
	Assert(0);
      } 
    }
  return val;
}


OZ_Term 
unmarshalOwner(MarshalerBuffer *bs, MarshalTag mt)
{
  Ext_OB_TIndex OTI;
  OZ_Term oz;
  RRinstance  *c = unmarshalCreditToOwner(bs, mt, OTI);
  PD((UNMARSHAL,"OWNER o:%d",OTI));
  OwnerEntry* oe = ownerTable->extOTI2ownerEntry(OTI);
  if (oe){
    oe->mergeReference(c);
    oz=oe->getValue();}
  else
    oz = createFailedEntity(OTI,TRUE);
  return oz;
}

//
void DPBuilderCodeAreaDescriptor::gc()
{
  Assert(current >= start && current <= end);
  // Unfortunately, 'ENDOFFILE' has to be recorded eagerly, because
  // 'CodeArea::gCollectCodeAreaStart()' (in its current incarnation)
  // has to be called before any codearea can be reached by other
  // means;
#if defined(DEBUG_CHECK)
  if (getCurrent()) {
    Opcode op = CodeArea::getOpcode(getCurrent());
    Assert(op == ENDOFFILE);
  }
#endif
}

//
//
OZ_Term dpUnmarshalTerm(ByteBuffer *bs, Builder *b)
{
  Assert(oz_onToplevel());
#if defined(DBG_TRACE)
  DBGINIT();
  fprintf(dbgout, "<bs: %p (getptr=%p,posMB=%p,endMB=%p)\n",
	  bs, bs->getGetptr(), bs->getPosMB(), bs->getEndMB());
  fflush(dbgout);
#endif

  while(1) {
    MarshalTag tag = (MarshalTag) bs->get();
    Assert(tag < DIF_LAST);
    dif_counter[tag].recv();	// kost@ : TODO: needed?
#if defined(DBG_TRACE)
    fprintf(dbgout, "< tag: %s(%d)", dif_names[tag].name, tag);
    fflush(dbgout);
#endif

    switch (tag) {
	
    case DIF_SMALLINT: 
      {
	OZ_Term ozInt = OZ_int(unmarshalNumber(bs));
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %d\n", ozInt);
	fflush(dbgout);
#endif
	b->buildValue(ozInt);
	break;
      }

    case DIF_FLOAT:
      {
	double f = unmarshalFloat(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %f\n", f);
	fflush(dbgout);
#endif
	b->buildValue(OZ_float(f));
	break;
      }

    case DIF_NAME_DEF:
      {
	OZ_Term value;
	int refTag = unmarshalRefTag(bs);
	GName *gname = unmarshalGName(&value, bs);
	int nameSize = unmarshalNumber(bs);
	char *printname = unmarshalString(bs);
	int strLen = strlen(printname);

	//
	// May be, we don't have a complete print name yet:
	if (nameSize > strLen) {
	  DPUnmarshalerNameSusp *ns =
	    new DPUnmarshalerNameSusp(refTag, nameSize, value,
				      gname, printname, strLen);
	  //
	  b->getAbstractEntity(ns);
	  b->suspend();
#if defined(DBG_TRACE)
	  fprintf(dbgout, " >>> (at %d)\n", refTag);
	  fflush(dbgout);
#endif
	  // returns '-1' for an additional consistency check - the
	  // caller should know whether that's a complete message;
	  return ((OZ_Term) -1);
	} else {
	  // complete print name;
	  Assert(strLen == nameSize);

	  //
	  if (gname) {
	    Name *aux;
	    if (strcmp("", printname) == 0) {
	      aux = Name::newName(am.currentBoard());
	    } else {
	      aux = NamedName::newNamedName(strdup(printname));
	    }
	    aux->import(gname);
	    value = makeTaggedLiteral(aux);
	    b->buildValue(value);
	    addGName(gname, value);
	  } else {
	    b->buildValue(value);
	  }

	  //
	  b->setTerm(value, refTag);
	  delete printname;
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	  fflush(dbgout);
#endif
	  break;
	}
      }

    case DIF_NAME:
      {
	OZ_Term value;
	GName *gname = unmarshalGName(&value, bs);
	int nameSize = unmarshalNumber(bs);
	char *printname = unmarshalString(bs);
	int strLen = strlen(printname);

	//
	// May be, we don't have a complete print name yet:
	if (nameSize > strLen) {
	  DPUnmarshalerNameSusp *ns =
	    new DPUnmarshalerNameSusp(0, nameSize, value,
				      gname, printname, strLen);
	  //
	  b->getAbstractEntity(ns);
	  b->suspend();
	  // returns '-1' for an additional consistency check - the
	  // caller should know whether that's a complete message;
	  return ((OZ_Term) -1);
	} else {
	  // complete print name;
	  Assert(strLen == nameSize);

	  //
	  if (gname) {
	    Name *aux;
	    if (strcmp("", printname) == 0) {
	      aux = Name::newName(am.currentBoard());
	    } else {
	      aux = NamedName::newNamedName(strdup(printname));
	    }
	    aux->import(gname);
	    value = makeTaggedLiteral(aux);
	    b->buildValue(value);
	    addGName(gname, value);
	  } else {
	    b->buildValue(value);
	  }

	  //
	  delete printname;
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s\n", toC(value));
	  fflush(dbgout);
#endif
	  break;
	}
      }

    case DIF_COPYABLENAME_DEF:
      {
	int refTag      = unmarshalRefTag(bs);
	int nameSize  = unmarshalNumber(bs);
	char *printname = unmarshalString(bs);
	int strLen = strlen(printname);

	//
	// May be, we don't have a complete print name yet:
	if (nameSize > strLen) {
	  DPUnmarshalerCopyableNameSusp *cns =
	    new DPUnmarshalerCopyableNameSusp(refTag, nameSize,
					      printname, strLen);
	  //
	  b->getAbstractEntity(cns);
	  b->suspend();
#if defined(DBG_TRACE)
	  fprintf(dbgout, " >>> (at %d)\n", refTag);
	  fflush(dbgout);
#endif
	  // returns '-1' for an additional consistency check - the
	  // caller should know whether that's a complete message;
	  return ((OZ_Term) -1);
	} else {
	  // complete print name;
	  Assert(strLen == nameSize);
	  OZ_Term value;

	  NamedName *aux = NamedName::newCopyableName(strdup(printname));
	  value = makeTaggedLiteral(aux);
	  b->buildValue(value);
	  b->setTerm(value, refTag);
	  delete printname;
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	  fflush(dbgout);
#endif
	}
	break;
      }

    case DIF_COPYABLENAME:
      {
	int nameSize  = unmarshalNumber(bs);
	char *printname = unmarshalString(bs);
	int strLen = strlen(printname);

	//
	// May be, we don't have a complete print name yet:
	if (nameSize > strLen) {
	  DPUnmarshalerCopyableNameSusp *cns =
	    new DPUnmarshalerCopyableNameSusp(0, nameSize,
					      printname, strLen);
	  //
	  b->getAbstractEntity(cns);
	  b->suspend();
	  // returns '-1' for an additional consistency check - the
	  // caller should know whether that's a complete message;
	  return ((OZ_Term) -1);
	} else {
	  // complete print name;
	  Assert(strLen == nameSize);
	  OZ_Term value;

	  NamedName *aux = NamedName::newCopyableName(strdup(printname));
	  value = makeTaggedLiteral(aux);
	  b->buildValue(value);
	  delete printname;
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s\n", toC(value));
	  fflush(dbgout);
#endif
	}
	break;
      }

    case DIF_UNIQUENAME_DEF:
      {
	int refTag      = unmarshalRefTag(bs);
	int nameSize  = unmarshalNumber(bs);
	char *printname = unmarshalString(bs);
	int strLen = strlen(printname);

	//
	if (nameSize > strLen) {
	  DPUnmarshalerUniqueNameSusp *uns = 
	    new DPUnmarshalerUniqueNameSusp(refTag, nameSize,
					    printname, strLen);
	  //
	  b->getAbstractEntity(uns);
	  b->suspend();
#if defined(DBG_TRACE)
	  fprintf(dbgout, " >>> (at %d)\n", refTag);
	  fflush(dbgout);
#endif
	  // returns '-1' for an additional consistency check - the
	  // caller should know whether that's a complete message;
	  return ((OZ_Term) -1);
	} else {
	  // complete print name;
	  Assert(strLen == nameSize);
	  OZ_Term value;

	  value = oz_uniqueName(printname);
	  b->buildValue(value);
	  b->setTerm(value, refTag);
	  delete printname;
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	  fflush(dbgout);
#endif
	}
	break;
      }

    case DIF_UNIQUENAME:
      {
	int nameSize  = unmarshalNumber(bs);
	char *printname = unmarshalString(bs);
	int strLen = strlen(printname);

	//
	if (nameSize > strLen) {
	  DPUnmarshalerUniqueNameSusp *uns = 
	    new DPUnmarshalerUniqueNameSusp(0, nameSize,
					    printname, strLen);
	  //
	  b->getAbstractEntity(uns);
	  b->suspend();
	  // returns '-1' for an additional consistency check - the
	  // caller should know whether that's a complete message;
	  return ((OZ_Term) -1);
	} else {
	  // complete print name;
	  Assert(strLen == nameSize);
	  OZ_Term value;

	  value = oz_uniqueName(printname);
	  b->buildValue(value);
	  delete printname;
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s\n", toC(value));
	  fflush(dbgout);
#endif
	}
	break;
      }

    case DIF_ATOM_DEF:
      {
	int refTag = unmarshalRefTag(bs);
	int nameSize  = unmarshalNumber(bs);
	char *aux  = unmarshalString(bs);
	int strLen = strlen(aux);

	//
	if (nameSize > strLen) {
	  DPUnmarshalerAtomSusp *as = 
	    new DPUnmarshalerAtomSusp(refTag, nameSize, aux, strLen);
	  //
	  b->getAbstractEntity(as);
	  b->suspend();
#if defined(DBG_TRACE)
	  fprintf(dbgout, " >>> (at %d)\n", refTag);
	  fflush(dbgout);
#endif
	  // returns '-1' for an additional consistency check - the
	  // caller should know whether that's a complete message;
	  return ((OZ_Term) -1);
	} else {
	  // complete print name;
	  Assert(strLen == nameSize);

	  OZ_Term value = OZ_atom(aux);
	  b->buildValue(value);
	  b->setTerm(value, refTag);
	  delete [] aux;
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	  fflush(dbgout);
#endif
	}
	break;
      }

    case DIF_ATOM:
      {
	int nameSize  = unmarshalNumber(bs);
	char *aux  = unmarshalString(bs);
	int strLen = strlen(aux);

	//
	if (nameSize > strLen) {
	  DPUnmarshalerAtomSusp *as = 
	    new DPUnmarshalerAtomSusp(0, nameSize, aux, strLen);
	  //
	  b->getAbstractEntity(as);
	  b->suspend();
	  // returns '-1' for an additional consistency check - the
	  // caller should know whether that's a complete message;
	  return ((OZ_Term) -1);
	} else {
	  // complete print name;
	  Assert(strLen == nameSize);

	  OZ_Term value = OZ_atom(aux);
	  b->buildValue(value);
	  delete [] aux;
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s\n", toC(value));
	  fflush(dbgout);
#endif
	}
	break;
      }

      //
    case DIF_LIT_CONT:
      {
	char *aux  = unmarshalString(bs);
	int strLen = strlen(aux);

	//
	GTAbstractEntity *bae = b->buildAbstractEntity();
	switch (bae->getType()) {
	case GT_NameSusp:
	  {
	    DPUnmarshalerNameSusp *ns = (DPUnmarshalerNameSusp *) bae;
	    ns->appendPrintname(strLen, aux);
	    if (ns->getNameSize() > ns->getPNSize()) {
	      // still not there;
	      b->getAbstractEntity(ns);
	      b->suspend();
#if defined(DBG_TRACE)
	      fprintf(dbgout, " >>> (at %d)\n", ns->getRefTag());
	      fflush(dbgout);
#endif
	      // returns '-1' for an additional consistency check - the
	      // caller should know whether that's a complete message;
	      return ((OZ_Term) -1);
	    } else {
	      Assert(ns->getNameSize() == ns->getPNSize());
	      OZ_Term value;

	      //
	      if (ns->getGName()) {
		Name *aux;
		if (strcmp("", ns->getPrintname()) == 0) {
		  aux = Name::newName(am.currentBoard());
		} else {
		  char *pn = ns->getPrintname();
		  aux = NamedName::newNamedName(strdup(pn));
		}
		aux->import(ns->getGName());
		value = makeTaggedLiteral(aux);
		b->buildValue(value);
		addGName(ns->getGName(), value);
	      } else {
		value = ns->getValue();
		b->buildValue(value);
	      }

	      //
	      int refTag = ns->getRefTag();
	      if (refTag)
		b->setTerm(value, refTag);
	      delete ns;
#if defined(DBG_TRACE)
	      fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	      fflush(dbgout);
#endif
	    }
	    break;
	  }

	case GT_AtomSusp:
	  {
	    DPUnmarshalerAtomSusp *as = (DPUnmarshalerAtomSusp *) bae;
	    as->appendPrintname(strLen, aux);
	    if (as->getNameSize() > as->getPNSize()) {
	      // still not there;
	      b->getAbstractEntity(as);
	      b->suspend();
#if defined(DBG_TRACE)
	      fprintf(dbgout, " >>> (at %d)\n", as->getRefTag());
	      fflush(dbgout);
#endif
	      // returns '-1' for an additional consistency check - the
	      // caller should know whether that's a complete message;
	      return ((OZ_Term) -1);
	    } else {
	      Assert(as->getNameSize() == as->getPNSize());

	      OZ_Term value = OZ_atom(as->getPrintname());
	      b->buildValue(value);
	      int refTag = as->getRefTag();
	      if (refTag)
		b->setTerm(value, refTag);
	      delete as;
#if defined(DBG_TRACE)
	      fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	      fflush(dbgout);
#endif
	    }
	    break;
	  }

	case GT_UniqueNameSusp:
	  {
	    DPUnmarshalerUniqueNameSusp *uns =
	      (DPUnmarshalerUniqueNameSusp *) bae;
	    uns->appendPrintname(strLen, aux);
	    if (uns->getNameSize() > uns->getPNSize()) {
	      // still not there;
	      b->getAbstractEntity(uns);
	      b->suspend();
#if defined(DBG_TRACE)
	      fprintf(dbgout, " >>> (at %d)\n", uns->getRefTag());
	      fflush(dbgout);
#endif
	      // returns '-1' for an additional consistency check - the
	      // caller should know whether that's a complete message;
	      return ((OZ_Term) -1);
	    } else {
	      Assert(uns->getNameSize() == uns->getPNSize());

	      OZ_Term value = oz_uniqueName(uns->getPrintname());
	      b->buildValue(value);
	      int refTag = uns->getRefTag();
	      if (refTag)
		b->setTerm(value, refTag);
	      delete uns;
#if defined(DBG_TRACE)
	      fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	      fflush(dbgout);
#endif
	    }
	    break;
	  }

	case GT_CopyableNameSusp:
	  {
	    DPUnmarshalerCopyableNameSusp *cns =
	      (DPUnmarshalerCopyableNameSusp *) bae;
	    cns->appendPrintname(strLen, aux);
	    if (cns->getNameSize() > cns->getPNSize()) {
	      // still not there;
	      b->getAbstractEntity(cns);
	      b->suspend();
#if defined(DBG_TRACE)
	      fprintf(dbgout, " >>> (at %d)\n", cns->getRefTag());
	      fflush(dbgout);
#endif
	      // returns '-1' for an additional consistency check - the
	      // caller should know whether that's a complete message;
	      return ((OZ_Term) -1);
	    } else {
	      Assert(cns->getNameSize() == cns->getPNSize());
	      OZ_Term value;
	      char *pn = cns->getPrintname();

	      NamedName *aux = NamedName::newCopyableName(strdup(pn));
	      value = makeTaggedLiteral(aux);
	      b->buildValue(value);
	      int refTag = cns->getRefTag();
	      if (refTag)
		b->setTerm(value, refTag);
	      delete cns;
#if defined(DBG_TRACE)
	      fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	      fflush(dbgout);
#endif
	    }
	    break;
	  }

	default:
	  OZ_error("Illegal GTAbstractEntity (builder) for a LitCont!");
	  b->buildValue(oz_nil());
	}
	break;
      }

    case DIF_BIGINT:
      {
	char *aux  = unmarshalString(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " %s\n", aux);
	fflush(dbgout);
#endif
	b->buildValue(OZ_CStringToNumber(aux));
	delete aux;
	break;
      }

    case DIF_BIGINT_DEF:
      {
	int refTag = unmarshalRefTag(bs);
	char *aux  = unmarshalString(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " %s\n", aux);
	fflush(dbgout);
#endif
	OZ_Term value = OZ_CStringToNumber(aux);
	b->buildValue(value);
	b->setTerm(value, refTag);
	delete aux;
	break;
      }

    case DIF_LIST_DEF:
      {
	int refTag = unmarshalRefTag(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " (at %d)\n", refTag);
	fflush(dbgout);
#endif
	b->buildListRemember(refTag);
	break;
      }

    case DIF_LIST:
      b->buildList();
      break;

    case DIF_TUPLE_DEF:
      {
	int refTag = unmarshalRefTag(bs);
	int argno  = unmarshalNumber(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " (at %d)\n", refTag);
	fflush(dbgout);
#endif
	b->buildTupleRemember(argno, refTag);
	break;
      }

    case DIF_TUPLE:
      {
	int argno  = unmarshalNumber(bs);
	b->buildTuple(argno);
	break;
      }

    case DIF_RECORD_DEF:
      {
	int refTag = unmarshalRefTag(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " (at %d)\n", refTag);
	fflush(dbgout);
#endif
	b->buildRecordRemember(refTag);
	break;
      }

    case DIF_RECORD:
      b->buildRecord();
      break;

    case DIF_REF:
      {
	int i = unmarshalNumber(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " (from %d)\n", i);
	fflush(dbgout);
#endif
	b->buildValueRef(i);
	break;
      }

      //
    case DIF_OWNER_DEF:
      {
	OZ_Term tert = unmarshalOwner(bs, tag);
	int refTag = unmarshalRefTag(bs);

#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(tert), refTag);
	fflush(dbgout);
#endif
	b->buildValueRemember(tert, refTag);
	break;
      }

      //
    case DIF_OWNER:
      {
	OZ_Term tert = unmarshalOwner(bs, tag);

#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(tert), refTag);
	fflush(dbgout);
#endif
	b->buildValue(tert);
	break;
      }

    case DIF_RESOURCE_DEF:
    case DIF_PORT_DEF:
    case DIF_CELL_DEF:
    case DIF_LOCK_DEF:
    case DIF_STUB_OBJECT_DEF:
    case DIF_VAR_OBJECT_DEF:
      {
	OZ_Term tert = unmarshalTertiary(bs, tag);
	int refTag = unmarshalRefTag(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(tert), refTag);
	fflush(dbgout);
#endif
	b->buildValueRemember(tert, refTag);
	break;
      }

    case DIF_RESOURCE:
    case DIF_PORT:
    case DIF_CELL:
    case DIF_LOCK:
    case DIF_STUB_OBJECT:
    case DIF_VAR_OBJECT:
      {
	OZ_Term tert = unmarshalTertiary(bs, tag);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s\n", toC(tert));
	fflush(dbgout);
#endif
	b->buildValue(tert);
	break;
      }

    case DIF_CHUNK_DEF:
      {
	int refTag = unmarshalRefTag(bs);
	OZ_Term value;
	GName *gname = unmarshalGName(&value, bs);
	if (gname) {
#if defined(DBG_TRACE)
	  fprintf(dbgout, " (at %d)\n", refTag);
	  fflush(dbgout);
#endif
	  b->buildChunkRemember(gname, refTag);
	} else {
	  b->knownChunk(value);
	  b->setTerm(value, refTag);
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	  fflush(dbgout);
#endif
	}
	break;
      }

    case DIF_CHUNK:
      {
	OZ_Term value;
	GName *gname = unmarshalGName(&value, bs);
	if (gname) {
	  b->buildChunk(gname);
	} else {
	  b->knownChunk(value);
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s\n", toC(value));
	  fflush(dbgout);
#endif
	}
	break;
      }

    case DIF_CLASS_DEF:
      {
	OZ_Term value;
	int refTag = unmarshalRefTag(bs);
	GName *gname = unmarshalGName(&value, bs);
	int flags = unmarshalNumber(bs);
	//
	if (gname) {
#if defined(DBG_TRACE)
	  fprintf(dbgout, " (at %d)\n", refTag);
	  fflush(dbgout);
#endif
	  b->buildClassRemember(gname, flags, refTag);
	} else {
	  // Either we have the class itself, or that's the lazy
	  // object protocol, in which case it must be a variable
	  // that denotes an object (of this class);
	  OZ_Term vd = oz_deref(value);
	  Assert(!oz_isRef(vd));
	  if (oz_isConst(vd)) {
	    Assert(tagged2Const(vd)->getType() == Co_Class);
	    b->knownClass(value);
	    b->setTerm(value, refTag);
#if defined(DBG_TRACE)
	    fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	    fflush(dbgout);
#endif
	  } else if (oz_isVar(vd)) {
	    OzVariable *var = tagged2Var(vd);
	    Assert(var->getType() == OZ_VAR_EXT);
	    ExtVar *evar = var2ExtVar(var);
	    Assert(evar->getIdV() == OZ_EVAR_LAZY);
	    LazyVar *lvar = (LazyVar *) evar;
	    Assert(lvar->getLazyType() == LT_CLASS);
	    ClassVar *cv = (ClassVar *) lvar;
	    // The binding of a class'es gname is kept until the
	    // construction of a class is finished.
	    gname = cv->getGName();
	    b->buildClassRemember(gname, flags, refTag);
#if defined(DBG_TRACE)
	    fprintf(dbgout, " (at %d)\n", refTag);
	    fflush(dbgout);
#endif
	  } else {
	    OZ_error("Unexpected value is bound to an object's gname");
	    b->buildValue(oz_nil());
	  }
	}
	break;
      }

    case DIF_CLASS:
      {
	OZ_Term value;
	GName *gname = unmarshalGName(&value, bs);
	int flags = unmarshalNumber(bs);
	//
	if (gname) {
	  b->buildClass(gname, flags);
	} else {
	  // Either we have the class itself, or that's the lazy
	  // object protocol, in which case it must be a variable
	  // that denotes an object (of this class);
	  OZ_Term vd = oz_deref(value);
	  Assert(!oz_isRef(vd));
	  if (oz_isConst(vd)) {
	    Assert(tagged2Const(vd)->getType() == Co_Class);
	    b->knownClass(value);
#if defined(DBG_TRACE)
	    fprintf(dbgout, " = %s\n", toC(value));
	    fflush(dbgout);
#endif
	  } else if (oz_isVar(vd)) {
	    OzVariable *var = tagged2Var(vd);
	    Assert(var->getType() == OZ_VAR_EXT);
	    ExtVar *evar = var2ExtVar(var);
	    Assert(evar->getIdV() == OZ_EVAR_LAZY);
	    LazyVar *lvar = (LazyVar *) evar;
	    Assert(lvar->getLazyType() == LT_CLASS);
	    ClassVar *cv = (ClassVar *) lvar;
	    // The binding of a class'es gname is kept until the
	    // construction of a class is finished.
	    gname = cv->getGName();
	    b->buildClass(gname, flags);
	  } else {
	    OZ_error("Unexpected value is bound to an object's gname");
	    b->buildValue(oz_nil());
	  }
	}
	break;
      }

    case DIF_VAR_DEF:
      {
	OZ_Term v = unmarshalVar(bs, FALSE, FALSE);
	int refTag = unmarshalRefTag(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(v), refTag);
	fflush(dbgout);
#endif
	b->buildValueRemember(v, refTag);
	break;
      }

    case DIF_VAR:
      {
	OZ_Term v = unmarshalVar(bs, FALSE, FALSE);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(v), refTag);
	fflush(dbgout);
#endif
	b->buildValue(v);
	break;
      }

    case DIF_READONLY_DEF:
      {
	OZ_Term f = unmarshalVar(bs, TRUE, FALSE);
	int refTag = unmarshalRefTag(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(f), refTag);
	fflush(dbgout);
#endif
	b->buildValueRemember(f, refTag);
	break;
      }

    case DIF_READONLY:
      {
	OZ_Term f = unmarshalVar(bs, TRUE, FALSE);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(f), refTag);
	fflush(dbgout);
#endif
	b->buildValue(f);
	break;
      }

    case DIF_VAR_AUTO_DEF: 
      {
	OZ_Term va = unmarshalVar(bs, FALSE, TRUE);
	int refTag = unmarshalRefTag(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(va), refTag);
	fflush(dbgout);
#endif
	b->buildValueRemember(va, refTag);
	break;
      }

    case DIF_VAR_AUTO: 
      {
	OZ_Term va = unmarshalVar(bs, FALSE, TRUE);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(va), refTag);
	fflush(dbgout);
#endif
	b->buildValue(va);
	break;
      }

    case DIF_READONLY_AUTO_DEF:
      {
	OZ_Term fa = unmarshalVar(bs, TRUE, TRUE);
	int refTag = unmarshalRefTag(bs);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(fa), refTag);
	fflush(dbgout);
#endif
	b->buildValueRemember(fa, refTag);
	break;
      }

    case DIF_READONLY_AUTO: 
      {
	OZ_Term fa = unmarshalVar(bs, TRUE, TRUE);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(fa), refTag);
	fflush(dbgout);
#endif
	b->buildValue(fa);
	break;
      }

    case DIF_OBJECT_DEF:
      {
	OZ_Term value;
	int refTag = unmarshalRefTag(bs);
	GName *gname = unmarshalGName(&value, bs);
	if (gname) {
	  // If objects are distributed only using the lazy
	  // protocol, this shouldn't happen: There must be a proxy
	  // already;
#if defined(DBG_TRACE)
	  fprintf(dbgout, " (at %d)\n", refTag);
	  fflush(dbgout);
#endif
	  b->buildObjectRemember(gname, refTag);
	} else {
	  // The same as for classes: either we have already the
	  // object (how comes? requested twice??), or that's the
	  // lazy object protocol;
	  OZ_Term vd = oz_deref(value);
	  Assert(!oz_isRef(vd));
	  if (oz_isConst(vd)) {
	    Assert(tagged2Const(vd)->getType() == Co_Object);
	    OZ_warning("Full object is received again.");
	    b->knownObject(value);
	    b->setTerm(value, refTag);
#if defined(DBG_TRACE)
	    fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	    fflush(dbgout);
#endif
	  } else if (oz_isVar(vd)) {
	    OzVariable *var = tagged2Var(vd);
	    Assert(var->getType() == OZ_VAR_EXT);
	    ExtVar *evar = var2ExtVar(var);
	    Assert(evar->getIdV() == OZ_EVAR_LAZY);
	    LazyVar *lvar = (LazyVar *) evar;
	    Assert(lvar->getLazyType() == LT_OBJECT);
	    ObjectVar *ov = (ObjectVar *) lvar;
	    gname = ov->getGName();
	    // Observe: the gname points to the proxy;
	    b->buildObjectRemember(gname, refTag);
#if defined(DBG_TRACE)
	    fprintf(dbgout, " (at %d)\n", refTag);
	    fflush(dbgout);
#endif
	  } else {
	    OZ_error("Unexpected value is bound to an object's gname.");
	    b->buildValue(oz_nil());
	  }
	}
	break;
      }

    case DIF_OBJECT:
      {
	OZ_Term value;
	GName *gname = unmarshalGName(&value, bs);
	if (gname) {
	  // If objects are distributed only using the lazy
	  // protocol, this shouldn't happen: There must be a proxy
	  // already;
	  b->buildObject(gname);
	} else {
	  // The same as for classes: either we have already the
	  // object (how comes? requested twice??), or that's the
	  // lazy object protocol;
	  OZ_Term vd = oz_deref(value);
	  Assert(!oz_isRef(vd));
	  if (oz_isConst(vd)) {
	    Assert(tagged2Const(vd)->getType() == Co_Object);
	    OZ_warning("Full object is received again.");
	    b->knownObject(value);
#if defined(DBG_TRACE)
	    fprintf(dbgout, " = %s\n", toC(value));
	    fflush(dbgout);
#endif
	  } else if (oz_isVar(vd)) {
	    OzVariable *var = tagged2Var(vd);
	    Assert(var->getType() == OZ_VAR_EXT);
	    ExtVar *evar = var2ExtVar(var);
	    Assert(evar->getIdV() == OZ_EVAR_LAZY);
	    LazyVar *lvar = (LazyVar *) evar;
	    Assert(lvar->getLazyType() == LT_OBJECT);
	    ObjectVar *ov = (ObjectVar *) lvar;
	    gname = ov->getGName();
	    // Observe: the gname points to the proxy;
	    b->buildObject(gname);
	  } else {
	    OZ_error("Unexpected value is bound to an object's gname.");
	    b->buildValue(oz_nil());
	  }
	}
	break;
      }

    case DIF_PROC_DEF:
      { 
	OZ_Term value;
	int refTag    = unmarshalRefTag(bs);
	GName *gname  = unmarshalGName(&value, bs);
	int arity     = unmarshalNumber(bs);
	int gsize     = unmarshalNumber(bs);
	int maxX      = unmarshalNumber(bs);
	int line      = unmarshalNumber(bs);
	int column    = unmarshalNumber(bs);
	int codesize  = unmarshalNumber(bs); // in ByteCode"s;

	//
	if (gname) {
	  //
	  CodeArea *code = new CodeArea(codesize);
	  ProgramCounter start = code->getStart();
	  ProgramCounter pc = start + sizeOf(DEFINITION);
	  //
	  Assert(codesize > 0);
	  DPBuilderCodeAreaDescriptor *desc =
	    new DPBuilderCodeAreaDescriptor(start, start+codesize, code);
	  b->buildBinary(desc);

	  //
	  b->buildProcRemember(gname, arity, gsize, maxX, line, column, 
			       pc, refTag);
#if defined(DBG_TRACE)
	  fprintf(dbgout,
		  " do arity=%d,line=%d,column=%d,codesize=%d (at %d)\n",
		  arity, line, column, codesize, refTag);
	  fflush(dbgout);
#endif
	} else {
	  Assert(oz_isAbstraction(oz_deref(value)));
	  // ('zero' descriptions are not allowed;)
	  DPBuilderCodeAreaDescriptor *desc =
	    new DPBuilderCodeAreaDescriptor(0, 0, 0);
	  b->buildBinary(desc);

	  //
	  b->knownProcRemember(value, refTag);
#if defined(DBG_TRACE)
	  fprintf(dbgout,
		  " skip %s (arity=%d,line=%d,column=%d,codesize=%d) (at %d)\n",
		  toC(value), arity, line, column, codesize, refTag);
	  fflush(dbgout);
#endif
	}
	break;
      }

    case DIF_PROC:
      { 
	OZ_Term value;
	GName *gname  = unmarshalGName(&value, bs);
	int arity     = unmarshalNumber(bs);
	int gsize     = unmarshalNumber(bs);
	int maxX      = unmarshalNumber(bs);
	int line      = unmarshalNumber(bs);
	int column    = unmarshalNumber(bs);
	int codesize  = unmarshalNumber(bs); // in ByteCode"s;

	//
	if (gname) {
	  //
	  CodeArea *code = new CodeArea(codesize);
	  ProgramCounter start = code->getStart();
	  ProgramCounter pc = start + sizeOf(DEFINITION);
	  //
	  Assert(codesize > 0);
	  DPBuilderCodeAreaDescriptor *desc =
	    new DPBuilderCodeAreaDescriptor(start, start+codesize, code);
	  b->buildBinary(desc);

	  //
	  b->buildProc(gname, arity, gsize, maxX, line, column, pc);
#if defined(DBG_TRACE)
	  fprintf(dbgout,
		  " do arity=%d,line=%d,column=%d,codesize=%d\n",
		  arity, line, column, codesize);
	  fflush(dbgout);
#endif
	} else {
	  Assert(oz_isAbstraction(oz_deref(value)));
	  // ('zero' descriptions are not allowed;)
	  DPBuilderCodeAreaDescriptor *desc =
	    new DPBuilderCodeAreaDescriptor(0, 0, 0);
	  b->buildBinary(desc);

	  //
	  b->knownProc(value);
#if defined(DBG_TRACE)
	  fprintf(dbgout,
		  " skip %s (arity=%d,line=%d,column=%d,codesize=%d)\n",
		  toC(value), arity, line, column, codesize);
	  fflush(dbgout);
#endif
	}
	break;
      }

      //
      // 'DIF_CODEAREA' is an artifact due to the non-recursive
      // unmarshaling of code areas: in order to unmarshal an Oz term
      // that occurs in an instruction, unmarshaling of instructions
      // must be interrupted and later resumed; 'DIF_CODEAREA' tells the
      // unmarshaler that a new code area chunk begins;
    case DIF_CODEAREA:
      {
	BuilderOpaqueBA opaque;
	DPBuilderCodeAreaDescriptor *desc = 
	  (DPBuilderCodeAreaDescriptor *) b->fillBinary(opaque);
	//
#if defined(DBG_TRACE)
	fprintf(dbgout,
		" [begin=%p, end=%p, current=%p]",
		desc->getStart(), desc->getEnd(), desc->getCurrent());
	fflush(dbgout);
#endif
	if (dpUnmarshalCode(bs, b, desc)) {
#if defined(DBG_TRACE)
	  fprintf(dbgout, " ..finished\n");
	  fflush(dbgout);
#endif
	  b->finishFillBinary(opaque);
	} else {
#if defined(DBG_TRACE)
	  fprintf(dbgout, " ..suspended\n");
	  fflush(dbgout);
#endif
	  b->suspendFillBinary(opaque);
	}
	break;
      }

    case DIF_DICT_DEF:
      {
	int refTag = unmarshalRefTag(bs);
	int size   = unmarshalNumber(bs);
	Assert(oz_onToplevel());
	b->buildDictionaryRemember(size,refTag);
#if defined(DBG_TRACE)
	fprintf(dbgout, " size=%d (at %d)\n", size, refTag);
	fflush(dbgout);
#endif
	break;
      }

    case DIF_DICT:
      {
	int size   = unmarshalNumber(bs);
	Assert(oz_onToplevel());
	b->buildDictionary(size);
#if defined(DBG_TRACE)
	fprintf(dbgout, " size=%d\n", size);
	fflush(dbgout);
#endif
	break;
      }

    case DIF_BUILTIN_DEF:
      {
	int refTag = unmarshalRefTag(bs);
	char *name = unmarshalString(bs);
	Builtin * found = string2CBuiltin(name);

	OZ_Term value;
	if (!found) {
	  OZ_warning("Builtin '%s' not in table.", name);
	  value = oz_nil();
	  delete name;
	} else {
	  if (found->isSited()) {
	    OZ_warning("Unpickling sited builtin: '%s'", name);
	  }
	  delete name;
	  value = makeTaggedConst(found);
	}
	b->buildValue(value);
	b->setTerm(value, refTag);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	fflush(dbgout);
#endif
	break;
      }

    case DIF_BUILTIN:
      {
	char *name = unmarshalString(bs);
	Builtin * found = string2CBuiltin(name);

	OZ_Term value;
	if (!found) {
	  OZ_warning("Builtin '%s' not in table.", name);
	  value = oz_nil();
	  delete name;
	} else {
	  if (found->isSited()) {
	    OZ_warning("Unpickling sited builtin: '%s'", name);
	  }
	  delete name;
	  value = makeTaggedConst(found);
	}
	b->buildValue(value);
#if defined(DBG_TRACE)
	fprintf(dbgout, " = %s\n", toC(value));
	fflush(dbgout);
#endif
	break;
      }

    case DIF_EXTENSION_DEF:
      {
	int refTag = unmarshalRefTag(bs);
	int type = unmarshalNumber(bs);

	//
	GTAbstractEntity *bae;
	OZ_Term value = oz_extension_unmarshal(type, bs, b, bae);
	switch (value) {
	case UnmarshalEXT_Susp:
	  Assert(bae);
	  b->getAbstractEntity(bae);
	  b->suspend();
#if defined(DBG_TRACE)
	  fprintf(dbgout, " suspended (for %d)\n", refTag);
	  fflush(dbgout);
#endif
	  return ((OZ_Term) -1);

	case UnmarshalEXT_Error:
	  OZ_error("Trouble with unmarshaling an extension!");
	  b->buildValue(oz_nil());
	  break;

	default:		// got it!
	  b->buildValue(value);
	  b->setTerm(value, refTag);
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s (at %d)\n", toC(value), refTag);
	  fflush(dbgout);
#endif
	  break;
	}
	break;
      }

    case DIF_EXTENSION:
      {
	int type = unmarshalNumber(bs);

	//
	GTAbstractEntity *bae;
	OZ_Term value = oz_extension_unmarshal(type, bs, b, bae);
	switch (value) {
	case UnmarshalEXT_Susp:
	  Assert(bae);
	  b->getAbstractEntity(bae);
	  b->suspend();
#if defined(DBG_TRACE)
	  fprintf(dbgout, " suspended\n");
	  fflush(dbgout);
#endif
	  return ((OZ_Term) -1);

	case UnmarshalEXT_Error:
	  OZ_error("Trouble with unmarshaling an extension!");
	  b->buildValue(oz_nil());
	  break;

	default:		// got it!
	  b->buildValue(value);
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s\n", toC(value));
	  fflush(dbgout);
#endif
	  break;
	}
	break;
      }

      // Continuation for "extensions";
    case DIF_EXT_CONT:
      {
	int type = unmarshalNumber(bs);

	//
	GTAbstractEntity *bae = b->buildAbstractEntity();
	Assert(bae->getType() == GT_ExtensionSusp);
	OZ_Term value = oz_extension_unmarshalCont(type, bs, b, bae);
	switch (value) {
	case UnmarshalEXT_Susp:
	  b->getAbstractEntity(bae);
	  b->suspend();
#if defined(DBG_TRACE)
	  fprintf(dbgout, " suspended\n");
	  fflush(dbgout);
#endif
	  return ((OZ_Term) -1);

	case UnmarshalEXT_Error:
	  OZ_error("Trouble with unmarshaling an extension!");
	  b->buildValue(oz_nil());
	  break;

	default:
#if defined(DBG_TRACE)
	  fprintf(dbgout, " = %s\n", toC(value));
	  fflush(dbgout);
#endif
	  b->buildValue(value);
	  break;
	}
	break;
      }

    case DIF_FSETVALUE:
#if defined(DBG_TRACE)
      fprintf(dbgout, "\n");
      fflush(dbgout);
#endif
      b->buildFSETValue();
      break;

    case DIF_ARRAY:
      OZ_error("not implemented!"); 
      b->buildValue(oz_nil());
      break;

      //
      // 'DIF_SYNC' and its handling is a part of the interface
      // between the builder object and the unmarshaler itself:
    case DIF_SYNC:
#if defined(DBG_TRACE)
      fprintf(dbgout, "\n");
      fflush(dbgout);
#endif
      b->processSync();
      break;

    case DIF_EOF: 
#if defined(DBG_TRACE)
      fprintf(dbgout, "\n");
      fflush(dbgout);
#endif
      return (b->finish());

    case DIF_SUSPEND:
#if defined(DBG_TRACE)
      fprintf(dbgout, "\n");
      fflush(dbgout);
#endif
      // 
      b->suspend();
      // returns '-1' for an additional consistency check - the
      // caller should know whether that's a complete message;
      return ((OZ_Term) -1);


    case DIF_UNUSED0:
    case DIF_UNUSED1:
    case DIF_UNUSED2:
    case DIF_UNUSED3:
    case DIF_UNUSED4:
    case DIF_UNUSED5:
    case DIF_UNUSED6:
    case DIF_UNUSED7:
    case DIF_UNUSED8:
      OZ_error("unmarshal: unexpected UNUSED tag: %d\n",tag);
      b->buildValue(oz_nil());
      break;

    default:
      OZ_error("unmarshal: unexpected tag: %d\n",tag);
      b->buildValue(oz_nil());
      break;
    }
  }
}

//
void DPMarshalers::dpAllocateMarshalers(int numof)
{
  if (musNum != numof) {
    MU *nmus = new MU[numof];
    int i = 0;

    //
    for (; i < min(musNum, numof); i++) {
      nmus[i] = mus[i];
    }
    for (; i < numof; i++) {
      nmus[i].flags = MUEmpty;
      nmus[i].m = (DPMarshaler *) 0; // lazy allocation;
      nmus[i].b = (Builder *) 0;
    }
    for (; i < musNum; i++) {
      if (mus[i].m) delete mus[i].m;
      if (mus[i].b) delete mus[i].b;
    }
    delete [] mus;

    //
    musNum = numof;
    mus = nmus;
  }
}

// 
DPMarshaler* DPMarshalers::dpGetMarshaler()
{
  for (int i = musNum; i--; ) {
    if (!(mus[i].flags & MUMarshalerBusy)) {
      if (!mus[i].m)
	mus[i].m = (DPMarshaler *) new DPMarshaler;
      mus[i].flags = mus[i].flags | MUMarshalerBusy;
      return (mus[i].m);
    }
  }
  OZ_error("dpGetMarshaler asked for an unallocated marshaler!");
  return ((DPMarshaler *) 0);
}
//
Builder* DPMarshalers::dpGetUnmarshaler()
{
  for (int i = musNum; i--; ) {
    if (!(mus[i].flags & MUBuilderBusy)) {
      if (!mus[i].b)
	mus[i].b = new Builder;
      mus[i].flags = mus[i].flags | MUBuilderBusy;
      return (mus[i].b);
    }
  }
  OZ_error("dpGetUnmarshaler asked for an unallocated builder!");
  return ((Builder *) 0);
}

//
void DPMarshalers::dpReturnMarshaler(DPMarshaler* dpm)
{
  for (int i = musNum; i--; ) {
    if (mus[i].m == dpm) {
      dpm->reset();
      Assert(mus[i].flags & MUMarshalerBusy);
      mus[i].flags = mus[i].flags & ~MUMarshalerBusy;
      return;
    }
  }
  OZ_error("dpReturnMarshaler got an unallocated builder!!");
}
//
void DPMarshalers::dpReturnUnmarshaler(Builder* dpb)
{
  for (int i = musNum; i--; ) {
    if (mus[i].b == dpb) {
      Assert(mus[i].flags & MUBuilderBusy);
      mus[i].flags = mus[i].flags & ~MUBuilderBusy;
      return;
    }
  }
  OZ_error("dpReturnMarshaler got an unallocated builder!!");
}
