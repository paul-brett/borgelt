/*----------------------------------------------------------------------
  File    : dtree1.c
  Contents: decision and regression tree management, base functions
  Author  : Christian Borgelt
  History : 1997.06.12 file created (as dtree.c)
            1997.08.08 first version completed
            1997.08.25 multiple child references made possible
            1997.08.30 treatment of new values during pruning enabled
            1997.08.25 multiple child references replaced by links
            1998.02.02 links between children simplified
            1998.02.03 adapted to changed interface of module 'table'
            1998.05.27 treatment of val == cut in exec() changed
            1998.05.31 rule extraction adapted to interface changes
            1998.10.15 major revision completed
            1998.10.20 output of relative class frequencies added
            1998.12.15 minor improvements, assertions added
            1999.11.13 bug in parse error reporting removed
            2000.12.02 growing and pruning functions moved to dtree2.c
            2000.12.16 decision tree parsing simplified
            2000.12.17 regression tree functionality added
            2001.07.22 all global variables removed
            2001.08.27 bug in aggr() concerning node->cls fixed
            2001.10.12 parse error recovery corrected/improved
            2002.01.11 number of attributes stored in DTREE structure
            2004.08.12 adapted to new module parse
            2007.02.13 adapted to modified module attset
            2007.08.13 bug in exec() fixed (null/non-occurring values)
            2007.08.23 more flexible treatment of non-occurring values
            2011.07.28 adapted to modified attset and utility modules
            2013.08.23 adapted to definitions ATTID, VALID, TPLID etc.
            2014.10.22 bug in function dt_exec() fixed (support)
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "dtree.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define EPSILON     1e-12       /* to handle roundoff errors */
#define BS_PATH     32          /* block size for the path array */

/* --- error messages --- */
/* error codes 0 to -15 defined in scanner.h */
#define E_ATTEXP    (-16)       /* attribute expected */
#define E_UNKATT    (-17)       /* unknown attribute */
#define E_VALEXP    (-18)       /* attribute value expected */
#define E_UNKVAL    (-19)       /* unknown attribute value */
#define E_DUPVAL    (-20)       /* duplicate attribute value */

#define islink(d,n) (((d)->link >= (n)->data) && \
                     ((d)->link <  (n)->data +(n)->size))

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- decision tree output info --- */
  FILE    *file;                /* output file */
  ATTSET  *attset;              /* attribute set of decision tree */
  int     type;                 /* target attribute type */
  int     ind;                  /* indentation (in characters) */
  int     pos;                  /* position in output line */
  int     max;                  /* maximal line length */
  int     mode;                 /* description mode, e.g. DT_ALIGN */
  char    buf[4*AS_MAXLEN+256]; /* output buffer */
} DTOUT;                        /* (decision tree output info) */

#ifdef DT_RULES
typedef struct {                /* --- rule extraction info --- */
  RULESET *ruleset;             /* rule set to add to */
  ATTSET  *attset;              /* attribute set */
  int     type;                 /* target type */
  VALID   subset[1];            /* subset of attribute values */
} DTRULE;                       /* (rule extraction info) */
#endif

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
#ifdef DT_PARSE
static const char *msgs[] = {   /* error messages */
  /*      0 to  -7 */  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /*     -8 to -15 */  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* E_ATTEXP  -16 */  "#attribute expected instead of '%s'",
  /* E_UNKATT  -17 */  "#unknown attribute '%s'",
  /* E_VALEXP  -18 */  "#attribute value expected instead of '%s'",
  /* E_UNKVAL  -19 */  "#unknown attribute value '%s'",
  /* E_DUPVAL  -20 */  "#duplicate attribute value '%s'",
  /* E_CMPOP   -21 */  "invalid comparison operator '%s'",
};
#endif

/*----------------------------------------------------------------------
  Recursive Parts of Main Functions
----------------------------------------------------------------------*/

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

static void aggr (DTNODE *node, VALID n, double *buf)
{                               /* --- aggregate node data */
  VALID  i, k;                  /* loop variables */
  DTDATA *data;                 /* to traverse data array */
  double *frq;                  /* to traverse the class frequencies */
  double sum;                   /* sum of the class frequencies */

  assert(node && buf);          /* check the function arguments */

  /* --- leaf node --- */
  if (node->flags & DT_LEAF) {  /* if the node is a leaf */
    if (n < 0) {                /* if target attribute is metric */
      assert(n == -3);          /* check the data array size */
      buf[0] += node->frq;      /* compute sums over f, f*v, f*v*v */
      buf[1] += sum = node->frq *(double)node->trg.f;
      buf[2] += node->err + sum *(double)node->trg.f; }
    else {                      /* if target attribute is nominal */
      assert(node->size == n);  /* check the data array size */
      data = node->data;        /* get the class frequency array */
      for (sum = 0.0, i = k = 0; i < n; i++) {
        sum    += data[i].frq;  /* traverse the class frequencies */
        buf[i] += data[i].frq;  /* sum and aggregate for classes */
        if (data[i].frq > data[k].frq) k = i;
      }                         /* find the most frequent class */
      node->frq   = sum;        /* note the node frequency and */
      node->err   = sum -data[k].frq;  /* the number of errors */
      node->trg.n = k;          /* note the most frequent class */
    }
    return;                     /* leaves terminate the recursion, */
  }                             /* so abort the function */

  /* --- test node --- */
  k    = (n < 0) ? -n : n;      /* clear the class frequency array */
  frq  = memset(buf +k, 0, (size_t)k *sizeof(double));
  data = node->data;            /* traverse the child array */
  for (i = node->size; --i >= 0; ) {
    if (data[i].child && !islink(data+i, node))
      aggr(data[i].child, n, frq);
  }                             /* aggregate frequencies recursively */
  if (n < 0) {                  /* if target attribute is metric */
    assert(n == -3);            /* check the data array size */
    buf[0] += frq[0];           /* aggregate sums over f, f*v, f*v*v */
    buf[1] += frq[1];           /* for the next higher level and */
    buf[2] += frq[2];           /* compute the node information */
    sum = frq[1] /((frq[0] > 0) ? frq[0] : 1.0);
    node->frq   = frq[0];       /* note the node frequency and the */
    node->err   = frq[2] -sum *frq[1];    /* sum of squared errors */
    node->trg.f = (DTFLT)sum; } /* note the mean value for the pred. */
  else {                        /* if target attribute is nominal */
    for (sum = 0.0, i = k = 0; i < n; i++) {
      sum    += frq[i];         /* traverse the class frequencies */
      buf[i] += frq[i];         /* sum and aggregate for classes */
      if (frq[i] > frq[k]) k = i;
    }                           /* find maximal class frequency */
    node->frq   = sum;          /* note the node frequency */
    node->err   = sum -frq[k];  /* and the number of errors */
    node->trg.n = k;            /* determine the most freq. class */
  }
}  /* aggr() */

