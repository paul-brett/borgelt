/*----------------------------------------------------------------------
  File    : table3.c
  Contents: tuple and table management, one point coverage functions
  Author  : Christian Borgelt
  History : 1996.03.06 file created
            1998.09.25 first step of major redesign completed
            1999.02.04 long int changed to int (assume 32 bit system)
            1999.03.15 one point coverage functions transf. from opc.c
            1999.03.17 one point coverage functions redesigned
            2001.06.24 table module split into two files
            2001.07.11 bug in function opc_cond() removed
            2003.01.16 tpl_compat(), tab_poss(), tab_possx() added
            2010.12.30 renamed to table3.c (table2.c replaces io.c)
            2013.07.31 adapted to definitions ATTID, VALID, TPLID etc.
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <assert.h>
#include "table.h"
#ifdef STORAGE
#include "storage.h"
#endif

extern int tab_resize (TABLE *tab, TPLID size);
/* tab_resize() is defined in table1.c */

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define BLKSIZE    256          /* tuple array block size */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- one point coverage data --- */
  TABLE  *table;                /* table to work on */
  TUPLE  **buf;                 /* tuple buffer */
  TUPLE  **htab;                /* hash table */
  TUPLE  *curr;                 /* current tuples */
  ATTID  *cids;                 /* column identifiers */
  WEIGHT *wgts;                 /* weights buffer */
} OPCDATA;                      /* (one point coverage data) */

/*----------------------------------------------------------------------
  Tuple Functions
----------------------------------------------------------------------*/

int tpl_isect (TUPLE *res, TUPLE *tpl1, const TUPLE *tpl2)
{                               /* --- compute a tuple intersection */
  ATTID k;                      /* loop variable */
  ATT   *att;                   /* to traverse the attributes */
  INST  *inst;                  /* to traverse the instances */
  CINST *col1, *col2;           /* to traverse the tuple columns */
  int   type;                   /* type of current column */
  int   ident = 1 | 2;          /* 'identical to intersection' flags */

  assert(tpl1 && tpl2           /* check the function arguments */
  &&    (as_attcnt(tpl1->attset) == as_attcnt(tpl2->attset)));
  for (k = as_attcnt(tpl1->attset); --k >= 0; ) {
    att  = as_att(tpl1->attset, k);     /* traverse the columns */
    col1 = tpl1->cols +k;       /* get the column attribute and */
    col2 = tpl2->cols +k;       /* the two source columns */
    inst = (res) ? res->cols +k : att_inst(att);
    type = att_type(att);       /* get instance and attribute type */
    if      (type == AT_FLT) {  /* if float column */
      if (col1->f == col2->f)   /* if the values are identical, */
        inst->f = col1->f;      /* just copy any of them */
      else if (isnan(col1->f)){ /* if first value is null, */
        inst->f = col2->f;      /* copy second value and */
        ident &= ~1; }          /* clear first tuple flag */
      else if (isnan(col2->f)){ /* if second value is null, */
        inst->f = col1->f;      /* copy first value and */
        ident &= ~2; }          /* clear second tuple flag */
      else return -1; }         /* if columns differ, abort */
    else if (type == AT_INT) {  /* if integer column */
      if (col1->i == col2->i)   /* if values are identical, */
        inst->i = col1->i;      /* just copy any of them */
      else if (isnull(col1->i)){/* if first value is null, */
        inst->i = col2->i;      /* copy second value and */
        ident &= ~1; }          /* clear first tuple flag */
      else if (isnull(col2->i)){/* if second value is null, */
        inst->i = col1->i;      /* copy first value and */
        ident &= ~2; }          /* clear second tuple flag */
      else return -1; }         /* if columns differ, abort */
    else {                      /* if nominal column */
      if (col1->n == col2->n)   /* if values are identical, */
        inst->n = col1->n;      /* just copy any of them */
      else if (isnone(col1->n)){/* if first value is null, */
        inst->n = col2->n;      /* copy second value and */
        ident &= ~1; }          /* clear first tuple flag */
      else if (isnone(col2->n)){/* if second value is null, */
        inst->n = col1->n;      /* copy first value and */
        ident &= ~2; }          /* clear second tuple flag */
      else return -1;           /* if columns differ, abort */
    }                           /* (ident indicates which tuple is */
  }                             /* identical to their intersection) */
  return ident;                 /* return 'identical to isect.' flags */
}  /* tpl_isect() */

