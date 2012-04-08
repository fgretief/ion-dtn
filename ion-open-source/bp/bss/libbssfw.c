/*
 *	libbssfw.c:	functions enabling the implementation of
 *			a regional forwarder for the BSS endpoint
 *			ID scheme.
 *
 *	Copyright (c) 2006, California Institute of Technology.	
 *	Copyright (c) 2011, Space Internetworking Center,
 *	Democritus University of Thrace.
 *
 *	All rights reserved.						
 *	
 *	Authors: Scott Burleigh, Jet Propulsion Laboratory
 *		 Sotirios-Angelos Lenas, Space Internetworking Center (SPICE) 
 *
 *
 *	Modification History:
 *	Date	  Who	What
 *	02-06-06  SCB	Original development.
 *	08-08-11  SAL	Bundle Streaming Service extension.
 */

#include "bssfw.h"

#define	BSS_DBNAME	"ipnRoute"

#ifndef LIBBSSFWDEBUG
#define LIBBSSFWDEBUG	0
#endif

/*	*	*	Globals used for BSS scheme service.	*	*/

static Object	_bssdbObject(Object *newDbObj)
{
	static Object	obj = 0;

	if (newDbObj)
	{
		obj = *newDbObj;
	}

	return obj;
}

static BssDB	*_bssConstants()
{
	static BssDB	buf;
	static BssDB	*db = NULL;

	if (db == NULL)
	{
		sdr_read(getIonsdr(), (char *) &buf, _bssdbObject(NULL),
				sizeof(BssDB));
		db = &buf;
	}

	return db;
}

/*	*	*	Routing information mgt functions	*	*/

static int	lookupBssEid(char *uriBuffer, char *neighborClId)
{
	Sdr	sdr = getIonsdr();
	Object	elt;
		OBJ_POINTER(BssPlan, plan);

	for (elt = sdr_list_first(sdr, (_bssConstants())->plans); elt;
			elt = sdr_list_next(sdr, elt))
	{
		GET_OBJ_POINTER(sdr, BssPlan, plan, sdr_list_data(sdr, elt));
		switch (clIdMatches(neighborClId, &plan->defaultDirective))
		{
		case -1:
			putErrmsg("Failed looking up BSS EID.", NULL);
			return -1;

		case 0:
			continue;	/*	No match.		*/

		default:

			/*	Found the plan for transmission to
			 *	this neighbor, so now we know the
			 *	neighbor's EID.				*/

			isprintf(uriBuffer, SDRSTRING_BUFSZ,
					"ipn:%lu.0", plan->nodeNbr);
			return 1;
		}
	}

	return 0;
}

int	ipnInit()
{
	/*	BSS is a plug-compatible replacement for IPN, which
	 *	subsumes all of the IPN forwarding logic and extends
	 *	it.  So it would never be linked into the same
	 *	deployment of ION as the original IPN, so we can
	 *	simplify software configuration somewhat by providing
	 *	an alternative implementation of the ipnInit() function
	 *	rather than an equivalen bssInit(), which would
	 *	require that other software be explicitly bss-aware.	*/

	Sdr	sdr = getIonsdr();
	Object	bssdbObject;
	BssDB	bssdbBuf;

	/*	Recover the BSS database, creating it if necessary.	*/

	sdr_begin_xn(sdr);
	oK(senderEidLookupFunctions(lookupBssEid));
	bssdbObject = sdr_find(sdr, BSS_DBNAME, NULL);
	switch (bssdbObject)
	{
	case -1:		/*	SDR error.			*/
		sdr_cancel_xn(sdr);
		putErrmsg("Failed seeking BSS database in SDR.", NULL);
		return -1;

	case 0:			/*	Not found; must create new DB.	*/
		bssdbObject = sdr_malloc(sdr, sizeof(BssDB));
		if (bssdbObject == 0)
		{
			sdr_cancel_xn(sdr);
			putErrmsg("No space for BSS database.", NULL);
			return -1;
		}

		memset((char *) &bssdbBuf, 0, sizeof(BssDB));
		bssdbBuf.plans = sdr_list_create(sdr);
		bssdbBuf.groups = sdr_list_create(sdr);
		bssdbBuf.bssList = sdr_list_create(sdr);
		sdr_write(sdr, bssdbObject, (char *) &bssdbBuf, sizeof(BssDB));
		sdr_catlg(sdr, BSS_DBNAME, 0, bssdbObject);
		if (sdr_end_xn(sdr))
		{
			putErrmsg("Can't create BSS database.", NULL);
			return -1;
		}

		break;

	default:		/*	Found DB in the SDR.		*/
		sdr_exit_xn(sdr);
	}

	oK(_bssdbObject(&bssdbObject));
	oK(_bssConstants());
	return 0;
}

Object	getBssDbObject()
{
	return _bssdbObject(NULL);
}

BssDB	*getBssConstants()
{
	return _bssConstants();
}

Object locateBssEntry (CbheEid dst, Object *nextEntry) 
{
	Sdr	sdr = getIonsdr();
	Object	elt;
		OBJ_POINTER(bssEntry, entry);

	unsigned long serviceNbr = dst.serviceNbr; 
	unsigned long nodeNbr = dst.nodeNbr;

	/*  
	 *  This function locates the bssEntry identified by the
	 *  specified service number (or pair of node-service number), 
	 *  if any.  If none, notes the location within the bssList at 
	 *  which such an entry should be inserted.			
	 */

	if (nextEntry) *nextEntry = 0;    /*      Default. */

	for (elt = sdr_list_first(sdr, (_bssConstants())->bssList); elt;
			elt = sdr_list_next(sdr, elt))
	{
		GET_OBJ_POINTER(sdr, bssEntry, entry, sdr_list_data(sdr, elt));
		
		/*	 Matched bssEntry's service number.	 */
		if (entry->dstServiceNbr < serviceNbr)
		{
			continue;
		}

		if (entry->dstServiceNbr > serviceNbr)
		{
			if (entry->dstServiceNbr != BSS_ALL_OTHERS)
			{						
				if (nextEntry) *nextEntry = elt;
				break;	/*	Same as end of list.	*/
			}
		}

		/*	      Matched bssEntry's node number.		*/

		if (entry->dstNodeNbr < nodeNbr)
		{
			continue;
		}

		if (entry->dstNodeNbr > nodeNbr)
		{
			if (entry->dstNodeNbr != BSS_ALL_OTHERS)
			{					
				if (nextEntry) *nextEntry = elt;
				break;	/*	Same as end of list.	*/
			}
		}

		/*	Exact match.					*/
		return elt;
	}

	return 0;
}

