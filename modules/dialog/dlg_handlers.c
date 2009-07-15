/*
 * $Id$
 *
 * Copyright (C) 2006 Voice System SRL
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 * 2006-11-28  Added support for tracking the number of early dialogs, and the
 *             number of failed dialogs. This involved updates to dlg_onreply()
 *             (Jeffrey Magder - SOMA Networks)
 * 2007-03-06  syncronized state machine added for dialog state. New tranzition
 *             design based on events; removed num_1xx and num_2xx (bogdan)
 * 2007-04-30  added dialog matching without DID (dialog ID), but based only
 *             on RFC3261 elements - based on an original patch submitted 
 *             by Michel Bensoussan <michel@extricom.com> (bogdan)
 * 2007-05-17  new feature: saving dialog info into a database if 
 *             realtime update is set(ancuta)
 * 2007-07-06  support for saving additional dialog info : cseq, contact, 
 *             route_set and socket_info for both caller and callee (ancuta)
 * 2007-07-10  Optimized dlg_match_mode 2 (DID_NONE), it now employs a proper
 *             hash table lookup and isn't dependant on the is_direction 
 *             function (which requires an RR param like dlg_match_mode 0 
 *             anyways.. ;) ; based on a patch from 
 *             Tavis Paquette <tavis@galaxytelecom.net> 
 *             and Peter Baer <pbaer@galaxytelecom.net>  (bogdan)
 * 2008-04-04  added direction reporting in dlg callbacks (bogdan)
 */


#include <string.h>
#include <time.h>

#include "../../trim.h"
#include "../../pvar.h"
#include "../../timer.h"
#include "../../statistics.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_cseq.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../tm/tm_load.h"
#include "../rr/api.h"
#include "dlg_hash.h"
#include "dlg_timer.h"
#include "dlg_cb.h"
#include "dlg_handlers.h"
#include "dlg_db_handler.h"
#include "dlg_profile.h"
#include "dlg_req_within.h"

static str       rr_param;
static int       dlg_flag;
static pv_spec_t *timeout_avp;
static int       default_timeout;
static int       seq_match_mode;
static int       shutdown_done = 0;

extern struct rr_binds d_rrb;
extern int bye_on_timeout_flag;

/* statistic variables */
extern stat_var *early_dlgs;
extern stat_var *processed_dlgs;
extern stat_var *expired_dlgs;
extern stat_var *failed_dlgs;


#define RR_DLG_PARAM_SIZE  (2*2*sizeof(int)+3+MAX_DLG_RR_PARAM_NAME)
#define DLG_SEPARATOR      '.'



void init_dlg_handlers(char *rr_param_p, int dlg_flag_p,
		pv_spec_t *timeout_avp_p ,int default_timeout_p, 
		int seq_match_mode_p)
{
	rr_param.s = rr_param_p;
	rr_param.len = strlen(rr_param.s);

	dlg_flag = 1<<dlg_flag_p;

	timeout_avp = timeout_avp_p;
	default_timeout = default_timeout_p;
	seq_match_mode = seq_match_mode_p;
}


void destroy_dlg_handlers(void)
{
	shutdown_done = 1;
}


static inline int add_dlg_rr_param(struct sip_msg *req, unsigned int entry,
													unsigned int id)
{
	static char buf[RR_DLG_PARAM_SIZE];
	str s;
	int n;
	char *p;

	s.s = p = buf;

	*(p++) = ';';
	memcpy(p, rr_param.s, rr_param.len);
	p += rr_param.len;
	*(p++) = '=';

	n = RR_DLG_PARAM_SIZE - (p-buf);
	if (int2reverse_hex( &p, &n, entry)==-1)
		return -1;

	*(p++) = DLG_SEPARATOR;

	n = RR_DLG_PARAM_SIZE - (p-buf);
	if (int2reverse_hex( &p, &n, id)==-1)
		return -1;

	s.len = p-buf;

	if (d_rrb.add_rr_param( req, &s)<0) {
		LM_ERR("failed to add rr param\n");
		return -1;
	}

	return 0;
}


