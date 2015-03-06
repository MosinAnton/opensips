/*
 * SNMPStats Module
 * Copyright (C) 2006 SOMA Networks, INC.
 * Written by: Jeffrey Magder (jmagder@somanetworks.com)
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * History:
 * --------
 * 2006-11-23 initial version (jmagder)
 * 2007-02-16 Moved all OID registrations from the experimental branch to
 *            OpenSER's IANA assigned enterprise branch. (jmagder)
 *
 * Note: this file originally auto-generated by mib2c using
 *        : mib2c.notify.conf,v 5.3 2004/04/15 12:29:19 dts12 Exp $
 *
 * This file contains functions for sending all traps supported by the SNMPStats
 * module.
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "openserMIBNotifications.h"
#include "../../dprint.h"
#include "../../config.h"

static oid snmptrap_oid[] = {1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0};

/*
 * Sends off an openserMsgQueueDepthMinorEvent trap to the master agent,
 * assigning the following variable bindings:
 *
 *  - openserMsgQueueDepth          = msgQueueDepth
 *  - openserMsgQueueMinorThreshold = minorThreshold
 *
 */
int send_openserMsgQueueDepthMinorEvent_trap(int msgQueueDepth,
		int minorThreshold)
{
	netsnmp_variable_list  *var_list = NULL;

	oid openserMsgQueueDepthMinorEvent_oid[] = {
		OPENSER_OID,3,1,3,2,0,1 };

	oid openserMsgQueueDepth_oid[]		 = {
		OPENSER_OID,3,1,3,1,2,3,1,0 };

	oid openserMsgQueueMinorThreshold_oid[]  = {
		OPENSER_OID,3,1,3,1,2,3,2,0 };

	snmp_varlist_add_variable(&var_list,
			snmptrap_oid,
			OID_LENGTH(snmptrap_oid),
			ASN_OBJECT_ID,
			(u_char *)openserMsgQueueDepthMinorEvent_oid,
			(int)sizeof(openserMsgQueueDepthMinorEvent_oid));

	snmp_varlist_add_variable(&var_list,
			openserMsgQueueDepth_oid,
			OID_LENGTH(openserMsgQueueDepth_oid),
			ASN_GAUGE,
			(u_char *)&msgQueueDepth,
			sizeof(msgQueueDepth));

	snmp_varlist_add_variable(&var_list,
			openserMsgQueueMinorThreshold_oid,
			OID_LENGTH(openserMsgQueueMinorThreshold_oid),
			ASN_INTEGER,
			(u_char *)&minorThreshold,
			sizeof(minorThreshold));

	send_v2trap( var_list );
	snmp_free_varbind( var_list );

	return SNMP_ERR_NOERROR;
}

/*
 * Sends off an openserMsgQueueDepthMajorEvent trap to the master agent,
 * assigning the following variable bindings:
 *
 *  - openserMsgQueueDepth          = msgQueueDepth
 *  - openserMsgQueueMajorThreshold = majorThreshold
 *
 */
int send_openserMsgQueueDepthMajorEvent_trap(int msgQueueDepth,
		int majorThreshold)
{
	netsnmp_variable_list  *var_list = NULL;

	oid openserMsgQueueDepthMajorEvent_oid[] = {
		OPENSER_OID,3,1,3,2,0,2 };

	oid openserMsgQueueDepth_oid[]		 = {
		OPENSER_OID,3,1,3,1,2,3,1,0 };

	oid openserMsgQueueMajorThreshold_oid[]  = {
		OPENSER_OID,3,1,3,1,2,3,3,0 };

	snmp_varlist_add_variable(&var_list,
		snmptrap_oid,
		OID_LENGTH(snmptrap_oid),
		ASN_OBJECT_ID,
		(u_char *)openserMsgQueueDepthMajorEvent_oid,
		(int)sizeof(openserMsgQueueDepthMajorEvent_oid));

	snmp_varlist_add_variable(&var_list,
			openserMsgQueueDepth_oid,
			OID_LENGTH(openserMsgQueueDepth_oid),
			ASN_GAUGE,
			(u_char *)&msgQueueDepth,
			sizeof(msgQueueDepth));

	snmp_varlist_add_variable(&var_list,
			openserMsgQueueMajorThreshold_oid,
			OID_LENGTH(openserMsgQueueMajorThreshold_oid),
			ASN_INTEGER,
			(u_char *)&majorThreshold,
			sizeof(majorThreshold));

	send_v2trap( var_list );
	snmp_free_varbind( var_list );

	return SNMP_ERR_NOERROR;
}