int bss_addBssEntry(long argServiceNbr, long argNodeNbr)

{
	Sdr		sdr = getIonsdr();
	unsigned long	dstServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	dstNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	
	bssEntry	entry;
	Object		entryObj;
	Object		nextEntry;
	CbheEid		eid;
	char 		memo[256];

	CHKERR(argServiceNbr && argNodeNbr);
	sdr_begin_xn(sdr);
	eid.serviceNbr = dstServiceNbr;
	eid.nodeNbr = dstNodeNbr;

	if (locateBssEntry(eid, &nextEntry) !=0)
	{
		sdr_exit_xn(sdr);
		isprintf(memo, sizeof memo, "[?] BSS duplicate entry: %lu-%lu", 
				 dstServiceNbr,dstNodeNbr);
		writeMemo(memo);
		return 0;
	}

	entry.dstServiceNbr = dstServiceNbr;
	entry.dstNodeNbr = dstNodeNbr;
	entryObj = sdr_malloc(sdr, sizeof(bssEntry));

	if (entryObj)
	{
		if (nextEntry)
		{
			oK(sdr_list_insert_before(sdr, nextEntry, entryObj));
		}
		else
		{
			oK(sdr_list_insert_last(sdr, 
				(_bssConstants())->bssList, entryObj));
		}

		sdr_write(sdr, entryObj, (char *) &entry, sizeof(bssEntry));
	}

	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't add entry.", NULL);
		return -1;
	}

	return 1;
}

int bss_removeBssEntry(long argServiceNbr, long argNodeNbr)
{
	Sdr		sdr = getIonsdr();
	unsigned long	dstServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	dstNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	Object	elt;
	Object	entryObj;
	CbheEid eid;
	eid.serviceNbr = dstServiceNbr;
	eid.nodeNbr = dstNodeNbr;

	CHKERR(argServiceNbr && argNodeNbr);
	sdr_begin_xn(sdr);
	elt = locateBssEntry(eid, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] BSS entry not found", utoa(argServiceNbr));
		return 0;
	}

	entryObj = sdr_list_data(sdr, elt);

	/*	Okay to remove this entry from the database.		*/

	sdr_list_delete(sdr, elt, NULL, NULL);
	sdr_free(sdr, entryObj);

	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't remove BSS entry.", utoa(dstServiceNbr));
		return -1;
	}

	return 1;
}

static LystElt locateStream (Lyst loggedStreams, Bundle *bundle, 
			      LystElt *nextElt)
{	
	LystElt elt;
	stream *strm = NULL;
	unsigned long srcNodeNbr = bundle->id.source.c.nodeNbr;
	unsigned long srcServiceNbr = bundle->id.source.c.serviceNbr;
	unsigned long dstNodeNbr = bundle->destination.c.nodeNbr;
	unsigned long dstServiceNbr = bundle->destination.c.serviceNbr;

	/*	
	 *  This function locates the BSS stream identified by the 
	 *  specified bundle arguments (pair of node-service numbers
	 *  both  for source and destination), if any. If none, notes the 
	 *  location within the loggedStreams list at which such an entry 
	 *  should be inserted.					      
	 */

	if (nextElt) *nextElt = 0; /*	Default */
	for (elt = lyst_first(loggedStreams); elt; elt = lyst_next(elt))
	{		
		strm = (stream *) lyst_data(elt);
		/*	    Matched entry's src node number.		*/
		if (strm->srcNodeNbr < srcNodeNbr)
		{
			continue;
		}

		if (strm->srcNodeNbr  > srcNodeNbr)
		{
			if (nextElt) *nextElt = elt;			
			break;		/*	Same as end of list.	*/
		}

		/*	   Matched entry's src service number.		*/
		if (strm->srcServiceNbr < srcServiceNbr)
		{
			continue;
		}

		if (strm->srcServiceNbr  > srcServiceNbr)
		{
			if (nextElt) *nextElt = elt;			
			break;		/*	Same as end of list.	*/
		}

		/*	    Matched entry's dst node number.		*/
		if (strm->dstNodeNbr < dstNodeNbr)
		{
			continue;
		}

		if (strm->dstNodeNbr  > dstNodeNbr)
		{
			if (nextElt) *nextElt = elt;			
			break;		/*	Same as end of list.	*/
		}
		
		/*	   Matched entry's dst service number.		*/
		if (strm->dstServiceNbr < dstServiceNbr)
		{
			continue;
		}

		if (strm->dstServiceNbr  > dstServiceNbr)
		{
			if (nextElt) *nextElt = elt;			
			break;		/*	Same as end of list.	*/
		}

		/*	Exact match.					*/
		return elt;
	}

	return NULL;
}

static int bss_addNewStream (Lyst loggedStreams, Bundle bundle, LystElt nextElt) 
{	
	stream *strm;	
	strm = (stream *) MTAKE(sizeof(stream));
	memset((char *) strm, 0, sizeof(stream));
	
	strm->srcNodeNbr = bundle.id.source.c.nodeNbr;
	strm->srcServiceNbr = bundle.id.source.c.serviceNbr;
	strm->dstNodeNbr = bundle.destination.c.nodeNbr;
	strm->dstServiceNbr = bundle.destination.c.serviceNbr;
	strm->latestTimeLogged = bundle.id.creationTime;

	if (nextElt)
	{
		if (lyst_insert_before(nextElt, strm)==NULL)
		{
			return -1;
		}
	}
	else
	{
		if (lyst_insert_last(loggedStreams, strm)==NULL)
		{
			return -1;
		}
	}

	return 0;
}

