/*----------------------------------------------------------------------
  File    : table1.c
  Contents: tuple and table management, basic functions
  Author  : Christian Borgelt
  History : 1996.03.06 file created
            1996.07.23 tuple and table management redesigned
            1997.02.26 functions tab_sort() and tab_search() added
            1997.02.27 function tab_reduce() added
            1997.03.28 several functions redesigned
            1997.03.29 several functions added or extended
            1997.08.07 function tab_tplexg() added
            1998.01.29 function tpl_fromas() added
            1998.02.24 function tab_shuffle() added
            1998.03.12 tuple sort function improved
            1998.09.06 tuple array enlargement modified
            1998.06.22 deletion function moved to function tab_create()
            1998.06.23 adapted to changes of attset functions
            1998.07.27 function tab_tplmove() added
            1998.09.25 first step of major redesign completed
            1998.10.01 function tab_rename() added
            1998.11.28 functions tab_colcut() and tab_colcopy() finished
            1998.11.29 functions tpl_clone() and tab_clone() added
            1999.02.04 all occurences of long int changed to int
            1999.03.15 one point coverage functions transf. from opc.c
            1999.03.17 one point coverage functions redesigned
            1999.03.23 functions tab_colcut() and tab_colcopy() debugged
            1999.04.03 parameter 'cloneas' added to function tab_clone()
            1999.04.17 function tab_getwgt() added
            1999.10.21 normalization of one point coverages added
            1999.11.25 bug in natural join (tab_join()) removed
            2001.06.24 module split into two files (table1/2.c)
            2002.02.26 bug in tab_balance() fixed (-2/-1 exchanged)
            2003.07.22 function tab_colnorm() added
            2007.01.17 tab_colnorm() returns offset and scaling factor
            2007.01.18 function tab_coltlin() added
            2007.02.13 adapted to modified module attset
            2007.06.06 bug in function tab_join() fixed (nominal cols.)
            2008.10.23 bug in function joinclean() fixed (free)
            2011.01.03 maintenance of total tuple weight added
            2011.01.21 functions tpl_cmpx() and tpl_cmp1() added
            2013.03.07 adapted to direction param. of sorting functions
            2013.07.18 adapted to definitions ATTID, VALID, TPLID etc.
            2013.07.26 parameter 'dir' added to function tab_sort()
            2013.09.05 return values for tab_reduce() and tab_balance()
            2015.08.01 function tab_colperm() added (permute columns)
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <assert.h>
#include "arrays.h"
#include "table.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define BLKSIZE    256          /* tuple array block size */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- join comparison data --- */
  ATTID cnt;                    /* number of columns to compare */
  ATTID *cis[3];                /* arrays of column indices */
  VALID **maps;                 /* maps for nominal values */
  ATTID *mis;                   /* column indices for the map */
  TUPLE **src, **dst;           /* buffers for tuples */
} JCDATA;                       /* (join comparison data) */

/*----------------------------------------------------------------------
  Tuple Functions
----------------------------------------------------------------------*/

TUPLE* tpl_create (ATTSET *attset, int fromas)
{                               /* --- create a tuple */
  ATTID k, n;                   /* loop variable, number of columns */
  TUPLE *tpl;                   /* created tuple */

  assert(attset);               /* check the function argument */
  n   = as_attcnt(attset);      /* get the number of columns */
  tpl = (TUPLE*)malloc(sizeof(TUPLE) +(size_t)(n-1) *sizeof(INST));
  if (!tpl) return NULL;        /* allocate memory for a tuple */
  tpl->attset = attset;         /* note the attribute set */
  tpl->table  = NULL;           /* clear the reference to a table */
  tpl->id     = -1;             /* and the tuple identifier */
  if (!fromas)                  /* if to create an empty tuple, */
    tpl->xwgt = tpl->wgt = 1.0; /* set the default weight of 1 */
  else {                        /* if to init. from attribute set, */
    for (k = 0; k < n; k++)     /* copy the attribute instances */
      tpl->cols[k] = *att_inst(as_att(attset, k));
    tpl->xwgt = tpl->wgt = as_getwgt(attset);
  }                             /* copy the instance weight */
  return tpl;                   /* return the created tuple */
}  /* tpl_create() */

/*--------------------------------------------------------------------*/

TUPLE* tpl_clone (const TUPLE *tpl)
{                               /* --- clone a tuple */
  ATTID n;                      /* number of columns */
  TUPLE *clone;                 /* created clone */

  assert(tpl);                  /* check the function argument */
  n = as_attcnt(tpl->attset);   /* get the number of columns */
  clone = (TUPLE*)malloc(sizeof(TUPLE) +(size_t)(n-1) *sizeof(INST));
  if (!clone) return NULL;      /* allocate memory */
  clone->attset = tpl->attset;  /* note the attribute set */
  clone->table  = NULL;         /* clear the reference to a table */
  clone->id     = -1;           /* and the tuple identifier */
  clone->mark   = tpl->mark;    /* copy the tuple marker, */
  clone->wgt    = tpl->wgt;     /* weight and extended weight, */
  clone->xwgt   = tpl->xwgt;    /* and the tuple columns */
  memcpy(clone->cols, tpl->cols, (size_t)n *sizeof(INST));
  return clone;                 /* return the created clone */
}  /* tpl_clone() */

/*--------------------------------------------------------------------*/

void tpl_copy (TUPLE *dst, const TUPLE *src)
{                               /* --- copy a tuple */
  ATTID n, k;                   /* numbers of columns */

  assert(src && dst);           /* check the function arguments */
  dst->mark = src->mark;        /* copy the tuple marker */
  dst->wgt  = src->wgt;         /* the tuple weight, and */
  dst->xwgt = src->xwgt;        /* the extended tuple weight */
  n = as_attcnt(src->attset);   /* get the number of columns */
  k = as_attcnt(dst->attset);   /* of source and destination */
  if (k < n) n = k;             /* and determine their minimum */
  memcpy(dst->cols, src->cols, (size_t)n *sizeof(INST));
}  /* tpl_copy() */             /* copy the tuple columns */

/*--------------------------------------------------------------------*/

void tpl_delete (TUPLE *tpl)
{                               /* --- delete a tuple */
  assert(tpl);                  /* check the function argument */
  if (tpl->table)               /* remove the tuple from cont. table */
    tab_tplrem(tpl->table, tpl->id);
  free(tpl);                    /* deallocate the memory */
}  /* tpl_delete() */

/*--------------------------------------------------------------------*/

int tpl_cmp (const TUPLE *tpl1, const TUPLE *tpl2, void *data)
{                               /* --- compare two tuples */
  const ATT  *att;              /* to traverse the attributes */
  const INST *col1, *col2;      /* to traverse the tuple columns */
  ATTID      cnt, i;            /* number of columns, loop variable */
  int        t;                 /* attribute type */
  int        r = 1;             /* return code (result of comparison) */

  if (!tpl1) { tpl1 = tpl2; tpl2 = NULL; r = -1; }
  assert(tpl1);                 /* if no first tuple, exchange */
  cnt = as_attcnt(tpl1->attset);/* get the number of columns */
  assert(!tpl2 || (as_attcnt(tpl2->attset) == cnt));
  for (i = 0; i < cnt; i++) {   /* traverse attributes and columns */
    att  = as_att(tpl1->attset, i);
    col1 =          tpl1->cols +i;
    col2 = (tpl2) ? tpl2->cols +i : att_inst(att);
    t    = att_type(att);       /* get the attribute type */
    if      (t == AT_FLT) {     /* floating point instance */
      if (col1->f < col2->f) return -r;
      if (col1->f > col2->f) return  r; }
    else if (t == AT_INT) {     /* integer instance */
      if (col1->i < col2->i) return -r;
      if (col1->i > col2->i) return  r; }
    else {                      /* nominal instance */
      if (col1->n < col2->n) return -r;
      if (col1->n > col2->n) return  r;
    }                           /* (traverse columns and if they */
  }                             /* differ, return comparison result) */
  return 0;                     /* return 'equal' */
}  /* tpl_cmp() */

/*--------------------------------------------------------------------*/

int tpl_cmpx (const TUPLE *tpl1, const TUPLE *tpl2, void *data)
{                               /* --- compare two tuples */
  const ATT  *att;              /* to traverse the attributes */
  const INST *col1, *col2;      /* to traverse the tuple columns */
  ATTID      *p;                /* to traverse the column indices */
  int        t;                 /* attribute type */
  int        r = 1;             /* return code (result of comparison) */

  if (!tpl1) { tpl1 = tpl2; tpl2 = NULL; r = -1; }
  assert(tpl1);                 /* if no first tuple, exchange */
  for (p = (ATTID*)data; *p >= 0; p++) { /* traverse the columns */
    assert((*p < as_attcnt(tpl1->attset))
    &&     (*p < as_attcnt(tpl2->attset)));
    att  = as_att(tpl1->attset, *p);
    col1 =          tpl1->cols +*p;
    col2 = (tpl2) ? tpl2->cols +*p : att_inst(att);
    t    = att_type(att);       /* get the attribute type */
    if      (t == AT_FLT) {     /* floating point instance */
      if (col1->f < col2->f) return -r;
      if (col1->f > col2->f) return  r; }
    else if (t == AT_INT) {     /* integer instance */
      if (col1->i < col2->i) return -r;
      if (col1->i > col2->i) return  r; }
    else {                      /* nominal instance */
      if (col1->n < col2->n) return -r;
      if (col1->n > col2->n) return  r;
    }                           /* (traverse columns and if they */
  }                             /* differ, return comparison result) */
  return 0;                     /* return 'equal' */
}  /* tpl_cmpx() */

/*--------------------------------------------------------------------*/

int tpl_cmp1 (const TUPLE *tpl1, const TUPLE *tpl2, void *data)
{                               /* --- compare two tuples */
  const INST *col1, *col2;      /* values of columns to compare */

  col1 = tpl_colval(tpl1, *(ATTID*)data);  /* get column values */
  col2 = tpl_colval(tpl2, *(ATTID*)data);  /* for both tuples */
  if (col1->n > col2->n) return  1;
  if (col1->n < col2->n) return -1;
  return 0;                     /* return sign of diff. of values */
}  /* tpl_cmp1() */

/*--------------------------------------------------------------------*/

