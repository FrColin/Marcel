/* DList.c
 *
 * Implementation of a Double Linked List
 *
 * Cover by Creative Commons Attribution-NonCommercial 3.0 License
 * (http://creativecommons.org/licenses/by-nc/3.0/)
 *
 * 16/07/2015 LF - Creation
 */

#include <stddef.h>
#include <stdio.h>
#include "DList.h"


void DLListInit( DList_t *l ){
	l->first = l->last = NULL;
}

void DLAdd( DList_t *l, DLNode_t *n ){
	n->next = NULL;
	if(!(n->prev = l->last))	/* The list is empty */
		l->first = n;
	else
		n->prev->next = n;
	l->last = n;
}

void DLRemove( DList_t *l, DLNode_t *n ){
	if(n->next)
		n->next->prev = n->prev;
	else	/* Last of the list */
		l->last = n->prev;

	if(n->prev)
		n->prev->next = n->next;
	else	/* First of the list */
		l->first = n->next;
}
