/* DList.h
 *
 * Definition of a Double Linked List
 *
 * Cover by Creative Commons Attribution-NonCommercial 3.0 License
 * (http://creativecommons.org/licenses/by-nc/3.0/)
 *
 * 16/07/2015 LF - Creation
 */

#ifndef DLIST_H
#define DLIST_H

typedef struct _DLNode {
	struct _DLNode *prev;
	struct _DLNode *next;
} DLNode_t;

typedef struct _DList {
	DLNode_t *first;
	DLNode_t *last;
} DList_t;

void DLListInit( DList_t * );
void DLAdd( DList_t *, DLNode_t * );
void DLRemove( DList_t *, DLNode_t * );

#endif