double tpl_sqrdist (const TUPLE *tpl1, const TUPLE *tpl2)
{                               /* --- compute distance of two tuples */
  ATTID  k;                     /* loop variable */
  double d, sum = 0;            /* (sum of) distances */
  const INST *i1, *i2;          /* to traverse the columns */

  assert( tpl1                  /* check the function arguments */
  &&    (!tpl2 || (tpl_attset(tpl1) == tpl_attset(tpl2))));
  for (k = tpl_colcnt(tpl1); --k >= 0; ) {
    if (att_getmark(tpl_colatt(tpl1, k)) < 0)
      continue;                 /* skip unmarked attributes */
    i1 = tpl_colval(tpl1, k);   /* traverse the columns of the tuples */
    i2 = (tpl2) ? tpl_colval(tpl2, k) : att_inst(tpl_colatt(tpl1, k));
    switch (att_type(tpl_colatt(tpl1, k))) {
      case AT_FLT: d = (double)i1->f -(double)i2->f; sum += d*d; break;
      case AT_INT: d = (double)i1->i -(double)i2->i; sum += d*d; break;
      default:     if (i1->n != i2->n)               sum += 1.0; break;
    }                           /* compute columnwise distances */
  }                             /* and sum the squared distances */
  return sum;                   /* return sum of squared distances */
}  /* tpl_sqrdist() */

/*----------------------------------------------------------------------
The parameter data, which is not used in the above function, is needed
to make this function usable with the functions tab_sort and tab_search.
----------------------------------------------------------------------*/

WEIGHT tpl_setwgt (TUPLE *tpl, WEIGHT wgt)
{                               /* --- set the tuple weight */
  assert(tpl);                  /* check the function arguments */
  if (tpl->table)               /* update weight of containing table */
    tpl->table->wgt += wgt -(double)tpl->wgt;
  return tpl->wgt = wgt;        /* set the new tuple weight */
}  /* tpl_setwgt() */

/*--------------------------------------------------------------------*/

WEIGHT tpl_incwgt (TUPLE *tpl, WEIGHT wgt)
{                               /* --- increase the tuple weight */
  assert(tpl);                  /* check the function arguments */
  if (tpl->table)               /* update weight of containing table */
    tpl->table->wgt += (double)wgt;
  return tpl->wgt += wgt;       /* increase the tuple weight */
}  /* tpl_incwgt() */

/*--------------------------------------------------------------------*/

void tpl_toas (TUPLE *tpl)
{                               /* --- copy tuple to attribute set */
  ATTID k, n;                   /* loop variable, number of columns */

  assert(tpl);                  /* check the function argument */
  n = as_attcnt(tpl->attset);   /* get the number of columns */
  for (k = 0; k < n; k++)       /* traverse the tuple columns */
    *att_inst(as_att(tpl->attset, k)) = tpl->cols[k];
  as_setwgt(tpl->attset, tpl->wgt);   /* copy the tuple columns */
}  /* tpl_toas() */                   /* and  the tuple weight */

/*--------------------------------------------------------------------*/

void tpl_fromas (TUPLE *tpl)
{                               /* --- copy tuple from attribute set */
  ATTID k, n;                   /* loop variable, number of columns */

  assert(tpl);                  /* check the function argument */
  n = as_attcnt(tpl->attset);   /* get the number of columns */
  for (k = 0; k < n; k++)       /* traverse the tuple columns */
    tpl->cols[k] = *att_inst(as_att(tpl->attset, k));
  tpl->wgt = as_getwgt(tpl->attset); /* copy the att. instances */
}  /* tpl_fromas() */                /* and the instance weight */

/*--------------------------------------------------------------------*/

size_t tpl_hash (TUPLE *tpl)
{                               /* --- hash function for tuples */
  ATTID  k, n;                  /* loop variable, number of columns */
  int    e;                     /* binary exponent */
  size_t h = 0, t;              /* hash value of the tuple, buffer */

  assert(tpl);                  /* check the function arguemnt */
  n = as_attcnt(tpl->attset);   /* get the number of columns */
  for (k = 0; k < n; k++) {     /* traverse the tuple columns */
    if (att_type(as_att(tpl->attset, k)) == AT_FLT) {
      t = (size_t)(INT_MAX *(frexp(tpl->cols[k].f, &e) -0.5));
      h ^= (h << 7) ^ (h << 1) ^ t ^ (size_t)e; }
    else                        /* split into mantissa and exponent */
      h ^= (h << 7) ^ (h << 1) ^ (size_t)tpl->cols[k].i;
  }                             /* use an integer value directly */
  return h;                     /* return the computed hash value */
}  /* tpl_hash() */

/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void tpl_show (TUPLE *tpl, TPL_APPFN showfn, void *data)
{                               /* --- show a tuple */
  ATTID k, n;                   /* loop variable, number of columns */
  ATT   *att;                   /* to traverse attributes */
  INST  *col;                   /* to traverse columns */

  assert(tpl);                  /* check the function argument */
  n = as_attcnt(tpl->attset);   /* get the number of columns */
  for (k = 0; k < n; k++) {     /* traverse the tuple columns */
    col = tpl->cols +k;         /* and the corresp. attributes */
    att = as_att(tpl->attset, k);
    switch (att_type(att)) {    /* and evaluate its type */
      case AT_FLT: if (isnan (col->f)) printf("? ");
                   else printf("%"DTFLT_FMT" ", col->f);         break;
      case AT_INT: if (isnull(col->i)) printf("? ");
                   else printf("%"DTINT_FMT" ", col->i);         break;
      default    : if (isnone(col->i)) printf("? ");
                   else printf("%s ", att_valname(att, col->n)); break;
    }                           /* print the column value */
  }                             /* w.r.t. the attribute type */
  printf(": %"WGT_FMT, tpl->wgt);        /* print the tuple weight */
  if (showfn) showfn(tpl,data); /* show the additional information */
  printf("\n");                 /* terminate the output line */
}  /* tpl_show() */

#endif
/*----------------------------------------------------------------------
  Auxiliary Functions
----------------------------------------------------------------------*/

int tab_resize (TABLE *tab, TPLID size)
{                               /* --- resize tuple array */
  TPLID n;                      /* current array size */
  TUPLE **p;                    /* reallocated tuple array */

  assert(tab);                  /* check the function argument */
  n = tab->size;                /* get the current array size */
  if (size > 0) {               /* if to enlarge the array */
    if (n >= size) return 0;    /* if array is large enough, abort */
    n += (n > BLKSIZE) ? n >> 1 : BLKSIZE;
    if (n >  size) size = n; }  /* compute the new array size */
  else {                        /* if to shrink the array */
    size = tab->cnt << 1;       /* get the maximal tolerable size */
    if (size < BLKSIZE) size = BLKSIZE;
    if (n <= size) return 0;    /* if array is small enough, abort */
    size = tab->cnt +(tab->cnt >> 1);
    if (size < BLKSIZE) size = BLKSIZE;
  }                             /* compute the new array size */
  p = (TUPLE**)realloc(tab->tpls, (size_t)size *sizeof(TUPLE*));
  if (!p) return -1;            /* resize the tuple array */
  tab->tpls = p;                /* and set the new array */
  tab->size = size;             /* set the new array size */
  return 1;                     /* return 'ok' */
}  /* tab_resize() */

/*--------------------------------------------------------------------*/

static void restore (TABLE *tab, TPLID rszcnt, TPLID addcnt)
{                               /* --- restore if expansion failed */
  size_t size;                  /* old size of the tuples */
  TUPLE  **p = tab->tpls;       /* to traverse the tuples */

  assert(tab);                  /* check the function argument */
  size = (size_t)as_attcnt(tab->attset);
  size = sizeof(TUPLE) +(size-1) *sizeof(INST);
  while (--rszcnt >= 0) {       /* traverse the resized tuples */
    *p = realloc(*p, size); p++; }
  tab->cnt -= addcnt;           /* restore the number of tuples */
  while (--addcnt >= 0)         /* traverse the added tuples */
    free(*p++);                 /* and delete them */
}  /* restore() */

/*--------------------------------------------------------------------*/

static int expand (TABLE *tab, TPLID tplcnt, ATTID colcnt)
{                               /* --- expand a table */
  TPLID  k;                     /* loop variable */
  size_t size;                  /* new tuple size (with add. columns) */
  TUPLE  **p, *tpl;             /* to traverse the tuples */

  assert(colcnt >= 0);          /* check the function argument */
  size = (size_t)(as_attcnt(tab->attset) +colcnt);
  size = sizeof(TUPLE) +(size-1) *sizeof(INST);
  if (colcnt > 0) {             /* if to add columns (expand tuples) */
    p = tab->tpls;              /* traverse the existing tuples */
    for (k = 0; k < tab->cnt; k++) {
      tpl = (TUPLE*)realloc(*p, size);
      if (!tpl) { restore(tab, k, 0); return -1; }
      *p++ = tpl;               /* resize the tuple and */
    }                           /* set the new tuple */
    tpl = (TUPLE*)realloc(tab->buf, size);
    if (!tpl) { restore(tab, k, 0); return -1; }
    tab->buf = tpl;             /* resize the tuple buffer */
  }                             /* and set the new buffer */
  if (tplcnt <= 0) return 0;    /* if no tuples to add, abort */
  if (tab_resize(tab, tab->cnt +tplcnt) < 0) {
    restore(tab, tab->cnt, 0); return -1; }
  p = tab->tpls +tab->cnt;      /* get next field in tuple array */
  for (k = 0; k < tplcnt; k++){ /* traverse the additional tuples */
    *p++ = tpl = (TUPLE*)malloc(size);
    if (!tpl) { restore(tab, tab->cnt, k); return -1; }
    tpl->attset = tab->attset;  /* allocate a new tuple */
    tpl->table  = tab;          /* and initialize fields */
    tpl->id     = tab->cnt +k;
    tpl->xwgt   = tpl->wgt = 1.0;
  }
  tab->cnt += tplcnt;           /* compute new number of tuples */
  tab->wgt += (double)tplcnt;   /* and the new table weight */
  return 0;                     /* return 'ok' */
}  /* expand() */

/*----------------------------------------------------------------------
  Table Functions
----------------------------------------------------------------------*/