void bss_monitorStream(Lyst loggedStreams, Bundle bundle) 
{	
	LystElt streamElt;
	LystElt nextElt;
	stream *strm = NULL;
	BpTimestamp loggedTime = bundle.id.creationTime;
		
	streamElt = locateStream(loggedStreams, &bundle, &nextElt);
	
	/*  
	 *  This function locates the BSS stream identified by the 
	 *  specified bundle arguments, if any, and updates its content. 
	 *  If none, it adds a new entry at the loggedStreams list.     
	 */

	if (streamElt == NULL)
	{
		if (bss_addNewStream(loggedStreams, bundle, nextElt) != 0)
		{
			putErrmsg("Failed to add new stream", NULL);
		}
	}
	else
	{			
		/*	Update current stream		*/			
		strm = (stream *) lyst_data(streamElt);
			
		if(loggedTime.seconds > strm->latestTimeLogged.seconds || 
		  (loggedTime.seconds == strm->latestTimeLogged.seconds && 
		   loggedTime.count > strm->latestTimeLogged.count))
		{	
			strm->latestTimeLogged = loggedTime; 
		}		
	}
}

static Object	locatePlan(unsigned long nodeNbr, Object *nextPlan)
{
	Sdr	sdr = getIonsdr();
	Object	elt;
		OBJ_POINTER(BssPlan, plan);

	/*	This function locates the BssPlan identified by the
	 *	specified node number, if any.  If none, notes the
	 *	location within the plans list at which such a plan
	 *	should be inserted.					*/

	if (nextPlan) *nextPlan = 0;	/*	Default.		*/
	for (elt = sdr_list_first(sdr, (_bssConstants())->plans); elt;
			elt = sdr_list_next(sdr, elt))
	{
		GET_OBJ_POINTER(sdr, BssPlan, plan, sdr_list_data(sdr, elt));
		if (plan->nodeNbr < nodeNbr)
		{
			continue;
		}
		
		if (plan->nodeNbr > nodeNbr)
		{
			if (nextPlan) *nextPlan = elt;
			break;		/*	Same as end of list.	*/
		}
		return elt;
	}
	return 0;
}

void	bss_findPlan(unsigned long nodeNbr, Object *planAddr, Object *eltp)
{
	Sdr	sdr = getIonsdr();
	Object	elt;

	/*	This function finds the BssPlan for the specified
	 *	node, if any.						*/

	CHKVOID(ionLocked());
	CHKVOID(nodeNbr && planAddr && eltp);
	*eltp = 0;
	elt = locatePlan(nodeNbr, NULL);
	if (elt == 0)
	{
		return;
	}

	*planAddr = sdr_list_data(sdr, elt);
	*eltp = elt;
}

static void	createXmitDirective(FwdDirective *directive,
			DuctExpression *parms)
{
	directive->action = xmit;
	directive->outductElt = parms->outductElt;
	if (parms->destDuctName == NULL)
	{
		directive->destDuctName = 0;
	}
	else
	{
		directive->destDuctName = sdr_string_create(getIonsdr(),
				parms->destDuctName);
	}
}

int	bss_addPlan(unsigned long nodeNbr, DuctExpression *defaultDuct,
		    DuctExpression *rtDuct, DuctExpression *pbDuct,
		    unsigned long expectedRTT)
{
	Sdr	sdr = getIonsdr();
	Object	nextPlan;
	BssPlan	plan;
	Object	planObj;

	CHKERR(nodeNbr && defaultDuct);
	sdr_begin_xn(sdr);
	if (locatePlan(nodeNbr, &nextPlan) != 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Duplicate plan", utoa(nodeNbr));
		return 0;
	}

	/*	Okay to add this plan to the database.			*/

	plan.nodeNbr = nodeNbr;
	createXmitDirective(&(plan.defaultDirective), defaultDuct);
	createXmitDirective(&(plan.rtDirective), rtDuct);
	createXmitDirective(&(plan.pbDirective), pbDuct);
	plan.rules = sdr_list_create(sdr);
	plan.expectedRTT = expectedRTT;
	planObj = sdr_malloc(sdr, sizeof(BssPlan));
	if (planObj)
	{
		if (nextPlan)
		{
			oK(sdr_list_insert_before(sdr, nextPlan, planObj));
		}
		else
		{
			oK(sdr_list_insert_last(sdr,
					(_bssConstants())->plans, planObj));
		}

		sdr_write(sdr, planObj, (char *) &plan, sizeof(BssPlan));
	}

	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't add plan.", NULL);
		return -1;
	}

	return 1;
}

static void	destroyXmitDirective(FwdDirective *directive)
{
	if (directive->destDuctName)
	{
		sdr_free(getIonsdr(), directive->destDuctName);
	}
}

int	bss_updatePlan(unsigned long nodeNbr, DuctExpression *defaultDuct,
		    DuctExpression *rtDuct, DuctExpression *pbDuct,
		    int expectedRTT)
{
	Sdr	sdr = getIonsdr();
	Object	elt;
	Object	planObj;
	BssPlan	plan;

	CHKERR(nodeNbr && defaultDuct);
	sdr_begin_xn(sdr);
	elt = locatePlan(nodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] This plan is not defined.", utoa(nodeNbr));
		return 0;
	}

	/*	Okay to update this plan.	*/

	planObj = sdr_list_data(sdr, elt);
	sdr_stage(sdr, (char *) &plan, planObj, sizeof(BssPlan));
	destroyXmitDirective(&(plan.defaultDirective));
	createXmitDirective(&(plan.defaultDirective), defaultDuct);
	destroyXmitDirective(&(plan.rtDirective));
	createXmitDirective(&(plan.rtDirective), rtDuct);
	destroyXmitDirective(&(plan.pbDirective));
	createXmitDirective(&(plan.pbDirective), pbDuct);
	plan.expectedRTT = expectedRTT;
	sdr_write(sdr, planObj, (char *) &plan, sizeof(BssPlan));
	
	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't update plan.", utoa(nodeNbr));
		return -1;
	}

	return 1;
}