/*--------------------------------------------------------------------*/

static void mark (ATTSET *attset, DTNODE *node)
{                               /* --- mark occurring attributes */
  VALID i;                      /* loop variable */

  assert(attset && node);       /* check the function arguments */
  if (node->flags & DT_LEAF)    /* if the node is a leaf, abort, */
    return;                     /* otherwise mark the test attribute */
  att_setmark(as_att(attset, node->attid), 1);
  for (i = 0; i < node->size; i++)
    if (node->data[i].child && !islink(node->data+i, node))
      mark(attset, node->data[i].child);
}  /* mark() */                 /* recursively mark the attributes */

/*----------------------------------------------------------------------
  Main Functions
----------------------------------------------------------------------*/

DTREE* dt_create (ATTSET *attset, ATTID trgid)
{                               /* --- create a decision tree */
  ATTID  colcnt;                /* number of table columns */
  VALID  clscnt;                /* number of classes (nom. target) */
  VALID  k;                     /* size of the buffer */
  ATT    *att;                  /* target attribute */
  int    type;                  /* type of the target attribute */
  DTREE  *dt;                   /* created decision tree */
  size_t y, z;                  /* size of array */

  assert(attset);               /* check the function argument */
  colcnt = as_attcnt(attset);   /* get the number of columns */
  if ((trgid < 0) || (trgid >= colcnt))
    trgid = colcnt-1;           /* check and adapt the target id */
  att  = as_att(attset, trgid); /* get the target attribute */
  type = att_type(att);         /* and its data type */
  if (type != AT_NOM) {         /* if target attribute is metric, */
    clscnt = 0; k = 3; }        /* there are no classes */
  else {                        /* if target attribute is nominal */
    clscnt = k = att_valcnt(att);
    assert(clscnt >= 1);        /* get and check */
  }                             /* the number of classes */
  dt = (DTREE*)malloc(sizeof(DTREE));
  if (!dt) return NULL;         /* create a dec./reg. tree body */
  z = (size_t)k      *sizeof(double);
  y = (size_t)colcnt *sizeof(char);
  if (y > z) z = y;             /* compute the array size */
  dt->frqs = (double*)malloc(z);/* create frequencies and used array */
  if (!dt->frqs) { free(dt); return NULL; }
  dt->evals  = dt->cuts = NULL; /* initialize the fields: */
  dt->attset = attset;          /* note the attribute set, */
  dt->target = att;             /* the target attribute, */
  dt->trgid  = trgid;           /* the target's id, */
  dt->type   = type;            /* the target's type */
  dt->clscnt = clscnt;          /* and the number of classes */
  dt->attcnt = 1;               /* clear the number of attributes, */
  dt->size   = dt->height = 0;  /* the number of nodes, the height, */
  dt->total  = 0.0;             /* the total of frequencies, */
  dt->adderr = 0.5;             /* add. errors for pessim. pruning */
  dt->beta   = 0.25;            /* beta = alpha/2 = (1-clvl)/2 */
  dt->z      = -0.67449;        /* z_{alpha/2}, alpha = (1-clvl)/2 */
  dt->z2     = 0.454936;        /* z^2_beta = z^2_{alpha/2} */
  dt->root   = dt->curr = NULL; /* and the node pointers */
  dt->newp   = &(dt->root);     /* (no root node is created) */
  dt->path   = NULL;            /* do not allocate a path */
  dt->plen   = dt->psize = 0;   /* until it is needed */
  return dt;                    /* return created decision tree */
}  /* dt_create() */

/*--------------------------------------------------------------------*/

void dt_delete (DTREE *dt, int delas)
{                               /* --- delete a decision tree */
  assert(dt);                   /* check argument */
  if (dt->root)  delete(dt->root);      /* recursively delete nodes */
  if (dt->path)  free(dt->path);        /* delete the path array */
  if (dt->evals) free(dt->evals);       /* delete the att. evals. */
  if (dt->frqs)  free(dt->frqs);        /* and the buffer array */
  if (delas)     as_delete(dt->attset); /* delete attribute set */
  free(dt);                     /* delete decision tree body */
}  /* dt_delete() */

/*--------------------------------------------------------------------*/

void dt_up (DTREE *dt, int root)
{                               /* --- go up in decision tree */
  assert(dt);                   /* check argument */
  if (root || (dt->plen <= 0)) {
    dt->curr = dt->root;        /* if to reset to root, */
    dt->plen = 0; }             /* make the root the current node */
  else                          /* if to go up one level */
    dt->curr = dt->path[--dt->plen];
}  /* dt_up() */                /* get preceding node in path */

/*--------------------------------------------------------------------*/

int dt_down (DTREE *dt, VALID index, int check)
{                               /* --- go down in decision tree */
  DTNODE **path;                /* new path array */
  ATTID  size;                  /* new size of path array */

  assert(dt);                   /* check arguments */
  if (!dt->curr) return -2;     /* and current node */
  assert((index >= 0) && (index < dt->curr->size));
  if (check)                    /* if only to check for child node */
    return (dt->curr->data[index].child) ? 0 : 1;
  if (dt->plen >= dt->psize) {  /* if the path array is full, */
    size = dt->psize +BS_PATH;  /* enlarge the path array */
    path = (DTNODE**)realloc(dt->path, (size_t)size *sizeof(DTNODE*));
    if (!path) return -1;       /* allocate a new path array */
    dt->path = path; dt->psize = size;
  }                             /* set the new path array */
  dt->path[dt->plen++] = dt->curr;  /* append node to path */
  dt->newp = &(dt->curr->data[index].child);
  dt->curr = *(dt->newp);       /* get the index-th child */
  return (dt->curr) ? 0 : 1;    /* return 'ok' or 'node missing' */
}  /* dt_down() */

/*--------------------------------------------------------------------*/