/*----------------------------------------------------------------------
Result of tpl_isect:
bit 0 is set: tpl1 is identical to intersection
bit 1 is set: tpl2 is identical to intersection
----------------------------------------------------------------------*/

int tpl_compat (const TUPLE *tpl1, const TUPLE *tpl2)
{                               /* --- check tuples for compatibility */
  ATTID k;                      /* loop variable */
  ATT   *att;                   /* to traverse the attributes */
  DTFLT f1, f2;                 /* float   values */
  DTINT i1, i2;                 /* integer values */
  VALID n1, n2;                 /* identifiers of attribute values */

  if (!tpl1) { tpl1 = tpl2; tpl2 = NULL; }
  if (!tpl1) return -1;         /* check for at least one tuple */
  for (k = as_attcnt(tpl1->attset); --k >= 0; ) {
    att = as_att(tpl1->attset, k);    /* traverse the columns and */
    switch (att_type(att)) {    /* get the attribute and its type */
      case AT_FLT:              /* if float column */
        f1 = tpl1->cols[k].f;   /* check for different known values */
        f2 = (tpl2) ? tpl2->cols[k].f : att_inst(att)->f;
        if ((f1 != f2) && !isnan(f1)  && !isnan(f2))  return 0;
        break;
      case AT_INT:              /* if integer column */
        i1 = tpl1->cols[k].i;   /* check for different known values */
        i2 = (tpl2) ? tpl2->cols[k].i : att_inst(att)->i;
        if ((i1 != i2) && !isnull(i1) && !isnull(i2)) return 0;
        break;
      default:                  /* if nominal column */
        n1 = tpl1->cols[k].n;   /* check for different known values */
        n2 = (tpl2) ? tpl2->cols[k].n : att_inst(att)->n;
        if ((n1 != n2) && !isnone(n1) && !isnone(n2)) return 0;
        break;                  /* if different known values exist, */
    }                           /* return 'tuples are incompatible' */
  }
  return -1;                    /* return 'tuples are compatible' */
}  /* tpl_compat() */

/*--------------------------------------------------------------------*/

ATTID tpl_nullcnt (const TUPLE *tpl)
{                               /* --- count null values */
  ATTID k;                      /* loop variable */
  ATTID n;                      /* number of null columns */
  CINST *col;                   /* to traverse the columns */

  assert(tpl);                  /* check the function argument */
  for (n = 0, k = as_attcnt(tpl->attset); --k >= 0; ) {
    col = tpl->cols +k;         /* traverse the tuple columns */
    switch (att_type(as_att(tpl->attset, k))) {
      case AT_FLT: if (isnan (col->f)) n++; break;
      case AT_INT: if (isnull(col->i)) n++; break;
      default    : if (isnone(col->n)) n++; break;
    }                           /* if a column is null/unknown, */
  }                             /* increment the column counter */
  return n;                     /* return number of null columns */
}  /* tpl_nullcnt() */

/*----------------------------------------------------------------------
  Table Functions
----------------------------------------------------------------------*/

static int htab (OPCDATA *opc, int init)
{                               /* --- create/resize hash table */
  TPLID i;                      /* loop variable */
  TABLE *tab = opc->table;      /* table to work on */
  TUPLE *tpl, **h;              /* to traverse tuples/hash buckets */

  assert(opc && tab);           /* check the function arguments */
  if (opc->htab) free(opc->htab);      /* delete old hash table */
  opc->htab = (TUPLE**)calloc((size_t)tab->size, sizeof(TUPLE*));
  if (!opc->htab) return -1;    /* allocate a new hash table */
  for (i = 0; i < tab->cnt; i++) {
    tpl = tab->tpls[i];         /* traverse the tuples in the table */
    if (init) tpl->id = (TPLID)tpl_hash(tpl);
    h = opc->htab +(size_t)tpl->id % (size_t)tab->size;
    tpl->table = (TABLE*)*h;    /* compute the hash value and get */
    *h = tpl;                   /* the corresponding bucket, then */
  }                             /* insert the tuple into the bucket */
  return 0;                     /* return 'ok' */
}  /* htab() */