TABLE* tab_create (const char *name, ATTSET *attset, TPL_DELFN delfn)
{                               /* --- create a table */
  TABLE *tab;                   /* created table */

  assert(attset && delfn && name);    /* check function arguments */
  tab = (TABLE*)malloc(sizeof(TABLE));
  if (!tab) return NULL;        /* allocate memory for table body */
  tab->name = (char*)malloc((strlen(name) +1) *sizeof(char));
  if (!tab->name)  { free(tab); return NULL; }
  tab->buf  = (TUPLE*)malloc(sizeof(TUPLE)
                   +(size_t)(as_attcnt(attset)-1) *sizeof(INST));
  if (!tab->buf) { free(tab->name); free(tab); return NULL; }
  strcpy(tab->name, name);      /* copy the table name and */
  tab->attset = attset;         /* initialize the fields */
  tab->cnt    = tab->size = 0;
  tab->tpls   = NULL;
  tab->delfn  = delfn;
  tab->wgt    = 0.0;
  return tab;                   /* return the created table */
}  /* tab_create() */

/*--------------------------------------------------------------------*/

TABLE* tab_clone (const TABLE *tab, int cloneas)
{                               /* --- clone a table */
  TPLID  k;                     /* loop variable */
  ATTSET *attset;               /* clone of the attribute set */
  TABLE  *clone;                /* created clone of the table */
  TUPLE  *d; const TUPLE *s;    /* to traverse the tuples */

  assert(tab);                  /* check the function argument */
  attset = (cloneas) ? as_clone(tab->attset) : tab->attset;
  if (!attset) return NULL;     /* get the underlying attribute set */
  clone = tab_create(tab->name, attset, tab->delfn);
  if (!clone) { if (cloneas) as_delete(attset); return NULL; }
  if (tab->cnt <= 0)            /* if there are no tuples, */
    return clone;               /* abort the function */
  clone->tpls = (TUPLE**)malloc((size_t)tab->cnt *sizeof(TUPLE*));
  if (!clone->tpls) { tab_delete(clone, cloneas); return NULL; }
  for (k = 0; k < tab->cnt; k++) { /* allocate a tuple array */
    s = tab->tpls[k];           /* traverse the source tuples */
    d = tpl_clone(s);           /* clone the source tuple */
    if (!d) break;              /* and check for success */
    d->table = clone;           /* set the table reference and */
    clone->tpls[d->id = k] = d; /* add it to the created clone */
  }
  if (k < tab->cnt) {           /* if an error occured */
    while (--k >= 0) free(clone->tpls[k]);
    tab_delete(clone, cloneas); return NULL;
  }                             /* delete the table and abort */
  clone->size = clone->cnt = k; /* set the number of tuples */
  clone->wgt  = tab->wgt;       /* and the total tuple weight */
  return clone;                 /* return the created clone */
}  /* tab_clone() */

/*--------------------------------------------------------------------*/

void tab_delete (TABLE *tab, int delas)
{                               /* --- delete a table */
  TPLID i;                      /* loop variable */
  TUPLE **p;                    /* to traverse the tuples */

  assert(tab);                  /* check the function argument */
  if (tab->tpls) {              /* if there are tuples */
    for (p = tab->tpls +(i = tab->cnt); --i >= 0; ) {
      (*--p)->table = NULL; (*p)->id = -1; tab->delfn(*p); }
    free(tab->tpls);            /* delete tuples, array, */
  }                             /* and the attribute set */
  if (delas) as_delete(tab->attset);
  free(tab->buf);               /* delete the tuple buffer, */
  free(tab->name);              /* the table name */
  free(tab);                    /* and the table body */
}  /* tab_delete() */

/*--------------------------------------------------------------------*/

int tab_rename (TABLE *tab, const char *name)
{                               /* --- rename a table */
  char *s;                      /* new name for table */

  assert(tab && name);          /* check the function arguments */
  s = (char*)realloc(tab->name, (strlen(name)+1) *sizeof(char));
  if (!s) return -1;            /* reallocate memory for new name */
  tab->name = strcpy(s, name);  /* copy the new table name */
  return 0;                     /* and return 'ok' */
}  /* tab_rename() */

/*--------------------------------------------------------------------*/

int tab_cmp (const TABLE *tab1, const TABLE *tab2,
             TPL_CMPFN cmpfn, void *data)
{                               /* --- compare two tables */
  TPLID k, n;                   /* loop variable, number of tuples */

  assert(tab1 && tab2 && cmpfn);/* check the function arguments */
  n  = (tab1->cnt < tab2->cnt) ? tab1->cnt : tab2->cnt;
  for (k = 0; k < n; k++)       /* traverse the tuples of the tables */
    if (cmpfn(tab1->tpls[k], tab2->tpls[k], data) != 0)
      return -1;                /* if the tuples differ, abort */
  return 0;                     /* otherwise return 'equal' */
}  /* tab_cmp() */

/*--------------------------------------------------------------------*/

TPLID tab_reduce (TABLE *tab)
{                               /* --- reduce a table */
  TPLID i;                      /* loop variable */
  TUPLE **d, **s;               /* to traverse the tuples */

  assert(tab);                  /* check the function argument */
  if (tab->cnt <= 0) return 0;  /* check whether table is empty */
  ptr_qsort(tab->tpls, (size_t)tab->cnt, +1, (CMPFN*)tpl_cmp, NULL);
  d = tab->tpls; s = d+1;       /* sort and traverse the tuple array */
  for (i = tab->cnt, tab->cnt = 1; --i > 0; s++) {
    if (tpl_cmp(*d, *s, NULL) != 0) {
      *++d = *s;                /* if the next tuple differs, keep it */
      (*d)->id = tab->cnt++; }  /* and increment the tuple counter */
    else {                      /* if the next tuple is identical */
      (*d)->wgt += (*s)->wgt;   /* sum the counters (weights), */
      (*s)->table = NULL;       /* remove the tuple from the table, */
      (*s)->id    = -1;         /* clear the tuple identifier */
      tab->delfn(*s);           /* and call the deletion function */
    }
  }
  tab_resize(tab, 0);           /* try to shrink the tuple array */
  return tab->cnt;              /* return the new number of tuples */
}  /* tab_reduce() */

/*--------------------------------------------------------------------*/

double tab_balance (TABLE *tab, ATTID colid,
                    double wgtsum, double *frqs, int intmul)
{                               /* --- balance a table w.r.t. a column*/
  TPLID  i;                     /* loop variable */
  VALID  k;                     /* current class */
  VALID  n;                     /* number of attribute values */
  TUPLE  *tpl;                  /* to traverse the tuples */
  double *facts, *f;            /* weighting factors, buffer */
  double w, sum;                /* (sum of the) tuple weight(s) */

  assert(tab                    /* check the function arguments */
  &&    (colid >= 0) && (colid <= tab_colcnt(tab))
  &&    (att_type(tab_col(tab, colid)) == AT_NOM));

  /* --- initialize --- */
  n     = att_valcnt(tab_col(tab, colid));
  facts = (double*)calloc((size_t)n, sizeof(double));
  if (!facts) return -1;        /* allocate a factor array */
  for (sum = 0.0, i = 0; i < tab->cnt; i++) {
    tpl = tab->tpls[i];         /* traverse the tuples in the table */
    sum += w = (double)tpl->wgt;/* get and sum the tuple weights */
    k = tpl->cols[colid].n;     /* get the class identifier and */
    if (!isnone(k)) facts[k] += w;     /* sum the tuple weights */
  }                             /* determine the value frequencies */
  tab->wgt = sum;               /* set the total tuple weight */
  if (sum <= 0) { free(facts); return 0; }

  /* --- compute weighting factors --- */
  f = facts +(k = n);           /* traverse the computed frequencies */
  if      (wgtsum <= -2) {      /* if to lower the tuple weights */
    for (w = WGT_MAX; --k >= 0; ) { if (*--f < w) w = *f; }
    wgtsum = (double)n *w; }    /* find the minimal frequency */
  else if (wgtsum <= -1) {      /* if to boost the tuple weights */
    for (w = 0;       --k >= 0; ) { if (*--f > w) w = *f; }
    wgtsum = (double)n *w; }    /* find the maximal frequency */
  else if (wgtsum <=  0)        /* if to shift the tuple weights, */
    wgtsum = sum;               /* use the sum of the tuple weights */
  if (!frqs) {                  /* if no relative freqs. requested */
    w = wgtsum /(double)n;      /* compute the weighting factors */
    for (k = n; --k >= 0; ) facts[k] = w / facts[k]; }
  else {                        /* if relative freqs. requested */
    f = frqs +(k = n);          /* sum the requested frequencies */
    for (sum = 0; --k >= 0; ) sum += *--f;
    w = wgtsum /sum;            /* compute the weighting factors */
    for (k = n; --k >= 0; ) facts[k] = w *(frqs[k] / facts[k]);
  }                             /* (facts contains the factors) */
  if (intmul)                   /* if to use integer multiples */
    for (k = n; --k >= 0; ) facts[k] = floor(facts[k] +0.5);

  /* --- weight the tuples --- */
  for (sum = 0.0, i = 0; i < tab->cnt; i++) {
    tpl = tab->tpls[i];         /* traverse the tuples again */
    k   = tpl->cols[colid].n;   /* get the class identifier */
    if (isnone(k)) { tpl->wgt = 0.0; continue; }
    sum += w = facts[k] *(double)tpl->wgt;
    tpl->wgt = (WEIGHT)w;       /* adapt the tuple weight and */
  }                             /* sum it over the tuples */

  free(facts);                  /* delete the factor array */
  return tab->wgt = sum;        /* set the new total tuple weight */
}  /* tab_balance() */

/*--------------------------------------------------------------------*/

double tab_getwgt (const TABLE *tab, TPLID off, TPLID cnt)
{                               /* --- get the tuple weight sum */
  double sum;                   /* sum of the tuple weights */

  assert(tab && (off >= 0));    /* check the function arguments */
  cnt += off;                   /* compute end index of range */
  if (cnt > tab->cnt) cnt = tab->cnt;
  assert(off <= cnt);           /* check for a valid index range */
  for (sum = 0; off < cnt; off++)
    sum += (double)tab->tpls[off]->wgt;
  return sum;                   /* return the tuple weight sum */
}  /* tab_getwgt() */

/*--------------------------------------------------------------------*/

