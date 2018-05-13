/*----------------------------------------------------------------------
  File    : dtree2.c
  Contents: decision and regression tree management,
            functions for growing and pruning
  Author  : Christian Borgelt
  History : 1997.06.12 file created (as dtree.c)
            1997.08.08 first version completed
            1997.08.15 pruning functions completed
            1997.08.25 multiple child references made possible
            1997.08.26 node counting added (function count())
            1997.08.28 pessimistic pruning added (increase error)
            1997.08.30 treatment of new values during pruning enabled
            1997.08.25 multiple child references replaced by links
            1997.09.18 bug in measure evaluation removed
            1997.09.25 subsets for nominal attributes added
            1997.10.01 bugs in function pr_tab() removed
            1998.02.02 links between children simplified
            1998.02.03 adapted to changed interface of module 'table'
            1998.03.30 bug in eval_flt() concerning null values removed
            1998.08.27 single class value coll. in eval_set() corrected
            1998.09.21 bug in function adapt() fixed (concerning links)
            1998.10.15 major revision completed
            1998.10.30 bug in function pr_tab() removed
            1998.12.15 minor improvements, assertions added
            1999.02.24 parameters 'params', 'minval' added to dt_grow()
            1999.03.20 roundoff error handling improved
            2000.02.29 bug in function eval_met() removed
            2000.12.02 growing and pruning moved to this file
            2000.12.03 decision tree growing partly redesigned
            2000.12.12 leaf creation in pruning with a table redesigned
            2000.12.17 regression tree functionality added
            2001.07.23 global variables removed (now in DTREE structure)
            2001.09.26 normal distribution table replaced by a function
            2001.10.19 conf. level pruning for metric targets added
            2002.03.01 check of largest branch in pr_tab() optional
            2004.07.21 pure attribute evaluation added
            2004.07.22 output of cut values added to pure evaluation
            2004.09.13 trivial pruning of grown tree made optional
            2007.02.13 adapted to modified module attset
            2011.07.28 adapted to external mathematical module normal
            2011.07.29 adaptation of confidence level corrected
            2013.07.04 bug in function grow() fixed (MINERROR/metric)
            2013.08.23 adapted to definitions ATTID, VALID, TPLID etc.
            2015.11.12 parameter thsel added to function dt_prune()
            2016.11.11 static variables eliminated from functions
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "arrays.h"
#include "normal.h"
#include "dtree.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define WORTHLESS   (-INFINITY) /* to indicate a worthless attribute */
#define EPSILON     1e-6        /* to handle roundoff errors */
#define MINERROR    0.1         /* minimum error for division */
#define MAXFACT     1e6         /* maximum factor for error estim. */

/* --- functions --- */
#define islink(d,n) (((d)->link >= (n)->data) && \
                     ((d)->link <  (n)->data +(n)->size))

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- tuple selection info. --- */
  ATTID  col;                   /* column index */
  VALID  nval;                  /* column value (nominal) */
  DTINT  ival;                  /* column value (integer) */
  double fval;                  /* column value (float) */
  DTDATA *data;                 /* data array of the current node */
} TSEL;                         /* (tuple selection information) */

typedef TPLID  GRPFN  (TUPLE **tpls, TPLID n, const TSEL *tsel);
typedef double ESTFN  (DTREE *dtree, double n, double e);

typedef struct grow   *PGROW;   /* to avoid warning in def. of EVALFN */
typedef double EVALFN (PGROW gi, TUPLE **tpls, TPLID n,
                       ATTID attid, double *cut);

typedef struct grow {           /* --- tree grow information --- */
  DTREE  *dtree;                /* current decision/regression tree */
  ATTSET *attset;               /* attribute set of decision tree */
  TUPLE  **tpls;                /* array of tuples with known class */
  EVALFN *eval_nom;             /* eval. function for nominal atts. */
  EVALFN *eval_met;             /* eval. function for metric  atts. */
  void   *curr;                 /* freq./var. table for current att. */
  void   *best;                 /* freq./var. table for best    att. */
  void   *mett;                 /* freq./var. table for metric  att. */
  double err;                   /* number of errors of subtree */
  double minerr;                /* minimum error for division */
  int    type;                  /* type of the target attribute */
  int    measure;               /* evaluation measure */
  double *params;               /* optional parameters */
  double minval;                /* minimal value of measure for split */
  double mincnt;                /* minimal number of tuples */
  ATTID  maxht;                 /* maximal height of decision tree */
  int    flags;                 /* grow flags, e.g. DT_SUBSET */
  char   *used;                 /* used flags for attributes */
  double *evals;                /* evaluations of attributes */
  double *cuts;                 /* cut values of attributes */
  VALID  sets[1];               /* index array for value subsets */
} GROW;                         /* (tree grow information) */

typedef struct {                /* --- tree pruning information --- */
  DTREE  *dtree;                /* current decision/regression tree */
  ATTSET *attset;               /* attribute set of decision tree */
  TUPLE  **tpls;                /* array of tuples with known class */
  void   *tab;                  /* freq./var. table for leaf creation */
  ESTFN  *est;                  /* error estimation function */
  double err;                   /* number of errors of subtree */
  ATTID  maxht;                 /* maximal height of decision tree */
  int    type;                  /* type of the target attribute */
  int    chklb;                 /* whether to check largest branch */
  double thsel;                 /* threshold for leaf node selection */
} PRUNE;                        /* (tree pruning information) */

/*----------------------------------------------------------------------
  Grouping Functions
----------------------------------------------------------------------*/

static TPLID grp_nom (TUPLE **tpls, TPLID n, const TSEL *tsel)
{                               /* --- group nominal values */
  TUPLE **src, **dst, *t;       /* source and destination, buffer */

  assert(tpls && tsel && (n >= 0));   /* check the function arguments */
  for (src = dst = tpls; --n >= 0; src++)
    if (tpl_colval(*src, tsel->col)->n == tsel->nval) {
      t = *src; *src = *dst; *dst++ = t; }
  return (TPLID)(dst -tpls);    /* group qualifying tuples */
}  /* grp_nom() */              /* and return their number */

/*--------------------------------------------------------------------*/

static TPLID grp_set (TUPLE **tpls, TPLID n, const TSEL *tsel)
{                               /* --- group a set of values */
  TUPLE **src, **dst, *t;       /* source and destination, buffer */
  VALID i;                      /* check value and link */

  assert(tpls && tsel && (n >= 0));   /* check the function arguments */
  for (src = dst = tpls; --n >= 0; src++)
    if (((i = tpl_colval(*src, tsel->col)->n) == tsel->nval)
    ||  (tsel->data[i].link -tsel->data       == tsel->nval)) {
      t = *src; *src = *dst; *dst++ = t; }
  return (TPLID)(dst -tpls);    /* group qualifying tuples */
}  /* grp_set() */              /* and return their number */

/*--------------------------------------------------------------------*/

static TPLID grp_nullint (TUPLE **tpls, TPLID n, const TSEL *tsel)
{                               /* --- group null values (integer) */
  TUPLE **src, **dst, *t;       /* source and destination, buffer */

  assert(tpls && tsel && (n >= 0));   /* check the function arguments */
  for (src = dst = tpls; --n >= 0; src++)
    if (isnull(tpl_colval(*src, tsel->col)->i)) {
      t = *src; *src = *dst; *dst++ = t; }
  return (TPLID)(dst -tpls);    /* group qualifying tuples */
}  /* grp_nullint() */          /* and return their number */

/*--------------------------------------------------------------------*/

static TPLID grp_int (TUPLE **tpls, TPLID n, const TSEL *tsel)
{                               /* --- group greater than (integer) */
  TUPLE **src, **dst, *t;       /* source and destination, buffer */

  assert(tpls && tsel && (n >= 0));   /* check the function arguments */
  for (src = dst = tpls; --n >= 0; src++)
   if (tpl_colval(*src, tsel->col)->i > tsel->ival) {
      t = *src; *src = *dst; *dst++ = t; }
  return (TPLID)(dst -tpls);    /* group qualifying tuples */
}  /* grp_int() */              /* and return their number */

/*--------------------------------------------------------------------*/

static TPLID grp_nullflt (TUPLE **tpls, TPLID n, const TSEL *tsel)
{                               /* --- group null values (float) */
  TUPLE **src, **dst, *t;       /* source and destination, buffer */

  assert(tpls && tsel && (n >= 0));   /* check the function arguments */
  for (src = dst = tpls; --n >= 0; src++)
    if (isnan(tpl_colval(*src, tsel->col)->f)) {
      t = *src; *src = *dst; *dst++ = t; }
  return (TPLID)(dst -tpls);    /* group qualifying tuples */
}  /* grp_nullflt() */          /* and return their number */

/*--------------------------------------------------------------------*/

static TPLID grp_flt (TUPLE **tpls, TPLID n, const TSEL *tsel)
{                               /* --- group greater than (float) */
  TUPLE **src, **dst, *t;       /* source and destination, buffer */

  assert(tpls && tsel && (n >= 0));   /* check the function arguments */
  for (src = dst = tpls; --n >= 0; src++)
    if (tpl_colval(*src, tsel->col)->f > tsel->fval) {
      t = *src; *src = *dst; *dst++ = t; }
  return (TPLID)(dst -tpls);    /* group qualifying tuples */
}  /* grp_flt() */              /* and return their number */

/*--------------------------------------------------------------------*/

static void mul_wgt (TUPLE **tpls, TPLID n, double f)
{                               /* --- multiply tuple weights */
  assert(tpls && (n >= 0));     /* check the function arguments */
  while (--n >= 0)              /* multiply with weighting factor */
    tpl_mulxwgt(tpls[n], (WEIGHT)f);
}  /* mul_wgt() */

/*--------------------------------------------------------------------*/
#ifdef DT_PRUNE

static double sum_wgt (TUPLE **tpls, TPLID n)
{                               /* --- sum tuple weights */
  double wgt = 0;               /* traverse the tuples */

  assert(tpls && (n >= 0));     /* check the function arguments */
  while (--n >= 0)              /* sum the tuple weights */
    wgt += tpl_getxwgt(tpls[n]);
  return wgt;                   /* return the computed sum */
}  /* sum_wgt() */

#endif
/*----------------------------------------------------------------------
  Comparison Functions
----------------------------------------------------------------------*/
#ifdef DT_GROW

static int cmp_int (const void *p1, const void *p2, void *data)
{                               /* --- compare a column of two tuples */
  DTINT i1 = tpl_colval((TUPLE*)p1, (ATTID)(ptrdiff_t)data)->i;
  DTINT i2 = tpl_colval((TUPLE*)p2, (ATTID)(ptrdiff_t)data)->i;
  if     (i1 < i2) return -1;   /* get column values and */
  return (i1 > i2) ? 1 : 0;     /* return sign of their difference */
}  /* cmp_int() */

/*--------------------------------------------------------------------*/

static int cmp_flt (const void *p1, const void *p2, void *data)
{                               /* --- compare a column of two tuples */
  DTFLT f1 = tpl_colval((TUPLE*)p1, (ATTID)(ptrdiff_t)data)->f;
  DTFLT f2 = tpl_colval((TUPLE*)p2, (ATTID)(ptrdiff_t)data)->f;
  if     (f1 < f2) return -1;   /* get column values and */
  return (f1 > f2) ? 1 : 0;     /* return sign of their difference */
}  /* cmp_flt() */

#endif
/*----------------------------------------------------------------------
  Auxiliary Functions for Growing and Pruning
----------------------------------------------------------------------*/