/*----------------------------------------------------------------------
The one point coverage functions below use a hash table to find tuples
quickly. This hash table is stored in the field tab->htab and is created
and resized with the above function. Within a hash bucket the tuples are
chained together with the (abused) tpl->table pointers. For a faster
reorganization of the hash table, the hash value of a tuple is stored in
the tuple's (abused) tpl->id field, so that it need not be recomputed.
----------------------------------------------------------------------*/

static int opcerr (OPCDATA *opc, TPLID n, int restore)
{                               /* --- clean up after o.p.c. error */
  TPLID i;                      /* loop variable */
  TABLE *tab = opc->table;      /* table to work on */
  TUPLE *tpl;                   /* to traverse the tuples */

  assert(opc && tab);           /* check the function arguments */
  while (--n >= tab->cnt) free(tab->tpls[n]);
  tab_resize(tab, 0);           /* delete the added tuples */
  if (opc->htab) {              /* if a hash table exists */
    for (i = 0; i < tab->cnt; i++) {
      tpl = tab->tpls[i]; tpl->table = tab; tpl->id = i; }
    free(opc->htab);            /* restore table ref. and tuple id */
  }                             /* and delete the hash table */
  if (opc->wgts) {              /* if there is a weight array */
    if (restore)                /* restore the tuple weights */
      for (i = 0; i < tab->cnt; i++)
        tab->tpls[i]->wgt = opc->wgts[i];
    free(opc->wgts);            /* delete the weight buffer */
  }                             /* then delete the work buffers */
  if (opc->cids) free(opc->cids);
  if (opc->curr) free(opc->curr);
  return -1;                    /* return an error code */
}  /* opcerr() */

/*--------------------------------------------------------------------*/