void tab_shuffle (TABLE *tab, TPLID off, TPLID cnt, RANDFN randfn)
{                               /* --- shuffle a table section */
  TPLID i;                      /* tuple index */
  TUPLE **p, *tpl;              /* to traverse the tuples, buffer */

  assert(tab && (off >= 0) && randfn);  /* check function arguments */
  if (cnt > (i = tab->cnt -off)) cnt = i;
  assert(cnt >= 0);             /* check and adapt number of tuples */
  for (p = tab->tpls +off; --cnt > 0; ) {
    i = (TPLID)((double)(cnt+1) *randfn());
    if      (i > cnt) i = cnt;  /* compute a random index in the */
    else if (i < 0)   i = 0;    /* remaining table section */
    tpl = p[i]; p[i] = *p; *p++ = tpl;
    tpl->id = off++;            /* exchange first and i-th tuple */
  }                             /* and adapt the tuple identifier */
  if (cnt >= 0) (*p)->id = off; /* adapt identifier of last tuple */
}  /* tab_shuffle() */

/*--------------------------------------------------------------------*/

void tab_sort (TABLE *tab, TPLID off, TPLID cnt, int dir,
               TPL_CMPFN cmpfn, void *data)
{                               /* --- sort a table section */
  TPLID n;                      /* number of tuples */

  assert(tab && (off >= 0) && cmpfn);   /* check function arguments */
  if (cnt > (n = tab->cnt -off)) cnt = n;
  ptr_qsort(tab->tpls +off, (size_t)cnt, dir, (CMPFN*)cmpfn, data);
  cnt += off;                   /* sort tuples with given function */
  if (cnt > tab->cnt) cnt = tab->cnt;
  for ( ; off < cnt; off++)     /* traverse the sorted tuples */
    tab->tpls[off]->id = off;   /* and set the new identifiers */
}  /* tab_sort() */

/*--------------------------------------------------------------------*/

TPLID tab_search (TABLE *tab, TPLID off, TPLID cnt,
                  TUPLE *tpl, TPL_CMPFN cmpfn, void *data)
{                               /* --- search a tuple in a table sec. */
  TPLID mid;                    /* index of middle tuple */
  int   r;                      /* result of comparison */

  assert(tab && (off >= 0) && cmpfn);   /* check function arguments */
  cnt += off-1;                 /* get and adapt end index of range */
  if (cnt >= tab->cnt) cnt = tab->cnt-1;
  do {                          /* binary search loop */
    mid = (off +cnt) >> 1;      /* compare the tuple in the middle */
    r = cmpfn(tpl, tab->tpls[mid], data);    /* with the given one */
    if (r > 0) off = mid +1;    /* if smaller, continue with right, */
    else       cnt = mid;       /* otherwise continue with left half */
  } while (off < cnt);          /* repeat until the section is empty */
  if (off > cnt) return -1;     /* if no tuple left, abort function */
  if (off != mid)               /* if last test on different tuple, */
    r = cmpfn(tpl, tab->tpls[off], data);      /* retest last tuple */
  return (r == 0) ? off : -1;   /* return the index of the tuple */
}  /* tab_search() */

/*--------------------------------------------------------------------*/

TPLID tab_group (TABLE *tab, TPLID off, TPLID cnt,
                 TPL_SELFN selfn, void *data)
{                               /* -- group tuples in a table section */
  TPLID i;                      /* tuple identifier */
  TUPLE *t;                     /* exchange buffer */
  TUPLE **src, **dst;           /* source and destination */

  assert(tab && (off >= 0) && selfn);   /* check function arguments */
  if (cnt > (i = tab->cnt -off)) cnt = i;
  assert(cnt >= 0);             /* check and adapt number of tuples */
  for (src = dst = tab->tpls +cnt; --cnt >= 0; ) {
    if (!selfn(*--src, data)) { /* traverse the tuples in the section */
      t = *src;  *src  = *--dst;     *dst       = t;
      i = t->id; t->id = (*src)->id; (*src)->id = i;
    }                           /* swap the qualifying tuples */
  }                             /* to the head of the section */
  return (TPLID)(dst -src);     /* and return their number */
}  /* tab_group() */

/*--------------------------------------------------------------------*/

void tab_apply (TABLE *tab, TPLID off, TPLID cnt,
                TPL_APPFN appfn, void *data)
{                               /* --- apply a function to all tuples */
  assert(tab && (off >= 0) && appfn);  /* check function arguments */
  cnt += off;                   /* get and adapt end index of range */
  if (cnt > tab->cnt) cnt = tab->cnt;
  for ( ; off < cnt; off++)     /* apply the function to the tuples */
    appfn(tab->tpls[off], data);
}  /* tab_apply() */

/*--------------------------------------------------------------------*/

void tab_fill (TABLE *tab, TPLID tploff, TPLID tplcnt,
                           ATTID coloff, ATTID colcnt)
{                               /* --- fill table part with nulls */
  ATTID k;                      /* loop variable */
  INST  *src, *dst;             /* to traverse the tuple columns */

  assert(tab && (tploff >= 0) && (coloff >= 0));  /* check arguments */
  tplcnt += tploff;             /* get and adapt end index of range */
  if (tplcnt > tab->cnt) tplcnt = tab->cnt;
  k = as_attcnt(tab->attset);   /* get and adapt column range */
  if (colcnt > k -coloff) colcnt = (k > coloff) ? k -coloff : 0;
  for (src = NULL; tploff < tplcnt; tploff++) {
    dst = tab->tpls[tploff]->cols +coloff;  /* traverse the tuples */
    if (src)                    /* copy null values from 1st tuple */
      memcpy(dst, src, (size_t)colcnt *sizeof(INST));
    else {                      /* if this is the first tuple */
      src = dst;                /* traverse the columns */
      for (k = 0; k < colcnt; k++) {
        switch (att_type(as_att(tab->attset, coloff+k))) {
          case AT_FLT: dst[k].f = NV_FLT; break;
          case AT_INT: dst[k].i = NV_INT; break;
          default    : dst[k].n = NV_NOM; break;
        }                       /* set the columns to null values */
      }                         /* w.r.t. to the column type */
    }                           /* (for the following tuples */
  }                             /* copy values from first tuple) */
}  /* tab_fill() */

/*--------------------------------------------------------------------*/

static int joincmp (const TUPLE *tpl1, const TUPLE *tpl2, JCDATA *jcd)
{                               /* --- compare tuples for join */
  ATTID i;                      /* loop variable */
  ATTID *p1, *p2;               /* to traverse the column indices */
  CINST *c1, *c2;               /* to traverse the columns to compare */
  VALID **mp;                   /* map for nominal values */
  VALID n1, n2;                 /* nominal values to compare */

  p1 = jcd->cis[1]; p2 = jcd->cis[2];
  mp = jcd->maps;               /* get the column index arrays */
  for (i = jcd->cnt; --i >= 0; mp++) {
    c1 = tpl1->cols +*p1++;     /* traverse the column indices */
    c2 = tpl2->cols +*p2++;     /* and get the columns to compare */
    if      (*mp == (void*)-1){ /* if float attribute */
      if (c1->f < c2->f) return -1;
      if (c1->f > c2->f) return  1; }
    else if (*mp == NULL) {     /* if integer attribute */
      if (c1->i < c2->i) return -1;
      if (c1->i > c2->i) return  1; }
    else {                      /* if nominal attribute */
      n1 = (jcd->mis == jcd->cis[1]) ? (*mp)[c1->n] : c1->n;
      n2 = (jcd->mis == jcd->cis[2]) ? (*mp)[c2->n] : c2->n;
      if (n1 < n2) return -1;   /* map indices of nominal values, */
      if (n1 > n2) return  1;   /* compare the atttribute values */
    }                           /* and if they differ, abort */
  }
  return 0;                     /* otherwise return 'equal' */
}  /* joincmp() */

/*--------------------------------------------------------------------*/

static void joinclean (JCDATA *jcd)
{                               /* --- clean up join data */
  ATTID i;                      /* loop variable */
  VALID **mp;                   /* to traverse the maps */

  for (mp = jcd->maps +(i = jcd->cnt); --i >= 0; )
    if (*--mp && (*mp != (void*)-1))
      free(*mp);                /* free the value maps */
  free(jcd->maps);              /* free the map array itself */
  free(jcd->cis[0]);            /* and the column index array */
}  /* joinclean() */

/*--------------------------------------------------------------------*/

static int joinerr (JCDATA *jcd, TPLID cnt)
{                               /* --- clean up if join failed */
  if (jcd->dst) {               /* if a (partial) result exists */
    while (--cnt >= 0) free(jcd->dst[cnt]);
    free(jcd->dst);             /* delete all result tuples */
  }                             /* and the result array */
  if (jcd->src) free(jcd->src); /* delete the source tuple buffer */
  joinclean(jcd);               /* clean up the join data */
  return -1;                    /* return an error code */
}  /* joinerr() */

/*--------------------------------------------------------------------*/