static TUPLE** tuples (TABLE *table, ATTID trgid, TPLID *n)
{                               /* --- collect tuples w. known target */
  TPLID i;                      /* loop variable */
  int   type;                   /* type of target attribute */
  TUPLE **q, **p, *tpl;         /* to traverse the tuple array */
  INST  *inst;                  /* to access the target value */

  assert(table && n             /* check the function arguments */
  &&    (trgid >= 0) && (trgid < tab_colcnt(table)));
  i = tab_tplcnt(table);        /* get the number of tuples */
  p = q = (TUPLE**)malloc((size_t)i *sizeof(TUPLE*));
  if (!p) return NULL;          /* allocate a tuple array */
  type = att_type(tab_col(table, trgid));
  while (--i >= 0) {            /* traverse the tuples in the table */
    tpl  = tab_tpl(table, i);   /* and check the target value */
    inst = tpl_colval(tpl, trgid);
    if (((type == AT_NOM) && isnone(inst->n))
    ||  ((type == AT_INT) && isnull(inst->i))
    ||  ((type == AT_FLT) && isnan (inst->f)))
      continue;                 /* skip tuples with null target, */
    *p++ = tpl;                 /* but collect all other tuples */
    tpl_setxwgt(tpl, tpl_getwgt(tpl));
  }                             /* copy the tuple weight */
  *n = (TPLID)(p-q);            /* compute the number of tuples */
  return q;                     /* and return the tuple array */
}  /* tuples() */

/*--------------------------------------------------------------------*/

static DTNODE* leaf_nom (DTREE *dt, FRQTAB *frqtab, VALID x)
{                               /* --- create a leaf (nominal target) */
  VALID  i, k;                  /* loop variable, class id */
  DTNODE *node;                 /* created leaf node */
  double sum, frq;              /* (sum of) the class frequencies */
  double max = 0;               /* maximum of the class frequencies */
  double wgt;                   /* weight of cases with null value */

  assert(dt && frqtab           /* check the function arguments */
  &&    (ft_known(frqtab)    > 0)
  &&    (ft_frq_x(frqtab, x) > 0));
  node = (DTNODE*)malloc(sizeof(DTNODE)
                        +sizeof(DTDATA) *(size_t)(dt->clscnt-1));
  if (!node) return NULL;       /* create a leaf node */
  node->flags |= DT_LEAF;       /* set the leaf flag */
  node->attid  = dt->trgid;     /* store the class attribute id */
  node->size   = dt->clscnt;    /* and the number of classes */
  /* It is convenient to abuse the cut value for storing the   */
  /* frequency of tuples with a known value. This is possible, */
  /* because the cut value is not used in leaf nodes. Hence:   */
  sum = ft_frq_x(frqtab, x);    /* get and store the number of */
  node->cut = sum;              /* tuples with a known value */
  frq = ft_frq_x(frqtab, -1);   /* check for null values */
  if (frq <= 0) {               /* if there are no null values */
    for (i = k = 0; i < node->size; i++) {
      frq = ft_frq_xy(frqtab, x, i);
      if (frq > max) { max = frq; k = i; }
      node->data[i].frq = frq;  /* note the class frequencies and */
    } }                         /* determine the most frequent class */
  else {                        /* if there are null values */
    wgt = ft_known(frqtab);     /* compute the weighting factor */
    wgt = (wgt > 0) ? sum/wgt : (1.0/(double)ft_xcnt(frqtab));
    sum += wgt *frq;            /* add the weight of nulls */
    for (i = k = 0; i < node->size; i++) {
      frq = ft_frq_xy(frqtab, x, i) +wgt *ft_frq_xy(frqtab, -1, i);
      if (frq > max) { max = frq; k = i; }
      node->data[i].frq = frq;  /* compute the class frequencies and */
    }                           /* determine the most frequent class */
  }
  node->frq   = sum;            /* store the total frequency, */
  node->err   = sum -max;       /* the number of leaf errors, */
  node->trg.n = k;              /* and the most frequent class */
  return node;                  /* return the created leaf node */
}  /* leaf_nom() */

/*--------------------------------------------------------------------*/

static DTNODE* leaf_met (DTREE *dt, VARTAB *vartab, VALID x)
{                               /* --- create a leaf (metric target) */
  DTNODE *node;                 /* created leaf node */
  double frq, null;             /* (sum of) the column frequencies */
  double wgt;                   /* weight of cases with null value */
  double m;                     /* temporary buffer */

  assert(dt && vartab           /* check the function arguments */
  &&    (vt_known (vartab)    > 0)
  &&    (vt_colfrq(vartab, x) > 0));
  node = (DTNODE*)malloc(sizeof(DTNODE) -sizeof(DTDATA));
  if (!node) return NULL;       /* create a leaf node */
  node->flags |= DT_LEAF;       /* set the leaf flag */
  node->attid  = dt->trgid;     /* store the target attribute id */
  node->size   = 0;             /* clear the array size */
  /* Again the cut value is abused for storing the frequency */
  /* of tuples with a known value:                           */
  frq = vt_colfrq(vartab, x);   /* get and store the number of */
  node->cut = frq;              /* tuples with a known value */
  null = vt_colfrq(vartab, -1); /* check for null values */
  if (null <= 0) {              /* if there are no null values, */
    node->frq   = frq;          /* copy the aggregates directly */
    node->err   = vt_colsse(vartab, x);
    node->trg.f = (DTFLT)vt_colmean(vartab, x); }
  else {                        /* if there are null values, */
    wgt = vt_known(vartab);     /* compute the weighting factor */
    null *= wgt = (wgt > 0) ? frq/wgt : (1.0/(double)vt_colcnt(vartab));
    node->err   = vt_colssv(vartab, x) +wgt *vt_colssv(vartab, -1);
    node->frq   = wgt = frq +null;
    node->trg.f = (DTFLT)(m = (frq  *vt_colmean(vartab, x)
                              +null *vt_colmean(vartab,-1)) /wgt);
    node->err  -= wgt *m*m;     /* combine the column aggregates by */
  }                             /* recomputing mean and sse */
  if (node->err < 0) node->err = 0;
  return node;                  /* return the created leaf node */
}  /* leaf_met() */

/*--------------------------------------------------------------------*/

static void delete (DTNODE *node)
{                               /* --- recursively delete a subtree */
  VALID i;                      /* loop variable */

  assert(node);                 /* check the function argument */
  if (!(node->flags & DT_LEAF)){/* if the node is no leaf */
    for (i = node->size; --i >= 0; )
      if (node->data[i].child && !islink(node->data+i, node))
        delete(node->data[i].child);
  }                             /* recursively delete all subtrees */
  free(node);                   /* delete the node itself */
}  /* delete() */

/*--------------------------------------------------------------------*/
#ifdef DT_PRUNE

static DTNODE* adapt (ATTSET *attset, DTNODE *node)
{                               /* --- adapt test node to new values */
  VALID  i, n;                  /* loop variable, number of values */
  DTNODE *adpt;                 /* adapted test node */
  DTDATA *data;                 /* address of old data array */

  assert(attset && node);       /* check the function arguments */
  n = att_valcnt(as_att(attset, node->attid));
  if (node->size >= n)          /* get number of values and check the */
    return node;                /* node's data array size against it */
  data = node->data;            /* note address of the old data array */
  adpt = (DTNODE*)realloc(node, sizeof(DTNODE)
                        +(size_t)(n-1) *sizeof(DTDATA));
  if (!adpt) return NULL;       /* create a new test node */
  if ((adpt != node)            /* if the node has been changed */
  &&  (adpt->flags & DT_LINK)){ /* and it contains links */
    for (i = 0; i < adpt->size; i++) {
      if ((adpt->data[i].link >= data)
      &&  (adpt->data[i].link <  data +adpt->size))
        adpt->data[i].link = adpt->data +(data->link -data);
    }                           /* adapt the link, i.e., compute the */
  }                             /* address of the new array field */
  while (adpt->size < n)        /* clear the new child pointers */
    adpt->data[adpt->size++].child = NULL;
  return adpt;                  /* return the enlarged node */
}  /* adapt() */

/*--------------------------------------------------------------------*/

static DTNODE* coll_rec (DTNODE *node, double *frq)
{                               /* --- recursive part of collapse */
  VALID  i;                     /* loop variable */
  DTNODE *res, *tmp;            /* resulting leaf node */

  assert(node && frq);          /* check the function arguments */
  if (node->flags & DT_LEAF) {  /* if the node is a leaf */
    for (i = 0; i < node->size; i++)
      frq[i] += node->data[i].frq;
    return node;                /* aggregate class frequencies and */
  }                             /* return leaf as a possible result */
  res = NULL;                   /* traverse the child nodes */
  for (i = node->size; --i >= 0; ) {
    if (!node->data[i].child || islink(node->data+i, node))
      continue;                 /* skip empty nodes and links */
    tmp = coll_rec(node->data[i].child, frq);
    if (!res) res = tmp;        /* recursively aggregate */
    else      free(tmp);        /* the class frequencies */
  }                             /* and keep one leaf node */
  free(node);                   /* delete the current node */
  return res;                   /* return the leaf node kept */
}  /* coll_rec() */

/*--------------------------------------------------------------------*/

static DTNODE* collapse (DTREE *dt, DTNODE *node)
{                               /* --- collapse subtree into a leaf */
  VALID  i, k;                  /* loop variables */
  double *frq;                  /* to traverse the class frequencies */
  double sum;                   /* sum of the class frequencies */

  assert(dt && node);           /* check the function arguments */
  if (!node || (node->flags & DT_LEAF))
    return node;                /* if the node is a leaf, abort */

  /* --- metric target attribute --- */
  if (dt->type != AT_NOM) {     /* if target attribute is metric */
    for (i = node->size; --i >= 0; )
      if (node->data[i].child && !islink(node->data+i, node))
        delete(node->data[i].child);   /* delete all subtrees */
    node->flags |= DT_LEAF;     /* turn the node into a leaf, */
    node->attid  = dt->trgid;   /* store the target identifier, */
    node->size   = 0;           /* and clear the array size */
    return (DTNODE*)realloc(node, sizeof(DTNODE) -sizeof(DTDATA));
  }                             /* shrink the node to leaf size */

  /* --- nominal target attribute --- */
  frq  = memset(dt->frqs, 0, (size_t)dt->clscnt *sizeof(double));
  node = coll_rec(node, frq);   /* collapse the subtree into a leaf */
  if (!node) return NULL;       /* if no result leaf found, abort */
  for (sum = 0, i = k = 0; i < dt->clscnt; i++) {
    sum += frq[i];              /* traverse the class frequencies, */
    if (frq[i] > frq[k]) k = i; /* compute the frequency sum, and */
  }                             /* determine the most frequent class */
  node->frq   = sum;            /* set the node frequency, */
  node->err   = sum -frq[k];    /* the number of errors, and */
  node->trg.n = k;              /* the most frequent class */
  for (i = 0; i < node->size; i++)
    node->data[i].frq = frq[i]; /* copy the class frequencies */
  return node;                  /* return the modified leaf node */
}  /* collapse() */

#endif  /* #ifdef DT_PRUNE */
/*--------------------------------------------------------------------*/

static ATTID count (DTREE *dt, DTNODE *node, ATTID height)
{                               /* --- count the number of nodes */
  VALID i;                      /* loop variable */
  ATTID n = 1;                  /* node counter */

  assert(dt && node && (height >= 0));  /* check the function args. */
  if (++height > dt->height)    /* if beyond the curr. max. height, */
    dt->height = height;        /* update the maximal height */
  if (!(node->flags & DT_LEAF)){/* if the current node is no leaf */
    for (i = node->size; --i >= 0; )
      if (node->data[i].child && !islink(node->data+i, node))
        n += count(dt, node->data[i].child, height);
  }                             /* recursively count the nodes */
  return n;                     /* return the number of nodes */
}  /* count() */