/*usage: dlg: the dialog to add cseq, contact & record_route
 * 		 msg: sip message
 * 		 flag: 0-for a request(INVITE), 
 * 		 		1- for a reply(200 ok)
 *
 *	for a request: get record route in normal order
 *	for a reply  : get in reverse order, skipping the ones from the request and
 *				   the proxies' own 
 */
static int populate_leg_info( struct dlg_cell *dlg, struct sip_msg *msg,
	struct cell* t, unsigned int leg, str *tag)
{
	unsigned int skip_recs;
	str cseq;
	str contact;
	str rr_set;

	/* extract the cseq number as string */
	if (leg==DLG_CALLER_LEG) {
		if((!msg->cseq && (parse_headers(msg,HDR_CSEQ_F,0)<0 || !msg->cseq)) ||
		!msg->cseq->parsed){
			LM_ERR("bad sip message or missing CSeq hdr :-/\n");
			goto error0;
		}
		cseq = (get_cseq(msg))->number;
	} else {
		/* use the same as in request */
		cseq = dlg->cseq[DLG_CALLER_LEG];
	}

	/* extract the contact address */
	if (!msg->contact&&(parse_headers(msg,HDR_CONTACT_F,0)<0||!msg->contact)){
		LM_ERR("bad sip message or missing Contact hdr\n");
		contact.s = NULL;
		contact.len = 0;
	} else {
		if ( parse_contact(msg->contact)<0 ||
		((contact_body_t *)msg->contact->parsed)->contacts==NULL ||
		((contact_body_t *)msg->contact->parsed)->contacts->next!=NULL ) {
			LM_ERR("bad Contact HDR\n");
			contact.s = NULL;
			contact.len = 0;
		} else {
			contact = ((contact_body_t *)msg->contact->parsed)->contacts->uri;
		}
	}

	/* extract the RR parts */
	skip_recs = 0;
	if(!msg->record_route && (parse_headers(msg,HDR_RECORDROUTE_F,0)<0)  ){
		LM_ERR("failed to parse record route header\n");
		rr_set.s = 0;
		rr_set.len = 0;
	} else {
		if (leg==DLG_CALLEE_LEG) {
			/* was the 200 OK received or local generated */
			skip_recs = dlg->from_rr_nb +
				((t->relaied_reply_branch>=0)?
					(t->uac[t->relaied_reply_branch].added_rr):0);
		}

		if(msg->record_route){
			if( print_rr_body(msg->record_route, &rr_set, leg, 
								&skip_recs) != 0 ){
				LM_ERR("failed to print route records \n");
				rr_set.s = 0;
				rr_set.len = 0;
			}
		} else {
			rr_set.s = 0;
			rr_set.len = 0;
		}
	}

	if(leg==DLG_CALLER_LEG)
		dlg->from_rr_nb = skip_recs;

	LM_DBG("route_set %.*s, contact %.*s, cseq %.*s and bind_addr %.*s\n",
		rr_set.len, ZSW(rr_set.s), contact.len, ZSW(contact.s),
		cseq.len, cseq.s, 
		msg->rcv.bind_address->sock_str.len,
		msg->rcv.bind_address->sock_str.s);

	if (dlg_set_leg_info( dlg, tag, &rr_set, &contact, &cseq, leg)!=0) {
		LM_ERR("dlg_set_leg_info failed\n");
		if (rr_set.s) pkg_free(rr_set.s);
		goto error0;
	}

	dlg->bind_addr[leg] = msg->rcv.bind_address;
	if (rr_set.s) pkg_free(rr_set.s);

	return 0;
error0:
	return -1;
}