int dt_node (DTREE *dt, ATTID attid, double cut)
{                               /* --- create a new tree node */
  ATT    *att;                  /* test attribute */
  DTNODE *node;                 /* new tree node */
  VALID  size;                  /* size of node data array */

  assert(dt && (attid < as_attcnt(dt->attset)));
  if (dt->curr) return -2;      /* check the current node */
  att  = (attid < 0) ? dt->target : as_att(dt->attset, attid);
  size = (att_type(att) == AT_NOM) ? att_valcnt(att) : 2;
  node = (DTNODE*)malloc(sizeof(DTNODE)
                       +(size_t)(size-1) *sizeof(DTDATA));
  if (!node) return -1;         /* create a new node and */
  node->size = size;            /* initialize its fields */
  if (attid < 0) { node->flags = DT_LEAF; node->attid = dt->trgid; }
  else           { node->flags = 0;       node->attid = attid; }
  node->flags |= att_type(att); /* set attribute id, flags */
  node->cut    = cut;           /* cut value (ignored for leaves), */
  node->frq    = 0.0;           /* node frequency (num of. cases) */
  node->err    = 0.0;           /* and number of errors */
  if (dt->type == AT_NOM) node->trg.i = 0;
  else                    node->trg.f = 0.0;
  if (attid < 0) {              /* if leaf node */
    while (--size >= 0)         /* initialize the frequency array */
      node->data[size].frq = 0.0;
    dt->total = -1; }           /* invalidate the total frequency */
  else                          /* if test node */
    while (--size >= 0)         /* initialize the child array */
      node->data[size].child = NULL;
  *(dt->newp) = dt->curr = node;/* set pointers to the new node */
  dt->size++;                   /* increase the node counter */
  if (dt->plen >= dt->height)   /* if path is longer than height, */
    dt->height = dt->plen+1;    /* update the tree height */
  dt->attcnt = -1;              /* invalidate number of attributes */
  return 0;                     /* return 'ok' */
}  /* dt_node() */

/*--------------------------------------------------------------------*/

int dt_link (DTREE *dt, VALID src, VALID dst)
{                               /* --- link a child to another child */
  DTDATA *data;                 /* data array element */

  assert(dt);                   /* check the function arguments */
  if (!dt->curr) return -2;     /* and the current node */
  assert((src >= 0) && (src < dt->curr->size)
      && (dst >= 0) && (dst < dt->curr->size));
  data = dt->curr->data +src;   /* get the data array element */
  if (data->child) return -1;   /* if a child/link exists, abort */
  data->link = dt->curr->data +dst;  /* link the child nodes */
  dt->curr->flags |= DT_LINK;        /* and set the link flag */
  return 0;                     /* return 'ok' */
}  /* dt_link() */

/*--------------------------------------------------------------------*/

VALID dt_dest (DTREE *dt, VALID index)
{                               /* --- get destination of a link */
  DTDATA *data;                 /* data array element */

  assert(dt);                   /* check the function argument */
  if (!dt->curr) return -2;     /* and the current node */
  assert((index >= 0) && (index < dt->curr->size));
  data = dt->curr->data +index; /* get the data array element */
  while (islink(data, dt->curr))/* while the element is a link, */
    data = data->link;          /* follow it to the next element */
  return (VALID)(data -dt->curr->data);
}  /* dt_dest() */              /* return the destination index */

/*--------------------------------------------------------------------*/

double dt_total (DTREE *dt)
{                               /* --- get total of frequencies */
  VALID  k, n;                  /* buffer section size */
  double *buf;                  /* aggr. buffer (for all levels) */

  assert(dt);                   /* check the function argument */
  if (dt->total >= 0)           /* if the frequency total is valid, */
    return dt->total;           /* return it directly */
  if (!dt->root)                /* if there is no tree, */
    return dt->total = 0.0;     /* return 0 (no tuples) */
  if (dt->type == AT_NOM) k = n = dt->clscnt;
  else                    k = -(n = 3);
  buf = (double*)malloc((size_t)dt->height *(size_t)n *sizeof(double));
  if (!buf) return -1;          /* allocate an aggregation buffer */
  memset(buf, 0, (size_t)n *sizeof(double));
  aggr(dt->root, k, buf);       /* aggregate the class frequencies */
  free(buf);                    /* and delete the aggregation buffer */
  return dt->total = dt->root->frq;
}  /* dt_total() */             /* return the total frequency */

/*--------------------------------------------------------------------*/

ATTID dt_attchk (DTREE *dt)
{                               /* --- check occurring attributes */
  ATTID i, n = 0;               /* loop variable, counter */

  assert(dt);                   /* check the function argument */
  as_setmark(dt->attset, -1);   /* unmark all attributes */
  att_setmark(dt->target, 0);   /* except the target attribute */
  if (dt->root)                 /* mark the target and */
    mark(dt->attset, dt->root); /* and all test attributes */
  for (i = as_attcnt(dt->attset); --i >= 0; )
    if (att_getmark(as_att(dt->attset, i)) >= 0)
      n++;                      /* count occurring attributes */
  return dt->attcnt = n;        /* and return their number */
}  /* dt_attchk() */

/*--------------------------------------------------------------------*/

ATTID dt_attcnt (DTREE *dt)
{                               /* --- get the number of occ. atts. */
  if (dt->attcnt > 0)           /* if the number of atts. is valid, */
    return dt->attcnt;          /* return it directly */
  return dt->attcnt = dt_attchk(dt);
}  /* dt_attcnt() */            /* otherwise determine it */

/*----------------------------------------------------------------------
  Execution Functions
----------------------------------------------------------------------*/

static void add_frqs (DTNODE *node, double *buf, double weight)
{                               /* --- add leaf frequencies */
  VALID i;                      /* loop variable, index */

  assert(node && buf);          /* check the function arguments */
  if (node->flags & DT_LEAF)    /* if leaf node, add frequencies */
    for (i = 0; i < node->size; i++)
      buf[i] += node->data[i].frq *weight;
  else {                        /* if test node */
    for (i = node->size; --i >= 0; )
      if (node->data[i].child && !islink(node->data+i, node))
        add_frqs(node->data[i].child, buf, weight);
  }                             /* aggregate data recursively */
}  /* add_frqs() */

/*--------------------------------------------------------------------*/