static int opc_full (TABLE *tab)
{                               /* --- det. full one point coverages */
  int     r;                    /* buffer for a function result */
  TPLID   i, n;                 /* loop variables for tuples */
  ATTID   k, m;                 /* loop variables for attributes */
  ATTID   attcnt;               /* number of attributes */
  VALID   valcnt;               /* number of attribute values */
  TUPLE   *x, *c;               /* current expansion and clone */
  TUPLE   *tpl, **p;            /* to traverse the tuples/hash bins */
  ATT     *att;                 /* to traverse the attributes */
  INST    *col;                 /* to traverse the tuple columns */
  TPLID   hash;                 /* hash value of the current tuple */
  OPCDATA opc;                  /* one point coverage data */

  assert(tab);                  /* check the function argument */

  /* --- initialize --- */
  attcnt    = as_attcnt(tab->attset);
  opc.table = tab;              /* note the table and */
  opc.htab  = opc.buf = NULL;   /* clear the buffer variables */
  opc.curr  = x = tpl_create(tab->attset, 0);
  opc.cids  = (ATTID*) malloc((size_t)attcnt   *sizeof(ATTID));
  opc.wgts  = (WEIGHT*)malloc((size_t)tab->cnt *sizeof(WEIGHT));
  if (!opc.curr                 /* create buffers for a tuple, */
  ||  !opc.cids || !opc.wgts    /* identifiers of null columns, */
  ||  (htab(&opc, 1) != 0))     /* and the tuple weights, */
    return opcerr(&opc, 0, 0);  /* and a hash table for the tuples */
  for (i = 0; i < tab->cnt; i++)/* note the weights of the tuples */
    opc.wgts[i] = tab->tpls[i]->wgt;

  /* --- expand tuples with null values --- */
  for (i = n = tab->cnt; --i >= 0; ) {
    tpl = tab->tpls[i];         /* traverse the tuples in the table */

    /* -- find an expandable tuple --- */
    for (k = m = 0; k < attcnt; k++) {
      col = tpl->cols +k;       /* traverse the tuple columns */
      att = as_att(tab->attset, k);     /* get the corresp. attribute */
      valcnt = att_valcnt(att); /* and the number of attribute values */
      if ((valcnt <= 0)         /* if the attribute is not nominal */
      ||  !isnone(col->n))      /* or the column value is known, */
        x->cols[k] = *col;      /* simply copy the value */
      else {                    /* if the nominal value is null */
        x->cols[k].n  = valcnt-1;
        opc.cids[m++] = k;      /* impute the last value of */
      }                         /* the attribute domain and note */
    }                           /* the index of the null column */
    if (m <= 0) continue;       /* check the number of null columns */
    x->wgt = tpl->wgt;          /* get the tuple weight for expansion */

    /* -- generate and add expansions -- */
    do {                        /* tuple creation loop */
      hash = (TPLID)tpl_hash(x);
      p    = opc.htab +(size_t)hash % (size_t)tab->size;
      for (c = *p; c; c = (TUPLE*)c->table)
        if (tpl_cmp(x, c, NULL) == 0)
          break;                /* try to find current expansion */
      if (c)                    /* if an identical tuple exists, */
        c->wgt += tpl->wgt;     /* merely sum the tuple weights */
      else {                    /* if no identical tuple exists */
        r = tab_resize(tab,n+1);/* resize the tuple array */
        if (r < 0) return opcerr(&opc, n, 1);
        if (r > 0) {            /* if the tuple array was resized, */
          if (htab(&opc, 0) != 0)    /* resize the hash table, too */
            return opcerr(&opc, n, 1);
          p = opc.htab +(size_t)hash % (size_t)tab->size;
        }                       /* recompute the hash bucket */
        c = tpl_clone(x);       /* clone the current expansion */
        if (!c) return opcerr(&opc, n, 1);
        tab->tpls[n++] = c;     /* add the new tuple to the table */
        c->id    = hash;        /* note the tuple's hash value */
        c->table = (TABLE*)*p;  /* insert the new tuple */
        *p = c;                 /* into the hash table */
      }
      for (k = m; --k >= 0; ) { /* traverse the null columns */
        col = x->cols +opc.cids[k];
        if (--col->n >= 0) break;
        col->n = att_valcnt(as_att(tab->attset, opc.cids[k]))-1;
      }                         /* compute the next value combination */
    } while (k >= 0);           /* while there is another combination */

    /* -- remove the expanded tuple -- */
    p = opc.htab +(size_t)tpl->id % (size_t)tab->size;
    while (*p != tpl) p = (TUPLE**)&(*p)->table;
    *p = (TUPLE*)tpl->table;    /* remove the expanded tuple from the */
    tpl->table = tab;           /* hash table and mark it for deletion*/
  }  /* for (i = n = tab->cnt; .. */

  /* --- clean up --- */
  free(opc.curr); free(opc.htab);  /* delete the work buffers */
  free(opc.wgts); free(opc.cids);
  tab->wgt = 0;                 /* init. the total tuple weight */
  for (i = tab->cnt = 0; i < n; i++) {
    tpl = tab->tpls[i];         /* traverse the tuples in the table */
    if (tpl->table == tab) {    /* delete expanded tuples */
      tpl->table = NULL; tpl->id = -1; tab->delfn(tpl); continue; }
    tab->tpls[tab->cnt] = tpl;  /* store the tuple in the array */
    tpl->table = tab;           /* set the table reference */
    tpl->id    = tab->cnt++;    /* and the tuple identifier */
    tab->wgt  += tpl->wgt;      /* sum the tuple weight */
  }
  tab_resize(tab, 0);           /* try to shrink the tuple array */
  return 0;                     /* return 'ok' */
}  /* opc_full() */

/*--------------------------------------------------------------------*/