/*----------------------------------------------------------------------
  Evaluation Functions for Nominal Target Attributes
----------------------------------------------------------------------*/
#ifdef DT_GROW

static double nom_nom (GROW *gi, TUPLE **tpls, TPLID n,
                       ATTID attid, double *cut)
{                               /* --- evaluate a nominal attribute */
  ATTID clsid;                  /* class attribute identifier */
  VALID k, m;                   /* loop variables for values */
  VALID cls, val;               /* class and attribute value */

  assert(gi && tpls && (n > 0));/* check the function arguments */
  k = att_valcnt(as_att(gi->attset, attid));
  ft_init((FRQTAB*)gi->curr, k, gi->dtree->clscnt);
  clsid = gi->dtree->trgid;     /* initialize the frequency table */
  while (--n >= 0) {            /* and traverse the tuples */
    cls = tpl_colval(tpls[n], clsid)->n;
    val = tpl_colval(tpls[n], attid)->n;
    ft_add((FRQTAB*)gi->curr, val, cls, tpl_getxwgt(tpls[n]));
  }                             /* fill the frequency table */
  ft_marg(gi->curr);            /* and marginalize it */
  if (gi->flags & (DT_SUBSET|DT_1INN))  /* if only called to compute */
    return 0;                   /* initial table, abort the function */
  for (m = 0; --k >= 0; )       /* count reasonable tuple sets */
    if (ft_frq_x((FRQTAB*)gi->curr, k) >= gi->mincnt)
      m++;                      /* there must be at least two values */
  if (m < 2) return WORTHLESS;  /* with at least mincnt cases each */
  return ft_eval((FRQTAB*)gi->curr, gi->measure, gi->params);
}  /* nom_nom() */              /* evaluate the frequency table */

/*--------------------------------------------------------------------*/

static double nom_bin (GROW *gi, TUPLE **tpls, TPLID n,
                       ATTID attid, double *cut)
{                               /* --- eval. bin splits of nom. att. */
  TPLID  m;                     /* loop variable  for tuples */
  VALID  i, k;                  /* loop variables for values */
  ATTID  clsid;                 /* class attribute identifier */
  VALID  valcnt;                /* number of values */
  VALID  cls, val;              /* class and attribute value */
  double curr, best;            /* current and best worth */

  assert(gi && tpls && (n > 0));/* check the function arguments */
  clsid  = gi->dtree->trgid;    /* get class id and number of values */
  valcnt = att_valcnt(as_att(gi->attset, attid));
  best   = WORTHLESS; k = -1;   /* init. evaluation and value index */
  for (i = 0; i < valcnt; i++){ /* and the attribute values */
    ft_init((FRQTAB*)gi->curr, 2, gi->dtree->clscnt);
    for (m = 0; m < n; m++) {   /* traverse the tuples */
      cls = tpl_colval(tpls[m], clsid)->n;
      val = tpl_colval(tpls[m], attid)->n;
      val = (val == i) ? 1 : (val < 0) ? val : 0;
      ft_add((FRQTAB*)gi->curr, val, cls, tpl_getxwgt(tpls[m]));
    }                           /* fill the frequency table */
    ft_marg(gi->curr);          /* and marginalize it */
    if ((ft_frq_x((FRQTAB*)gi->curr, 0) < gi->mincnt)
    ||  (ft_frq_x((FRQTAB*)gi->curr, 1) < gi->mincnt))
      continue;                 /* skip impossible splits */
    curr = ft_eval((FRQTAB*)gi->curr, gi->measure, gi->params);
    if (curr > best) { best = curr; k = i; }
  }                             /* find best evaluation */
  if (k < 0) return WORTHLESS;  /* check for a useful split */
  nom_nom(gi, tpls, n, attid, cut);
  val = (k <= 0) ? 1 : 0;       /* compute initial frequency table */
  for (i = 0; i < valcnt; i++)  /* combine all but one value */
    if ((i != k) && (i != val)) ft_comb((FRQTAB*)gi->curr, i, val);
  return best;                  /* evaluate freq. table evaluation */
}  /* nom_bin() */

/*--------------------------------------------------------------------*/

static double nom_set (GROW *gi, TUPLE **tpls, TPLID n,
                       ATTID attid, double *cut)
{                               /* -- evaluate subsets of nom. att. */
  VALID  i, k, cls;             /* loop variables */
  VALID  valcnt;                /* number of values/subsets */
  TPLID  rtscnt;                /* number of reasonable tuple sets */
  VALID  *sets;                 /* to traverse subset index array */
  VALID  isrc, idst, imax;      /* indices     of sets to combine */
  double fsrc, fdst, fmax;      /* frequencies of sets to combine */
  double curr, prev, best;      /* current/previous/best worth */

  assert(gi && tpls && (n > 0));     /* check the function arguments */
  nom_nom(gi, tpls, n, attid, cut);  /* compute initial freq. table */

  /* --- remove unsupported and merge single class values --- */
  valcnt = att_valcnt(as_att(gi->attset, attid));
  imax   = 0; rtscnt = 0;       /* initialize the variables and */
  fmax   = 0; sets = gi->sets;  /* traverse the set index array */
  for (i = 0; i < valcnt; i++){ /* and the attribute values */
    if (ft_dest((FRQTAB*)gi->curr, i) != i)
      continue;                 /* skip linked/combined values */
    fdst = ft_frq_x((FRQTAB*)gi->curr, i);
    if (fdst <= 0) continue;    /* skip unsupported attribute values */
    *sets++ = i;                /* note value index in index array */
    for (cls = gi->dtree->clscnt; --cls >= 0; ) {
      fsrc = ft_frq_xy((FRQTAB*)gi->curr, i, cls);
      if (fsrc < fdst *(1-EPSILON)) { /* if no single class set, */
        if (fsrc > EPSILON) break;    /* if class is supp., abort */
        else continue;                /* otherwise continue */
      }                               /* with the next class */
      for (k = i; ++k < valcnt; ) {   /* if single class tuple set, */
        fsrc = ft_frq_x((FRQTAB*)gi->curr, k);
        if (fsrc <= 0) continue;      /* traverse remaining values */
        if (ft_frq_xy((FRQTAB*)gi->curr, k, cls) >= fsrc *(1-EPSILON)) {
          ft_comb((FRQTAB*)gi->curr, k, i); fdst += fsrc; }
      } break;                  /* if second single class set with */
    }                           /* same class found, combine values */
    if (fdst > fmax) { fmax = fdst; imax = i; }
    if (fdst >= gi->mincnt) rtscnt++;
  }                             /* count reasonable tuple sets */
  valcnt = (VALID)(sets -gi->sets);
  sets   = gi->sets;            /* compute new number of values */

  /* --- find best pair mergers --- */
  prev = ft_eval((FRQTAB*)gi->curr, gi->measure, gi->params);
  if (valcnt <= 2)              /* check whether a merge is possible */
    return (rtscnt >= 2) ? prev : WORTHLESS;
  while (1) {                   /* select and merge loop */
    best = WORTHLESS;           /* initialize worth of partition */
    isrc = idst = 0;            /* and value set indices */
    for (i = 0; i < valcnt; i++) {  /* traverse the value sets */
      if ((rtscnt <= 2)         /* if less than two reasonable sets, */
      &&  (i == imax)) continue;    /* skip mergers with largest set */
      for (k = i+1; k < valcnt; k++) {    /* traverse remaining sets */
        if ((rtscnt <= 2)       /* if less than two reasonable sets, */
        &&  (k == imax)) continue;  /* skip mergers with largest set */
        ft_comb((FRQTAB*)gi->curr, sets[k], sets[i]);/* combine sets */
        curr = ft_eval((FRQTAB*)gi->curr, gi->measure, gi->params);
        ft_uncomb((FRQTAB*)gi->curr, sets[k]); /* evaluate partition, */
        if (curr <= best) continue;            /* then uncombine sets */
        best = curr;            /* if the current partition is better */
        isrc = k;               /* than the best one found up to now, */
        idst = i;               /* note worth and value set indices */
      }                         /* of the current pair merger */
    }
    if (best <= WORTHLESS)      /* if best partition is worthless, */
      break;                    /* abort the merge loop */
    if (gi->flags & DT_BINARY){ /* if to enforce a binary tree, */
      if (valcnt <= 2) break; } /* merge until only two values left */
    else {                      /* if best partition found is worse */
      if (best < prev) break; } /* than the previous one, abort loop */
    prev = best;                /* note the best worth */
    fsrc = ft_frq_x((FRQTAB*)gi->curr, sets[isrc]);
    if (fsrc >= gi->mincnt)     /* get the source frequency */
      rtscnt--;                 /* and update reasonable tuple sets */
    fdst = ft_frq_x((FRQTAB*)gi->curr, sets[idst]);
    if (fdst >= gi->mincnt)     /* get the destination frequency */
      rtscnt--;                 /* and update reasonable tuple sets */
    ft_comb(gi->curr, sets[isrc], sets[idst]);
    fdst = ft_frq_x((FRQTAB*)gi->curr, sets[idst]);
    if (fdst >= gi->mincnt)     /* combine sets, get new frequency, */
      rtscnt++;                 /* and update reasonable tuple sets */
    if (fdst >= fmax) { fmax = fdst; imax = idst; }
    if (imax > isrc) imax--;    /* adapt array index of largest set */
    for (i = isrc; i < valcnt-1; i++)
      sets[i] = sets[i+1];      /* remove source set from array */
    valcnt--;                   /* (shift to preserve value order) */
  }                             /* and reduce number of values */
  if (rtscnt < 2) return WORTHLESS;
  if ((gi->flags & DT_BINARY) && (valcnt > 2)) return WORTHLESS;
  return prev;                  /* return the evaluation result */
}  /* nom_set() */

/*--------------------------------------------------------------------*/