ATTID tab_join (TABLE *dst, TABLE *src,
                ATTID *dcis, ATTID *scis, ATTID cnt)
{                               /* --- join two tables */
  ATTID  i, k, x;               /* loop variables, buffers */
  VALID  n, m, z;               /* (number of) nominal value(s) */
  ATTID  ncs, ncd, ncr;         /* number of columns in src/dst/res */
  TPLID  size;                  /* size of the tuple buffer */
  TPLID  rescnt, resvsz;        /* number of tuples in result array */
  TUPLE  *tpl;                  /* created (joined) tuple */
  TUPLE  **d, **s, **r, **t;    /* to traverse the tuples */
  INST   *dc, *sc;              /* to traverse the tuple columns */
  ATT    *da, *sa;              /* to traverse the column attributes */
  ATTID  *cis;                  /* non-join column index array */
  VALID  *map;                  /* to traverse nominal value maps */
  int    c;                     /* comparison result, buffer */
  double sum, w;                /* (sum of) tuple weight(s) */
  JCDATA jcd;                   /* column data for sorting */

  assert(dst && src             /* check the function arguments */
  &&    ((cnt <= 0) || (dcis && scis)));

  /* --- build column index arrays --- */
  ncs = as_attcnt(src->attset); /* get the number of columns */
  ncd = as_attcnt(dst->attset); /* in source and destination */
  k   = ((x = cnt) < 0) ? ncs +ncd : 0;
  cis = jcd.cis[0] = (ATTID*)malloc((size_t)(ncs +k) *sizeof(ATTID));
  if (!cis) return -1;          /* create colum index array(s) */
  if (cnt < 0) {                /* if to do a natural join */
    scis = cis +ncs; dcis = scis +ncs;
    for (cnt = i = 0; i < ncs; i++) {
      k = as_attid(dst->attset, att_name(as_att(src->attset, i)));
      if (k >= 0) { dcis[cnt] = k; scis[cnt++] = i; }
    }                           /* collect columns with same names */
  }                             /* in source and destination */
  memset(cis, 0, (size_t)ncs *sizeof(ATTID));
  for (i = cnt; --i >= 0; )     /* mark the join columns */
    cis[scis[i]] = -1;          /* of the source table */
  for (i = k = 0; i < ncs; i++) {
    if (cis[i]) continue;       /* collect the indices */
    cis[k++] = i;               /* of the non-join columns */
    if (x < 0)  continue;       /* no check needed for natural join */
    if (as_attid(dst->attset, att_name(as_att(src->attset, i))) >= 0) {
      free(cis); return i+1; }  /* non-join columns must not exist */
  }                             /* already in the destination table */
  ncr = ncd +k;                 /* get the number of result columns */

  /* --- build maps for nominal values --- */
  jcd.cnt  = cnt;               /* store the number of join columns */
  jcd.mis  = scis;              /* and build value maps for source */
  jcd.maps = (VALID**)calloc((size_t)cnt, sizeof(VALID*));
  if (!jcd.maps) { free(cis); return -1; }
  jcd.src  = jcd.dst = NULL;    /* clear the tuple buffers */
  for (i = 0; i < cnt; i++) {   /* traverse the join attributes */
    sa = as_att(src->attset, scis[i]);
    da = as_att(dst->attset, dcis[i]);
    c  = att_type(sa);          /* get and check the column type */
    if (c != att_type(da)) return -2;
    if (c == AT_FLT) { jcd.maps[i] = (void*)-1; continue; }
    if (c == AT_INT) { jcd.maps[i] = NULL;      continue; }
    n = att_valcnt(sa);         /* get the number of values and */
    m = att_valcnt(da);         /* create a map for nominal values */
    map = jcd.maps[i] = (VALID*)malloc((size_t)n *sizeof(VALID));
    if (!map) return joinerr(&jcd, 0);
    for (z = 0; z < n; z++) {   /* traverse the nominal values */
      map[z] = att_valid(da, att_valname(sa, z));
      if (isnone(map[k])) map[z] = m;
    }                           /* build the value map */
  }                             /* for the source attribute */

  /* --- copy and sort tuple arrays --- */
  size = src->cnt +dst->cnt;    /* allocate the tuple buffers */
  jcd.src = (TUPLE**)malloc((size_t)(size+2) *sizeof(TUPLE*));
  if (!jcd.src) return joinerr(&jcd, 0);
  memcpy(s = jcd.src,       src->tpls, (size_t)src->cnt*sizeof(TUPLE*));
  s[src->cnt] = NULL;           /* copy source tuples to the buffer */
  jcd.cis[1] = jcd.cis[2] = scis;     /* and sort the source tuples */
  ptr_qsort(s, (size_t)src->cnt, +1, (CMPFN*)joincmp, &jcd);
  memcpy(d = s +src->cnt+1, dst->tpls, (size_t)dst->cnt*sizeof(TUPLE*));
  d[dst->cnt] = NULL;           /* copy dest.  tuples to the buffer */
  jcd.cis[1] = jcd.cis[2] = dcis;     /* and sort the dest.  tuples */
  ptr_qsort(d, (size_t)dst->cnt, +1, (CMPFN*)joincmp, &jcd);

  /* --- join tuples --- */
  resvsz = rescnt = 0;          /* initialize the counters */
  sum = 0.0;                    /* and the total tuple weight */
  jcd.cis[2] = scis;            /* prepare for join comparisons */
  while (*d && *s) {            /* while not at end of arrays */
    c = joincmp(*d, *s, &jcd);  /* compare the current tuples */
    if (c < 0) { d++; continue; }  /* and find next pair */
    if (c > 0) { s++; continue; }  /* of joinable tuples */
    r = s;                      /* get the next source tuple */
    do {                        /* tuple join loop */
      if (rescnt >= resvsz) {   /* if the result array is full */
        resvsz += (resvsz > BLKSIZE) ? resvsz >> 1 : BLKSIZE;
        t = (TUPLE**)realloc(jcd.dst, (size_t)resvsz *sizeof(TUPLE*));
        if (!t) return joinerr(&jcd, rescnt);
        jcd.dst = t;            /* resize the result array */
      }                         /* and set the new array */
      tpl = (TUPLE*)malloc(sizeof(TUPLE) +(size_t)(ncr-1)*sizeof(INST));
      if (!tpl) return joinerr(&jcd, rescnt);
      dc = tpl->cols +ncr;      /* create a new tuple and */
      sc = (*r)->cols;          /* copy the source columns */
      for (i = ncs -cnt; --i >= 0; ) *--dc = sc[cis[i]];
      sc = (*d)->cols +ncd;     /* copy the destination columns */
      for (i = ncd;      --i >= 0; ) *--dc = *--sc;
      sum +=    w = (*s)->wgt *(double)(*d)->wgt;
      tpl->xwgt   = tpl->wgt = (WEIGHT)w;
      tpl->mark   = (*d)->mark; /* compute weight of joined tuple */
      tpl->attset = dst->attset;/* and copy other fields from the */
      tpl->table  = dst;        /* dest. tuple to the created one */
      tpl->id     = rescnt;
      jcd.dst[rescnt++] = tpl;  /* insert the created (joined) tuple */
    } while (*++r               /* while more joins are possible */
    &&      (joincmp(*d, *r, &jcd) == 0));
    d++;                        /* go to the next tuple */
  }                             /* in the destination array */
  free(jcd.src); jcd.src = NULL;/* delete the source tuple buffer */

  /* --- replace the destination table --- */
  for (k = ncr -ncd, i = 0; i < k; i++) {
    sa = att_clone(as_att(src->attset, cis[i]));
    if (!sa || (as_attadd(dst->attset, sa) != 0)) {
      as_attcut(NULL, dst->attset, AS_RANGE, ncd, ATTID_MAX);
      return joinerr(&jcd, rescnt);
    }                           /* add the non-join attributes */
  }                             /* to the destination table */
  tab_tplrem(dst, -1);          /* delete all destination tuples and */
  dst->size = resvsz;           /* set the created (joined) tuples */
  dst->cnt  = rescnt;           /* as the new destination tuples */
  dst->tpls = jcd.dst;          /* (replace the tuple array) */
  dst->wgt  = sum;              /* set the total tuple weight */
  joinclean(&jcd);              /* clean up the join data */
  return 0;                     /* return 'ok' */
}  /* tab_join() */

/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void tab_show (const TABLE *tab, TPLID off, TPLID cnt,
               TPL_APPFN showfn, void *data)
{                               /* --- show a table */
  ATTID  i, colcnt;             /* loop variable, number of columns */
  TPLID  n;                     /* number of tuples */
  TUPLE  *const*p;              /* to traverse tuples */
  double wgt = 0.0;             /* sum of tuple weights */

  assert(tab && (off >= 0));    /* check the function arguments */
  if (cnt > (n = tab->cnt -off)) cnt = n;
  assert(cnt >= 0);             /* check and adapt number of tuples */
  colcnt = tab_colcnt(tab);     /* get the number of columns */
  for (i = 0; i < colcnt; i++)  /* and traverse the columns */
    printf("%s ", att_name(tab_col(tab, i)));
  printf(": #\n");              /* print the table header */
  for (p = tab->tpls +off; --cnt >= 0; ) {
    tpl_show(*p, showfn, data); /* show the tuple and */
    wgt += (*p++)->wgt;         /* sum the tuple weights */
  }
  printf("%"TPLID_FMT"/%g tuple(s)\n", tab->cnt, wgt);
}  /* tab_show() */

#endif
/*----------------------------------------------------------------------
  Table Column Functions
----------------------------------------------------------------------*/

int tab_coladdm (TABLE *tab, ATT **atts, ATTID cnt)
{                               /* --- add multiple columns */
  assert(tab && atts);          /* check the function arguments */
  if (expand(tab, 0, (cnt >= 0) ? cnt : -cnt) != 0)
    return -1;                  /* expand the table */
  if (as_attaddm(tab->attset, atts, (cnt >= 0) ? cnt : -cnt) != 0) {
    restore(tab, tab->cnt, 0);  /* add the attributes */
    return -1;                  /* to the attribute set */
  }
  if (cnt < 0)                  /* fill new columns with null values */
    tab_fill(tab, 0, tab->cnt, as_attcnt(tab->attset) +cnt, -cnt);
  return 0;                     /* return 'ok' */
}  /* tab_coladdm() */

/*--------------------------------------------------------------------*/

void tab_colrem (TABLE *tab, ATTID colid)
{                               /* --- remove a column from a table */
  TPLID  i;                     /* loop variable */
  ATTID  cnt;                   /* number of columns (to shift) */
  size_t size;                  /* new tuple size */
  TUPLE  **p;                   /* to traverse the tuples */
  INST   *col;                  /* to traverse the tuple columns */

  assert(tab                    /* check the function arguments */
  &&    (colid >= 0) && (colid < as_attcnt(tab->attset)));
  as_attcut(NULL, tab->attset, AS_RANGE, colid, 1);
  cnt  = as_attcnt(tab->attset);/* remove attribute from att. set */
  size = sizeof(TUPLE) +(size_t)(cnt-1) *sizeof(INST);
  cnt -= colid+1;               /* compute new number of columns */
  for (p = tab->tpls +(i = tab->cnt); --i >= 0; ) {
    col = (*--p)->cols +colid;  /* traverse tuples and shift columns */
    memmove(col, col+1, (size_t)cnt *sizeof(INST));
    *p = realloc(*p, size);     /* (try to) shrink the tuple */
  }                             /* (remove the last column) */
}  /* tab_colrem() */

/*--------------------------------------------------------------------*/