static void exec (DTREE *dt, DTNODE *node)
{                               /* --- recursively classify a tuple */
  VALID  k;                     /* loop variable, index */
  int    type;                  /* type of test attribute */
  DTDATA *data;                 /* to traverse the data array */
  double *frq, t;               /* to traverse the frequencies */
  DTINT  i;                     /* value of integer attribute */
  DTFLT  f;                     /* value of float   attribute */

  assert(dt && node);           /* check the function arguments */

  /* --- leaf node --- */
  if (node->flags & DT_LEAF) {  /* if the node is a leaf */
    frq = dt->frqs;             /* get the buffer array of the tree */
    if (dt->type != AT_NOM) {   /* if target attribute is metric, */
      frq[0] += node->frq;      /* aggregate the prediction data */
      frq[1] += t = node->frq *(double)node->trg.f;
      frq[2] += node->err +t  *(double)node->trg.f; }
    else {                      /* if target attribute is nominal */
      assert(node->size == dt->clscnt);
      for (k = 0; k < node->size; k++)
        frq[k] += node->data[k].frq;
    } return;                   /* sum the class frequencies */
  }                             /* and terminate the recursion */

  /* --- test node --- */
  type = att_type(as_att(dt->attset, node->attid));
  if      (type == AT_NOM) {    /* if nominal attribute */
    if (dt->tuple) k = tpl_colval(dt->tuple, node->attid)->n;
    else           k = att_inst(as_att(dt->attset, node->attid))->n; }
  else if (type == AT_INT) {    /* if integer attribute */
    if (dt->tuple) i = tpl_colval(dt->tuple, node->attid)->i;
    else           i = att_inst(as_att(dt->attset, node->attid))->i;
    k = (isnull(i) || (i == node->cut))
      ? -1 : ((i <= node->cut) ? 0 : 1); }
  else {                        /* if real/float attribute */
    if (dt->tuple) f = tpl_colval(dt->tuple, node->attid)->f;
    else           f = att_inst(as_att(dt->attset, node->attid))->f;
    k = (isnan(f)  || (f == node->cut))
      ? -1 : ((f <= node->cut) ? 0 : 1);
  }                             /* determine the array index */

  /* --- recurse to child node --- */
  if ((k >= 0) && (k < node->size)) {
    data = node->data +k;       /* get corresponding array element */
    while (islink(data,node))   /* and follow the subset links */
      data = data->link;        /* to the true child node */
    if (data->child) {          /* if the child node exists */
      exec(dt, data->child); return; }
  }                             /* execute the subtree recursively */

  /* --- attribute value is known --- */
  if ((k >= 0) && (dt->weight >= 0)) {
    frq = dt->frqs;             /* if non-occurring known value */
    if (dt->type == AT_NOM)     /* if target attribute is nominal */
      add_frqs(node, frq, dt->weight); /* add weighted leaf frqs. */
    else {                      /* if target attribute is metric */
      frq[0] += t  = node->frq *dt->weight;
      frq[1] += t *= (double)node->trg.f;
      frq[2] += dt->weight *node->err +t *(double)node->trg.f;
    }                           /* aggregate the prediction data */
  }                             /* (use the node as a fallback) */

  /* -- attribute value is null --- */
  else {                        /* if to traverse all values */
    for (k = node->size; --k >= 0; )
      if (node->data[k].child && !islink(node->data+k, node))
        exec(dt, node->data[k].child);
  }                             /* execute the subtrees recursively */
}  /* exec() */

/*--------------------------------------------------------------------*/

int dt_exec (DTREE *dt, TUPLE *tuple, double weight,
             INST *res, double *supp, double *conf)
{                               /* --- classify tuple with dec. tree */
  VALID  i, k;                  /* loop variable, buffer size */
  double *frq;                  /* to traverse the class frequencies */
  double sum, t;                /* sum of frequencies, buffer */

  assert(dt);                   /* check the function argument */
  dt->tuple = tuple;            /* store the tuple to classify */
  if (dt->total < 0) {          /* if total is not valid, compute it */
    if (dt_total(dt) < 0) return -1; }
  dt->weight = weight;          /* note weight for non-occ. values */
  k   = (dt->type == AT_NOM) ? dt->clscnt : 3;
  frq = memset(dt->frqs, 0, (size_t)k *sizeof(double));
  if (dt->root)                 /* if a decision tree exists, */
    exec(dt, dt->root);         /* execute the decision tree */
  if (dt->type != AT_NOM) {     /* if target attribute is metric */
    sum = (frq[0] > 0) ? frq[0] : 1.0;
    res->f = (DTFLT)(t = frq[1] /sum);
    if (supp) *supp = frq[0];   /* compute average value */
    if (conf) *conf = sqrt((frq[2] -t *frq[1]) /sum); }
  else {                        /* if target attribute is nominal */
    for (sum = 0, i = k = 0; i < dt->clscnt; i++) {
      if (frq[i] > frq[k]) k = i;
      sum += frq[i];            /* find the most frequent class */
    }                           /* and sum the class frequencies */
    dt->supp = sum;             /* store this as the support */
    sum = (sum > 0) ? 1.0/sum : 1.0;
    for (i = 0; i < dt->clscnt; i++)
      frq[i] *= sum;            /* normalize the frequencies */
    res->n = k;                 /* set the classification result */
    if (supp) *supp = dt->supp; /* store classification support */
    if (conf) *conf = frq[k];   /* and   classification confidence */
  }
  return 0;                     /* return 'ok' */
}  /* dt_exec() */

/*----------------------------------------------------------------------
  Description Functions
----------------------------------------------------------------------*/

static void indent (DTOUT *out, int newline)
{                               /* --- indent output line */
  int i;                        /* loop variable */

  assert(out);                  /* check the function arguments */
  if (newline) {                /* terminate line */
    fputc('\n', out->file); out->pos = 0; }
  for (i = out->pos; i < out->ind; i++)
    fputc(' ',  out->file);     /* indent next line */
  if (out->ind > out->pos) out->pos = out->ind;
}  /* indent() */               /* note new position */

/*--------------------------------------------------------------------*/

