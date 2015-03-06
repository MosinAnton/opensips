/**
 * Copyright (C) 2005 Voice Sistem SRL (Voice-System.RO)
 *
 * This file is part of opensips, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2005-01-25  first tree version (ramona)
 */


#ifndef _PDTREE_H_
#define _PDTREE_H_

#include "../../str.h"

typedef struct _pdt_node
{
	str domain;
	struct _pdt_node *child;
} pdt_node_t;

#define PDT_MAX_DEPTH	32

#define PDT_NODE_SIZE	pdt_char_list.len

typedef struct _pdt_tree
{
	str sdomain;
	pdt_node_t *head;

	struct _pdt_tree *next;
} pdt_tree_t;


/* prefix tree operations */
int add_to_tree(pdt_tree_t *pt, str *code, str *domain);
int pdt_add_to_tree(pdt_tree_t **dpt, str* sdomain, str *code, str *domain);

pdt_tree_t* pdt_get_tree(pdt_tree_t *pl, str *sdomain);

str* get_domain(pdt_tree_t *pt, str *code, int *plen);
str* pdt_get_domain(pdt_tree_t *pt, str* sdomain, str *code, int *plen);

pdt_tree_t* pdt_init_tree(str* sdomain);
void pdt_free_tree(pdt_tree_t *pt);
int pdt_print_tree(pdt_tree_t *pt);

int pdt_check_pd(pdt_tree_t *pt, str* sdomain, str *sp, str *sd);

/* used to get the index for the PDT Tree hash*/
#define strpos(s,c) (strchr(s,c)-s)

#endif