static double nom_met (GROW *gi, TUPLE **tpls, TPLID n,
                       ATTID attid, double *cut)
{                               /* --- evaluate a metric attribute */
  TPLID  i;                     /* loop variable for tuples */
  ATTID  k;                     /* class attribute identifier */
  int    type;                  /* type of the test attribute */
  VALID  cls;                   /* class of current tuple */
  INST   *val, *pval;           /* value of current and prec. tuple */
  double wgt,  sum = 0;         /* tuple weight, sum of tuple weights */
  double curr, best;            /* current and best cut worth */

  assert(gi && tpls && (n > 0) && cut);   /* check the function args. */

  /* --- build initial frequency table --- */
  type = att_type(as_att(gi->attset, attid));
  ptr_qsort(tpls, (size_t)n, +1, (type == AT_FLT)
            ? cmp_flt : cmp_int, (void*)(ptrdiff_t)attid);
  ft_init((FRQTAB*)gi->mett, 2, gi->dtree->clscnt);
  k = gi->dtree->trgid;         /* sort tuples and initialize table */
  while (1) {                   /* traverse tuples with null value */
    if (n <= 1)                 /* if there is at most one tuple */
      return WORTHLESS;         /* with a known att. value, abort */
    val = tpl_colval(*tpls, attid);  /* get the next value */
    if ((type == AT_INT) ? !isnull(val->i) : !isnan(val->f))
      break;                    /* if the value is known, abort loop */
    n--;                        /* go to the next tuple */
    cls = tpl_colval (*tpls, k)->n;    /* get the class */
    wgt = tpl_getxwgt(*tpls++); /* and the tuple weight */
    ft_add((FRQTAB*)gi->mett, -1, cls, wgt);
  }                             /* store the tuple weight */
  do {                          /* traverse tuples with known value */
    if (--n <= 0)               /* if there is no tuple left for */
      return WORTHLESS;         /* the other side of the cut, abort */
    cls = tpl_colval (*tpls, k)->n;    /* get the class */
    wgt = tpl_getxwgt(*tpls++); /* and the tuple weight */
    ft_add((FRQTAB*)gi->mett, 0, cls, wgt);
    sum += wgt;                 /* store the tuple weight */
  } while (sum < gi->mincnt);   /* while minimal size not reached */
  for (i = n; --i >= 0; ) {     /* traverse the remaining tuples */
    cls = tpl_colval (tpls[i], k)->n;  /* get the class */
    wgt = tpl_getxwgt(tpls[i]); /* and the tuple weight */
    ft_add((FRQTAB*)gi->mett, 1, cls, wgt);
  }                             /* store the tuple weight */
  ft_marg(gi->mett);            /* marginalize frequency table and */
  sum = ft_frq_x((FRQTAB*)gi->mett,1); /* get weight of upper part */
  if (sum < gi->mincnt) return WORTHLESS;

  /* --- find the best cut value --- */
  pval = tpl_colval(*(tpls-1), attid);
  best = WORTHLESS;             /* note the class and the value and */
  *cut = NAN;                   /* init. the worth and the cut value */
  while (1) {                   /* traverse the tuples above the cut */
    val = tpl_colval(*tpls, attid);  /* if next value differs */
    if ((type == AT_INT) ? (val->i > pval->i)
    :     (0.5F *(pval->f +val->f) > pval->f)) {
      curr = ft_eval((FRQTAB*)gi->mett, gi->measure, gi->params);
      if (curr >= best) {       /* evaluate the frequency table, */
        best = curr;            /* update the best cut worth and */
        ft_copy((FRQTAB*)gi->curr, gi->mett);  /* copy the table */
        *cut = (type == AT_INT) /* depending on the attribute type */
             ? (0.5F *((double)val->i +pval->i))
             : (0.5F *(        val->f +pval->f));
      }                         /* compute the cut value as the */
    }                           /* average of the adjacent values */
    if (--n <= 0) break;        /* if on the last tuple, abort loop */
    sum -= wgt = tpl_getxwgt(*tpls); /* get and sum the tuple weight */
    if (sum < gi->mincnt)       /* if the weight of the remaining */
      break;                    /* tuples is too small, abort loop */
    cls = tpl_colval(*tpls, k)->n;
    ft_move((FRQTAB*)gi->mett, 1, 0, cls, wgt);
    pval = val; tpls++;         /* move weight in frequency table, */
  }                             /* note the value, and get next tuple */
  return best;                  /* return worth of best cut */
}  /* nom_met() */

/*----------------------------------------------------------------------
  Evaluation Functions for Metric Target Attributes
----------------------------------------------------------------------*/

static double met_nom (GROW *gi, TUPLE **tpls, TPLID n,
                       ATTID attid, double *cut)
{                               /* --- evaluate a nominal attribute */
  ATTID  i;                     /* class attribute identifier */
  VALID  k, m;                  /* loop variables for values */
  VALID  val;                   /* attribute value */
  INST   *inst;                 /* instance of the target attribute */
  double trg;                   /* value of target attribute */

  assert(gi && tpls && (n > 0));/* check the function arguments */
  k = att_valcnt(as_att(gi->attset, attid));
  vt_init((VARTAB*)gi->curr, k);
  i = gi->dtree->trgid;         /* initialize the variation table */
  for (tpls += n; --n >= 0; ) { /* and traverse the tuples */
    inst = tpl_colval(*--tpls, i);
    trg  = (gi->type == AT_INT) ? (double)inst->i : (double)inst->f;
    val  = tpl_colval(*tpls, attid)->n;
    vt_add((VARTAB*)gi->curr, val, trg, tpl_getxwgt(*tpls));
  }                             /* fill the variation table */
  vt_calc(gi->curr);            /* and calculate aggregates */
  if (gi->flags & (DT_SUBSET|DT_1INN))  /* if only called to compute */
    return 0;                   /* initial table, abort the function */
  for (m = 0; --k >= 0; )       /* count reasonable tuple sets */
    if (vt_colfrq((VARTAB*)gi->curr, k) >= gi->mincnt)
      m++;                      /* there must be at least two values */
  if (m < 2) return WORTHLESS;  /* with at least mincnt cases each */
  return vt_eval((VARTAB*)gi->curr, gi->measure, gi->params);
}  /* met_nom() */              /* evaluate the variation table */

/*--------------------------------------------------------------------*/

static double met_bin (GROW *gi, TUPLE **tpls, TPLID n,
                       ATTID attid, double *cut)
{                               /* --- eval. bin splits of nom. att. */
  TPLID  m;                     /* loop variable  for tuples */
  VALID  i, k;                  /* loop variables for values */
  ATTID  clsid;                 /* class attribute identifier */
  VALID  valcnt;                /* number of values */
  VALID  val;                   /* attribute value */
  INST   *inst;                 /* instance of the target attribute */
  double trg;                   /* value of target attribute */
  double curr, best;            /* current and best worth */

  assert(gi && tpls && (n > 0));/* check the function arguments */
  clsid  = gi->dtree->trgid;    /* get class id and number of values */
  valcnt = att_valcnt(as_att(gi->attset, attid));
  best   = WORTHLESS; k = -1;   /* init. evaluation and value index */
  for (i = 0; i < valcnt; i++){ /* and the attribute values */
    vt_init((VARTAB*)gi->curr, 2);
    for (m = 0; m < n; m++) {   /* traverse the tuples */
      inst = tpl_colval(tpls[m], clsid);
      trg  = (gi->type == AT_INT) ? (double)inst->i : (double)inst->f;
      val  = tpl_colval(tpls[m], attid)->n;
      val  = (val == i) ? 1 : (val < 0) ? val : 0;
      vt_add((VARTAB*)gi->curr, val, trg, tpl_getxwgt(tpls[m]));
    }                           /* fill the variation table */
    vt_calc(gi->curr);          /* and calculate aggregates */
    if ((vt_colfrq((VARTAB*)gi->curr, 0) < gi->mincnt)
    ||  (vt_colfrq((VARTAB*)gi->curr, 1) < gi->mincnt))
      continue;                 /* skip impossible splits */
    curr = vt_eval((VARTAB*)gi->curr, gi->measure, gi->params);
    if (curr > best) { best = curr; k = i; }
  }                             /* find best evaluation */
  if (k < 0) return WORTHLESS;  /* check for a useful split */
  met_nom(gi, tpls, n, attid, cut);
  val = (k <= 0) ? 1 : 0;       /* compute initial frequency table */
  for (i = 0; i < valcnt; i++)  /* combine all but one value */
    if ((i != k) && (i != val)) vt_comb((VARTAB*)gi->curr, i, val);
  return best;                  /* return var. table evaluation */
}  /* met_bin() */

/*--------------------------------------------------------------------*/

static double met_set (GROW *gi, TUPLE **tpls, TPLID n,
                       ATTID attid, double *cut)
{                               /* -- evaluate subsets of nom. att. */
  VALID  i, k;                  /* loop variables */
  VALID  valcnt;                /* number of values/subsets */
  TPLID  rtscnt;                /* number of reasonable tuple sets */
  VALID  *sets;                 /* to traverse subset index array */
  VALID  isrc, idst, imax;      /* indices     of sets to combine */
  double fsrc, fdst, fmax;      /* frequencies of sets to combine */
  double curr, prev, best;      /* current/previous/best worth */

  assert(gi && tpls && (n > 0));     /* check the function arguments */
  met_nom(gi, tpls, n, attid, cut);  /* compute initial freq. table */

  /* --- collect supported values --- */
  valcnt = att_valcnt(as_att(gi->attset, attid));
  imax   = 0; rtscnt = 0;       /* initialize the variables and */
  fmax   = 0; sets = gi->sets;  /* traverse the set index array */
  for (i = 0; i < valcnt; i++){ /* and the attribute values */
    if (vt_dest((VARTAB*)gi->curr, i) != i)
      continue;                 /* skip linked/combined values */
    fdst = vt_colfrq((VARTAB*)gi->curr, i);
    if (fdst <= 0) continue;    /* skip unsupported attribute values */
    *sets++ = i;                /* note value index in index array */
    if (fdst > fmax) { fmax = fdst; imax = i; }
    if (fdst >= gi->mincnt) rtscnt++;
  }                             /* count reasonable tuple sets */
  valcnt = (VALID)(sets -gi->sets);
  sets   = gi->sets;            /* compute new number of values */

  /* --- find best pair mergers --- */
  prev = vt_eval((VARTAB*)gi->curr, gi->measure, gi->params);
  if (valcnt <= 2)              /* check whether a merge is possible */
    return (rtscnt >= 2) ? prev : WORTHLESS;
  while (1) {                   /* select and merge loop */
    best = WORTHLESS;           /* initialize worth of partition */
    isrc = idst = 0;            /* and value set indices */
    for (i = 0; i < valcnt; i++) {  /* traverse the value sets */
      if ((rtscnt <= 2)         /* if less than two reasonable sets, */
      &&  (i == imax)) continue;    /* skip mergers with largest set */
      for (k = i+1; k < valcnt; k++) {    /* traverse remaining sets */
        if ((rtscnt <= 2)       /* if less than two reasonable sets, */
        &&  (k == imax)) continue;  /* skip mergers with largest set */
        vt_comb((VARTAB*)gi->curr, sets[k], sets[i]);/* combine sets */
        curr = vt_eval(gi->curr, gi->measure, gi->params);
        vt_uncomb((VARTAB*)gi->curr, sets[k]); /* evaluate partition, */
        if (curr <= best) continue;            /* then uncombine sets */
        best = curr;            /* if the current partition is better */
        isrc = k;               /* then the best one found up to now, */
        idst = i;               /* note worth and value set indices */
      }                         /* of the current pair merger */
    }
    if ((best <= WORTHLESS)     /* if no useful partition found */
    ||  (best <  prev))         /* or best partition found is worse */
      break;                    /* than the previous one, abort loop */
    prev = best;                /* note the best worth */
    fsrc = vt_colfrq((VARTAB*)gi->curr, sets[isrc]);
    if (fsrc >= gi->mincnt)     /* get the source frequencies */
      rtscnt--;                 /* and update reasonable tuple sets */
    fdst = vt_colfrq((VARTAB*)gi->curr, sets[idst]);
    if (fdst >= gi->mincnt)     /* get the destination frequency */
      rtscnt--;                 /* and update reasonable tuple sets */
    vt_comb(gi->curr, sets[isrc], sets[idst]);
    fdst = vt_colfrq((VARTAB*)gi->curr, sets[idst]);
    if (fdst >= gi->mincnt)     /* combine sets, get new frequency, */
      rtscnt++;                 /* and update reasonable tuple sets */
    if (fdst > fmax) { fmax = fdst; imax = idst; }
    if (imax > isrc) imax--;    /* adapt array index of largest set */
    for (i = isrc; i < valcnt-1; i++)
      sets[i] = sets[i+1];      /* remove source set from array */
    valcnt--;                   /* (shift to preserve value order) */
  }                             /* and reduce number of values */
  if (rtscnt < 2) return WORTHLESS;
  return prev;                  /* return the evaluation result */
}  /* met_set() */

