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
 *
 * Note: this file originally auto-generated by mib2c using
 *        : mib2c.scalar.conf,v 1.9 2005/01/07 09:37:18 dts12 Exp $
 *
 * This file implements all scalars defined in the OPENSER-SIP-SERVER-MIB.
 * For a full description of the scalars, please see OPENSER-SIP-SERVER-MIB.
 *
 */
#ifndef OPENSERSIPSERVEROBJECTS_H
#define OPENSERSIPSERVEROBJECTS_H

/* function declarations */
void init_openserSIPServerObjects(void);
Netsnmp_Node_Handler handle_openserSIPProxyStatefulness;
Netsnmp_Node_Handler handle_openserSIPProxyRecordRoute;
Netsnmp_Node_Handler handle_openserSIPProxyAuthMethod;
Netsnmp_Node_Handler handle_openserSIPNumProxyRequireFailures;
Netsnmp_Node_Handler handle_openserSIPRegMaxContactExpiryDuration;
Netsnmp_Node_Handler handle_openserSIPRegMaxUsers;
Netsnmp_Node_Handler handle_openserSIPRegCurrentUsers;
Netsnmp_Node_Handler handle_openserSIPRegDfltRegActiveInterval;
Netsnmp_Node_Handler handle_openserSIPRegUserLookupCounter;
Netsnmp_Node_Handler handle_openserSIPRegAcceptedRegistrations;
Netsnmp_Node_Handler handle_openserSIPRegRejectedRegistrations;

#define PROXY_STATEFULNESS_STATELESS            1
#define PROXY_STATEFULNESS_TRANSACTION_STATEFUL 2
#define PROXY_STATEFULNESS_CALL_STATEFUL        3

#define SIP_AUTH_METHOD_NONE   (128 >> 0)
#define SIP_AUTH_METHOD_TLS    (128 >> 1)
#define SIP_AUTH_METHOD_DIGEST (128 >> 2)

#endif /* OPENSERSIPSERVEROBJECTS_H */
