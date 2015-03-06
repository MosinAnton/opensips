/*
 * Copyright (C) 2007 SOMA Networks, Inc.
 * Written By Ovidiu Sas (osas)
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
 * 2007-07-16 initial version (osas)
 */

#ifndef _QOS_HANDLERS_H_
#define _QOS_HANDLERS_H_

#include "../../sr_module.h" /* Needed for find_export() */
#include "../../parser/msg_parser.h"
#include "../dialog/dlg_load.h"

/**
 * The static (opening) callback function for all dialog creations
 */
void qos_dialog_created_CB(struct dlg_cell *did, int type, struct dlg_cb_params * params);

#endif /* _QOS_HANDLERS_H_ */