static void clsout (DTOUT *out, DTNODE *node)
{                               /* --- output of class frequencies */
  VALID  i, k;                  /* loop variables, buffer */
  ATT    *att;                  /* class attribute */
  int    len;                   /* length of the class output */
  double r = 0;                 /* sum of class frequencies */

  att = as_att(out->attset, node->attid);
  if (out->mode & DT_REL) {     /* if to print relative numbers */
    for (i = 0; i < node->size; i++)
      r += node->data[i].frq;   /* compute the class frequency sum */
    r = (r > 0) ? 100.0/r : 1.0;
  }                             /* compute the percentage factor */
  for (i = k = 0; i < node->size; i++) {
    if (node->data[i].frq <= 0) /* if a class does not occur, */
      continue;                 /* skip it, otherwise get its name */
    if (k > 0) {                /* if not the first class value, */
      fputc(',', out->file); out->pos++; }  /* print a separator */
    len  = (int)scn_format(out->buf, att_valname(att, i), 0);
    len += sprintf(out->buf +len, ": %.16g", node->data[i].frq);
    if (out->mode & DT_REL)     /* if to print relative numbers */
      len += sprintf(out->buf +len, " (%.1f%%)", node->data[i].frq *r);
    if ((out->pos +len > out->max -4)   /* if the current line */
    &&  (out->pos      > out->ind))     /* would get too long, */
      indent(out, 1);           /* start a new line and indent */
    else if (k > 0) {           /* if there is sufficient space, */
      fputc(' ', out->file); out->pos += 1; }   /* print a blank */
    fputs(out->buf, out->file); /* print the class information */
    out->pos += len; k++;       /* compute the new position */
  }                             /* and count the printed class */
}  /* clsout() */

/*--------------------------------------------------------------------*/

static void dtout (DTOUT *out, DTNODE *node)
{                               /* --- output of decision tree */
  VALID  i, k, n;               /* loop variables */
  ATT    *att;                  /* target/test attribute */
  DTDATA *data, *d2;            /* to traverse node's data array */
  int    len, swd;              /* length of the class output */
  int    type;                  /* attribute type */

  assert(out && node);          /* check the function arguments */
  fputs("{ ", out->file);       /* start a branch and */
  out->pos = out->ind += 2;     /* compute the new position */

  /* --- print class frequencies/predicted value --- */
  if (node->flags & DT_LEAF) {  /* if the node is a leaf */
    if (out->type == AT_NOM)    /* if the target is nominal, */
      clsout(out, node);        /* print the class frequencies */
    else {                      /* if the target is metric */
      out->pos += fprintf(out->file, "%.16g ", node->trg.f);
      out->pos += fprintf(out->file, "~%.16g ", (node->frq > 0)
                          ? sqrt(node->err/node->frq) : 0);
      out->pos += fprintf(out->file, "[%.16g]", node->frq);
    }                           /* print the predicted value */
    fputs(" }", out->file);     /* terminate the leaf, */
    out->pos += 2;              /* compute the new position, */
    out->ind -= 2; return;      /* reset the indentation, */
  }                             /* and terminate the function */

  /* --- print attribute test --- */
  att = as_att(out->attset, node->attid);
  scn_format(out->buf, att_name(att), 0);
  fputc('(', out->file);        /* format and print */
  fputs(out->buf, out->file);   /* the attribute name */
  type = att_type(att);         /* get the attribute type */
  if (type != AT_NOM) fprintf(out->file, "|%.16g", node->cut);
  fputc(')', out->file);        /* print cut value for metric atts. */
  indent(out, 1);               /* start a new line for the children */
  for (i = k = 0; i < node->size; i++) {
    data = node->data +i;       /* traverse child nodes (subtrees) */
    if (!data->child || islink(data, node))
      continue;                 /* skip empty subtrees and links */
    if (k++ > 0) {              /* if this is not the first subtree, */
      fputc(',', out->file); indent(out, 1); }  /* print a separator */
    if (type != AT_NOM) {       /* if metric attribute */
      out->buf[0] = (i > 0) ? '>' : '<';
      out->buf[1] = '\0';       /* get one a character name */
      len = swd = 1; }          /* representing the comparison */
    else {                      /* if nominal attribute */
      len = (int)scn_format(out->buf, att_valname(att, i), 0);
      swd = att_valwd(att, 1);  /* get value name and column width */
      for (n = 0; n < node->size; n++) {
        if (n == i) continue;   /* traverse children except current */
        d2 = node->data +n;     /* follow link chain */
        while (islink(d2, node)) d2 = d2->link;
        if (d2 != data)         /* skip children that are */
          continue;             /* not linked to current child */
        fputs(out->buf, out->file);
        fputc(',', out->file);  /* print value name and separator */
        if (out->mode & DT_ALIGN)  /* if to align values, */
          indent(out, 1);          /* start a new line */
        len = (int)scn_format(out->buf, att_valname(att, n), 0);
      }                         /* get name of next value */
    }                           /* (in the value subset) */
    fputs(out->buf, out->file); /* print name of attribute value */
    out->pos += len;            /* calculate new position */
    out->ind += (out->mode & DT_ALIGN) ? swd : 1;
    indent(out, 0);             /* set indentation and indent */
    fputc(':', out->file); out->pos++; out->ind++;
    dtout(out, data->child);    /* print next level (recursively) */
    out->ind -= (out->mode & DT_ALIGN) ? swd : 1;
    out->ind--;                 /* reset indentation */
  }
  out->ind -= 2;                /* reset indentation */
  if ((out->pos >= out->max)    /* if line would get too long */
  || ((out->ind <= 0) && (out->pos >= out->max-1)))
    indent(out, 1);             /* start a new line */
  fputc('}', out->file); out->pos++;
}  /* dtout() */                /* terminate branch */

/*--------------------------------------------------------------------*/