/*
 * Sends off an openserDialogLimitMinorEvent trap to the master agent,
 * assigning the following variable bindings:
 *
 *  - openserCurNumDialogs             = numDialogs
 *  - openserDialogLimitMinorThreshold = threshold
 *
 */
int send_openserDialogLimitMinorEvent_trap(int numDialogs, int threshold)
{
	netsnmp_variable_list  *var_list = NULL;

	oid openserDialogLimitMinorEvent_oid[]    = {
		OPENSER_OID,3,1,3,2,0,3 };

	oid openserCurNumDialogs_oid[]            = {
		OPENSER_OID,3,1,3,1,3,2,1, 0 };

	oid openserDialogLimitMinorThreshold_oid[] = {
		OPENSER_OID,3,1,3,1,3,2,5, 0 };

	snmp_varlist_add_variable(&var_list,
		snmptrap_oid,
		OID_LENGTH(snmptrap_oid),
		ASN_OBJECT_ID,
		(u_char *)openserDialogLimitMinorEvent_oid,
		(int)sizeof(openserDialogLimitMinorEvent_oid));

	snmp_varlist_add_variable(&var_list,
		openserCurNumDialogs_oid,
		OID_LENGTH(openserCurNumDialogs_oid),
		ASN_GAUGE,
		(u_char *)&numDialogs,
		sizeof(numDialogs));

	snmp_varlist_add_variable(&var_list,
		openserDialogLimitMinorThreshold_oid,
		OID_LENGTH(openserDialogLimitMinorThreshold_oid),
		ASN_INTEGER,
		(u_char *)&threshold,
		sizeof(threshold));

	send_v2trap( var_list );
	snmp_free_varbind( var_list );

	return SNMP_ERR_NOERROR;
}


/*
 * Sends off an openserDialogLimitMinorEvent trap to the master agent,
 * assigning the following variable bindings:
 *
 *  - openserCurNumDialogs             = numDialogs
 *  - openserDialogLimitMinorThreshold = threshold
 *
 */
int send_openserDialogLimitMajorEvent_trap(int numDialogs, int threshold)
{
	netsnmp_variable_list  *var_list = NULL;

	oid openserDialogLimitMajorEvent_oid[]     = {
		OPENSER_OID,3,1,3,2,0,4 };

	oid openserCurNumDialogs_oid[]             = {
		OPENSER_OID,3,1,3,1,3,2,1, 0 };

	oid openserDialogLimitMajorThreshold_oid[] = {
		OPENSER_OID,3,1,3,1,3,2,6, 0 };

	snmp_varlist_add_variable(&var_list,
		snmptrap_oid,
		OID_LENGTH(snmptrap_oid),
		ASN_OBJECT_ID,
		(u_char *)openserDialogLimitMajorEvent_oid,
		(int)sizeof(openserDialogLimitMajorEvent_oid));

	snmp_varlist_add_variable(&var_list,
		openserCurNumDialogs_oid,
		OID_LENGTH(openserCurNumDialogs_oid),
		ASN_GAUGE,
		(u_char *)&numDialogs,
		sizeof(numDialogs));

	snmp_varlist_add_variable(&var_list,
		openserDialogLimitMajorThreshold_oid,
		OID_LENGTH(openserDialogLimitMajorThreshold_oid),
		ASN_INTEGER,
		(u_char *)&threshold,
		sizeof(threshold));

	send_v2trap( var_list );
	snmp_free_varbind( var_list );

	return SNMP_ERR_NOERROR;
}