int	bss_removePlan(unsigned long nodeNbr)
{
	Sdr	sdr = getIonsdr();
	Object	elt;
	Object	planObj;
		OBJ_POINTER(BssPlan, plan);

	CHKERR(nodeNbr);
	sdr_begin_xn(sdr);
	elt = locatePlan(nodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Unknown plan", utoa(nodeNbr));
		return 0;
	}

	planObj = sdr_list_data(sdr, elt);
	GET_OBJ_POINTER(sdr, BssPlan, plan, planObj);
	if (sdr_list_length(sdr, plan->rules) > 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Can't remove plan; still has rules",
				utoa(nodeNbr));
		return 0;
	}

	/*	Okay to remove this plan from the database.		*/

	sdr_list_delete(sdr, elt, NULL, NULL);
	destroyXmitDirective(&(plan->defaultDirective));
	destroyXmitDirective(&(plan->rtDirective));
	destroyXmitDirective(&(plan->pbDirective));
	sdr_list_destroy(sdr, plan->rules, NULL, NULL);
	sdr_free(sdr, planObj);
	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't remove plan.", utoa(nodeNbr));
		return -1;
	}

	return 1;
}

static Object	locateRule(Object rules, unsigned long srcServiceNbr,
			unsigned long srcNodeNbr, Object *nextRule)
{
	Sdr	sdr = getIonsdr();
	Object	elt;
		OBJ_POINTER(BssRule, rule);

	/*	This function locates the BssRule for the specified
	 *	service number and source node number, for the
	 *	specified destination node number, if any; if
	 *	none, notes the location within the rules list at
	 *	which such a rule should be inserted.			*/

	if (nextRule) *nextRule = 0;	/*	Default.		*/
	for (elt = sdr_list_first(sdr, rules); elt;
			elt = sdr_list_next(sdr, elt))
	{
		GET_OBJ_POINTER(sdr, BssRule, rule, sdr_list_data(sdr, elt));
		if (rule->srcServiceNbr < srcServiceNbr)
		{
			continue;
		}

		if (rule->srcServiceNbr > srcServiceNbr)
		{
			if (nextRule) *nextRule = elt;
			break;		/*	Same as end of list.	*/
		}

		/*	Matched rule's service number.			*/

		if (rule->srcNodeNbr < srcNodeNbr)
		{
			continue;
		}

		if (rule->srcNodeNbr > srcNodeNbr)
		{
			if (nextRule) *nextRule = elt;
			break;		/*	Same as end of list.	*/
		}

		/*	Exact match.					*/
		
		return elt;
	}

	return 0;
}

void	bss_findPlanRule(unsigned long nodeNbr, long argServiceNbr,
		long argNodeNbr, BssPlan *plan, Object *ruleAddr, Object *eltp)
{
	Sdr		sdr = getIonsdr();
	unsigned long	srcServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	srcNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	Object		elt;
			OBJ_POINTER(BssPlan, planPtr);

	/*	This function finds the BssRule for the specified
	 *	service number and source node number, for the
	 *	specified destination node number, if any.		*/

	CHKVOID(ionLocked());
	CHKVOID(ruleAddr);
	CHKVOID(eltp);
	*eltp = 0;
	if (plan == NULL)
	{
		if (nodeNbr == 0)
		{
			return;
		}

		elt = locatePlan(nodeNbr, NULL);
		if (elt == 0)
		{
			return;
		}

		GET_OBJ_POINTER(sdr, BssPlan, planPtr, sdr_list_data(sdr, elt));
		plan = planPtr;
	}

	elt = locateRule(plan->rules, srcServiceNbr, srcNodeNbr, NULL);
	if (elt == 0)
	{
		return;
	}

	*ruleAddr = sdr_list_data(sdr, elt);
	*eltp = elt;
}

int	bss_addPlanRule(unsigned long nodeNbr, long argServiceNbr,
		long argNodeNbr, DuctExpression *directive, 
		DuctExpression *rtDuct, DuctExpression *pbDuct)
{
	Sdr		sdr = getIonsdr();
	unsigned long	srcServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	srcNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	Object		elt;
			OBJ_POINTER(BssPlan, plan);
	Object		nextRule;
	BssRule		ruleBuf;
	Object		addr;

	CHKERR(nodeNbr && srcNodeNbr);
	sdr_begin_xn(sdr);
	elt = locatePlan(nodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] No plan defined for this node",
				utoa(nodeNbr));
		return 0;
	}

	GET_OBJ_POINTER(sdr, BssPlan, plan, sdr_list_data(sdr, elt));

	if (locateRule(plan->rules, srcServiceNbr, srcNodeNbr, &nextRule) != 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Duplicate rule", utoa(srcNodeNbr));
		return 0;
	}

	/*	All parameters validated, okay to add the rule.		*/

	
	memset((char *) &ruleBuf, 0, sizeof(BssRule));
	ruleBuf.srcServiceNbr = srcServiceNbr;
	ruleBuf.srcNodeNbr = srcNodeNbr;
	createXmitDirective(&ruleBuf.directive, directive); 
	createXmitDirective(&ruleBuf.rtDirective, rtDuct);
	createXmitDirective(&ruleBuf.pbDirective, pbDuct);

	addr = sdr_malloc(sdr, sizeof(BssRule));
	if (addr)
	{
		if (nextRule)
		{
			elt = sdr_list_insert_before(sdr, nextRule, addr);
		}
		else
		{
			elt = sdr_list_insert_last(sdr, plan->rules, addr);
		}

		sdr_write(sdr, addr, (char *) &ruleBuf, sizeof(BssRule));
	}

	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't add rule.", NULL);
		return -1;
	}

	return 1;
}