/*--------------------------------------------------------------------*/

static double met_met (GROW *gi, TUPLE **tpls, TPLID n,
                       ATTID attid, double *cut)
{                               /* --- evaluate a metric attribute */
  TPLID  i;                     /* loop variable for tuples */
  ATTID  k;                     /* class attribute identifier */
  int    type, t;               /* type of the test/target attribute */
  INST   *inst;                 /* target instance of current tuple */
  double trg;                   /* target value    of current tuple */
  INST   *val, *pval;           /* value of current and prec. tuple */
  double wgt,  sum = 0;         /* tuple weight, sum of tuple weights */
  double curr, best;            /* current and best cut worth */

  assert(gi && tpls && (n > 0) && cut);   /* check the function args. */

  /* --- build initial variation table --- */
  type = att_type(as_att(gi->attset, attid));
  ptr_qsort(tpls, (size_t)n, +1, (type == AT_FLT)
            ? cmp_flt : cmp_int, (void*)(ptrdiff_t)attid);
  vt_init((VARTAB*)gi->mett,2); /* sort tuples and initialize table */
  k = gi->dtree->trgid;         /* note the target attribute index */
  t = gi->dtree->type;          /* and the target type */
  while (1) {                   /* traverse tuples with null value */
    if (n <= 1)                 /* if there is at most one tuple */
      return WORTHLESS;         /* with a known att. value, abort */
    val = tpl_colval(*tpls, attid);   /* get the next value */
    if ((type == AT_INT) ? !isnull(val->i) : !isnan(val->f))
      break;                    /* if the value is known, abort loop */
    n--;                        /* go to the next tuple */
    inst = tpl_colval(*tpls, k);
    trg  = (t == AT_INT) ? (double)inst->i : (double)inst->f;
    wgt  = tpl_getxwgt(*tpls++);/* get the target value */
    vt_add((VARTAB*)gi->mett, -1, trg, wgt);
  }                             /* store the tuple weight */
  do {                          /* traverse tuples with known value */
    if (--n <= 0)               /* if there is no tuple left for */
      return WORTHLESS;         /* the other side of the cut, abort */
    inst = tpl_colval(*tpls, k);
    trg  = (t == AT_INT) ? (double)inst->i : (double)inst->f;
    wgt  = tpl_getxwgt(*tpls++);   /* get the target value and */
    vt_add(gi->mett, 0, trg, wgt); /* store the tuple weight */
    sum += wgt;                    /* in the variation table */
  } while (sum < gi->mincnt);   /* while minimal size not reached */
  for (i = n; --i >= 0; ) {     /* traverse the remaining tuples */
    inst = tpl_colval(tpls[i], k);
    trg  = (t == AT_INT) ? (double)inst->i : (double)inst->f;
    wgt  = tpl_getxwgt(tpls[i]);/* get the target value */
    vt_add((VARTAB*)gi->mett, 1, trg, wgt);
  }                             /* store the tuple weight */
  vt_calc((VARTAB*)gi->mett);   /* calculate aggregates and get */
  sum = vt_colfrq((VARTAB*)gi->mett,1); /* weight of upper part */
  if (sum < gi->mincnt) return WORTHLESS;

  /* --- find the best cut value --- */
  pval = tpl_colval(*(tpls-1), attid);
  best = WORTHLESS;             /* note the class and the value and */
  *cut = NAN;                   /* init. the worth and the cut value */
  while (1) {                   /* traverse the tuples above the cut */
    val = tpl_colval(*tpls, attid);  /* if next value differs */
    if ((type == AT_INT) ? (val->i > pval->i)
    :     (0.5F *(pval->f +val->f) > pval->f)) {
      curr = vt_eval((VARTAB*)gi->mett, gi->measure, gi->params);
      if (curr >= best) {       /* evaluate the variation table, */
        best = curr;            /* update the best cut worth, and */
        vt_copy((VARTAB*)gi->curr, gi->mett);   /* copy the table */
        *cut = (type == AT_INT) /* depending on the attribute type */
             ? (0.5F *((double)val->i +pval->i))
             : (0.5F *(        val->f +pval->f));
      }                         /* compute the cut value as the */
    }                           /* average of the adjacent values */
    if (--n <= 0) break;        /* if on the last tuple, abort loop */
    sum -= wgt = tpl_getxwgt(*tpls); /* get and sum the tuple weight */
    if (sum < gi->mincnt)       /* if the weight of the remaining */
      break;                    /* tuples is too small, abort loop */
    inst = tpl_colval(*tpls, k);
    trg  = (t == AT_INT) ? (double)inst->i : (double)inst->f;
    vt_move((VARTAB*)gi->mett, 1, 0, trg, wgt);
    pval = val; tpls++;         /* move weight in variation table, */
  }                             /* note the value, and get next tuple */
  return best;                  /* return worth of best cut */
}  /* met_met() */

/*----------------------------------------------------------------------
  Growing Functions
----------------------------------------------------------------------*/

static DTNODE* grow (GROW *gi, DTNODE *leaf, TUPLE **tpls, TPLID n)
{                               /* --- recursively grow tree */
  ATTID  i, k;                  /* loop variables */
  VALID  m, b;                  /* value identifier/node size */
  TPLID  r, g;                  /* number of grouped tuples */
  ATT    *att;                  /* current/test attribute */
  double curr, best;            /* current and best worth */
  double cut;                   /* cut value for best attribute */
  void   *tab;                  /* exchange buffer for eval. tables */
  DTNODE *node;                 /* created test node (subtree) */
  DTDATA *data;                 /* to traverse the data array */
  TSEL   tsel;                  /* tuple selection information */
  GRPFN  *group;                /* tuple grouping function */
  double frq, known;            /* tuple frequencies for weighting */
  double e_tree;                /* sum of subtree errors */

  assert(leaf && tpls && (n > 0)); /* check the function arguments */

  /* --- check the leaf node --- */
  gi->err = leaf->err;          /* set the number of leaf errors */
  if ((leaf->frq < 2*gi->mincnt)/* if too few tuples to divide */
  ||  (leaf->err < gi->minerr)  /* or all cases have the same value */
  ||  (gi->maxht <= 0))         /* or maximal tree height reached, */
    return leaf;                /* simply return the leaf node */

  /* --- search for a test attribute --- */
  best      = WORTHLESS;        /* clear worth of best attribute, */
  tsel.col  = -1;               /* identifier of best attribute, */
  tsel.fval = cut = NAN;        /* and cut value (metric atts.) */
  k = as_attcnt(gi->attset);    /* get the number of attributes */
  for (i = 0; i < k; i++) {     /* traverse the attributes, but */
    if (gi->used[i]) continue;  /* skip used/unusable attributes */
    att = as_att(gi->attset,i); /* evaluate acc. to attribute type */
    if (att_type(att) == AT_NOM) curr = gi->eval_nom(gi, tpls,n,i,NULL);
    else                         curr = gi->eval_met(gi, tpls,n,i,&cut);
    if (gi->flags & DT_EVAL) {  /* if only to evaluate attributes */
      gi->evals[i] = curr;      /* store the attribute evaluation */
      gi->cuts [i] = (att_type(att) == AT_NOM) ? 0 : cut;
    }                           /* store a possible cut value */
    if (curr <= best) continue; /* evaluate attribute (compute worth) */
    best      = curr;           /* if the current worth is better */
    tsel.col  = i;              /* than that of the best attribute, */
    tsel.fval = cut;            /* note worth, attribute id, and cut */
    tab       = gi->best;       /* exchange the freq./var. tables, */
    gi->best  = gi->curr;       /* so that the current table */
    gi->curr  = tab;            /* becomes the best table */
  }                             /* (much more efficient than copying) */
  if ((gi->flags & DT_LEAF)     /* if only to evaluate attributes */
  ||  (tsel.col < 0)            /* or no test attribute found */
  ||  (best     < gi->minval))  /* or the best attribute is not */
    return leaf;                /* good enough, abort the function */

  /* --- create a test node --- */
  att  = as_att(gi->attset, tsel.col);
  m    = (att_type(att) == AT_NOM) ? att_valcnt(att) : 2;
  node = (DTNODE*)malloc(sizeof(DTNODE) +(size_t)(m-1) *sizeof(DTDATA));
  if (!node) { gi->err = -1; return leaf; }
  node->flags = 0;              /* create a test node */
  node->attid = tsel.col;       /* set attribute id */
  node->cut   = tsel.fval;      /* and cut value */
  node->frq   = leaf->frq;      /* copy the node frequency, */
  node->err   = leaf->err;      /* the number of errors, */
  node->trg   = leaf->trg;      /* and the target value */
  node->size  = m;              /* set the data array size */

  /* --- create child nodes --- */
  data = node->data;            /* traverse the test values */
  for (m = 0; m < node->size; m++) {
    if (gi->type == AT_NOM) b = ft_dest(gi->best, m);
    else                    b = vt_dest(gi->best, m);
    if (b == m)                 /* if the value is not linked, */
      data[m].link = NULL;      /* clear the link pointer */
    else {                      /* if the value is linked */
      data[m].link = data +b;   /* set a link to the dest. value */
      node->flags |= DT_LINK;   /* and set the flag for node links */
    }                           /* (init. pointers for cleanup) */
  }
  if (gi->type == AT_NOM) {     /* if the target is nominal */
    for (m = node->size; --m >= 0; ) {
      if (!data[m].link         /* traverse all uncombined values */
      && (ft_frq_x((FRQTAB*)gi->best, m) >= EPSILON)) {
        data[m].child = leaf_nom(gi->dtree, (FRQTAB*)gi->best, m);
        if (!data[m].child) { delete(node); gi->err = -1; return leaf; }
      }                         /* create a leaf node */
    }                           /* for all supported values */
    frq = ft_known((FRQTAB*)gi->best); }
  else {                        /* if the target is metric */
    for (m = node->size; --m >= 0; ) {
      if (!data[m].link         /* traverse all uncombined values */
      && (vt_colfrq((VARTAB*)gi->best, m) >= EPSILON)) {
        data[m].child = leaf_met(gi->dtree, (VARTAB*)gi->best, m);
        if (!data[m].child) { delete(node); gi->err = -1; return leaf; }
      }                         /* create a leaf node */
    }                           /* for all supported values */
    frq = vt_known((VARTAB*)gi->best);
  }                             /* note the sum of the weights of */
  known  = frq;                 /* tuples with a known att. value */
  e_tree = 0;                   /* clear the number of errors */
  gi->maxht--;                  /* reduce the maximal tree height */

  /* --- branch on nominal attribute --- */
  if (att_type(att) == AT_NOM){ /* if the test attribute is nominal */
    if (!(gi->flags & (DT_SUBSET|DT_1INN)))
      gi->used[tsel.col] = -1;  /* mark attribute as used */
    tsel.nval = NV_NOM;         /* group tuples with */
    g = grp_nom(tpls, n, &tsel);/* a null attribute value */
    group     = (node->flags & DT_LINK) ? grp_set : grp_nom;
    frq       = known;          /* get the denominator of the weight */
    tsel.data = data;           /* traverse the attribute values */
    for (data += tsel.nval = node->size; --tsel.nval >= 0; ) {
      if (!(--data)->child      /* if an att. value is not supported */
      ||  islink(data, node))   /* or combined with another value, */
        continue;               /* skip the attribute value */
      r = group(tpls+g, n-g, &tsel); /* group tuples with next value */
      if (g > 0) {              /* if there are null values */
        mul_wgt(tpls, g, data->child->cut/frq);
        frq = data->child->cut; /* weight tuples with a null value, */
      }                         /* note the denom. for reweighting */
      data->child = grow(gi, data->child, tpls, r+g);
      if (gi->err < 0) { delete(node); return leaf; }
      e_tree += gi->err;        /* grow a leaf/subtree for the value */
      if (g > 0)                /* if there are null values, */
        group(tpls, r+g, &tsel);/* regroup tuples with current value */
      tpls += r; n -= r;        /* and skip processed tuples */
    }
    if (g > 0) mul_wgt(tpls, g, known/frq);
  }                             /* reweight tuples with null values */

  /* --- branch on metric attribute --- */
  else {                        /* if the test attribute is metric */
    if (att_type(att) == AT_FLT) {              group = grp_flt; }
    else { tsel.ival = (DTINT)floor(tsel.fval); group = grp_int; }
    r = group(tpls, n, &tsel);  /* group tuples greater than the cut */
    group = (att_type(att) == AT_FLT) ? grp_nullflt : grp_nullint;
    g = group(tpls+r,n-r,&tsel);/* group tuples with a null value */
    if (g > 0) {                /* if there are null values */
      mul_wgt(tpls+r, g, data[0].child->cut/known);
      frq = data[0].child->cut; /* weight tuples with null value */
    }                           /* and note freq. for reweighting */
    data[0].child = grow(gi, data[0].child, tpls+r, n-r);
    if (gi->err < 0) { delete(node); return leaf; }
    e_tree += gi->err;          /* grow a leaf/subtree for <= cut */
    if (g > 0) {                /* if there are null values, */
      group(tpls+r,n-r, &tsel); /* regroup tuples with a null value */
      mul_wgt(tpls+r, g, data[1].child->cut/frq);
      frq = data[1].child->cut; /* weight tuples with null value */
    }                           /* and note freq. for reweighting */
    data[1].child = grow(gi, data[1].child, tpls, r+g);
    if (gi->err < 0) { delete(node); return leaf; }
    e_tree += gi->err;          /* grow a leaf/subtree for > cut */
    if (g > 0) {                /* if there are null values, */
      group(tpls, r+g, &tsel);  /* regroup tuples with null */
      mul_wgt(tpls, g, known/frq);
    }                           /* reweight tuples with null value */
  }
  gi->maxht++;                  /* restore maximal (sub)tree height */

  /* --- test subtree against leaf --- */
  if (!(gi->flags & DT_NOPRUNE)
  &&  (leaf->err < e_tree *(1+EPSILON))) {
    delete(node);               /* if the leaf is not worse */
    gi->err = leaf->err;        /* than the subtree, */
    return leaf;                /* discard the subtree */
  }                             /* and return the leaf */
  free(leaf);                   /* if the subtree is better, */
  gi->err = e_tree;             /* discard the leaf and */
  return node;                  /* return the subtree */
}  /* grow() */