static void dlg_onreply(struct cell* t, int type, struct tmcb_params *param)
{
	struct sip_msg *rpl;
	struct dlg_cell *dlg;
	int new_state;
	int old_state;
	int unref;
	int event;
	str tag;

	dlg = (struct dlg_cell *)(*param->param);
	if (shutdown_done || dlg==0)
		return;

	rpl = param->rpl;

	if (type==TMCB_RESPONSE_FWDED) {
		/* The state does not change, but the msg is mutable in this callback*/
		run_dlg_callbacks(DLGCB_RESPONSE_FWDED, dlg, rpl, DLG_DIR_UPSTREAM, 0);
		return;
	}

	if (type==TMCB_TRANS_DELETED)
		event = DLG_EVENT_TDEL;
	else if (param->code<200)
		event = DLG_EVENT_RPL1xx;
	else if (param->code<300)
		event = DLG_EVENT_RPL2xx;
	else
		event = DLG_EVENT_RPL3xx;

	next_state_dlg( dlg, event, &old_state, &new_state, &unref);

	if (new_state==DLG_STATE_EARLY) {
		run_dlg_callbacks(DLGCB_EARLY, dlg, rpl, DLG_DIR_UPSTREAM, 0);
		if (old_state!=DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, 1);
		return;
	}

	if (new_state==DLG_STATE_CONFIRMED_NA &&
	old_state!=DLG_STATE_CONFIRMED_NA && old_state!=DLG_STATE_CONFIRMED ) {
		LM_DBG("dialog %p confirmed\n",dlg);

		if (rpl!=FAKED_REPLY) {
			/* get to tag*/
			if ( !rpl->to && ((parse_headers(rpl,HDR_TO_F,0)<0) || !rpl->to) ){
				LM_ERR("bad reply or missing TO hdr :-/\n");
				tag.s = 0;
				tag.len = 0;
			} else {
				tag = get_to(rpl)->tag_value;
				if (tag.s==0 || tag.len==0) {
					LM_ERR("missing TAG param in TO hdr :-/\n");
					tag.s = 0;
					tag.len = 0;
				}
			}
			LM_DBG("%p totag in rpl is <%.*s> (%d)\n",
				dlg, tag.len,tag.s,tag.len);

			/* save callee's tag, cseq, contact and record route*/
			if (populate_leg_info( dlg, rpl, t, DLG_CALLEE_LEG, &tag) !=0) {
				LM_ERR("could not add further info to the dialog\n");
			}

		} else {
			LM_DBG("dialog replied from script - cannot get callee info\n");
		}


		/* set start time */
		dlg->start_ts = (unsigned int)(time(0));

		/* save the settings to the database, 
		 * if realtime saving mode configured- save dialog now
		 * else: the next time the timer will fire the update*/
		dlg->flags |= DLG_FLAG_NEW;
		if ( dlg_db_mode==DB_MODE_REALTIME )
			update_dialog_dbinfo(dlg);

		if (0 != insert_dlg_timer( &dlg->tl, dlg->lifetime )) {
			LM_CRIT("Unable to insert dlg %p [%u:%u] on event %d [%d->%d] "
				"with clid '%.*s' and tags '%.*s' '%.*s'\n",
				dlg, dlg->h_entry, dlg->h_id, event, old_state, new_state,
				dlg->callid.len, dlg->callid.s,
				dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
				dlg->tag[DLG_CALLEE_LEG].len, ZSW(dlg->tag[DLG_CALLEE_LEG].s));
		} else {
			/* reference dialog as kept in timer list */
			ref_dlg(dlg,1);
		}

		/* dialog confirmed */
		run_dlg_callbacks( DLGCB_CONFIRMED, dlg, rpl, DLG_DIR_UPSTREAM, 0);

		if (old_state==DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, -1);

		if_update_stat(dlg_enable_stats, active_dlgs, 1);
		return;
	}

	if ( old_state!=DLG_STATE_DELETED && new_state==DLG_STATE_DELETED ) {
		LM_DBG("dialog %p failed (negative reply)\n", dlg);
		/* dialog setup not completed (3456XX) */
		run_dlg_callbacks( DLGCB_FAILED, dlg, rpl, DLG_DIR_UPSTREAM, 0);
		/* do unref */
		if (unref)
			unref_dlg(dlg,unref);
		if (old_state==DLG_STATE_EARLY)
			if_update_stat(dlg_enable_stats, early_dlgs, -1);
		return;
	}

	/* in any other case, check if the dialog state machine
	   requests to unref the dialog */
	if (unref)
		unref_dlg(dlg,unref);

	return;
}