static int opc_cond (TABLE *tab)
{                               /* --- det. condensed one point cov. */
  int     r;                    /* buffer for a function result */
  TPLID   i, k, m, n;           /* loop variables for tuples */
  TPLID   u, z;                 /* number of tuples with null values */
  TUPLE   *x, *c;               /* current intersection and clone */
  TUPLE   *tpl, **p;            /* to traverse the tuples/hash bins */
  TPLID   hash;                 /* hash value of a tuple */
  OPCDATA opc;                  /* one point coverage data */

  assert(tab);                  /* check the function argument */

  /* --- initialize --- */
  z = n     = tab->cnt;         /* get the number of tuples */
  if (z <= 1) return 0;         /* if at most one tuple, abort */
  opc.table = tab;              /* note the table and */
  opc.htab  = NULL;             /* clear the buffer variables */
  opc.curr  = NULL; opc.cids = NULL; opc.wgts = NULL;
  opc.buf   = (TUPLE**)malloc((size_t)z *sizeof(TUPLE*));
  if (!opc.buf) return -1;      /* allocate a tuple array */
  for (i = m = 0; i < tab->cnt; i++) {
    tpl = tab->tpls[i];         /* traverse the tuples */
    if (tpl_nullcnt(tpl) > 0) opc.buf[m++] = tpl;
  }                             /* collect tuples with null values */
  /* The tuple array tab->tpls must be traversed in this order */
  /* (forward) because of the way the weights are distributed below. */
  if (m <= 0) {                 /* if no tuples contain null values, */
    opcerr(&opc, 0, 0); return 0; }        /* there is nothing to do */
  opc.curr = x = tpl_create(tab->attset, 0);
  if (!x || (htab(&opc, 1) != 0))         /* create a tuple buffer */
    return opcerr(&opc, 0, 0);  /* and a hash table for the tuples */
  x->wgt = 0;                   /* clear the current tuple weight */

  /* --- compute closure under tuple intersection --- */
  for (u = m, i = 0; i < u; i++) {
    tpl = opc.buf[i];           /* traverse tuples with null values */
    for (k = i; --k >= 0; ) {   /* traverse the preceding tuples */
      if (tpl_isect(x, tpl, opc.buf[k]) != 0)
        continue;               /* skip tuples with no intersection, */
      hash = (TPLID)tpl_hash(x);/* otherwise compute intersection */
      p    = opc.htab +(size_t)hash % (size_t)tab->size;
      for (c = *p; c; c = (TUPLE*)c->table)
        if (tpl_cmp(x, c, NULL) == 0)
          break;                /* try to find current intersection */
      if (c) continue;          /* skip existing intersections */
      r = tab_resize(tab, n+1); /* resize the tuple array */
      if (r < 0) return opcerr(&opc, n, 0);
      if (r > 0) {              /* if the tuple array was resized, */
        if (htab(&opc, 0) != 0) /* resize the hash table as well */
          return opcerr(&opc, n, 0);
        p = opc.htab +(size_t)hash % (size_t)tab->size;
      }                         /* recompute the hash bucket */
      c = tpl_clone(x);         /* clone the current expansion */
      if (!c) return opcerr(&opc, n, 0);
      tab->tpls[n++] = c;       /* add the new tuple to the table */
      c->id    = hash;          /* note the tuple's hash value */
      c->table = (TABLE*)*p;    /* insert the new tuple */
      *p = c;                   /* into the hash table */
      if (tpl_nullcnt(c) <= 0)  /* if the new tuple contains */
        continue;               /* no null values, continue */
      if (u >= z) {             /* if the tuple array is full */
        z += (z > BLKSIZE) ? (z >> 1) : BLKSIZE;
        p = (TUPLE**)realloc(opc.buf, (size_t)z *sizeof(TUPLE*));
        if (!p) return opcerr(&opc, n, 0);
        opc.buf = p;            /* resize the tuple array */
      }                         /* and set the new array */
      opc.buf[u++] = c;         /* add the new tuple */
    }  /* for (k = i; ... */    /* to the tuple array */
  }  /* for (i = 0; ... */

  /* --- distribute weights --- */
  /* This way of distributing the weights assumes that the tuples  */
  /* with null values are in the same order in the tuple buffer as */
  /* in the complete table. The weight distribution process may be */
  /* improvable by exploiting a sorted table.                      */
  tab->wgt = 0;                 /* init. the total tuple weight */
  for (k = tab->cnt = n; --k >= 0; ) {
    tpl = tab->tpls[k];         /* traverse all tuples in the table */
    tpl->id = k;                /* restore the tuple identifiers */
    for (i = m; --i >= 0; ) {   /* traverse all initial u.v. tuples, */
      c = opc.buf[i];           /* but skip the tuple itself */
      if (c == tpl) { m--; continue; }
      if (tpl_isect(x, c, tpl) >= 2)
        tpl->wgt += c->wgt;     /* if a table tuple is more specific, */
    }                           /* add the weight of the tuple */
    tab->wgt += tpl->wgt;       /* with the unknown value(s), */
  }                             /* finally sum the tuple weights */

  /* --- clean up --- */
  free(opc.curr); free(opc.buf);
  free(opc.htab);               /* delete the work buffers and */
  tab_resize(tab, 0);           /* try to shrink the tuple array */
  return 0;                     /* return 'ok' */
}  /* opc_cond() */

/*--------------------------------------------------------------------*/