/*--------------------------------------------------------------------*/

static DTREE* cleanup (GROW *gi, int err)
{                               /* --- clean up after an error */
  if (gi->type == AT_NOM) {     /* if the target is nominal */
    if (gi->curr) ft_delete(gi->curr);
    if (gi->best) ft_delete(gi->best);
    if (gi->mett) ft_delete(gi->mett); }
  else {                        /* if the target is metric */
    if (gi->curr) vt_delete(gi->curr);
    if (gi->best) vt_delete(gi->best);
    if (gi->mett) vt_delete(gi->mett);
  }                             /* delete frequency/variation tables */
  if (gi->tpls) free(gi->tpls); /* and the tuple array */
  if (err) {                    /* if to clean up after an error */
    if (gi->dtree) dt_delete(gi->dtree, 0);
    if (gi->attset && (gi->flags & DT_DUPAS))
      as_delete(gi->attset);    /* delete grown decision tree and */
  }                             /* corresponding attribute set */
  free(gi); return NULL;        /* delete tree grow information */
}  /* cleanup() */

/*--------------------------------------------------------------------*/

DTREE* dt_grow (TABLE *table, ATTID trgid, int measure, double *params,
                double minval, ATTID maxht, double mincnt, int flags)
{                               /* --- grow dec./reg. tree from table */
  ATTID  m, i;                  /* number of attributes */
  VALID  k;                     /* loop variable for values */
  TPLID  r;                     /* loop variable for tuples */
  int    e;                     /* flagless measure, error status */
  TUPLE  **tp;                  /* to traverse the tuple array */
  TPLID  tplcnt;                /* number of tuples */
  VALID  valcnt;                /* number of possible values */
  VALID  maxcnt;                /* maximal number of values */
  ATTSET *attset;               /* attribute set */
  INST   *inst;                 /* instance of the target attribute */
  double val;                   /* value    of the target attribute */
  GROW   *gi;                   /* tree grow information */
  DTREE  *dt;                   /* created decision tree */

  assert(table);                /* check the function argument */

  /* --- create an empty decision/regression tree --- */
  attset = tab_attset(table);   /* get the attribute set */
  for (maxcnt = 2, i = m = as_attcnt(attset); --i >= 0; ) {
    if (i == trgid) continue;   /* traverse attributes except target */
    valcnt = att_valcnt(as_att(attset, i));
    if (valcnt > maxcnt) maxcnt = valcnt;
  }                             /* determine maximum number of values */
  k  = (flags & DT_SUBSET) ? maxcnt : 1;
  gi = (GROW*)calloc(1, sizeof(GROW) +(size_t)(k-1) *sizeof(VALID));
  if (!gi) return NULL;         /* create the tree grow information */
  gi->flags  = flags;           /* and store the induction flags */
  if (flags & DT_DUPAS) attset = as_clone(attset);
  if (!attset) return cleanup(gi, 1);
  gi->attset = attset;          /* duplicate att. set if requested */
  gi->type   = att_type(as_att(attset, trgid));
  if (gi->type == AT_NOM) {     /* if target attribute is nominal */
    e = measure & ~FEF_WGTD;    /* remove the flags from measure code */
    if ((e <= FEM_NONE) || (e >= FEM_UNKNOWN))
      measure = e = FEM_NONE; } /* check the selection measure code */
  else {                        /* if the target attribute is metric */
    e = measure & ~VEF_WGTD;    /* remove the flags from measure code */
    if ((e <= VEM_NONE) || (e >= VEM_UNKNOWN))
      measure = e = VEM_NONE;   /* check the selection measure code */
  }
  gi->measure = measure;        /* note measure, parameters, */
  gi->params  = params;         /* minimal value for a split, */
  gi->minval  = minval;         /* and minimal number of cases */
  gi->mincnt  = (mincnt <= 0) ? EPSILON   : mincnt;
  gi->maxht   = (maxht  <= 0) ? ATTID_MAX : (maxht-1);
  gi->flags   = flags;          /* note maximal tree height and flags */
  if (flags & DT_EVAL) {        /* if only to evaluate attributes */
    gi->evals = (double*)malloc((size_t)(m+m) *sizeof(double));
    if (!gi->evals) return cleanup(gi, 1);
    gi->cuts  = gi->evals +m;   /* create the evaluation arrays */
  }                             /* and the cut value array */

  /* --- create decision/regression tree --- */
  dt = dt_create(attset, trgid);
  if (!dt) return cleanup(gi, 1);
  gi->dtree = dt;               /* create an empty decision tree */
  dt->evals = gi->evals;        /* store the evaluation array */
  dt->cuts  = gi->cuts;         /* and the cuts array */
  gi->used = (char*)dt->frqs;   /* use freq. array for used flags */
  for (i = 0; i < m; i++)       /* traverse the attributes and */
    gi->used[i] = (att_getmark(as_att(attset, i)) >= 0) ? 0 : +1;
  gi->used[trgid] = -1;         /* target is always used */
  tp = tuples(table, trgid, &tplcnt);
  if (!tp) return cleanup(gi, 1);
  gi->tpls = tp;                /* collect tuples with known class */

  /* --- create a root node --- */
  if (gi->type == AT_NOM) {     /* if target attribute is nominal */
    gi->minerr = MINERROR;      /* set minimum error for division */
    gi->mett = ft_create(2, dt->clscnt);
    if (!gi->mett) return cleanup(gi, 1);
    ft_init((FRQTAB*)gi->mett, 1, dt->clscnt);
                                /* create and init. a freq. table */
    for (tp += r = tplcnt; --r >= 0;) {
      inst = tpl_colval(*--tp, trgid);
      ft_add((FRQTAB*)gi->mett, 0, inst->n, tpl_getxwgt(*tp));
    }                           /* aggregate the tuple weight and */
    ft_marg(gi->mett);          /* marginalize the frequency table */
    if (ft_known((FRQTAB*)gi->mett) > 0) {  /* if there are tuples */
      dt->root = leaf_nom(dt, gi->mett, 0);
      if (!dt->root) return cleanup(gi, 1);
    } }                         /* create a leaf node as the root */
  else {                        /* if target attribute is metric */
    gi->minerr = -1.0;          /* set minimum error for division */
    gi->mett = vt_create(2);    /* create a variation table */
    if (!gi->mett) return cleanup(gi, 1);
    vt_init((VARTAB*)gi->mett, 1); /* init. the variation table */
    for (tp += r = tplcnt; --r >= 0;) {
      inst = tpl_colval(*--tp, trgid);
      val  = (dt->type == AT_INT) ? (double)inst->i : (double)inst->f;
      vt_add((VARTAB*)gi->mett, 0, val, tpl_getxwgt(*tp));
    }                           /* aggregate the tuple weight and */
    vt_calc(gi->mett);          /* calculate the var. aggregates */
    if (vt_known((VARTAB*)gi->mett) > 0) { /* if there are tuples */
      dt->root = leaf_met(dt, (VARTAB*)gi->mett, 0);
      if (!dt->root) return cleanup(gi, 1);
    }                           /* create a root node */
  }                             /* from the variation table */

  /* --- recursively grow a tree --- */
  if (dt->root                  /* if a root node has been created */
  && (gi->maxht > 0)            /* and the maximum height is not zero */
  && (m > ((dt->type == AT_NOM) /* and a selection measure is given */
        ? FEM_NONE : VEM_NONE))) {
    for (i = as_attcnt(attset); --i >= 0; ) {
      if (i == trgid) continue; /* traverse attributes except class */
      valcnt = att_valcnt(as_att(attset, i));
      if (valcnt > maxcnt) maxcnt = valcnt;
    }                           /* determine maximal number of values */
    if (gi->type != AT_NOM) {   /* if target attribute is metric */
      gi->best = vt_create(maxcnt);  /* create */
      gi->curr = vt_create(maxcnt);  /* variation tables */
      if (!gi->best || !gi->curr) return cleanup(gi, 1);
      if      (flags & DT_1INN)   gi->eval_nom = met_bin;
      else if (flags & DT_SUBSET) gi->eval_nom = met_set;
      else                        gi->eval_nom = met_nom;
      gi->eval_met = met_met; } /* set the evaluation functions */
    else {                      /* if target attribute is nominal */
      gi->best = ft_create(maxcnt, dt->clscnt);
      gi->curr = ft_create(maxcnt, dt->clscnt);
      if (!gi->best || !gi->curr) return cleanup(gi, 1);
      if      (flags & DT_1INN)   gi->eval_nom = nom_bin;
      else if (flags & DT_SUBSET) gi->eval_nom = nom_set;
      else                        gi->eval_nom = nom_nom;
      gi->eval_met = nom_met;   /* create frequency tables and */
    }                           /* set the evaluation functions */
    dt->root = grow(gi, dt->root, tp, tplcnt);
  }                             /* recursively grow a decision tree */
  e = (gi->err < 0);            /* get the error status */
  cleanup(gi, e);               /* clean up the temporary objects */
  if (e) return NULL;           /* if an error occured, abort */

  /* --- compute statistics --- */
  dt->attcnt = -1;              /* invalidate the attribute counter */
  dt->height =  0;              /* determine the tree information */
  dt->total  = (!dt->root) ? 0.0 : dt->root->frq;
  dt->size   = (!dt->root) ? 0   : count(dt, dt->root, 0);
  dt->curr   = dt->root;        /* set the node cursor to the root */
  return dt;                    /* return the grown dec./reg. tree */
}  /* dt_grow() */