int	bss_updatePlanRule(unsigned long nodeNbr, long argServiceNbr,
		long argNodeNbr, DuctExpression *directive, 
		DuctExpression *rtDuct, DuctExpression *pbDuct)
{
	Sdr		sdr = getIonsdr();
	unsigned long	srcServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	srcNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	Object		elt;
			OBJ_POINTER(BssPlan, plan);
	Object		ruleAddr;
	BssRule		ruleBuf;

	CHKERR(nodeNbr && srcNodeNbr);
	sdr_begin_xn(sdr);
	elt = locatePlan(nodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] No plan defined for this node",
				utoa(nodeNbr));
		return 0;
	}

	GET_OBJ_POINTER(sdr, BssPlan, plan, sdr_list_data(sdr, elt));
	bss_findPlanRule(nodeNbr, srcServiceNbr, srcNodeNbr, plan, &ruleAddr,
			&elt);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Unknown rule", utoa(srcNodeNbr));
		return 0;
	}

	/*	All parameters validated, okay to update the rule.	*/

	sdr_stage(sdr, (char *) &ruleBuf, ruleAddr, sizeof(BssRule));
	destroyXmitDirective(&ruleBuf.directive);
	createXmitDirective(&ruleBuf.directive, directive);
	destroyXmitDirective(&ruleBuf.rtDirective); 
	createXmitDirective(&ruleBuf.rtDirective, rtDuct);
	destroyXmitDirective(&ruleBuf.pbDirective); 
	createXmitDirective(&ruleBuf.pbDirective, pbDuct);
	sdr_write(sdr, ruleAddr, (char *) &ruleBuf, sizeof(BssRule));
	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't update rule.", NULL);
		return -1;
	}

	return 1;
}

int	bss_removePlanRule(unsigned long nodeNbr, long argServiceNbr,
		long argNodeNbr)
{
	Sdr		sdr = getIonsdr();
	unsigned long	srcServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	srcNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	Object		elt;
			OBJ_POINTER(BssPlan, plan);
	Object		ruleAddr;
			OBJ_POINTER(BssRule, rule);

	CHKERR(nodeNbr && srcNodeNbr);
	sdr_begin_xn(sdr);
	elt = locatePlan(nodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] No plan defined for this node",
				utoa(nodeNbr));
		return 0;
	}

	GET_OBJ_POINTER(sdr, BssPlan, plan, sdr_list_data(sdr, elt));
	bss_findPlanRule(nodeNbr, srcServiceNbr, srcNodeNbr, plan, &ruleAddr,
			&elt);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Unknown rule", utoa(srcNodeNbr));
		return 0;
	}

	/*	All parameters validated, okay to remove the rule.	*/

	GET_OBJ_POINTER(sdr, BssRule, rule, ruleAddr);
	destroyXmitDirective(&rule->directive);
	destroyXmitDirective(&(rule->rtDirective));
	destroyXmitDirective(&(rule->pbDirective));
	sdr_free(sdr, ruleAddr);
	sdr_list_delete(sdr, elt, NULL, NULL);
	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't remove rule.", NULL);
		return -1;
	}

	return 1;
}

int bss_copyDirective(Bundle *bundle, FwdDirective *directive, 
		      FwdDirective *defaultDirective,
		      FwdDirective *rtDirective,
		      FwdDirective *pbDirective,
		      Lyst loggedStreams)
{
	LystElt streamElt;
	stream *strm = NULL;	
	
	if(locateBssEntry(bundle->destination.c, NULL)!=0)
	{
		streamElt = locateStream(loggedStreams, bundle, NULL);
	
		if (streamElt == NULL)
		{
			if(rtDirective->outductElt!=0)
			{						
				memcpy((char *) directive, (char *) rtDirective,
					sizeof(FwdDirective));
				#if LIBBSSFWDEBUG
				printf("No entry detected in loggedStreams list\n");
				printf("Real-time mode directive was returned\n");
				#endif
			}
			else
			{					
				memcpy((char *) directive, 
					(char *) defaultDirective,
					sizeof(FwdDirective));
				#if LIBBSSFWDEBUG
				printf("No entry detected in loggedStreams list\n");
				printf("The default directive was returned\n");
				#endif
			}
		}
		else
		{			
			strm = (stream *) lyst_data(streamElt);
				
			if (bundle->id.creationTime.seconds > 
			    strm->latestTimeLogged.seconds || 
			    (bundle->id.creationTime.seconds == 
			    strm->latestTimeLogged.seconds && 
		    	    bundle->id.creationTime.count > 
			    strm->latestTimeLogged.count))
			{											
				if(rtDirective->outductElt!=0)
				{					
					memcpy((char *) directive, 
						(char *) rtDirective,
						sizeof(FwdDirective));
					#if LIBBSSFWDEBUG
					printf("Real-time mode directive was returned\n");
					#endif
				}
				else
				{
					memcpy((char *) directive, 
						(char *) defaultDirective,
						sizeof(FwdDirective));
					#if LIBBSSFWDEBUG
					printf("The default directive was returned\n");
					#endif
				}	
			}
			else
			{
				if(pbDirective->outductElt!=0)
				{						
					memcpy((char *) directive, 
						(char *) pbDirective,
						sizeof(FwdDirective));
					#if LIBBSSFWDEBUG
					printf("Playback mode directive was returned\n");
					#endif
				}
				else
				{				
					memcpy((char *) directive, 
						(char *) defaultDirective,
						sizeof(FwdDirective));
					#if LIBBSSFWDEBUG
					printf("The default directive was returned\n");
					#endif
				}
			}
		}
	}
	else
	{			
		memcpy((char *) directive, (char *) defaultDirective,
			sizeof(FwdDirective));
		#if LIBBSSFWDEBUG
		printf("Not a BSS traffic bundle\n");
		printf("The default directive was returned\n");
		#endif
	}

	return 1;		
}

static int	lookupRule(Object rules, unsigned long sourceServiceNbr,
			unsigned long sourceNodeNbr, Bundle *bundle,
			FwdDirective *dirbuf, Lyst loggedStreams)
{
	Sdr	sdr = getIonsdr();
	Object	addr;
	Object	elt;
		OBJ_POINTER(BssRule, rule);

	/*	Universal wild-card match (BSS_ALL_OTHERS), if any,
	 *	is at the end of the list, so there's no way to
	 *	terminate the search early.				*/

	for (elt = sdr_list_first(sdr, rules); elt;
			elt = sdr_list_next(sdr, elt))
	{
		addr = sdr_list_data(sdr, elt);
		GET_OBJ_POINTER(sdr, BssRule, rule, addr);
		if (rule->srcServiceNbr < sourceServiceNbr)
		{
			continue;
		}

		if (rule->srcServiceNbr > sourceServiceNbr
			&& rule->srcServiceNbr != BSS_ALL_OTHERS)
		{
			continue;
		}

		/*	Found a rule with matching service number.	*/

		if (rule->srcNodeNbr < sourceNodeNbr)
		{
			continue;
		}

		if (rule->srcNodeNbr > sourceNodeNbr
			&& rule->srcNodeNbr != BSS_ALL_OTHERS)
		{
			continue;
		}

		break;			/*	Stop searching.		*/
	}

	if (elt)			/*	Found a matching rule.	*/
	{	
		bss_copyDirective(bundle, dirbuf, &rule->directive,
				  &rule->rtDirective, &rule->pbDirective, 
				  loggedStreams);
		return 1;		
	}
	return 0;
}