int dt_desc (DTREE *dt, FILE *file, int mode, int maxlen)
{                               /* --- describe a decision tree */
  int   len, l;                 /* loop variables for comments */
  DTOUT out;                    /* output information */

  assert(dt && file);           /* check the function arguments */
  len = (maxlen > 0) ? maxlen -2 : 70;

  /* --- print header (as a comment) --- */
  if (mode & DT_TITLE) {        /* if the title flag is set */
    fputs("/*", file); for (l = len; --l >= 0; ) fputc('-', file);
    fprintf(file, (dt->type == AT_NOM)
            ? "\n  decision tree\n" : "\n  regression tree\n");
    for (l = len; --l >= 0; ) fputc('-', file);
    fputs("*/\n", file);        /* print a title header */
  }                             /* (as a comment) */
  out.max    = (maxlen <= 0) ? INT_MAX : maxlen;
  out.file   = file;            /* note the function arguments */
  out.attset = dt->attset;      /* in the output structure */
  out.mode   = mode;            /* that will be passed down */
  out.type   = dt->type;

  /* --- print decision tree --- */
  fputs("dtree(", file);        /* decision tree indicator */
  scn_format(out.buf, att_name(dt->target), 0);
  fputs(out.buf, file);         /* format and print */
  fputs(") =\n", file);         /* the target attribute's name */
  out.ind = 0;                  /* initialize the indentation */
  if (dt->root) dtout(&out, dt->root);
  else fputs("{ }", file);      /* recursively print the tree */
  fputs(";\n", file);           /* terminate the last output line */

  /* --- print additional information (as a comment) --- */
  if (mode & DT_INFO) {         /* if the add. info. flag is set */
    fputs("\n/*", file); for (l = len; --l >= 0; ) fputc('-', file);
    fprintf(file, "\n  number of attributes: %"ATTID_FMT"+1",
            dt->attcnt-1);
    fprintf(file, "\n  number of levels    : %"ATTID_FMT, dt->height);
    fprintf(file, "\n  number of nodes     : %"ATTID_FMT, dt->size);
    fprintf(file, "\n  number of tuples    : %g\n",       dt->total);
    for (l = len; --l >= 0; ) fputc('-', file);
    fputs("*/\n", file);        /* print additional tree info. */
  }                             /* (as a comment) */
  return ferror(file) ? -1 : 0; /* return the writing result */
}  /* dt_desc() */

/*----------------------------------------------------------------------
  Parse Functions
----------------------------------------------------------------------*/
#ifdef DT_PARSE

static int clsin (DTREE *dt, SCANNER *scan, DTNODE *node)
{                               /* --- read class frequencies */
  VALID  cls;                   /* class value identifier */
  int    t;                     /* buffer for token/result */
  double f;                     /* buffer for class frequency */

  assert(dt && scan && node);   /* check the function arguments */
  for (cls = 0; cls < dt->clscnt; cls++)
    node->data[cls].frq = -1.0; /* clear the class frequencies */
  while (1) {                   /* class read loop */
    t = scn_token(scan);        /* check for a name */
    if ((t != T_ID) && (t != T_NUM)) SCN_ERRVAL(scan, E_VALEXP);
    cls = att_valid(dt->target, scn_value(scan));
    if (cls < 0)                     SCN_ERRVAL(scan, E_UNKVAL);
    if (node->data[cls].frq >= 0)    SCN_ERRVAL(scan, E_DUPVAL);
    SCN_NEXT(scan);             /* get and check the class value */
    SCN_CHAR(scan, ':');        /* consume ':' */
    SCN_NUM (scan);             /* check for a number */
    f = strtod(scn_value(scan), NULL);
    if ((f < 0) || (f > DTFLT_MAX))  SCN_ERROR(scan, E_NUMBER);
    node->data[cls].frq = f;    /* set the class frequency */
    SCN_NEXT(scan);             /* and consume it */
    if (scn_token(scan) == '('){/* if a relative number follows */
      SCN_NEXT(scan);           /* consume '(' */
      SCN_NUM (scan);           /* check for a number */
      f = strtod(scn_value(scan), NULL);
      if ((f < 0) || (f > 100))      SCN_ERROR(scan, E_NUMBER);
      SCN_NEXT(scan);           /* consume the rel. frequency */
      SCN_CHAR(scan, '%');      /* consume '%' */
      SCN_CHAR(scan, ')');      /* consume ')' */
    }
    if (scn_token(scan) != ',') break;
    SCN_NEXT(scan);             /* if at the end of the list, abort, */
  }                             /* otherwise consume ',' */
  for (cls = 0; cls < dt->clscnt; cls++)
    if (node->data[cls].frq < 0) node->data[cls].frq = 0.0;
  return 0;                     /* clear the unset frequencies */
}  /* clsin() */                /* and return 'ok' */

/*--------------------------------------------------------------------*/

static VALID getval (DTREE *dt, SCANNER *scan, ATT *att)
{                               /* --- read (list of) values */
  VALID v1, v2;                 /* attribute values */
  int   t;                      /* buffer for token */

  assert(dt && scan && att);    /* check the function arguments */
  t = scn_token(scan);          /* check for a name */
  if ((t != T_ID) && (t != T_NUM)) SCN_ERRVAL(scan, E_VALEXP);
  v1 = att_valid(att, scn_value(scan));
  if (v1 < 0)                      SCN_ERRVAL(scan, E_UNKVAL);
  if (dt->curr->data[v1].child)    SCN_ERRVAL(scan, E_DUPVAL);
  SCN_NEXT(scan);               /* get and consume an attribute value */
  while (scn_token(scan) == ',') { /* read a value list (subset) */
    SCN_NEXT(scan);             /* consume ',' */
    t = scn_token(scan);        /* check for a name */
    if ((t != T_ID) && (t != T_NUM)) SCN_ERROR(scan, E_VALEXP);
    v2 = att_valid(att, scn_value(scan));
    if (v2 < 0)                      SCN_ERROR(scan, E_UNKVAL);
    if ((v2 == v1)              /* get and check the attribute value */
    ||  (dt_link(dt, v2, v1) < 0))   SCN_ERROR(scan, E_DUPVAL);
    SCN_NEXT(scan);             /* link subtrees of decision tree */
  }                             /* and consume the attribute value */
  SCN_CHAR(scan, ':');          /* consume ':' */
  return v1;                    /* return the value identifier */
}  /* getval() */

/*--------------------------------------------------------------------*/