#endif  /* #ifdef DT_GROW */
/*----------------------------------------------------------------------
  Error Estimation Functions
----------------------------------------------------------------------*/
#ifdef DT_PRUNE

static double nom_pess (DTREE *dt, double n, double e)
{                               /* --- pessimistic error estimation */
  if (n < 0) {                  /* initialize add. number of errors */
    dt->adderr = (e < 0) ? 0 : e; return 0; }
  e += dt->adderr;              /* estimate number of errors */
  return (e > n) ? n : e;       /* and return it */
}  /* nom_pess() */

/*--------------------------------------------------------------------*/

static double nom_clvl (DTREE *dt, double n, double e)
{                               /* --- conf. level error estimation */
  double t0, t1;                /* temporary buffers */

  /* --- initialize --- */
  if (n < 0) {                  /* if to set the confidence level */
    if (e <= 0) e = DBL_MIN;    /* check the confidence level and */
    if (e >= 1) e = 1 -DBL_EPSILON;   /* correct it, if necessary */
    dt->beta = 0.5 *(1-e);      /* note beta (percentage of errors) */
    dt->z2   = unitqtl(dt->beta);
    dt->z2  *= dt->z2;          /* compute quantile of normal dist. */
    return 0;                   /* for beta = alpha/2 = (1-clvl)/2 */
  }                             /* and note its square */

  /* --- estimate errors --- */
  assert(e <= n);               /* check the function arguments */
  if (dt->z2 <= 0) return e;    /* check for a confidence level of 0 */
  if (e < EPSILON) {            /* if no errors were observed */
    return (n <= 0) ? 0 : (n *(1 -pow(dt->beta, 1/n)));
  }                             /* compute a precise upper limit */
  if (e < 1) {                  /* if less than one error observed */
    t0 = nom_clvl(dt, n, 0);    /* compute estimate for zero errors */
    t1 = nom_clvl(dt, n, 1);    /* and estimate for one error and */
    return t0 +e *(t1 -t0);     /* interpolate between these, */
  }                             /* otherwise use an approx. formula */
  t0 = n *(e +dt->z2/2 +sqrt(dt->z2 *(e *(1 -e/n) +dt->z2/4)))
     / (n +dt->z2);             /* compute the error estimate */
  return (e > t0) ? e : t0;     /* return the error estimate */
}  /* nom_clvl() */

/*----------------------------------------------------------------------
Confidence level error estimation idea:
  J.R. Quinlan.
  C4.5: Programs for Machine Learning.
  Morgan Kaufman, San Mateo, CA, USA 1992, p.40--41
Mathematical formulae used in the above function:
  R.L. Larsen and M.L. Marx.
  An Introduction to Mathematical Statistics and its Applications.
  Prentice Hall, Englewood Cliffs, NJ, USA 1986, p.278--279
----------------------------------------------------------------------*/

static double met_pess (DTREE *dt, double n, double e)
{                               /* --- pessimistic error estimation */
  if (n < 0) {                  /* initialize add. number of errors */
    dt->adderr = (e < 0) ? 0 : e; return 0; }
  return e +dt->adderr;      /* return increased sum */
}  /* met_pess() */             /* of squared errors */

/*--------------------------------------------------------------------*/

static double met_clvl (DTREE *dt, double n, double e)
{                               /* --- conf. level error estimation */
  double t0, t1;                /* temporary buffer */

  /* --- initialize --- */
  if (n < 0) {                  /* if to set the confidence level */
    if (e <= 0) e = DBL_MIN;    /* check the confidence level and */
    if (e >= 1) e = 1 -DBL_EPSILON;   /* correct it, if necessary */
    dt->z = unitqtl(0.5 *(1-e));
    return 0;                   /* compute quantile of normal dist. */
  }                             /* for beta = alpha/2 = (1-clvl)/2 */

  /* --- estimate errors --- */
  t0 = 2*n; t1 = sqrt(t0) +dt->z;  /* compute denominator of fraction */
  if (t1 <= 0) t1 = (1.0/MAXFACT)*t0;
  else         t1 *= t1;        /* check and adapt the denominator */
  t0 /= t1; if (t0 > MAXFACT) t0 = MAXFACT;    /* bound the factor */
  return t0*e;                  /* return the increased sum */
}  /* met_clvl() */             /* of squared errors */

/*----------------------------------------------------------------------
The above function computes a very coarse approximation of the upper
bound of a confidence interval for the sum of squared errors based on
the upper bound of the confidence interval for the variance:
  u(Var,clvl) = \frac{(n-1) Var^2}{\chi^2_{(1-clvl)/2}
              = \frac{sum of squared errors}{\chi^2_{(1-clvl)/2},
where clvl is the chosen confidence level and \chi^2_p the p quantile
of the \chi^2 distribution.
Source: K. Bosch.
        Elementare Einf\"uhrung in die angewandte Statistik.
        Vieweg, Braunschweig/Wiesbaden, 1995
For the quantile of the \chi^2 distribution the approximation
  \chi^2_p \approx 0.5*(\sqrt(2n-1) +z_p)^2        (same source)
is used, with 2n-1 replaced by 2n to avoid problems with very small n
(z_p is the p quantile of the normal distribution). Of course, this
approximation is justifiable only for large n (n >= 30 is a common rule
of thumb), but since the whole approach is based on crude heuristics
anyway ... (normal distribution assumption is usually not satisfied,
set of cases is not a random sample etc.)
----------------------------------------------------------------------*/

/*----------------------------------------------------------------------
  Pruning Functions
----------------------------------------------------------------------*/

static DTNODE* prune (PRUNE *pi, DTNODE *node)
{                               /* --- recursively prune dec. tree */
  VALID  i;                     /* loop variable */
  double e_leaf, e_tree;        /* errors of the leaf/subtree */
  double f;                     /* relative frequency of 1st class */

  assert(pi && node);           /* check the function arguments */
  e_leaf  = (pi->est) ? pi->est(pi->dtree, node->frq, node->err) : 0;
  pi->err = e_leaf;             /* estimate errors for a leaf */
  node->flags &= ~DT_SEL;       /* clear the node selection flag */
  if (node->flags & DT_LEAF) {  /* if the node is a leaf */
    if (pi->thsel != 0) {       /* if to select by class frequency */
      f = node->data[0].frq +node->data[1].frq;
      f = node->data[0].frq /((f > 0) ? f : 1);
      if (((pi->thsel > 0) && (f >=  pi->thsel))
      ||  ((pi->thsel < 0) && (f <= -pi->thsel)))
        node->flags |= DT_SEL;  /* set the node selection flag */
    }                           /* based on the 1st class freq. */
    return node;                /* there is nothing else to do */
  }
  if (pi->maxht <= 0)           /* if maximal tree height is reached, */
    return collapse(pi->dtree, node);    /* collapse tree into a leaf */
  pi->maxht--;                  /* reduce maximal (sub)tree height */
  e_tree = 0;                   /* traverse the data array */
  for (i = node->size; --i >= 0; ) {
    if (!node->data[i].child || islink(node->data+i, node))
      continue;                 /* skip empty and linked subtrees */
    node->data[i].child = prune(pi, node->data[i].child);
    node->flags |= node->data[i].child->flags & DT_SEL;
    e_tree += pi->err;          /* prune the subtree (branch) */
  }                             /* and sum the number of errors */
  pi->maxht++;                  /* restore maximal (sub)tree height */
  if (((pi->thsel != 0) && !(node->flags & DT_SEL))
  ||  (pi->est && (e_leaf < e_tree *(1+EPSILON)))) {
    pi->err = e_leaf;           /* if to prune and leaf is not worse */
    return collapse(pi->dtree, node);
  }                             /* collapse the subtree into */
  pi->err = e_tree;             /* a leaf (prune the tree), */
  return node;                  /* otherwise keep the subtree */
}  /* prune() */

/*--------------------------------------------------------------------*/