int	bss_lookupPlanDirective(unsigned long nodeNbr,
		unsigned long sourceServiceNbr, unsigned long sourceNodeNbr,
		Bundle *bundle, FwdDirective *dirbuf, Lyst loggedStreams)
{
	Sdr	sdr = getIonsdr();
	Object	addr;
	Object	elt;
		OBJ_POINTER(BssPlan, plan);

	/*	This function determines the relevant FwdDirective for
	 *	the specified eid, if any.  Wild card match is okay.	*/

	CHKERR(ionLocked());
	CHKERR(nodeNbr && dirbuf);

	/*	Find the matching plan.					*/

	bss_findPlan(nodeNbr, &addr, &elt);
	if (elt == 0)
	{
		return 0;		/*	No plan found.		*/
	}

	GET_OBJ_POINTER(sdr, BssPlan, plan, addr);

	/*	Find best matching rule.				*/

	if (lookupRule(plan->rules, sourceServiceNbr, sourceNodeNbr, bundle,
			dirbuf, loggedStreams) == 0)	/*	No rule found.		*/
	{
		bss_copyDirective(bundle, dirbuf, &plan->defaultDirective,
				  &plan->rtDirective, &plan->pbDirective, 
				  loggedStreams);
	}	
	return 1;
}

static int getRTT(unsigned long pxNodeNbr)
{
	Sdr	sdr = getIonsdr();
	Object	addr;
	Object	planElt;
		OBJ_POINTER(BssPlan, plan);
	
	CHKERR(ionLocked());

	/*	This function retrieves the expected RTT from the 
	 *	specified proximate node's plan				*/
	
	bss_findPlan(pxNodeNbr, &addr, &planElt);
	if (planElt == 0)
	{
		return 0;		/*	No plan found.		*/
	}

	GET_OBJ_POINTER(sdr, BssPlan, plan, addr);

	return plan->expectedRTT;
}

int 	bss_setCtDueTimer(Bundle bundle, Object bundleAddr)
{
	Sdr		sdr = getIonsdr();
	char		proxNodeEid[SDRSTRING_BUFSZ];
	MetaEid		metaEid;
	VScheme		*vscheme;
	PsmAddress	vschemeElt;
	int		RTT;

	CHKERR(ionLocked());
	
	if (bundle.ductXmitElt)		/*	Queued by forwarder.	*/
	{
		/* 
		 *  Checking whether bundle is enqueued to limbo queue.
		 *  In  that case do nothing. Otherwise, set the custody      
		 *  expiration event for this particular bundle according
		 *  to BSS configuration.				*/

		if (sdr_list_list(sdr, bundle.ductXmitElt) ==
				(getBpConstants())->limboQueue)
		{
			return 0;	/*	Bundle is in limbo.	*/
		}

		sdr_string_read(sdr, proxNodeEid, bundle.proxNodeEid);	
		if (parseEidString(proxNodeEid, &metaEid, &vscheme, 
				&vschemeElt) == 0)
		{
			putErrmsg("Can't parse node EID string.", proxNodeEid);
			return -1;
		}
	
		RTT = getRTT(metaEid.nodeNbr);
		if (RTT > 0)
		{ 
			if (bpMemo(bundleAddr, RTT) < 0)
			{
				return -1; /*	bpMemo failed	*/
			}

			return 1;
		}
	}

	/*	Bundle not queued for transmission or an invalid RTT
	 *	value was returned.					*/	

	return -1;
}

static Object	locateGroup(unsigned long firstNodeNbr,
			unsigned long lastNodeNbr, Object *nextGroup)
{
	Sdr	sdr = getIonsdr();
	int	targetSize;
	int	groupSize;
	Object	elt;
		OBJ_POINTER(IpnGroup, group);

	/*	This function locates the IpnGroup for the specified
	 *	first node number, if any; if none, notes the
	 *	location within the rules list at which such a rule
	 *	should be inserted.					*/

	if (nextGroup) *nextGroup = 0;	/*	Default.		*/
	targetSize = lastNodeNbr - firstNodeNbr;
	for (elt = sdr_list_first(sdr, (_bssConstants())->groups); elt;
			elt = sdr_list_next(sdr, elt))
	{
		GET_OBJ_POINTER(sdr, IpnGroup, group, sdr_list_data(sdr, elt));
		groupSize = group->lastNodeNbr - group->firstNodeNbr;
		if (groupSize < targetSize)
		{
			continue;
		}

		if (groupSize > targetSize)
		{
			if (nextGroup) *nextGroup = elt;
			break;		/*	Same as end of list.	*/
		}

		if (group->firstNodeNbr < firstNodeNbr)
		{
			continue;
		}

		if (group->firstNodeNbr > firstNodeNbr)
		{
			if (nextGroup) *nextGroup = elt;
			break;		/*	Same as end of list.	*/
		}

		/*	Matched group's first node number.		*/

		return elt;
	}

	return 0;
}

void	bss_findGroup(unsigned long firstNodeNbr, unsigned long lastNodeNbr,
		Object *groupAddr, Object *eltp)
{
	Sdr	sdr = getIonsdr();
	Object	elt;

	/*	This function finds the IpnGroup for the specified
	 *	node range, if any.					*/

	CHKVOID(ionLocked());
	CHKVOID(firstNodeNbr && groupAddr && eltp);
	CHKVOID(firstNodeNbr <= lastNodeNbr);
	*eltp = 0;
	elt = locateGroup(firstNodeNbr, lastNodeNbr, NULL);
	if (elt == 0)
	{
		return;
	}

	*groupAddr = sdr_list_data(sdr, elt);
	*eltp = elt;
}