static void dlg_seq_up_onreply(struct cell* t, int type,
													struct tmcb_params *param)
{
	struct dlg_cell *dlg;

	dlg = (struct dlg_cell *)(*param->param);
	if (shutdown_done || dlg==0)
		return;

	if (type==TMCB_RESPONSE_FWDED) {
		run_dlg_callbacks(DLGCB_RESPONSE_WITHIN, dlg, param->rpl,
			DLG_DIR_UPSTREAM, 0);
		return;
	}

	return;
}



static void dlg_seq_down_onreply(struct cell* t, int type,
													struct tmcb_params *param)
{
	struct dlg_cell *dlg;

	dlg = (struct dlg_cell *)(*param->param);
	if (shutdown_done || dlg==0)
		return;

	if (type==TMCB_RESPONSE_FWDED) {
		run_dlg_callbacks(DLGCB_RESPONSE_WITHIN, dlg, param->rpl,
			DLG_DIR_DOWNSTREAM, 0);
		return;
	}

	return;
}


inline static int get_dlg_timeout(struct sip_msg *req)
{
	pv_value_t pv_val;

	if( timeout_avp && pv_get_spec_value( req, timeout_avp, &pv_val)==0
	&& pv_val.flags&PV_VAL_INT && pv_val.ri>0 ) {
		return pv_val.ri;
	}
	LM_DBG("invalid AVP value, use default timeout\n");
	return default_timeout;
}


static void unreference_dialog(void *dialog)
{
	/* if the dialog table is gone, it means the system is shutting down.*/
	if (!d_table)
		return;
	unref_dlg((struct dlg_cell*)dialog, 1);
}


static void unreference_dialog_create(void *dialog)
{
	struct tmcb_params params;

	memset(&params, 0, sizeof(struct tmcb_params));
	params.param = (void*)&dialog;
	/* just a wapper */
	dlg_onreply( 0, TMCB_TRANS_DELETED, &params);
}


void dlg_onreq(struct cell* t, int type, struct tmcb_params *param)
{
	/* is the dialog already created? */
	if (current_dlg_pointer!=NULL) {
		/* new, un-initialized dialog ? */
		if ( current_dlg_pointer->flags & DLG_FLAG_ISINIT )
			return;

		/* dialog was previously created by create_dialog() 
		   -> just do the last settings */
		run_create_callbacks( current_dlg_pointer, param->req);

		current_dlg_pointer->lifetime = get_dlg_timeout(param->req);

		if (param->req->flags&bye_on_timeout_flag)
			current_dlg_pointer->flags |= DLG_FLAG_BYEONTIMEOUT;

		t->dialog_ctx = (void*)current_dlg_pointer;

		/* dialog is fully initialized */
		current_dlg_pointer->flags |= DLG_FLAG_ISINIT;
	} else {
		/* should we create dialog? */
		if ( (param->req->flags & dlg_flag) != dlg_flag)
			return;

		/* create the dialog */
		dlg_create_dialog(t, param->req);
	}
}


