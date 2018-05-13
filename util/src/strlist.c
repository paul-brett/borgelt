/*----------------------------------------------------------------------
  File    : strlist.c
  Contents: memory efficient list of strings
  Author  : Christian Borgelt
  History : 2015.04.14 file created
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "strlist.h"

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

STRLIST* sl_create (size_t bsize)
{                               /* --- create a string list */
  STRLIST  *list;               /* created string list */
  STRBLOCK *block;              /* first block of list */

  list = (STRLIST*)malloc(sizeof(STRLIST));
  if (!list) return NULL;       /* allocate the base memory */
  if (bsize <= 0) bsize = 1024*1024;
  block = (STRBLOCK*)malloc(sizeof(STRBLOCK) +(bsize-1) *sizeof(char));
  if (!block) { free(list); return NULL; }
  block->succ = NULL;           /* allocate the first block */
  block->next = block->strings; /* and initialize its fields */
  block->cnt  = 0;
  list->cnt   = 0;              /* initialize the list fields */
  list->bsize = bsize;
  list->cblk  = list->tail = list->head = block;
  list->curr  = NULL;
  return list;                  /* return the created string list */
}  /* sl_create() */

/*--------------------------------------------------------------------*/

void sl_delete (STRLIST *list)
{                               /* --- delete a string list */
  STRBLOCK *block;              /* to traverse the blocks */

  assert(list);                 /* check the function argument */
  while (list->head) {          /* while the block list is not empty */
    block = list->head;         /* get the next block and */
    list->head = block->succ;   /* remove it from the block list */
    free(block);                /* free the current block */
  }
  free(list);                   /* free the base memory */
}  /* sl_delete() */

/*--------------------------------------------------------------------*/

void sl_clear (STRLIST *list)
{                               /* --- clear a string list */
  STRBLOCK *block, *curr;       /* to traverse the blocks */

  assert(list);                 /* check the function argument */
  curr = list->head->succ;      /* get the second block */
  while (curr) {                /* while there is another block */
    block = curr;               /* get the next block and */
    curr  = block->succ;        /* remove it from the block list */
    free(block);                /* free the current block */
  }
  block = list->head;           /* get the only remaining block */
  block->succ = NULL;           /* and re-initialize its fields */
  block->next = block->strings;
  block->cnt  = 0;
  list->cnt   = 0;              /* re-initialize the list */
  list->cblk  = list->tail = list->head = block;
  list->curr  = NULL;
}  /* sl_clear() */

/*--------------------------------------------------------------------*/

int sl_add (STRLIST *list, const char *s)
{                               /* --- add string to a string list */
  STRBLOCK *block;              /* block to add string to */
  size_t   n;                   /* length of the string to add */

  assert(list && s);            /* check the function arguments */
  n   = strlen(s)+1;            /* get the length of the string */
  if (n <= list->bsize)         /* if the string is too long, */
    return -2;                  /* abort with an error */
  block = list->tail;           /* get the last block in the list */
  if (block->next +n > block->strings +list->bsize) {
    block = (STRBLOCK*)malloc(sizeof(STRBLOCK)
                            +(list->bsize-1)*sizeof(char));
    if (!block) return -1;      /* allocate a new list block */
    block->succ = NULL;         /* and initialize it */
    block->next = block->strings;
    block->cnt  = 0;
    list->tail  = list->tail->succ = block;
  }                             /* add block at end of block list */
  strcpy(block->next, s);       /* copy the string into the block */
  block->next += n;             /* get position for next string */
  block->cnt  += 1;             /* count string in the block */
  list->cnt   += 1;             /* and in the list */
  return 0;                     /* return 'ok' */
}  /* sl_add() */

/*--------------------------------------------------------------------*/

const char* sl_first (STRLIST *list)
{                               /* --- get first string in list */
  assert(list);                 /* check the function argument */
  list->cblk = list->head;      /* get first string in first block */
  list->curr = list->cblk->strings;
  if (list->curr >= list->cblk->next)
    return NULL;                /* if there is no string, abort */
  return list->curr;            /* return the first string */
}  /* sl_first() */

/*--------------------------------------------------------------------*/

const char* sl_ith (STRLIST *list, int i)
{                               /* --- get ith string in list */
  STRBLOCK *block;              /* to traverse the blocks */
  int      off;                 /* index offset to current block */

  assert(list);                 /* check the function argument */
  if ((i < 0) || (i >= list->cnt))
    return NULL;                /* check whether index is in range */
  block = list->head;           /* get the first block */
  off   = 0;                    /* initialize the index offset */
  while (i >= off +block->cnt){ /* while correct block not found */
    off  += block->cnt;         /* skip strings in current block */
    block = block->succ;        /* (update the index offset) */
  }                             /* and go to the next block */
  list->cblk = block;           /* note the current block */
  list->curr = block->strings;  /* and get first string in it */
  while (i > off) {             /* while correct string not found */
    list->curr += strlen(list->curr)+1;
    off += 1;                   /* skip the next string and */
  }                             /* increment the index offset */
  return list->curr;            /* return the first string */
}  /* sl_ith() */

/*--------------------------------------------------------------------*/

const char* sl_next (STRLIST *list)
{                               /* --- get next string in list */
  assert(list);                 /* check the function argument */
  if (!list->curr) return NULL; /* if after all strings, abort */
  list->curr += strlen(list->curr)+1;
  if (list->curr < list->cblk->next)
    return list->curr;          /* if next string in same block */
  list->cblk = list->cblk->succ;/* otherwise get the next block */
  if (!list->cblk)              /* if there is no next block, */
    return list->curr = NULL;   /* abort with failure */
  list->curr = list->cblk->strings;
  if (list->curr < list->cblk->next)
    return list->curr;          /* if next string exists, return it */
  return NULL;                  /* return 'no more strings' */
}  /* sl_next() */