int tab_colconv (TABLE *tab, ATTID colid, int type)
{                               /* --- convert a table column */
  TPLID i;                      /* loop variable */
  int   old;                    /* old attribute type */
  ATT   *att;                   /* attribute to convert */
  TUPLE *tpl;                   /* to traverse the tuples */
  INST  *col;                   /* tuple column to convert */
  INST  *map = NULL;            /* conversion map */
  INST  null;                   /* type specific null value */
  INST  min, max, curr;         /* attribute values */
  char  s[32];                  /* value name for conversion */

  assert(tab                    /* check the function arguments */
  &&    (colid >= 0) && (colid < as_attcnt(tab->attset)));
  att = as_att(tab->attset, colid);
  old = att_type(att);          /* get attribute and note its type */
  if (old == AT_NOM) {          /* if the attribute is nominal */
    map = (INST*)malloc((size_t)att_valcnt(att) *sizeof(INST));
    if (!map) return -1;        /* allocate memory */
  }                             /* for a value map */
  if (att_convert(att, type, map) != 0) {
    if (map) free(map);         /* convert attribute to new type */
    return 1;                   /* on error delete the attribute map */
  }                             /* and abort the function */
  type = att_type(att);         /* get new attribute type */
  if ((old  == AT_INT)          /* if to convert an integer column */
  &&  (type == AT_FLT)) {       /* to a real-valued column */
    for (i = 0; i < tab->cnt; i++) {
      col = tab->tpls[i]->cols +colid;
      col->f = asu_int2flt(col->i);
    }                           /* convert the column values */
    return 0;                   /* return 'ok' */
  }
  if ((old  == AT_FLT)          /* if to convert a real-valued column */
  &&  (type == AT_INT)) {       /* to an integer column */
    for (i = 0; i < tab->cnt; i++) {
      col = tab->tpls[i]->cols +colid;
      col->i = asu_flt2int(col->f);
    }                           /* convert the column values */
    return 0;                   /* return 'ok' */
  }
  if (old == AT_NOM) {          /* if to convert nominal to numeric */
    if (type == AT_FLT) null.f = NV_FLT;  /* set the instance */
    else                null.i = NV_INT;  /* to a null value */
    for (i = 0; i < tab->cnt; i++) {
      col = tab->tpls[i]->cols +colid;
      *col = isnone(col->n) ? null : map[col->i];
    }                           /* convert the column value */
    free(map); return 0;        /* delete the value map */
  }                             /* and return 'ok' */
  min  = *att_valmin(att);      /* note the minimal */
  max  = *att_valmax(att);      /* and the maximal value */
  curr = *att_inst(att);        /* and the instance (current value) */
  if (old == AT_INT) {          /* if to convert integer to nominal */
    for (i = 0; i < tab->cnt; i++) {
      tpl = tab->tpls[i];       /* traverse the tuples */
      col = tpl->cols +colid;   /* get the column to change */
      if (isnull(col->i)) {     /* skip null values */
        tpl->id = (TPLID)NV_NOM; continue; }
      sprintf(s, "%"DTINT_FMT, col->i); /* format integer value */
      if (att_valadd(att, s, NULL) < 0) break;
      tpl->id = (TPLID)att_inst(att)->n;
    } }                         /* add the value and note its ident. */
  else {                        /* if to convert real to nominal */
    for (i = 0; i < tab->cnt; i++) {
      tpl = tab->tpls[i];       /* traverse the tuples */
      col = tpl->cols +colid;   /* get the column to change */
      if (isnan(col->f)) {      /* skip null values */
        tpl->id = (TPLID)NV_NOM; continue; }
      sprintf(s, "%.*"DTFLT_FMT, att_getsd2p(att), col->f);
      if (att_valadd(att, s, NULL) < 0) break;
      tpl->id = (TPLID)att_inst(att)->n;
    }                           /* add the value and note its ident. */
  }
  if (i < tab->cnt) {           /* if an error occurred */
    while (--i >= 0) tab->tpls[i]->id = i;
    att_convert(att, old, NULL);/* restore the tuple identifiers, */
    att_valadd(att, NULL, &min);/* the old attribute type, */
    att_valadd(att, NULL, &max);/* the minimal and maximal value, */
    *att_inst(att) = curr;      /* and the instance (current value) */
    return -1;                  /* return an error code */
  }
  for (i = 0; i < tab->cnt; i++) {
    tpl = tab->tpls[i];         /* traverse the tuples again */
    tpl->cols[colid].n = (VALID)tpl->id;
    tpl->id = i;                /* set the value identifier and */
  }                             /* restore the tuple identifier */
  return 0;                     /* return 'ok' */
}  /* tab_colconv() */

/*--------------------------------------------------------------------*/

void tab_colnorm (TABLE *tab, ATTID colid, double exp, double sdev,
                  double *pscl, double *poff)
{                               /* --- convert a table column */
  TPLID  i;                     /* loop variable */
  TUPLE  *tpl;                  /* to traverse the tuples */
  INST   *col;                  /* tuple column to convert */
  double min, max;              /* minimum and maximum value */
  double sum, sqr;              /* sum of values/squared values */
  double scl, off;              /* scaling factor and offset */
  double cnt;                   /* sum of tuple weights */

  assert(tab                    /* check the function arguments */
  &&    (colid >= 0) && (colid < as_attcnt(tab->attset))
  &&    (att_type(as_att(tab->attset, colid)) == AT_FLT));
  if (sdev < 0) {               /* if minimum and range are given */
    min =  INFINITY;            /* clear minimum and maximum */
    max = -INFINITY;            /* and traverse the tuples */
    for (i = 0; i < tab->cnt; i++) {
      col = tab->tpls[i]->cols +colid;
      if (isnan(col->f)) continue;   /* skip null values */
      if ((double)col->f < min) min = (double)col->f;
      if ((double)col->f > max) max = (double)col->f;
    }                           /* determine range of values */
    if (min > max) {            /* if no values found, */
      off = 0; scl = 1; }       /* set the identity scaling */
    else {                      /* if there are values in the column */
      scl = (max > min) ? sdev /(min -max) : 1;
      off = exp -min *scl;      /* scaling factor to new range and */
    } }                         /* offset to new minimum value */
  else {                        /* if exp. value and std. dev. given */
    sum = sqr = cnt = 0;        /* clear the sums and tuple weight */
    for (i = 0; i < tab->cnt; i++) {
      tpl = tab->tpls[i];       /* traverse the tuples */
      col = tpl->cols +colid;   /* get the column to normalize */
      if (isnan(col->f)) continue; /* skip null values */
      sqr += (double)col->f *(double)col->f;
      sum += (double)col->f;    /* sum the column values */
      cnt += (double)tpl->wgt;  /* and their squares */
    }                           /* and the tuple weights */
    if (cnt <= 0) {             /* if no values found, */
      off = 0; scl = 1; }       /* set the identity scaling */
    else {                      /* if there are values in the column */
      off = sum /cnt;           /* compute exp. value and std. dev. */
      scl = sqrt((sqr -off *sum) /cnt);
      scl = (scl > 0) ? sdev/scl : 1;
      off = exp -off *scl;      /* compute scaling factor and offset */
    }                           /* for a linear transformation */
  }                             /* of the column values */
  if (poff) *poff = off;        /* if requested, return the offset */
  if (pscl) *pscl = scl;        /* and the scaling factor */
  for (i = 0; i < tab->cnt; i++) { /* traverse the tuples */
    col = tab->tpls[i]->cols +colid;
    if (!isnan(col->f)) col->f = (DTFLT)(scl *(double)col->f +off);
  }                             /* scale the column values */
}  /* tab_colnorm() */

/*--------------------------------------------------------------------*/

void tab_coltlin (TABLE *tab, ATTID colid, double scl, double off)
{                               /* --- transform a column linearly */
  TPLID i;                      /* loop variable */
  INST  *col;                   /* tuple column to convert */

  assert(tab                    /* check the function arguments */
  &&    (colid >= 0) && (colid < as_attcnt(tab->attset))
  &&    (att_type(as_att(tab->attset, colid)) == AT_FLT));
  for (i = 0; i < tab->cnt; i++) { /* traverse the tuples */
    col = tab->tpls[i]->cols +colid;
    if (!isnan(col->f)) col->f = (DTFLT)(scl *col->f +off);
  }                             /* scale the column values */
}  /* tab_coltlin() */

/*--------------------------------------------------------------------*/

void tab_colexg (TABLE *tab, ATTID colid1, ATTID colid2)
{                               /* --- exchange two table columns */
  TPLID i;                      /* loop variable */
  INST  *cols, t;               /* column array and exchange buffer */

  assert(tab                    /* check the function arguments */
  &&    (colid1 >= 0) && (colid1 < as_attcnt(tab->attset))
  &&    (colid2 >= 0) && (colid2 < as_attcnt(tab->attset)));
  as_attexg(tab->attset, colid1, colid2); /* exchange attributes */
  for (i = 0; i < tab->cnt; i++) {
    cols = tab->tpls[i]->cols;  /* traverse the tuples */
    t = cols[colid1]; cols[colid1] = cols[colid2]; cols[colid2] = t;
  }                             /* exchange the column values */
}  /* tab_colexg() */

/*--------------------------------------------------------------------*/

void tab_colmove (TABLE *tab, ATTID off, ATTID cnt, ATTID pos)
{                               /* --- move table columns */
  TPLID i;                      /* loop variable for tuples */
  ATTID k;                      /* loop variable for attributes */

  assert(tab);                  /* check function arguments */
  k = as_attcnt(tab->attset);   /* check and adapt the insert pos. */
  if (pos > k)      pos = k;    /* and the number of columns */
  if (cnt > k -off) cnt = k -off;
  assert((cnt >= 0) &&  (off >= 0)
  &&     (pos >= 0) && ((pos <= off) || (pos >= off +cnt)));
  for (i = 0; i < tab->cnt; i++)/* move columns in tuples */
    obj_move(tab->tpls[i]->cols, (size_t)off, (size_t)cnt,
             (size_t)pos, sizeof(INST));
  as_attmove(tab->attset, off, cnt, pos);
}  /* tab_colmove() */          /* afterwards move attributes */

/*--------------------------------------------------------------------*/