int	bss_addGroup(unsigned long firstNodeNbr, unsigned long lastNodeNbr,
		char *viaEid)
{
	Sdr		sdr = getIonsdr();
	Object		nextGroup;
	IpnGroup	group;
	Object		addr;

	CHKERR(firstNodeNbr && lastNodeNbr && viaEid);
	CHKERR(firstNodeNbr <= lastNodeNbr);
	CHKERR(strlen(viaEid) <= MAX_SDRSTRING);
	sdr_begin_xn(sdr);
	if (locateGroup(firstNodeNbr, lastNodeNbr, &nextGroup) != 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Duplicate group", utoa(firstNodeNbr));
		return 0;
	}

	/*	All parameters validated, okay to add the group.	*/

	memset((char *) &group, 0, sizeof(IpnGroup));
	group.firstNodeNbr = firstNodeNbr;
	group.lastNodeNbr = lastNodeNbr;
	group.rules = sdr_list_create(sdr);
	group.defaultDirective.action = fwd;
	group.defaultDirective.eid = sdr_string_create(sdr, viaEid);
	addr = sdr_malloc(sdr, sizeof(IpnGroup));
	if (addr)
	{
		if (nextGroup)
		{
			sdr_list_insert_before(sdr, nextGroup, addr);
		}
		else
		{
			sdr_list_insert_last(sdr, (_bssConstants())->groups,
					addr);
		}

		sdr_write(sdr, addr, (char *) &group, sizeof(IpnGroup));
	}

	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't add group.", NULL);
		return -1;
	}

	return 1;
}

int	bss_updateGroup(unsigned long firstNodeNbr, unsigned long lastNodeNbr,
		char *viaEid)
{
	Sdr		sdr = getIonsdr();
	Object		elt;
	Object		addr;
	IpnGroup	group;

	CHKERR(firstNodeNbr && lastNodeNbr && viaEid);
	CHKERR(firstNodeNbr <= lastNodeNbr);
	CHKERR(strlen(viaEid) <= MAX_SDRSTRING);
	sdr_begin_xn(sdr);
	elt = locateGroup(firstNodeNbr, lastNodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Unknown group", utoa(firstNodeNbr));
		return 0;
	}

	/*	All parameters validated, okay to update the group.	*/

	addr = (Object) sdr_list_data(sdr, elt);
	sdr_stage(sdr, (char *) &group, addr, sizeof(IpnGroup));
	sdr_free(sdr, group.defaultDirective.eid);
	group.defaultDirective.eid = sdr_string_create(sdr, viaEid);
	sdr_write(sdr, addr, (char *) &group, sizeof(IpnGroup));
	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't update group.", NULL);
		return -1;
	}

	return 1;
}

int	bss_removeGroup(unsigned long firstNodeNbr, unsigned long lastNodeNbr)
{
	Sdr	sdr = getIonsdr();
	Object	elt;
	Object	addr;
		OBJ_POINTER(IpnGroup, group);

	CHKERR(firstNodeNbr && lastNodeNbr);
	CHKERR(firstNodeNbr <= lastNodeNbr);
	sdr_begin_xn(sdr);
	elt = locateGroup(firstNodeNbr, lastNodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Unknown group", utoa(firstNodeNbr));
		return 0;
	}

	addr = (Object) sdr_list_data(sdr, elt);
	GET_OBJ_POINTER(sdr, IpnGroup, group, addr);
	if (sdr_list_length(sdr, group->rules) > 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Can't remove group; still has rules",
				utoa(firstNodeNbr));
		return 0;
	}

	/*	All parameters validated, okay to remove the group.	*/

	sdr_list_delete(sdr, elt, NULL, NULL);
	sdr_free(sdr, group->defaultDirective.eid);
	sdr_free(sdr, addr);
	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't remove group.", NULL);
		return -1;
	}

	return 1;
}

void	bss_findGroupRule(unsigned long firstNodeNbr, unsigned long lastNodeNbr,
		long argServiceNbr, long argNodeNbr, IpnGroup *group,
		Object *ruleAddr, Object *eltp)
{
	Sdr		sdr = getIonsdr();
	unsigned long	srcServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	srcNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	Object		elt;
			OBJ_POINTER(IpnGroup, groupPtr);

	/*	This function finds the BssRule for the specified
	 *	service number and source node number, for the
	 *	specified destination node number, if any.		*/

	CHKVOID(ionLocked());
	CHKVOID(ruleAddr);
	CHKVOID(eltp);
	*eltp = 0;
	if (group == NULL)
	{
		if (firstNodeNbr == 0 || lastNodeNbr < firstNodeNbr)
		{
			return;
		}

		elt = locateGroup(firstNodeNbr, lastNodeNbr, NULL);
		if (elt == 0)
		{
			return;
		}

		GET_OBJ_POINTER(sdr, IpnGroup, groupPtr,
				sdr_list_data(sdr, elt));
		group = groupPtr;
	}

	elt = locateRule(group->rules, srcServiceNbr, srcNodeNbr, NULL);
	if (elt == 0)
	{
		return;
	}

	*ruleAddr = sdr_list_data(sdr, elt);
	*eltp = elt;
}