int dlg_create_dialog(struct cell* t, struct sip_msg *req)
{
	struct dlg_cell *dlg;
	str s;

	if ( (!req->to && parse_headers(req, HDR_TO_F,0)<0) || !req->to ) {
		LM_ERR("bad request or missing TO hdr :-/\n");
		return -1;
	}
	s = get_to(req)->tag_value;
	if (s.s!=0 && s.len!=0)
		return -1;

	if (req->first_line.u.request.method_value==METHOD_CANCEL)
		return -1;

	if ( parse_from_header(req)) {
		LM_ERR("bad request or missing FROM hdr :-/\n");
		return -1;
	}
	if ((!req->callid && parse_headers(req,HDR_CALLID_F,0)<0) || !req->callid){
		LM_ERR("bad request or missing CALLID hdr :-/\n");
		return -1;
	}
	s = req->callid->body;
	trim(&s);

	/* some sanity checks */
	if (s.len==0 || get_from(req)->tag_value.len==0) {
		LM_ERR("invalid request -> callid (%d) or from TAG (%d) empty\n",
			s.len, get_from(req)->tag_value.len);
		return -1;
	}

	dlg = build_new_dlg( &s /*callid*/, &(get_from(req)->uri) /*from uri*/,
		&(get_to(req)->uri) /*to uri*/,
		&(get_from(req)->tag_value)/*from_tag*/ );
	if (dlg==0) {
		LM_ERR("failed to create new dialog\n");
		return -1;
	}

	/* save caller's tag, cseq, contact and record route*/
	if (populate_leg_info(dlg, req, t, DLG_CALLER_LEG,
	&(get_from(req)->tag_value)) !=0) {
		LM_ERR("could not add further info to the dialog\n");
		shm_free(dlg);
		return -1;
	}

	/* set current dialog */
	set_current_dialog(dlg);

	link_dlg( dlg , 2/* extra ref for the callback and current dlg hook */);

	if ( seq_match_mode!=SEQ_MATCH_NO_ID &&
	add_dlg_rr_param( req, dlg->h_entry, dlg->h_id)<0 ) {
		LM_ERR("failed to add RR param\n");
		goto error;
	}

	if ( d_tmb.register_tmcb( req, t,
				TMCB_RESPONSE_PRE_OUT|TMCB_RESPONSE_FWDED,
				dlg_onreply, (void*)dlg, unreference_dialog_create)<0 ) {
		LM_ERR("failed to register TMCB\n");
		goto error;
	}

	/* complete the dialog setup only if transaction aleady exists;
	   if not, wait for the TMCB_REQUEST_IN callback to do this job */
	if (t) {
		/* first INVITE seen (dialog created, unconfirmed) */
		run_create_callbacks( dlg, req);

		dlg->lifetime = get_dlg_timeout(req);

		if (req->flags&bye_on_timeout_flag)
			dlg->flags |= DLG_FLAG_BYEONTIMEOUT;

		t->dialog_ctx = (void*) dlg;

		dlg->flags |= DLG_FLAG_ISINIT;
	}

	if_update_stat( dlg_enable_stats, processed_dlgs, 1);

	return 0;
error:
	unref_dlg(dlg,1);
	dialog_cleanup( req, NULL);
	update_stat(failed_dlgs, 1);
	return -1;
}



static inline int parse_dlg_rr_param(char *p, char *end,
													int *h_entry, int *h_id)
{
	char *s;

	for ( s=p ; p<end && *p!=DLG_SEPARATOR ; p++ );
	if (*p!=DLG_SEPARATOR) {
		LM_ERR("malformed rr param '%.*s'\n", (int)(long)(end-s), s);
		return -1;
	}

	if ( (*h_entry=reverse_hex2int( s, p-s))<0 ) {
		LM_ERR("invalid hash entry '%.*s'\n", (int)(long)(p-s), s);
		return -1;
	}

	if ( (*h_id=reverse_hex2int( p+1, end-(p+1)))<0 ) {
		LM_ERR("invalid hash id '%.*s'\n", (int)(long)(end-(p+1)), p+1 );
		return -1;
	}

	return 0;
}