void tab_colperm (TABLE *tab, ATTID *perm)
{                               /* --- permute table columns */
  ATTID  k, n;                  /* loop variables for attributes */
  TPLID  i;                     /* loop variable  for tuples */
  TUPLE  *tpl;                  /* to traverse the tuples */
  size_t z;                     /* size of the column array */
  INST   *buf;                  /* buffer for permutation */

  assert(tab && perm);          /* check the function arguments */
  n = tab_attcnt(tab);          /* get the number of columns and */
  z = (size_t)n *sizeof(INST);  /* the size of the column vector */
  buf = tab->buf->cols;         /* get the column buffer */
  for (i = 0; i < tab->cnt; i++) {
    tpl = tab->tpls[i];         /* traverse the tuples and */
    memcpy(buf, tpl->cols, z);  /* permute columns in tuples */
    for (k = 0; k < n; k++) tpl->cols[k] = buf[perm[k]];
  }
  as_attperm(tab->attset,perm); /* afterwards permute attributes */
}  /* tab_colperm() */

/*--------------------------------------------------------------------*/

int tab_colcut (TABLE *dst, TABLE *src, int mode, ...)
{                               /* --- cut some table columns */
  int     r;                    /* result of functions */
  ATTID   i, k, m = 0;          /* loop variables, buffers */
  TPLID   n;                    /* loop variable */
  TPLID   old = 0;              /* old number of tuples in dest. */
  TPLID   add = 0;              /* number of tuples added to dest. */
  ATTID   off, cnt, end, rem;   /* range of columns */
  TUPLE   **s, **d;             /* to traverse the tuples */
  va_list args;                 /* list of variable arguments */

  assert(src);                  /* check the function arguments */

  /* --- get range of columns --- */
  if (mode & TAB_RANGE) {       /* if an index range is given, */
    va_start(args, mode);       /* start variable argument evaluation */
    off = va_arg(args, ATTID);  /* get the offset to the first column */
    cnt = va_arg(args, ATTID);  /* and the number of columns */
    va_end(args);               /* end variable argument evaluation */
    i = as_attcnt(src->attset) -off;
    if (cnt > i) cnt = i; }     /* check and adapt number of tuples */
  else {                        /* if no index range is given */
    cnt = as_attcnt(src->attset);
    off = 0;                    /* get the full index range */
  }                             /* afterward check range params. */
  assert((off >= 0) && (cnt >= 0));
  end = off +cnt;               /* get end index of column range and */
  rem = as_attcnt(src->attset) -end;        /* the remaining columns */

  /* --- prepare destination --- */
  if (mode & TAB_MARKED) {      /* if to cut marked columns, */
    for (k = off; k < end; k++) /* traverse the range of columns */
      if (att_getmark(as_att(src->attset, k)) < 0) cnt--;
  }                             /* determine number of marked columns */
  if (dst) {                    /* if there is a destination table */
    m = as_attcnt(dst->attset); /* note the old number of attributes */
    old = dst->cnt;             /* and the number of tuples in dest. */
    add = src->cnt -old;        /* and get the number of add. tuples */
    if (expand(dst, add, cnt) != 0) return -1;
  }                             /* expand the destination table */

  /* --- cut source columns --- */
  r = as_attcut((dst) ? dst->attset : NULL, src->attset,
                AS_RANGE|mode, off, end-off);
  if (r != 0) {                 /* copy attributes to destination */
    if (dst) restore(dst, old, dst->cnt-old);
    return -1;                  /* on error restore old state */
  }                             /* and abort the function */
  if (mode & TAB_MARKED) {      /* if to cut marked columns, */
    for (k = off; k < end; k++){/* traverse the range of columns */
      if (att_getmark(as_att(src->attset, k)) >= 0) {
        if (!dst) continue;     /* if there is no dest., skip column */
        d = dst->tpls +src->cnt;/* otherwise traverse the tuples */
        for (s = src->tpls +(n = src->cnt); --n >= 0; )
          (*--d)->cols[m] = (*--s)->cols[k];
        m++; }                  /* copy marked columns to the dest. */
      else {                    /* if a column is not marked/selected */
        for (s = src->tpls +(n = src->cnt); --n >= 0; ) {
          --s; (*s)->cols[off] = (*s)->cols[k]; }
        off++;                  /* shift the unmarked columns */
      }                         /* to close the gap in the tuple */
    } }                         /* left by the cut columns */
  else {                        /* if to cut a range of columns */
    if (dst) {                  /* if there is a destination table, */
      s = src->tpls +src->cnt;  /* traverse the tuples and the */
      d = dst->tpls +src->cnt;  /* column range of each tuple */
      for (n = src->cnt; --n >= 0; )
        memcpy ((*--d)->cols +m, (*--s)->cols +off,
                (size_t)cnt *sizeof(INST));
    }                           /* copy the source columns to dest. */
    if (rem > 0) {              /* if columns remain in source, */
      s = src->tpls;            /* traverse the source tuples */
      for (n = src->cnt; --n >= 0; s++)
        memmove((*s)->cols +off, (*s)->cols +end,
                (size_t)rem *sizeof(INST));
    }                           /* copy remaining columns to close */
  }                             /* the gap left by the cut columns */
  if (!dst) return 0;           /* if there is no dest. table, abort */

  /* --- fill uncopied table section --- */
  m = as_attcnt(dst->attset) -cnt;
  if (add > 0) {                /* if tuples were added, */
    s = src->tpls +src->cnt;    /* copy the weights of these tuples */
    d = dst->tpls +src->cnt;    /* and fill uncopied table section */
    for (n = add; --n >= 0; ) (*--d)->wgt = (*--s)->wgt;
    tab_fill(dst, old,       add, 0, m); }
  else                          /* if no tuples were added */
    tab_fill(dst, src->cnt, -add, m, cnt);
  return 0;                     /* fill uncopied table section */
}  /* tab_colcut() */           /* and return 'ok' */

/*--------------------------------------------------------------------*/

int tab_colcopy (TABLE *dst, const TABLE *src, int mode, ...)
{                               /* --- copy some table columns */
  va_list args;                 /* list of variable arguments */
  ATTID   i, k, m;              /* loop variables, buffers */
  TPLID   n;                    /* loop variable */
  TPLID   old;                  /* old number of tuples in dest. */
  TPLID   add;                  /* number of tuples to add to dest. */
  ATTID   off, cnt, end;        /* range of columns */
  TUPLE   *const*s, **d;        /* to traverse the tuples */

  assert(src && dst);           /* check the function arguments */

  /* --- get range of columns --- */
  if (mode & TAB_RANGE) {       /* if an index range is given, */
    va_start(args, mode);       /* start variable argument evaluation */
    off = va_arg(args, ATTID);  /* get the offset to the first column */
    cnt = va_arg(args, ATTID);  /* and the number of columns */
    va_end(args);               /* end variable argument evaluation */
    i = as_attcnt(src->attset) -off;
    if (cnt > i) cnt = i; }     /* check and adapt number of tuples */
  else {                        /* if no index range is given */
    cnt = as_attcnt(src->attset);
    off = 0;                    /* get the full index range */
  }                             /* afterward check range params. */
  assert((off >= 0) && (cnt >= 0));
  end = off +cnt;               /* get end index of column range */

  /* --- prepare destination --- */
  if (mode & TAB_MARKED) {      /* if to cut marked columns, */
    for (k = off; k < end; k++) /* traverse the range of columns */
      if (att_getmark(as_att(src->attset, k)) < 0) cnt--;
  }                             /* determine number of marked columns */
  old = dst->cnt;               /* note the number of tuples in dest. */
  add = src->cnt -old;          /* and get the number of add. tuples */
  if (expand(dst, add, cnt) != 0)
    return -1;                  /* expand the destination table */

  /* --- copy source columns --- */
  m = as_attcnt(dst->attset);   /* note the old number of attributes */
  i = as_attcopy(dst->attset,   /* and copy the attributes */
                 src->attset, AS_RANGE|mode, off, end-off);
  if (i != 0) { restore(dst, old, src->cnt -old); return -1; }
  if (mode & TAB_MARKED) {      /* if to copy marked columns, */
    for (k = off; k < end; k++){/* traverse the range of columns */
      if (att_getmark(as_att(src->attset, k)) >= 0) {
        s = src->tpls +src->cnt;  /* if the column */
        d = dst->tpls +src->cnt;  /* is marked */
        for (n = src->cnt; --n >= 0; )
          (*--d)->cols[m] = (*--s)->cols[k];
        m++;                    /* traverse the source columns */
      }                         /* and copy the marked columns */
    } }                         /* to the destination table */
  else {                        /* if to copy a range of columns */
    s = src->tpls;              /* traverse the tuples and the */
    d = dst->tpls;              /* column range of each tuple */
    for (n = src->cnt; --n >= 0; s++, d++)
      memcpy((*s)->cols +off, (*d)->cols +m, (size_t)cnt *sizeof(INST));
  }                             /* copy the source columns */

  /* --- fill uncopied table section --- */
  m = as_attcnt(dst->attset) -cnt;
  if (add > 0) {                /* if tuples were added, */
    s = src->tpls +src->cnt;    /* copy the weights of these tuples */
    d = dst->tpls +src->cnt;    /* and fill uncopied table section */
    for (n = add; --n >= 0; ) (*--d)->wgt = (*--s)->wgt;
    tab_fill(dst, old,       add, 0, m); }
  else                          /* if no tuples were added */
    tab_fill(dst, src->cnt, -add, m, cnt);
  return 0;                     /* fill uncopied table section */
}  /* tab_colcopy() */          /* and return 'ok' */

/*----------------------------------------------------------------------
  Table Tuple Functions
----------------------------------------------------------------------*/

int tab_tpladd (TABLE *tab, TUPLE *tpl)
{                               /* --- add one tuple to a table */
  assert(tab);                  /* check the function argument */
  if (tab_resize(tab, tab->cnt +1) < 0)
    return -1;                  /* resize the tuple array */
  if (tpl) {                    /* if a tuple is given, */
    if (tpl->table)             /* remove it from the old table */
      tab_tplrem(tpl->table, tpl->id); }
  else {                        /* if no tuple is given */
    tpl = tpl_create(tab->attset, 1);
    if (!tpl) return -1;        /* create a tuple from the */
  }                             /* underlying attribute set */
  tpl->table = tab;             /* set the table reference */
  tpl->id    = tab->cnt;        /* and the tuple identifier */
  tab->tpls[tab->cnt++] = tpl;  /* insert the tuple into the table */
  tab->wgt  += tpl->wgt;        /* sum the tuple weight */
  return 0;                     /* return 'ok' */
}  /* tab_tpladd() */

/*--------------------------------------------------------------------*/

