/*----------------------------------------------------------------------
  File    : strlist.h
  Contents: memory efficient list of strings
  Author  : Christian Borgelt
  History : 2015.04.14 file created
----------------------------------------------------------------------*/
#ifndef __STRLIST__
#define __STRLIST__

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct strblk {         /* --- string list block --- */
  struct strblk *succ;          /* pointer to successor block */
  char          *next;          /* position of next string */
  int           cnt;            /* number of strings in this block */
  char          strings[1];     /* buffer for strings */
} STRBLOCK;                     /* (string list block) */

typedef struct {                /* --- string list --- */
  int           cnt;            /* number of strings in list */
  size_t        bsize;          /* size of the blocks */
  STRBLOCK      *head;          /* head (start) of block list */
  STRBLOCK      *tail;          /* tail (end)   of block list */
  STRBLOCK      *cblk;          /* block of current string */
  const char    *curr;          /* current string */
} STRLIST;

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/
extern STRLIST*    sl_create (size_t bsize);
extern void        sl_delete (STRLIST *list);
extern void        sl_clear  (STRLIST *list);
extern int         sl_add    (STRLIST *list, const char *s);
extern int         sl_cnt    (STRLIST *list);
extern const char* sl_first  (STRLIST *list);
extern const char* sl_ith    (STRLIST *list, int i);
extern const char* sl_next   (STRLIST *list);
extern const char* sl_get    (STRLIST *list);

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define sl_cnt(l)         ((l)->cnt)
#define sl_get(l)         ((l)->curr)

#endif