static inline int pre_match_parse( struct sip_msg *req, str *callid,
														str *ftag, str *ttag)
{
	if (parse_headers(req,HDR_CALLID_F|HDR_TO_F,0)<0 || !req->callid ||
	!req->to ) {
		LM_ERR("bad request or missing CALLID/TO hdr :-/\n");
		return -1;
	}

	if (get_to(req)->tag_value.len==0) {
		/* out of dialog request with preloaded Route headers; ignore. */
		return -1;
	}

	if (parse_from_header(req)<0 || get_from(req)->tag_value.len==0) {
		LM_ERR("failed to get From header\n");
		return -1;
	}

	/* callid */
	*callid = req->callid->body;
	trim(callid);
	/* to tag */
	*ttag = get_to(req)->tag_value;
	/* from tag */
	*ftag = get_from(req)->tag_value;
	return 0;
}


static inline int update_cseqs(struct dlg_cell *dlg, struct sip_msg *req,
															unsigned int dir)
{
	if ( (!req->cseq && parse_headers(req,HDR_CSEQ_F,0)<0) || !req->cseq ||
	!req->cseq->parsed) {
		LM_ERR("bad sip message or missing CSeq hdr :-/\n");
		return -1;
	}

	if ( dir==DLG_DIR_UPSTREAM) {
		return dlg_update_cseq(dlg, DLG_CALLEE_LEG,&((get_cseq(req))->number));
	} else if ( dir==DLG_DIR_DOWNSTREAM) {
		return dlg_update_cseq(dlg, DLG_CALLER_LEG,&((get_cseq(req))->number));
	} else {
		LM_CRIT("dir is not set!\n");
		return -1;
	}
}