static DTNODE* pr_tab (PRUNE *pi, DTNODE *node,
                       TUPLE **tpls, TPLID n, int exec)
{                               /* --- recursively prune with table */
  TPLID  i, k;                  /* loop variables for tuples */
  VALID  c;                     /* loop variable for values/children */
  ATTID  trgid;                 /* target attribute identifier */
  int    type;                  /* type of test attribute */
  TUPLE  **tp;                  /* to traverse the tuple array */
  DTDATA *data;                 /* to traverse the data array */
  INST   *inst;                 /* instance of the target attribute */
  DTNODE *leaf;                 /* buffer for leaf */
  DTNODE *lbra;                 /* buffer for largest branch */
  double lbcnt;                 /* number of tuples in largest branch */
  TSEL   tsel;                  /* tuple selection information */
  GRPFN  *group;                /* tuple grouping function */
  double e_leaf,e_bran,e_tree;  /* errors of the leaf/branch/subtree */
  double frq, t0, t1;           /* temporary buffers */

  assert(pi && node && tpls && (n >= 0)); /* check the function args. */

  /* --- create a leaf node --- */
  if (n <= 0) {                 /* if there are no tuples, there */
    leaf = NULL; e_leaf = 0; }  /* can be no leaf and no errors */
  else {                        /* if there are tuples */
    trgid = pi->dtree->trgid;   /* get the target attribute index */
    if (pi->type == AT_NOM) {   /* if target attribute is nominal */
      ft_init(pi->tab, 1, pi->dtree->clscnt); /* init. the  table */
      for (tp = tpls +(i = n); --i >= 0; ) {
        inst = tpl_colval(*--tp, trgid);   /* traverse the tuples */
        ft_add((FRQTAB*)pi->tab, 0, inst->n, tpl_getxwgt(*tp));
      }                         /* aggregate the tuple weights and */
      ft_marg((FRQTAB*)pi->tab);/* marginalize the frequency table */
      leaf = leaf_nom(pi->dtree, (FRQTAB*)pi->tab, 0);
      }                         /* create a new leaf node */
    else {                      /* if target attribute is metric */
      vt_init(pi->tab, 1);      /* initialize the variation table */
      for (tp = tpls +(i = n); --i >= 0; ) {
        inst = tpl_colval(*--tp, trgid);   /* traverse the tuples */
        t0 = (pi->type == AT_INT) ? (double)inst->i : (double)inst->f;
        vt_add((VARTAB*)pi->tab, 0, t0, tpl_getxwgt(*tp));
      }                         /* aggregate the tuple weights and */
      vt_calc((VARTAB*)pi->tab);/* marginalize the variation table */
      leaf = leaf_met(pi->dtree, (VARTAB*)pi->tab, 0);
    }                           /* create a new leaf node */
    if (!leaf) { pi->err = -1; return node; }
    e_leaf = (pi->est) ? pi->est(pi->dtree, leaf->frq, leaf->err) : 0;
  }                             /* estimate errors for a leaf */
  if (!leaf || !node            /* if no leaf created/no node exists */
  ||  (node->flags & DT_LEAF)   /* or the node is a leaf */
  ||  (pi->maxht <= 0)) {       /* or maximal tree height is reached */
    pi->err = e_leaf;           /* set the number of leaf errors */
    if (!exec) { if (leaf) free(leaf); return node; }
    if (node) delete(node);     /* if not to execute pruning, abort */
    return leaf;                /* if a node exists, delete subtree */
  }                             /* and return the created leaf node */

  /* --- reset test node --- */
  lbra = adapt(pi->attset, node);
  if (!lbra) { pi->err = -1; return node; }
  node = lbra;                  /* adapt the test node (more values) */
  if (exec) {                   /* if to execute pruning, */
    node->frq = leaf->frq;      /* copy the node frequency, */
    node->trg = leaf->trg;      /* the target value, */
    node->err = leaf->err;      /* and the number of leaf errors */
  }
  frq      = leaf->frq;         /* note the node frequency */
  tsel.col = node->attid;       /* get the column index and */
  e_tree   =  0;                /* clear the number of errors */
  lbcnt    = -1; lbra = NULL;   /* init. largest branch variables */
  pi->maxht--;                  /* reduce maximal (sub)tree height */

  /* --- branch on nominal attribute --- */
  type = att_type(as_att(pi->attset, node->attid));
  if (type == AT_NOM){          /* if test attribute is nominal */
    tp = tpls;                  /* traverse the tuple array */
    tsel.nval = NV_NOM;         /* group tuples with */
    i  = grp_nom(tp, n, &tsel); /* a null attribute value */
    t0 = sum_wgt(tp, i);        /* and sum their weights */
    t0 = frq -= t0;             /* compute weight of other tuples */
    group = (node->flags & DT_LINK) ? grp_set : grp_nom;
    tsel.data = node->data;     /* traverse attribute values */
    for (tsel.nval = 0; tsel.nval < node->size; tsel.nval++) {
      data = tsel.data +tsel.nval;
      if (islink(data, node))   /* skip all links */
        continue;               /* to other values */
      k  = group(tp+i, n-i, &tsel); /* group tuples with next value */
      t1 = sum_wgt(tp+i, k);        /* and sum their weights */
      if (t1 < EPSILON) {       /* if there are no tuples */
        if (data->child && exec) {     /* with this value */
          delete(data->child); data->child = NULL; }
        continue;               /* delete subtree and */
      }                         /* continue with next value */
      if (i > 0) {              /* weight tuples with null values */
        mul_wgt(tp, i, t1 /t0); t0 = t1; }
      data->child = pr_tab(pi, data->child, tp, i+k, exec);
      if (pi->err < 0) { free(leaf); return node; }
      e_tree += pi->err;        /* prune the subtree */
      if (t1 > lbcnt) {         /* update largest branch */
        lbcnt = t1; lbra = data->child; }
      group(tp, i+k, &tsel);    /* regroup tuples with current value */
      tp += k; n -= k;          /* and skip processed tuples */
    }
    if (i > 0) mul_wgt(tp, i, frq /t0);
    n = (TPLID)(tp -tpls) +i;   /* reweight tuples with null values */
  }                             /* and restore the tuple counter */

  /* --- branch on metric attribute --- */
  else {                        /* if test attribute is metric */
    if (type == AT_FLT) { tsel.fval = node->cut; group = grp_flt; }
    else {  tsel.ival = (DTINT)floor(node->cut); group = grp_int; }
    k  = group(tpls, n, &tsel); /* group tuples greater than the cut */
    t1 = sum_wgt(tpls, k);      /* and sum their weights */
    group = (type == AT_FLT) ? grp_nullflt : grp_nullint;
    i  = group(tpls+k, n-k, &tsel); /* group tuples with nulls, */
    t0 = sum_wgt(tpls+k, i);    /* sum their weights, and */
    t0 = frq -= t0;             /* compute weight of other tuples */
    data  = node->data;         /* get pointer to first subtree */
    lbcnt = t0 -t1;             /* compute number of tuples <= cut */
    if (lbcnt < EPSILON) {      /* if there are no such tuples, */
      if (data->child && exec){ /* delete the subtree */
        delete(data->child); data->child = NULL; } }
    else {                      /* if tuples with value <= cut exist */
      if (i > 0) {              /* weight tuples with null values */
        mul_wgt(tpls+k, i, lbcnt /t0); t0 = lbcnt; }
      data->child = pr_tab(pi, data->child, tpls+k, n-k, exec);
      if (pi->err < 0) { free(leaf); return node; }
      e_tree += pi->err;        /* prune the 1st subtree (< cut) */
      lbra = data->child;       /* note child as largest branch */
      if (i > 0) group(tpls+k, n-k, &tsel);
    }                           /* regroup tuples with null values */
    data++;                     /* get pointer to second subtree */
    if (t1 < EPSILON) {         /* if no tuples with value > cut */
      if (data->child && exec){ /* exist, delete subtree */
        delete(data->child); data->child = NULL; } }
    else {                      /* if tuples with value > cut exist */
      if (i > 0) {              /* weight tuples with null values */
        mul_wgt(tpls+k, i, t1 /t0); t0 = t1; }
      data->child = pr_tab(pi, data->child, tpls, i+k, exec);
      if (pi->err < 0) { free(leaf); return node; }
      e_tree += pi->err;        /* prune the 2nd subtree (> cut) */
      if (t1 > lbcnt) lbra = data->child;  /* update largest branch */
      if (i > 0) { group(tpls, i+k, &tsel); k = 0; }
    }                           /* regroup tuples with null values */
    if (i > 0) mul_wgt(tpls+k, i, frq/t0);
  }                             /* reweight tuples with null values */
  pi->maxht++;                  /* restore max. (sub)tree height */
  if (!pi->est) {               /* if no pruning requested, abort */
    pi->err = e_tree; free(leaf); return node; }

  /* --- check largest branch --- */
  if (!pi->chklb                /* if not to test largest branch */
  ||  !lbra                     /* or no largest branch exists */
  ||  (lbra->flags & DT_LEAF))  /* or it is a leaf node, */
    e_bran = e_leaf;            /* simply set the leaf errors, */
  else {                        /* otherwise (branch is a subtree) */
    lbra   = pr_tab(pi, lbra, tpls, n, 0);
    e_bran = pi->err;           /* compute the number of errors */
  }                             /* of the largest branch */

  /* --- prune according to results --- */
  if (!exec) {                  /* if not to execute pruning, */
    free(leaf);                 /* always discard the leaf node */
    pi->err = e_leaf;           /* determine the minimal error */
    if (e_tree *(1+EPSILON) < pi->err) pi->err = e_tree;
    if (e_bran *(1+EPSILON) < pi->err) pi->err = e_bran;
    return node;                /* return the original subtree */
  }
  if ((e_bran *(1+EPSILON) < e_leaf)    /* if the largest branch */
  &&  (e_bran *(1-EPSILON) < e_tree)) { /* is the best alternative */
    for (c = node->size; --c >= 0; ) {
      if (node->data[c].child && !islink(node->data+c, node)
      && (node->data[c].child != lbra)) /* traverse the child nodes */
        delete(node->data[c].child);    /* and delete all subtrees */
    }                                   /* except the largest branch */
    free(node);                 /* delete the subtree root */
    node = pr_tab(pi, lbra, tpls, n, 1);
    if (pi->err < 0) { free(leaf); return node; }
    e_tree = pi->err;           /* prune the largest branch */
  }                             /* and get the number of errors */
  if (e_leaf < e_tree *(1+EPSILON)) {
    delete(node);               /* if the leaf is better */
    pi->err = e_leaf;           /* than the subtree/branch, */
    return leaf;                /* discard the subtree/branch */
  }                             /* and return the leaf */
  free(leaf);                   /* if the subtree is better, */
  pi->err = e_tree;             /* discard the leaf and */
  return node;                  /* and return the subtree */
}  /* pr_tab() */

/*--------------------------------------------------------------------*/

int dt_prune (DTREE *dt, int method, double param, ATTID maxht,
              int chklb, double thsel, TABLE *table)
{                               /* --- prune a dec./reg. tree */
  TPLID  tplcnt = 0;            /* number of tuples */
  VALID  clscnt;                /* number of classes */
  double *frq;                  /* buffer for class frequency array */
  PRUNE  pi;                    /* prune information */

  assert(dt);                   /* check the function arguments */

  /* --- initialize variables --- */
  pi.dtree  = dt;               /* note the decision tree, its */
  pi.attset = dt->attset;       /* attribute set and the tree height */
  pi.maxht  = (maxht <= 0) ? ATTID_MAX : (maxht-1);
  pi.type   = dt->type;         /* note the target attribute type and */
  pi.chklb  = chklb;            /* whether to check largest branch */
  pi.thsel  = 0;                /* threshold for leaf node selection */
  switch (method) {             /* evaluate the pruning method */
    case PM_PESS: pi.est = (dt->type == AT_NOM)
                         ? nom_pess : met_pess; break;
    case PM_CLVL: pi.est = (dt->type == AT_NOM)
                         ? nom_clvl : met_clvl; break;
    default     : pi.est = (ESTFN*)0;           break;
  }                             /* set the error estimation function */
  if (pi.est) pi.est(dt, -1, param);            /* and initialize it */
  if (dt_total(dt) < 0)         /* get (or compute) */
    return -1;                  /* the total frequency */

  /* --- adapt the number of classes --- */
  if (dt->type == AT_NOM) {     /* if target attribute is nominal */
    clscnt = att_valcnt(as_att(dt->attset, dt->trgid));
    if (clscnt > dt->clscnt) {  /* get and check number of classes */
      frq = (double*)realloc(dt->frqs, (size_t)clscnt *sizeof(double));
      if (!frq) return -1;      /* enlarge the buffer array */
      dt->frqs = frq; dt->clscnt = clscnt;
    }                           /* set the new buffer array */
  }                             /* and the new number of classes */
  if (dt->root && (dt->type == AT_NOM) && (dt->clscnt == 2))
    pi.thsel = thsel;           /* threshold for leaf node selection */

  /* --- prune the decision/regression tree --- */
  if (!table) {                 /* if no table given, prune directly */
    if (dt->root) dt->root = prune(&pi, dt->root); }
  else {                        /* if a table is given */
    pi.tpls = tuples(table, dt->trgid, &tplcnt);
    if (!pi.tpls) return -1;    /* collect tuples with known class */
    if (dt->type != AT_NOM)     /* if target attribute is metric */
      pi.tab = vt_create(1);    /* create a variation table, */
    else                        /* otherwise create a freq. table */
      pi.tab = ft_create(1, dt->clscnt);
    if (!pi.tab) { free(pi.tpls); return -1; }
    dt->root = pr_tab(&pi, dt->root, pi.tpls, tplcnt, 1);
                                /* prune the tree with the table */
    if (dt->type == AT_NOM) ft_delete((FRQTAB*)pi.tab);
    else                    vt_delete((VARTAB*)pi.tab);
    free(pi.tpls);              /* delete the freq./var. table */
    if (pi.err < 0) return -1;  /* and the tuple array and */
  }                             /* check for a pruning error */

  /* --- compute statistics --- */
  dt->attcnt = -1;              /* invalidate the attribute counter */
  dt->height =  0;              /* determine the tree information */
  dt->total  = (!dt->root) ? 0.0 : dt->root->frq;
  dt->size   = (!dt->root) ? 0   : count(dt, dt->root, 0);
  dt->curr   = dt->root;        /* set the node cursor to the root */
  return 0;                     /* return 'ok' */
}  /* dt_prune() */

#endif  /* #ifdef DT_PRUNE */