int	bss_addGroupRule(unsigned long firstNodeNbr, unsigned long lastNodeNbr,
		long argServiceNbr, long argNodeNbr, char *viaEid)
{
	Sdr		sdr = getIonsdr();
	unsigned long	srcServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	srcNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	Object		elt;
			OBJ_POINTER(IpnGroup, group);
	Object		nextRule;
	BssRule		ruleBuf;
	Object		addr;

	CHKERR(firstNodeNbr && lastNodeNbr && srcNodeNbr);
	CHKERR(firstNodeNbr <= lastNodeNbr);
	sdr_begin_xn(sdr);
	elt = locateGroup(firstNodeNbr, lastNodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Group is unknown", utoa(firstNodeNbr));
		return 0;
	}

	GET_OBJ_POINTER(sdr, IpnGroup, group, sdr_list_data(sdr, elt));
	if (locateRule(group->rules, srcServiceNbr, srcNodeNbr, &nextRule) != 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Duplicate rule", utoa(srcNodeNbr));
		return 0;
	}

	/*	All parameters validated, okay to add the rule.		*/

	memset((char *) &ruleBuf, 0, sizeof(BssRule));
	ruleBuf.srcServiceNbr = srcServiceNbr;
	ruleBuf.srcNodeNbr = srcNodeNbr;
	ruleBuf.directive.eid = sdr_string_create(sdr, viaEid);
	addr = sdr_malloc(sdr, sizeof(BssRule));
	if (addr)
	{
		if (nextRule)
		{
			elt = sdr_list_insert_before(sdr, nextRule, addr);
		}
		else
		{
			elt = sdr_list_insert_last(sdr, group->rules, addr);
		}

		sdr_write(sdr, addr, (char *) &ruleBuf, sizeof(BssRule));
	}

	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't add rule.", NULL);
		return -1;
	}

	return 1;
}

int	bss_updateGroupRule(unsigned long firstNodeNbr,
		unsigned long lastNodeNbr, long argServiceNbr, long argNodeNbr,
		char *viaEid)
{
	Sdr		sdr = getIonsdr();
	unsigned long	srcServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	srcNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	Object		elt;
			OBJ_POINTER(IpnGroup, group);
	Object		ruleAddr;
	BssRule		ruleBuf;

	CHKERR(firstNodeNbr && lastNodeNbr && srcNodeNbr);
	CHKERR(firstNodeNbr <= lastNodeNbr);
	sdr_begin_xn(sdr);
	elt = locateGroup(firstNodeNbr, lastNodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Group is unknown", utoa(firstNodeNbr));
		return 0;
	}

	GET_OBJ_POINTER(sdr, IpnGroup, group, sdr_list_data(sdr, elt));
	bss_findGroupRule(firstNodeNbr, lastNodeNbr, srcServiceNbr, srcNodeNbr,
			group, &ruleAddr, &elt);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Unknown rule", utoa(srcNodeNbr));
		return 0;
	}

	/*	All parameters validated, okay to update the rule.	*/

	sdr_stage(sdr, (char *) &ruleBuf, ruleAddr, sizeof(BssRule));
	sdr_free(sdr, ruleBuf.directive.eid);
	ruleBuf.directive.eid = sdr_string_create(sdr, viaEid);
	sdr_write(sdr, ruleAddr, (char *) &ruleBuf, sizeof(BssRule));
	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't update rule.", NULL);
		return -1;
	}

	return 1;
}

int	bss_removeGroupRule(unsigned long firstNodeNbr,
		unsigned long lastNodeNbr, long argServiceNbr, long argNodeNbr)
{
	Sdr		sdr = getIonsdr();
	unsigned long	srcServiceNbr = (argServiceNbr == -1 ? BSS_ALL_OTHERS
				: argServiceNbr);
	unsigned long	srcNodeNbr = (argNodeNbr == -1 ? BSS_ALL_OTHERS
				: argNodeNbr);
	Object		elt;
			OBJ_POINTER(IpnGroup, group);
	Object		ruleAddr;
			OBJ_POINTER(BssRule, rule);

	CHKERR(firstNodeNbr && lastNodeNbr && srcNodeNbr);
	CHKERR(firstNodeNbr <= lastNodeNbr);
	sdr_begin_xn(sdr);
	elt = locateGroup(firstNodeNbr, lastNodeNbr, NULL);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Group is unknown", utoa(firstNodeNbr));
		return 0;
	}

	GET_OBJ_POINTER(sdr, IpnGroup, group, sdr_list_data(sdr, elt));
	bss_findGroupRule(firstNodeNbr, lastNodeNbr, srcServiceNbr, srcNodeNbr,
			group, &ruleAddr, &elt);
	if (elt == 0)
	{
		sdr_exit_xn(sdr);
		writeMemoNote("[?] Unknown rule", utoa(srcNodeNbr));
		return 0;
	}

	/*	All parameters validated, okay to remove the rule.	*/

	GET_OBJ_POINTER(sdr, BssRule, rule, ruleAddr);
	sdr_free(sdr, rule->directive.eid);
	sdr_free(sdr, ruleAddr);
	sdr_list_delete(sdr, elt, NULL, NULL);
	if (sdr_end_xn(sdr) < 0)
	{
		putErrmsg("Can't remove rule.", NULL);
		return -1;
	}

	return 1;
}

int	bss_lookupGroupDirective(unsigned long nodeNbr,
		unsigned long sourceServiceNbr, unsigned long sourceNodeNbr,
		Bundle *bundle, FwdDirective *dirbuf, Lyst loggedStreams)
{
	Sdr		sdr = getIonsdr();
	Object		elt;
	Object		addr;
	IpnGroup	group;

	/*	This function determines the relevant FwdDirective for
	 *	the specified eid, if any.  Wild card match is okay.	*/

	CHKERR(ionLocked());
	CHKERR(nodeNbr && dirbuf);

	/*	Find best matching group.  Groups are sorted by first
	 *	node number within group size, both ascending.  So
	 *	the first group whose range encompasses the node number
	 *	is the best fit (narrowest applicable range), but
	 *	there's no way to terminate the search early.		*/

	for (elt = sdr_list_first(sdr, (_bssConstants())->groups); elt;
			elt = sdr_list_next(sdr, elt))
	{
		addr = sdr_list_data(sdr, elt);
		sdr_read(sdr, (char *) &group, addr, sizeof(IpnGroup));
		if (group.lastNodeNbr < nodeNbr || group.firstNodeNbr > nodeNbr)
		{
			continue;
		}

		break;
	}

	if (elt == 0)
	{
		return 0;		/*	No group found.		*/
	}

	/*	Find best matching rule.				*/

	if (lookupRule(group.rules, sourceServiceNbr, sourceNodeNbr, bundle,
			dirbuf, loggedStreams) == 0)	/*	None.	*/
	{
		memcpy((char *) dirbuf, (char *) &group.defaultDirective,
				sizeof(FwdDirective));
	}

	return 1;
}