void dlg_onroute(struct sip_msg* req, str *route_params, void *param)
{
	struct dlg_cell *dlg;
	str val;
	str callid;
	str ftag;
	str ttag;
	int h_entry;
	int h_id;
	int new_state;
	int old_state;
	int unref;
	int event;
	int timeout;
	unsigned int dir;
	int ret = 0;

	/* as this callback is triggered from loose_route, which can be 
	   accidentaly called more than once from script, we need to be sure 
	   we do this only once !*/
	if (current_dlg_pointer)
		return;

	/* skip initial requests - they may end up here because of the
	 * preloaded route */
	if ( (!req->to && parse_headers(req, HDR_TO_F,0)<0) || !req->to ) {
		LM_ERR("bad request or missing TO hdr :-/\n");
		return;
	}
	if ( get_to(req)->tag_value.len==0 )
		return;

	dlg = 0;
	dir = DLG_DIR_NONE;

	if ( seq_match_mode!=SEQ_MATCH_NO_ID ) {
		if( d_rrb.get_route_param( req, &rr_param, &val)!=0) {
			LM_DBG("Route param '%.*s' not found\n", rr_param.len,rr_param.s);
			if (seq_match_mode==SEQ_MATCH_STRICT_ID )
				return;
		} else {
			LM_DBG("route param is '%.*s' (len=%d)\n",val.len,val.s,val.len);

			if ( parse_dlg_rr_param( val.s, val.s+val.len, &h_entry, &h_id)<0 )
				return;

			dlg = lookup_dlg( h_entry, h_id);
			if (dlg==0) {
				LM_WARN("unable to find dialog for %.*s "
					"with route param '%.*s'\n",
					req->first_line.u.request.method.len,
					req->first_line.u.request.method.s,
					val.len,val.s);
				return;
			}

			// lookup_dlg has incremented the ref count by 1

			if (pre_match_parse( req, &callid, &ftag, &ttag)<0) {
				unref_dlg(dlg, 1);
				return;
			}
			if (match_dialog( dlg, &callid, &ftag, &ttag, &dir )==0) {
				LM_WARN("tight matching failed for %.*s with callid='%.*s'/%d, "
						"ftag='%.*s'/%d, ttag='%.*s'/%d and direction=%d\n",
						req->first_line.u.request.method.len,
						req->first_line.u.request.method.s,
						callid.len, callid.s, callid.len,
						ftag.len, ftag.s, ftag.len,
						ttag.len, ttag.s, ttag.len, dir);
				LM_WARN("dialog identification elements are callid='%.*s'/%d, "
						"caller tag='%.*s'/%d, callee tag='%.*s'/%d\n",
						dlg->callid.len, dlg->callid.s, dlg->callid.len,
						dlg->tag[DLG_CALLER_LEG].len,
						dlg->tag[DLG_CALLER_LEG].s,
						dlg->tag[DLG_CALLER_LEG].len,
						dlg->tag[DLG_CALLEE_LEG].len,
						ZSW(dlg->tag[DLG_CALLEE_LEG].s),
						dlg->tag[DLG_CALLEE_LEG].len);
				unref_dlg(dlg, 1);
				return;
			}
		}
	}

	if (dlg==0) {
		if (pre_match_parse( req, &callid, &ftag, &ttag)<0)
			return;
		/* TODO - try to use the RR dir detection to speed up here the
		 * search -bogdan */
		dlg = get_dlg(&callid, &ftag, &ttag, &dir);
		if (!dlg){
			LM_DBG("Callid '%.*s' not found\n",
				req->callid->body.len, req->callid->body.s);
			return;
		}
	}

	/* run state machine */
	switch ( req->first_line.u.request.method_value ) {
		case METHOD_PRACK:
			event = DLG_EVENT_REQPRACK; break;
		case METHOD_ACK:
			event = DLG_EVENT_REQACK; break;
		case METHOD_BYE:
			event = DLG_EVENT_REQBYE; break;
		default:
			event = DLG_EVENT_REQ;
	}

	next_state_dlg( dlg, event, &old_state, &new_state, &unref);

	/* set current dialog - it will keep a ref! */
	set_current_dialog(dlg);

	/* run actions for the transition */
	if (event==DLG_EVENT_REQBYE && new_state==DLG_STATE_DELETED &&
	old_state!=DLG_STATE_DELETED) {
		LM_DBG("BYE successfully processed\n");
		/* remove from timer */
		ret = remove_dlg_timer(&dlg->tl);
		if (ret < 0) {
			LM_CRIT("unable to unlink the timer on dlg %p [%u:%u] "
				"with clid '%.*s' and tags '%.*s' '%.*s'\n",
				dlg, dlg->h_entry, dlg->h_id,
				dlg->callid.len, dlg->callid.s,
				dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
				dlg->tag[DLG_CALLEE_LEG].len, ZSW(dlg->tag[DLG_CALLEE_LEG].s));
		} else if (ret > 0) {
			LM_DBG("dlg expired (not in timer list) on dlg %p [%u:%u] "
				"with clid '%.*s' and tags '%.*s' '%.*s'\n",
				dlg, dlg->h_entry, dlg->h_id,
				dlg->callid.len, dlg->callid.s,
				dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
				dlg->tag[DLG_CALLEE_LEG].len, ZSW(dlg->tag[DLG_CALLEE_LEG].s));
		} else {
			/* dialog sucessfully removed from timer -> unref */
			unref++;
		}
		/* dialog terminated (BYE) */
		run_dlg_callbacks( DLGCB_TERMINATED, dlg, req, dir, 0);

		/* delete the dialog from DB */
		if (should_remove_dlg_db())
			remove_dialog_from_db(dlg);

		/* destroy dialog */
		unref_dlg(dlg, unref);

		if_update_stat( dlg_enable_stats, active_dlgs, -1);
		return;
	}

	if ( (event==DLG_EVENT_REQ || event==DLG_EVENT_REQACK)
	&& new_state==DLG_STATE_CONFIRMED) {
		LM_DBG("sequential request successfully processed\n");
		timeout = get_dlg_timeout(req);
		/* update timer during sequential request? */
		if (timeout!=default_timeout) {
			dlg->lifetime = timeout;
			if (update_dlg_timer( &dlg->tl, dlg->lifetime )==-1)
				LM_ERR("failed to update dialog lifetime\n");
		}
		if (update_cseqs(dlg, req, dir)!=0) {
			LM_ERR("cseqs update failed\n");
		} else {
			dlg->flags |= DLG_FLAG_CHANGED;
			if ( dlg_db_mode==DB_MODE_REALTIME )
				update_dialog_dbinfo(dlg);
		}

		/* within dialog request */
		run_dlg_callbacks( DLGCB_REQ_WITHIN, dlg, req, dir, 0);

		if ( (event!=DLG_EVENT_REQACK) &&
		(dlg->cbs.types)&DLGCB_RESPONSE_WITHIN ) {
			/* ref the dialog as registered into the transaction callback.
			 * unref will be done when the callback will be destroyed */
			ref_dlg( dlg , 1);
			/* register callback for the replies of this request */
			if ( d_tmb.register_tmcb( req, 0, TMCB_RESPONSE_FWDED,
			(dir==DLG_DIR_UPSTREAM)?dlg_seq_down_onreply:dlg_seq_up_onreply,
			(void*)dlg, unreference_dialog)<0 ) {
				LM_ERR("failed to register TMCB (2)\n");
					unref_dlg( dlg , 1);
			}
		}
	}

	if(new_state==DLG_STATE_CONFIRMED && old_state==DLG_STATE_CONFIRMED_NA){
		dlg->flags |= DLG_FLAG_CHANGED;
		if(dlg_db_mode == DB_MODE_REALTIME)
			update_dialog_dbinfo(dlg);
	}

	return;
}



