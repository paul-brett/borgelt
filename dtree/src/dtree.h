/*----------------------------------------------------------------------
  File    : dtree.h
  Contents: decision and regression tree management
  Author  : Christian Borgelt
  History : 1997.05.27 file created
            1997.08.08 first version completed
            1997.08.25 multiple child references made possible
            1997.08.26 function dt_size() added
            1997.08.28 pruning methods added
            1997.09.10 function dt_height() added
            1997.09.16 function dt_link() added
            1997.09.17 parameter 'check' added to function dt_down()
                       parameter 'mode'  added to function dt_desc()
            1998.02.02 field 'link' added to structure DTDATA
            1998.02.08 function dt_parse() transferred from parser.c
            1998.09.03 parameters 'supp' and 'err' added to dt_exec()
            1999.02.24 parameters 'params', 'minval' added to dt_grow()
            1999.03.11 definition of DT_DUPAS added
            1999.03.20 DTREE.buf changed from 'float' to 'double'
            2000.12.16 regression tree functionality added
            2001.07.19 adapted to modified module scan
            2002.01.11 number of attributes stored in DTREE structure
            2002.03.01 parameter testlb added to function dt_prune()
            2004.08.12 adapted to new module parse (deleted later)
            2004.09.13 trivial pruning of grown tree made optional
            2006.02.07 cut value changed from 'float' to 'double'
            2007.08.23 parameter 'weight' added to function dt_exec()
            2011.07.28 adapted to modified attset and utility modules
            2013.08.23 adapted to definitions ATTID, VALID, TPLID etc.
            2015.09.30 functions dt_supp() and dt_conf() added
            2015.11.12 parameter thsel added to function dt_prune()
----------------------------------------------------------------------*/
#ifndef __DTREE__
#define __DTREE__
#ifdef DT_PARSE
#ifndef SCN_SCAN
#define SCN_SCAN
#endif
#include "scanner.h"
#endif
#include "table.h"
#ifdef DT_GROW
#ifndef FT_EVAL
#define FT_EVAL
#endif
#ifndef VT_EVAL
#define VT_EVAL
#endif
#endif
#include "frqtab.h"
#include "vartab.h"
#ifdef DT_RULES
#include "rules.h"
#endif

#if VALID_MAX != DIMID_MAX
#error "dtree requires VALID_MAX == DIMID_MAX"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
/* --- pruning methods --- */
#define PM_NONE     0           /* no pruning */
#define PM_PESS     1           /* pessimistic pruning */
#define PM_CLVL     2           /* confidence level pruning */
#define PM_MDL      3           /* minimum description length pruning */
                                /* (PM_MDL not implemented yet) */
/* --- flags/modes --- */
#define DT_LEAF     0x0100      /* whether node is a leaf */
#define DT_LINK     0x0200      /* whether node contains links */
#define DT_SEL      0x0400      /* whether node is selected */
#define DT_EVAL     0x0001      /* only evaluate attributes */
#define DT_1INN     0x0002      /* implicitly encode 1-in-n */
#define DT_SUBSET   0x0004      /* try to form subsets */
#define DT_BINARY   0x0008      /* enforce a binary tree (2 subsets) */
#define DT_DUPAS    0x0010      /* duplicate attribute set */
#define DT_NOPRUNE  0x0020      /* no trivial pruning of grown tree */

/* --- description modes --- */
#define DT_TITLE    0x0001      /* print a title (as a comment) */
#define DT_INFO     0x0002      /* print add. info. (as a comment) */
#define DT_ALIGN    0x0004      /* align values of test attribute */
#define DT_REL      0x0008      /* print relative numbers */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef union dtdata {          /* --- decision tree node data --- */
  double        frq;            /* class frequency (for leaves) */
  struct dtnode *child;         /* child node (for inner nodes) */
  union  dtdata *link;          /* link to another data field */
} DTDATA;                       /* (decision tree node data) */

typedef struct dtnode {         /* --- decision tree node --- */
  int    flags;                 /* node flags, e.g. DT_LEAF */
  ATTID  attid;                 /* identifier of attribute to test */
  double cut;                   /* cut value for metric attribute */
  double frq;                   /* frequency (number of tuples) */
  double err;                   /* number of leaf errors */
  INST   trg;                   /* most frequent class/expected value */
  VALID  size;                  /* size of the data array */
  DTDATA data[1];               /* data array (children/frequencies) */
} DTNODE;                       /* (decision tree node) */