static int dtin (DTREE *dt, SCANNER *scan)
{                               /* --- recursively read dec. tree */
  int    type;                  /* type of test attribute */
  ATT    *att;                  /* test attribute */
  ATTID  i;                     /* attribute identifier */
  VALID  k, v;                  /* value identifier, counter */
  double f;                     /* floating point number */
  int    t, err = 0;            /* token, parse error status */

  assert(dt && scan);           /* check the function arguments */
  SCN_CHAR(scan, '{');          /* consume '{' */
  if (scn_token(scan) == '}') { /* if the (sub)tree is empty, */
    SCN_NEXT(scan); return 0; } /* consume '}' and return */

  /* --- read a leaf node --- */
  if (scn_token(scan) != '(') { /* if at a leaf (no further test) */
    if (dt_node(dt, -1, NAN) != 0)
      SCN_ERROR(scan, E_NOMEM); /* create a leaf node */
    if (dt->type == AT_NOM) {   /* if the target attrib. is nominal, */
      t = clsin(dt, scan, dt->curr);
      if (t) return t; }        /* read the class frequencies */
    else {                      /* if target attribute is metric */
      SCN_NUM(scan);            /* check for a number */
      f = strtod(scn_value(scan), NULL);
      if ((f < DTFLT_MIN) || (f > DTFLT_MAX)) SCN_ERROR(scan, E_NUMBER);
      SCN_NEXT(scan);           /* consume the target value */
      dt->curr->trg.f = (DTFLT)f;           /* and store it */
      SCN_CHAR(scan, '~');      /* consume '~' */
      SCN_NUM (scan);           /* check for a number */
      f = strtod(scn_value(scan), NULL);
      if ((f < 0) || (f >= INFINITY)) SCN_ERROR(scan, E_NUMBER);
      SCN_NEXT(scan);           /* consume the prediction error */
      dt->curr->err = f*f;      /* and store its square (mse) */
      SCN_CHAR(scan, '[');      /* consume '[' */
      SCN_NUM (scan);           /* check for a number */
      f = strtod(scn_value(scan), NULL);
      if ((f < 0) || (f >= INFINITY)) SCN_ERROR(scan, E_NUMBER);
      SCN_NEXT(scan);           /* consume the node frequency */
      dt->curr->frq = f;        /* and store it */
      dt->curr->err *= f;       /* compute the sse from the mse */
      SCN_CHAR(scan, ']');      /* consume ']' */
    }
    SCN_CHAR(scan, '}'); return 0;     /* consume '}' and */
  }                             /* terminate the function */

  /* --- read a test node --- */
  SCN_NEXT(scan);               /* consume '(' */
  t = scn_token(scan);          /* check for a name */
  if ((t != T_ID) && (t != T_NUM)) SCN_ERRVAL(scan, E_ATTEXP);
  i = as_attid(dt->attset, scn_value(scan));
  if (i < 0)                       SCN_ERRVAL(scan, E_UNKATT);
  SCN_NEXT(scan);               /* get and consume the attribute */
  if (dt_node(dt, i, 0) != 0)      SCN_ERROR(scan, E_NOMEM);
  att  = as_att(dt->attset, i); /* create a test node and */
  type = att_type(att);         /* get the test attribute */
  if (type != AT_NOM) {         /* if the attribute is metric, */
    SCN_CHAR(scan, '|');        /* consume '|' */
    SCN_NUM (scan);             /* check for a number */
    f = strtod(scn_value(scan), NULL);
    if ((f < DTFLT_MIN) || (f > DTFLT_MAX)) SCN_ERROR(scan, E_NUMBER);
    dt->curr->cut = f;          /* store the cut value */
    SCN_NEXT(scan);             /* and consume it */
  }
  SCN_CHAR(scan, ')');          /* consume ')' */
  k = 0;                        /* initialize the read counter */
  while (scn_token(scan) != '}') { /* attribute value read loop */
    if (++k > 1) {              /* if this is not the first value, */
      SCN_CHAR(scan, ','); }    /* consume ',' (separator) */
    if (type != AT_NOM) {       /* --- if the attribute is metric */
      t = scn_token(scan);      /* get the next token */
      if      (t == '<') v = 0; /* if lower   than cut: index 0 */
      else if (t == '>') v = 1; /* if greater than cut: index 1 */
      else { scn_error(scan, E_UNKVAL, scn_value(scan));
             SCN_RECOVER(scan, ';', '{', '}', 0);
             err = 1; continue; }
      if (dt->curr->data[v].child) {
             scn_error(scan, E_DUPVAL, scn_value(scan));
             SCN_RECOVER(scan, ';', '{', '}', 0);
             err = 1; continue; }
      SCN_NEXT(scan);           /* consume '<' or '>' */
      SCN_CHAR(scan, ':'); }    /* consume ':' */
    else {                      /* --- if the attribute is nominal */
      v = getval(dt, scan, att);
      if (v < 0) { SCN_RECOVER(scan, ';', '{', '}', 0);
                   err = 1; continue; }
    }                           /* read a (list of) value(s) */
    if (dt_down(dt, v, 0) < 0)  /* go down in the tree */
      SCN_ERROR(scan, E_NOMEM);
    t = dtin(dt, scan);         /* get subtree recursively */
    dt_up(dt, 0);               /* go up again in the tree */
    if (t) {                    /* if an error occurred */
      if      (t < E_FWRITE) { SCN_RECOVER(scan, ';', '{', '}', 0); }
      else if (t < 0)        return t;
      err = 1;                  /* try to recover, but */
    }                           /* abort on fatal errors */
  }
  SCN_NEXT(scan);               /* consume '}' */
  return err;                   /* return parse error status */
}  /* dtin() */

/*--------------------------------------------------------------------*/

static int parse (ATTSET *attset, SCANNER *scan, DTREE **pdt)
{                               /* --- read decision tree */
  ATTID trgid;                  /* class attribute identifier */
  int   t;                      /* buffer for token/type/result */

  assert(attset && scan && pdt);/* check the function arguments */
  if ((scn_token(scan) != T_ID)
  ||  (strcmp(scn_value(scan), "dtree") != 0))
    SCN_ERROR(scan, E_STREXP, "dtree");
  SCN_NEXT(scan);               /* consume 'dtree' */
  SCN_CHAR(scan, '(');          /* consume '(' */
  t = scn_token(scan);          /* check for a name */
  if ((t != T_ID) && (t != T_NUM)) SCN_ERRVAL(scan, E_ATTEXP);
  trgid = as_attid(attset, scn_value(scan));
  if (trgid < 0)                   SCN_ERRVAL(scan, E_UNKATT);
  *pdt = dt_create(attset, trgid); /* get the target attribute and */
  if (!*pdt) SCN_ERROR(scan, E_NOMEM);  /* create a dec./reg. tree */
  SCN_NEXT(scan);               /* consume the target attribute name */
  SCN_CHAR(scan, ')');          /* consume ')' */
  SCN_CHAR(scan, '=');          /* consume '=' */
  if (dtin(*pdt, scan) != 0)    /* recursively read dec./reg. tree */
    return -1;                  /* and check for an error */
  SCN_CHAR(scan, ';');          /* consume ';' */
  return 0;                     /* return 'ok' */
}  /* parse() */

/*--------------------------------------------------------------------*/