#define get_dlg_tl_payload(_tl_)  ((struct dlg_cell*)((char *)(_tl_)- \
		(unsigned long)(&((struct dlg_cell*)0)->tl)))

/* When done, this function also has the job to unref the dialog as removed
 * from timer list. This must be done in all cases!!
 */
void dlg_ontimeout( struct dlg_tl *tl)
{
	struct dlg_cell *dlg;
	int new_state;
	int old_state;
	int unref;

	dlg = get_dlg_tl_payload(tl);

	if ( (dlg->flags&DLG_FLAG_BYEONTIMEOUT) &&
	(dlg->state==DLG_STATE_CONFIRMED_NA || dlg->state==DLG_STATE_CONFIRMED)) {
		/* we just send the BYEs in both directions */
		dlg_end_dlg( dlg, NULL);
		/* dialog is no longer refed by timer; from now one it is refed
		   by the send_bye functions */
		unref_dlg( dlg, 1);
		/* is not 100% sure, but do it */
		if_update_stat( dlg_enable_stats, expired_dlgs, 1);
		return ;
	}

	next_state_dlg( dlg, DLG_EVENT_REQBYE, &old_state, &new_state, &unref);

	if (new_state==DLG_STATE_DELETED && old_state!=DLG_STATE_DELETED) {
		LM_DBG("timeout for dlg with CallID '%.*s' and tags '%.*s' '%.*s'\n",
			dlg->callid.len, dlg->callid.s,
			dlg->tag[DLG_CALLER_LEG].len, dlg->tag[DLG_CALLER_LEG].s,
			dlg->tag[DLG_CALLEE_LEG].len, ZSW(dlg->tag[DLG_CALLEE_LEG].s));

		/* dialog timeout */
		run_dlg_callbacks( DLGCB_EXPIRED, dlg, 0, DLG_DIR_NONE, 0);

		/* delete the dialog from DB */
		if (should_remove_dlg_db())
			remove_dialog_from_db(dlg);

		unref_dlg(dlg, unref + 1 /*timer list*/);

		if_update_stat( dlg_enable_stats, expired_dlgs, 1);
		if_update_stat( dlg_enable_stats, active_dlgs, -1);
	} else {
		unref_dlg(dlg, 1 /*just timer list*/);
	}

	return;
}