int tab_tpladdm (TABLE *tab, TUPLE **tpls, TPLID cnt)
{                               /* --- add several tuples */
  TUPLE *tpl;                   /* to traverse new tuples */

  assert(tab && ((cnt >= 0) || !tpls));  /* check function arguments */

  /* --- add empty tuples --- */
  if (!tpls) {                  /* if no tuples are given */
    if (expand(tab, (cnt >= 0) ? cnt : -cnt, 0) != 0) return -1;
    if (cnt < 0)                /* expand the table by 'cnt' tuples */
      tab_fill(tab, tab->cnt +cnt,-cnt, 0, as_attcnt(tab->attset));
    return 0;                   /* fill the tuples with null values */
  }                             /* and abort the function */

  /* --- add given tuples --- */
  if (tab_resize(tab, tab->cnt +cnt) < 0)
    return -1;                  /* resize the tuple array */
  while (--cnt >= 0) {          /* traverse the new tuples */
    tpl = *tpls++;              /* get the next tuple and */
    if (tpl->table)             /* remove it from old table */
      tab_tplrem(tpl->table, tpl->id);
    tpl->table = tab;           /* set the table reference */
    tpl->id    = tab->cnt;      /* and the tuple identifier */
    tab->tpls[tab->cnt++] = tpl;
    tab->wgt  += tpl->wgt;      /* insert the tuple into the table */
  }                             /* and sum the tuple weight */
  return 0;                     /* return 'ok' */
}  /* tab_tpladdm() */

/*--------------------------------------------------------------------*/

TUPLE* tab_tplrem (TABLE *tab, TPLID tplid)
{                               /* --- remove a tuple from a table */
  TUPLE **p;                    /* to traverse the tuples */
  TUPLE *tpl;                   /* removed tuple */

  assert(tab && (tplid < tab->cnt));  /* check function arguments */

  /* --- remove all tuples --- */
  if (tplid < 0) {              /* if no tuple identifier given */
    if (!tab->tpls) return NULL;/* if there are no tuples, abort */
    for (p = tab->tpls +(tplid = tab->cnt); --tplid >= 0; ) {
      (*--p)->table = NULL; (*p)->id = -1; tab->delfn(*p); }
    free(tab->tpls);            /* delete all tuples */
    tab->tpls = NULL;           /* and the tuple array */
    tab->cnt  = 0;              /* clear the tuple counter */
    tab->wgt  = 0.0;            /* and the total tuple weight */
    return NULL;                /* abort the function */
  }

  /* --- remove one tuple --- */
  tpl = tab->tpls[tplid];       /* get the tuple to remove */
  p   = tab->tpls +tplid;       /* traverse the remaining tuples */
  for (tplid = --tab->cnt -tplid; --tplid >= 0; ) {
    *p = p[1]; (*p++)->id--; }  /* shift tuples and adapt identifiers */
  tpl->table = NULL;            /* clear the table reference */
  tpl->id    = -1;              /* and the tuple identifier */
  tab->wgt  -= (double)tpl->wgt;/* subtract the tuple weight */
  tab_resize(tab, 0);           /* try to shrink the tuple array */
  return tpl;                   /* return the removed tuple */
}  /* tab_tplrem() */

/*--------------------------------------------------------------------*/

void tab_tplexg (TABLE *tab, TPLID tplid1, TPLID tplid2)
{                               /* --- exchange two tuples */
  TUPLE **p, **q, *tpl;         /* tuples to exchange, buffer */

  assert(tab                    /* check the function arguments */
  &&    (tplid1 >= 0) && (tplid1 < tab->cnt)
  &&    (tplid2 >= 0) && (tplid2 < tab->cnt));
  p  = tab->tpls +tplid1; tpl = *p;
  q  = tab->tpls +tplid2;      /* get pointers to the tuples, */
  *p = *q; (*p)->id = tplid1;  /* exchange them, and */
  *q = tpl; tpl->id = tplid2;  /* set the new identifiers */
}  /* tab_tplexg() */

/*--------------------------------------------------------------------*/

void tab_tplmove (TABLE *tab, TPLID off, TPLID cnt, TPLID pos)
{                               /* --- move tuples */
  assert(tab);                  /* check for a valid table */
  if (pos > tab->cnt)      pos = tab->cnt;  /* check and adapt */
  if (off > tab->cnt)      off = tab->cnt;  /* parameters */
  if (cnt > tab->cnt -off) cnt = tab->cnt -off;
  assert((cnt >= 0) &&  (off >= 0)
  &&     (pos >= 0) && ((pos <= off) || (pos >= off +cnt)));
  ptr_move(tab->tpls, (size_t)off, (size_t)cnt, (size_t)pos);
  if (pos <= off) {             /* move the tuples in the array */
    cnt += off; off = pos; pos = cnt; }
  for ( ; off < pos; off++)     /* traverse affected tuples and */
    tab->tpls[off]->id = off;   /* set their new identifiers */
}  /* tab_tplmove() */

/*--------------------------------------------------------------------*/

int tab_tplcut (TABLE *dst, TABLE *src, int mode, ...)
{                               /* --- cut some tuples */
  TPLID   n;                    /* loop variable, buffer */
  TPLID   off, cnt;             /* range of tuples */
  TUPLE   *tpl;                 /* to traverse the tuples */
  va_list args;                 /* list of variable arguments */

  assert(src);                  /* check the function arguments */

  /* --- get range of tuples --- */
  if (!(mode & TAB_RANGE)) {    /* if no index range is given, */
    off = 0; cnt = src->cnt; }  /* get the full index range */
  else {                        /* if an index range is given, */
    va_start(args, mode);       /* start variable argument evaluation */
    off = va_arg(args, TPLID);  /* get the offset to the first tuple */
    cnt = va_arg(args, TPLID);  /* and the number of tuples */
    va_end(args);               /* end variable argument evaluation */
    n = src->cnt -off;          /* check and adapt */
    if (cnt > n) cnt = n;       /* the number of tuples */
    assert((off >= 0) && (cnt >= 0));
  }                             /* check the range parameters */
  if (cnt <= 0) return 0;       /* if the range is empty, abort */
  if (dst && (tab_resize(dst, dst->cnt +cnt) < 0))
    return -1;                  /* resize the dest. tuple array */
  cnt += off;                   /* get end index of tuple range */

  /* --- cut source tuples --- */
  for (n = off; off < cnt; off++) {
    tpl = src->tpls[off];       /* traverse the range of tuples */
    if ((mode & TAB_MARKED)     /* if in marked mode and the tuple */
    &&  (tpl->mark < 0)) {      /* is not marked, merely shift it */
      src->tpls[tpl->id = n++] = tpl; continue; }
    src->wgt  -= tpl->wgt;      /* subtract the tuple weight */
    tpl->table = dst;           /* set/clear the table reference */
    if (!dst) {                 /* if no dest., simply delete tuple */
      tpl->id = -1; src->delfn(tpl); continue; }
    dst->tpls[tpl->id = dst->cnt++] = tpl;
    dst->wgt += tpl->wgt;       /* store tuple in the destination */
  }                             /* and sum the tuple weight */
  while (off < src->cnt) {      /* traverse the remaining tuples */
    src->tpls[n] = tpl = src->tpls[off++];
    tpl->id = n++;              /* shift tuples left/down and */
  }                             /* set their new identifiers */
  src->cnt = n;                 /* set the new number of tuples */
  tab_resize(src, 0);           /* try to shrink the tuple array */
  if (dst) tab_resize(dst, 0);  /* of the source and the destination */
  return 0;                     /* return 'ok' */
}  /* tab_tplcut() */

/*--------------------------------------------------------------------*/

int tab_tplcopy (TABLE *dst, const TABLE *src, int mode, ...)
{                               /* --- copy some tuples */
  TPLID   n;                    /* loop variables */
  TPLID   off, cnt;             /* range of tuples */
  TUPLE   *tpl, **d;            /* to traverse the tuples */
  size_t  z;                    /* size of the tuples */
  double  sum;                  /* sum of the tuple weights */
  va_list args;                 /* list of variable arguments */

  assert(src && dst);           /* check the function arguments */

  /* --- get range of tuples --- */
  if (!(mode & TAB_RANGE)) {    /* if no index range is given, */
    off = 0; cnt = src->cnt; }  /* get the full index range */
  else {                        /* if an index range is given, */
    va_start(args, mode);       /* start variable argument evaluation */
    off = va_arg(args, TPLID);  /* get the offset to the first tuple */
    cnt = va_arg(args, TPLID);  /* and the number of tuples */
    va_end(args);               /* end variable argument evaluation */
    n = src->cnt -off;          /* check and adapt */
    if (cnt > n) cnt = n;       /* the number of tuples */
    assert((off >= 0) && (cnt >= 0));
  }                             /* check the range parameters */
  if (cnt <= 0) return 0;       /* if the range is empty, abort */
  if (tab_resize(dst, dst->cnt +cnt) < 0)
    return -1;                  /* resize the dest. tuple array */
  cnt += off;                   /* get end index of tuple range */

  /* --- copy source tuples --- */
  z = sizeof(TUPLE) +(size_t)(as_attcnt(src->attset)-1) *sizeof(INST);
  d = dst->tpls +(n = dst->cnt);/* get tuple size and destination */
  for (sum = 0; off < cnt; off++) {
    tpl = src->tpls[off];       /* traverse the range of tuples */
    if ((mode & TAB_MARKED)     /* if in marked mode */
    &&  (tpl->mark < 0))        /* and the tuple is not marked */
      continue;                 /* skip this tuple */
    sum += tpl->wgt;            /* sum the tuple weights */
    *d = (TUPLE*)malloc(z);     /* create a new tuple and */
    if (!*d) break;             /* store it in the destination */
    memcpy(*d, tpl, z);         /* copy the source tuple into it */
    (*d)->table = dst;          /* set the table reference */
    (*d++)->id  = n++;          /* and the tuple identifier */
  }
  if (off < cnt) {              /* if an error occurred */
    while (--n >= dst->cnt) free(*--d);
    return -1;                  /* delete all copied tuples */
  }                             /* and abort the function */
  dst->cnt  = n;                /* set the new number of tuples */
  dst->wgt += sum;              /* and the new tuple weight */
  tab_resize(dst, 0);           /* try to shrink the tuple array */
  return 0;                     /* return 'ok' */
}  /* tab_tplcopy() */