int tab_opc (TABLE *tab, int mode)
{                               /* --- compute one point coverages */
  int    r;                     /* result of opc function */
  TPLID  i;                     /* loop variable for tuples */
  TUPLE  *tpl;                  /* to traverse the tuples */
  double w, norm = 0;           /* weight, normalization factor */

  assert(tab);                  /* check the function argument */
  if (mode & TAB_NORM) {        /* if to normalize the one point cov. */
    if (tab->wgt <= 0) return 0;
    norm = 1/(double)tab->wgt;  /* check the total tuple weight and */
  }                             /* compute the normalization factor */
  r = (mode & TAB_FULL) ? opc_full(tab) : opc_cond(tab);
  if (r) return r;              /* compute one point coverages */
  if (mode & TAB_NORM) {        /* if to normalize the one point cov. */
    tab->wgt = 0;               /* init. the total tuple weight */
    for (i = 0; i < tab->cnt; i++) {
      tpl = tab->tpls[i];       /* traverse the tuples */
      tab->wgt += w = norm *(double)tpl->wgt;
      tpl->wgt = (WEIGHT)w;     /* normalize the tuple weights */
    }                           /* and sum them over the tuples */
  }
  /* Note that the sum of the normalized tuple weights need not be 1, */
  /* because the weight of created tuple intersections is added. */
  return 0;                     /* return 'ok' */
}  /* tab_opc() */

/*----------------------------------------------------------------------
The function tab_opc requires the argument table to be reduced (and
sorted, but a call to tab_reduce() automatically sorts the table).
----------------------------------------------------------------------*/

WEIGHT tab_poss (TABLE *tab, TUPLE *tpl)
{                               /* --- det. degrees of possibility */
  TPLID  i;                     /* loop variable for tuples */
  TUPLE  *cur;                  /* to traverse the tuples */
  WEIGHT poss = 0;              /* degree of possibility */

  assert(tab);                  /* check the function arguments */
  for (i = 0; i < tab->cnt; i++) {
    cur = tab->tpls[i];         /* traverse the tuples in the table */
    if (tpl_compat(cur, tpl)    /* if the given tuple is compatible */
    && (cur->wgt > poss))       /* with the tuple in the table, */
      poss = cur->wgt;          /* update the degree of possibility */
  }                             /* (find maximum degree) */
  return poss;                  /* return the degree of possibility */
}  /* tab_poss() */

/*--------------------------------------------------------------------*/

void tab_possx (TABLE *tab, TUPLE *tpl, double res[])
{                               /* --- det. degrees of possibility */
  ATTID  i, m;                  /* loop variable for columns */
  TPLID  n;                     /* tuple counter */
  ATT    *att;                  /* to traverse the attributes */
  INST   *col, *inst;           /* to traverse the columns */
  double p;                     /* degree of possibility */

  assert(tab && tpl && res);    /* check the function arguments */
  for (m = 0, i = tpl_colcnt(tpl); --i >= 0; ) {
    col  = tpl->cols +i;        /* traverse the tuple columns */
    att  = as_att(tab->attset, i);
    if (!isnone(col->n)) { att_inst(att)->n = col->n; }
    else            { m++; att_inst(att)->n = att_valcnt(att)-1; }
  }                             /* copy the tuple to the att. set */
  if (m <= 0) {                 /* if precise tuple, no loop needed */
    res[0] = res[1] = res[2] = tab_poss(tab, tpl); return; }
  res[2] = res[0] = 0; n = 0;   /* init. the aggregates to compute */
  res[1] = INFINITY;            /* and the instantiation counter */
  do {                          /* loop over compat. precise tuples */
    p = tab_poss(tab, NULL);    /* determine the possibility */
    if (p < res[1]) res[1] = p; /* update minimum, */
    if (p > res[2]) res[2] = p; /* maximum, and sum */
    res[0] += p; n++;           /* and count the tuple */
    for (i = tab_colcnt(tpl); --i >= 0; ) {
      col  = tpl->cols +i;      /* traverse the tuple columns */
      if (!isnone(col->n)) continue;
      att  = as_att(tab->attset, i);
      inst = att_inst(att);     /* traverse the null columns */
      if (--inst->n >= 0) break;
      inst->n = att_valcnt(att)-1;
    }                           /* compute next value combination */
  } while (i >= 0);             /* while there is another combination */
  if (n      >  0) res[0] /= (double)n;/* determine average value and */
  if (res[2] <= 0) res[1]  = 0; /* correct minimum if maximum is zero */
}  /* tab_possx() */