typedef struct {                /* --- decision tree --- */
  ATTSET *attset;               /* attribute set */
  ATTID  trgid;                 /* identifier of target attribute */
  ATT    *target;               /* target attribute */
  DTNODE *root;                 /* root node */
  DTNODE *curr;                 /* current node */
  DTNODE **newp;                /* position of new node */
  DTNODE **path;                /* nodes on path to current node */
  ATTID  plen;                  /* length of path (number of nodes) */
  ATTID  psize;                 /* size of the path array */
  int    type;                  /* type of target attribute */
  VALID  clscnt;                /* number of classes (nominal target) */
  ATTID  attcnt;                /* number of attributes (incl. class) */
  ATTID  height;                /* height (length of longest path) */
  ATTID  size;                  /* number of nodes (including leaves) */
  double total;                 /* total frequency (number of tuples) */
  double weight;                /* exec. weight for non-occ. values */
  double adderr;                /* add. errors for pessim. pruning */
  double beta;                  /* beta = alpha/2 = (1-clvl)/2 */
  double z;                     /* z_{alpha/2}, alpha = (1-clvl)/2 */
  double z2;                    /* z^2_beta = z^2_{alpha/2} */
  double supp;                  /* support of classification */
  double *frqs;                 /* buffer for exec. frequencies */
  double *evals;                /* attribute evaluations */
  double *cuts;                 /* cut values of metric attributes */
  TUPLE  *tuple;                /* tuple to classify */
} DTREE;                        /* (decision tree) */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/
extern DTREE*   dt_create (ATTSET *attset, ATTID trgid);
extern void     dt_delete (DTREE *dt, int delas);
extern ATTSET*  dt_attset (DTREE *dt);
extern ATT*     dt_target (DTREE *dt);
extern ATTID    dt_trgid  (DTREE *dt);
extern int      dt_type   (DTREE *dt);
extern VALID    dt_clscnt (DTREE *dt);
extern ATTID    dt_height (DTREE *dt);
extern ATTID    dt_size   (DTREE *dt);
extern double   dt_total  (DTREE *dt);
extern double*  dt_evals  (DTREE *dt);
extern double*  dt_cuts   (DTREE *dt);

extern int      dt_atleaf (DTREE *dt);
extern ATTID    dt_attid  (DTREE *dt);
extern VALID    dt_width  (DTREE *dt);
extern double   dt_cutval (DTREE *dt);
extern double   dt_freq   (DTREE *dt);
extern VALID    dt_mfcls  (DTREE *dt);
extern DTFLT    dt_value  (DTREE *dt);
extern double   dt_errs   (DTREE *dt);

extern void     dt_up     (DTREE *dt, int root);
extern int      dt_down   (DTREE *dt, VALID index, int check);
extern int      dt_node   (DTREE *dt, ATTID attid, double cutval);
extern int      dt_link   (DTREE *dt, VALID src, VALID dst);
extern VALID    dt_dest   (DTREE *dt, VALID index);
extern double   dt_setfrq (DTREE *dt, VALID index, double frq);
extern double   dt_getfrq (DTREE *dt, VALID index);

extern ATTID    dt_attchk (DTREE *dt);
extern ATTID    dt_attcnt (DTREE *dt);

#ifdef DT_GROW
extern DTREE*   dt_grow   (TABLE *table, ATTID trgid,
                           int measure, double *params, double minval,
                           ATTID maxht, double mincnt, int flags);
#endif
#ifdef DT_PRUNE
extern int      dt_prune  (DTREE *dt, int method, double param,
                           ATTID maxht, int chklb, double thsel,
                           TABLE *table);
#endif
extern int      dt_exec   (DTREE *dt, TUPLE *tuple, double weight,
                           INST *res, double *supp, double *conf);
extern double   dt_supp   (DTREE *dt);
extern double   dt_conf   (DTREE *dt, VALID clsid);

extern int      dt_desc   (DTREE *dt, FILE *file, int mode, int maxlen);
#ifdef DT_PARSE
extern DTREE*   dt_parse  (ATTSET *attset, SCANNER *scan);
#endif
#ifdef DT_RULES
extern RULESET* dt_rules  (DTREE *dt);
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define dt_attset(t)     ((t)->attset)
#define dt_target(t)     ((t)->target)
#define dt_trgid(t)      ((t)->trgid)
#define dt_type(t)       ((t)->type)
#define dt_clscnt(t)     ((t)->clscnt)
#define dt_height(t)     ((t)->height)
#define dt_size(t)       ((t)->size)
#define dt_evals(t)      ((t)->evals)
#define dt_cuts(t)       ((t)->cuts)

#define dt_atleaf(t)     ((t)->curr && ((t)->curr->flags & DT_LEAF))
#define dt_attid(t)      (((t)->curr) ? (t)->curr->attid : -1)
#define dt_width(t)      (((t)->curr) ? (t)->curr->size  : -1)
#define dt_cutval(t)     (((t)->curr) ? (t)->curr->cut   : UV_FLT)
#define dt_freq(t)       (((t)->curr) ? (t)->curr->frq   : -1)
#define dt_mfcls(t)      (((t)->curr) ? (t)->curr->trg.n : -1)
#define dt_value(t)      (((t)->curr) ? (t)->curr->trg.f : -1)
#define dt_errs(t)       (((t)->curr) ? (t)->curr->err   : -1)

#define dt_setfrq(t,i,f) ((t)->total = -1, \
                          (t)->curr->vec[i].frq = (f))
#define dt_getfrq(t,i)   ((t)->curr->vec[i].frq)

#define dt_supp(t)       ((t)->supp)
#define dt_conf(t,i)     ((t)->frqs[i])

#endif