DTREE* dt_parse (ATTSET *attset, SCANNER *scan)
{                               /* --- parse a decision tree */
  DTREE *dt = NULL;             /* created decision tree */

  assert(attset && scan);       /* check the function arguments */
  scn_setmsgs(scan, msgs, (int)(sizeof(msgs)/sizeof(*msgs)));
  scn_first(scan);              /* set messages, get first token */
  if (parse(attset, scan, &dt) != 0) {
    if (dt) dt_delete(dt, 0);   /* parse a decision tree */
    return NULL;                /* if an error occurred, */
  }                             /* delete the tree and abort */
  dt_total(dt);                 /* compute the node frequencies and */
  return dt;                    /* return the created decision tree */
}  /* dt_parse() */

#endif  /* #ifdef DT_PARSE */
/*----------------------------------------------------------------------
  Rule Extraction Functions
----------------------------------------------------------------------*/
#ifdef DT_RULES

static int rules (DTRULE *rx, DTNODE *node, RULE *path)
{                               /* --- recursively collect rules */
  RULE   *rule;                 /* created rule */
  VALID  i, k, m;               /* loop variables for values */
  ATTID  r;                     /* result of r_condadd() */
  DTDATA *data, *d2;            /* to traverse data array */
  RCVAL  val;                   /* value for rule head/condition */

  assert(rx && node);           /* check the function arguments */

  /* --- leaf node --- */
  if (node->flags & DT_LEAF) {  /* if the node is a leaf */
    rule = r_create(rx->attset, (path) ? r_condcnt(path) : 0);
    if (!rule) return -1;       /* create a new rule and */
    if (path) r_copy(rule,path);/* copy the path into it */
    if ((node->flags & AT_ALL) == AT_NOM) val.n = node->trg.n;
    else                                  val.f = node->trg.f;
    r_headset(rule, node->attid, R_EQ, &val);
    r_setsupp(rule, node->frq); /* set head and rule information */
    if (rx->type == AT_NOM)     /* if target attribute is nominal */
      r_setconf(rule, (node->frq > 0) ? 1 -node->err/node->frq : 1);
    else                        /* if target attribute is metric */
      r_setconf(rule, (node->frq > 0) ? sqrt(node->err/node->frq) : 0);
    if (rs_ruleadd(rx->ruleset, rule) < 0) {
      r_delete(rule); return -1; }
    return 0;                   /* add the rule to the rule set */
  }                             /* and return 'ok' */

  /* --- test node --- */
  rule = r_create(rx->attset, (path) ? (r_condcnt(path)+1) : 1);
  if (!rule) return -1;         /* create a new rule */
  if (att_type(as_att(rx->attset, node->attid)) != AT_NOM) {
    data  = node->data;         /* if the attribute is metric */
    val.f = (DTFLT)node->cut;   /* get the cut value */
    if (data->child) {          /* if there is a left subtree */
      if (path) r_copy(rule, path);   /* copy path into rule */
      r = r_condadd(rule, node->attid, R_LE, &val);
      if ((r == -1)             /* add condition to the rule */
      || ((r >=  0) && (rules(rx, data->child, rule) != 0))) {
        r_delete(rule); return -1; }
    }                           /* collect rules from left subtree */
    if ((++data)->child) {      /* if there is a right subtree */
      if (path) r_copy(rule, path);   /* copy path into rule or */
      else      r_condrem(rule, -1);  /* remove all conditions */
      r = r_condadd(rule, node->attid, R_GE, &val);
      if ((r == -1)             /* add condition to the rule */
      || ((r >=  0) && (rules(rx, data->child, rule) != 0))) {
        r_delete(rule); return -1; }
    } }                         /* collect rules from right subtree */
  else {                        /* if attribute is nominal */
    val.s = rx->subset;         /* set the subset array */
    for (i = 0; i < node->size; i++) {
      data = node->data +i;     /* traverse attribute values */
      if (!data->child || islink(data, node))
        continue;               /* skip empty children and links */
      for (k = m = 0; k < node->size; k++) {
        if (k == i) continue;   /* traverse children except current */
        d2 = node->data +k;     /* follow link chain */
        while (islink(d2, node)) d2 = d2->link;
        if (d2 != data)         /* skip children that are */
          continue;             /* not linked to current child */
        rx->subset[++m] = k;    /* note the value identifier */
      }                         /* in the value subset array */
      rx->subset[++m] = i;      /* store the current value */
      rx->subset[0]   = m;      /* and the number of values */
      if (path) r_copy(rule, path);   /* copy path into rule */
      else      r_condrem(rule, -1);  /* remove all conditions */
      r = r_condadd(rule, node->attid, R_IN, &val);
      if ((r == -1)             /* add condition to the rule */
      || ((r >=  0) && (rules(rx, data->child, rule) != 0))) {
        r_delete(rule); return -1; }
    }                           /* collect the rules */
  }                             /* from the subtree */
  r_delete(rule);               /* delete the rule buffer */
  return 0;                     /* and return 'ok' */
}  /* rules() */

/*--------------------------------------------------------------------*/

RULESET* dt_rules (DTREE *dt)
{                               /* --- create a rule set */
  ATTID   i;                    /* loop variable for attributes */
  VALID   n, max = 0;           /* (maximal) number of values */
  RULESET *ruleset;             /* created rule set */
  DTRULE  *rx;                  /* rule extraction info */

  assert(dt);                   /* check the function argument */
  ruleset = rs_create(dt->attset, r_delete);
  if (!ruleset || !dt->root)    /* create an empty rule set */
    return ruleset;             /* and check for an empty tree */
  for (i = as_attcnt(dt->attset); --i >= 0; ) {
    n = att_valcnt(as_att(dt->attset, i));
    if (n > max) max = n;       /* determine the maximal number */
  }                             /* of values per attribute */
  rx = (DTRULE*)malloc(sizeof(DTRULE) +(size_t)max *sizeof(VALID));
  if (!rx) { rs_delete(ruleset, 0); return NULL; }
  rx->ruleset = ruleset;        /* create a rule extraction info. */
  rx->attset  = dt->attset;     /* and note the relevant objects */
  rx->type    = dt->type;
  n = rules(rx, dt->root, NULL);/* collect rules from the tree */
  free(rx);                     /* delete the rule extraction info. */
  if (n != 0) { rs_delete(ruleset, 0); return NULL; }
  return ruleset;               /* return the created rule set */
}  /* dt_rules() */

#endif  /* #ifdef DT_RULES */
