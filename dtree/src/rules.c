/*----------------------------------------------------------------------
  File    : rules.c
  Contents: rule and rule set management
  Author  : Christian Borgelt
  History : 1998.05.17 file created
            1998.05.26 function rs_desc added
            1998.05.27 comparison operators added
            1998.06.23 adapted to modified attset functions
            1998.08.13 parameter delas added to function rs_delete
            1998.08.24 management of subsets of values added
            1998.08.26 function rs_ruleexg added
            1998.09.29 assertions added, minor improvements
            2001.07.19 global variables removed, rs_desc improved
            2002.06.06 rule and rule set parsing functions added
            2003.09.25 bug in function r_check fixed (condition dir)
            2004.08.12 adapted to new module parse
            2007.02.13 adapted to modified module attset
            2011.07.28 adapted to modified attset and utility modules
            2013.03.10 adapted to modified bsearch/bisect interface
            2013.08.23 adapted to definitions ATTID, VALID, TPLID etc.
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "rules.h"
#include "arrays.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/

#define int         1           /* to check definitions */
#define long        2           /* for certain types */
#define ptrdiff_t   3

/*--------------------------------------------------------------------*/

#if   VALID==int
#ifndef vid_qsort
#define vid_qsort   int_qsort
#endif
#ifndef vid_bsearch
#define vid_bsearch int_bsearch
#endif

#elif VALID==long
#ifndef vid_qsort
#define vid_qsort   lng_qsort
#endif
#ifndef vid_bsearch
#define vid_bsearch lng_bsearch
#endif

#elif VALID==ptrdiff_t
#ifndef vid_qsort
#define vid_qsort   dif_qsort
#endif
#ifndef vid_bsearch
#define vid_bsearch dif_bsearch
#endif

#else
#error "VALID must be either 'int', 'long' or 'ptrdiff_t'"
#endif

/*--------------------------------------------------------------------*/

#undef int                      /* remove preprocessor definitions */
#undef long                     /* needed for the type checking */
#undef ptrdiff_t

/*--------------------------------------------------------------------*/

#define BLKSIZE      256        /* block size for rule vector */
#define MARK        0x80        /* mark for conditions to adapt */
#define MSBIT         31        /* most significant bit */

/* --- error messages --- */
/* error codes 0 to -15 defined in scanner.h */
#define E_ATTEXP    (-16)       /* attribute expected */
#define E_UNKATT    (-17)       /* unknown attribute */
#define E_VALEXP    (-18)       /* attribute value expected */
#define E_UNKVAL    (-19)       /* unknown attribute value */
#define E_DUPVAL    (-20)       /* duplicate attribute value */
#define E_CMPOP     (-21)       /* invalid comparison operator */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
#ifdef RS_DESC
typedef struct {                /* --- description information --- */
  FILE   *file;                 /* output file */
  int    pos;                   /* position in output line */
  int    max;                   /* maximal length of output line */
  ATTSET *attset;               /* attribute set */
} DESC;                         /* (description information) */
#endif

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
#ifdef RS_PARSE
static const char *msgs[] = {   /* error messages */
  /*      0 to  -7 */  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /*     -8 to -15 */  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* E_ATTEXP  -16 */  "#attribute expected instead of '%s'",
  /* E_UNKATT  -17 */  "#unknown attribute '%s'",
  /* E_VALEXP  -18 */  "#attribute value expected instead of '%s'",
  /* E_UNKVAL  -19 */  "#unknown attribute value '%s'",
  /* E_DUPVAL  -20 */  "#duplicate attribute value '%s'",
  /* E_CMPOP   -21 */  "#invalid comparison operator '%s'",
};
#endif

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
#if defined RS_DESC || defined RS_PARSE
static const char *opsigns[] = {
  NULL, "=", "<", "<=", ">", ">=", "!=", NULL, "in", "in" };
#endif                          /* signs for comparison operators */

/*----------------------------------------------------------------------
  Auxiliary Functions
----------------------------------------------------------------------*/

static int valcmp (const void *p1, const void *p2)
{                               /* --- compare two attribute values */
  if (*(const VALID*)p1 < *(const VALID*)p2) return -1;
  if (*(const VALID*)p1 > *(const VALID*)p2) return  1;
  return 0;                     /* return sign of difference */
}  /* valcmp() */

/*--------------------------------------------------------------------*/

static int condset (RCOND *cond, ATTID att, int op, RCVAL *val)
{                               /* --- set a rule condition */
  VALID i;                      /* loop variable, number of values */
  VALID *s, *d, *tmp;           /* to traverse attribute values */
  int   bits;                   /* bitflag rep. of a value set */

  assert(cond && val);          /* check the function arguments */
  if (op != R_IN) {             /* if there is no value vector, */
    cond->op  = op;             /* simply copy the operator */
    cond->val = *val; }         /* and the attribute value */
  else {                        /* if there is a value vector, */
    s = val->s; i = *s;         /* get vector and number of values */
    if (i == 1) {               /* if there is only one value */
      cond->op = R_EQ; cond->val.n = s[1]; }
    else {                      /* if there is more than one value */
      for (bits = 0, s++; --i >= 0; ) {
        if (*s > MSBIT) break;  /* traverse the value vector */
        bits |= 1 << *s++;      /* set bits corresponding to values */
      }
      if (i < 0) {              /* if conversion was successful */
        cond->op = R_BF; cond->val.i = (DTINT)bits; }
      else {                    /* if conversion was not successful */
        s = val->s; i = *s+1;   /* get vector and number of values */
        d = tmp = malloc((size_t)i *sizeof(VALID));
        if (!d) return -1;      /* allocate a value vector */
        do { *d++ = *s++; } while (--i >= 0);
        vid_qsort(tmp+1, (size_t)*tmp, 1);
        cond->op = R_IN; cond->val.s = tmp;
      }                         /* copy and sort the values and */
    }                           /* set the sorted value vector */
  }                             /* as the value of the condition */
  cond->att = att;              /* set attribute identifier */
  return 0;                     /* return 'ok' */
}  /* condset() */

/*--------------------------------------------------------------------*/

static int condcopy (RCOND *dst, RCOND *src)
{                               /* --- set a rule condition */
  VALID i;                      /* loop variable, number of values */
  VALID *s, *d;                 /* to traverse attribute values */

  assert(dst && src);           /* check the function arguments */
  if (src->op != R_IN) {        /* if there is no value vector, */
    *dst = *src; return 0; }    /* simply copy the condition */
  s = src->val.s; i = *s;       /* get vector and number of values */
  d = malloc((size_t)(i+1) *sizeof(VALID));
  if (!d) return -1;            /* allocate a value vector */
  dst->att   = src->att;        /* set attribute identifier, */
  dst->op    = src->op;         /* comparison operator, */
  dst->val.s = d;               /* and new value vector */
  memcpy(d, s, (size_t)(i+1) *sizeof(VALID));
  return 0;                     /* copy value vector and return 'ok' */
}  /* condcopy() */

/*--------------------------------------------------------------------*/

static int condcmp (const void *p1, const void *p2)
{                               /* --- compare two conditions */
  const RCOND *c1 = p1, *c2 = p2;    /* conditions to compare */

  if (c1->att < c2->att) return -1;
  if (c1->att > c2->att) return  1;
  if (c1->op  < c2->op)  return  1;
  if (c1->op  > c2->op)  return -1;
  return 0;                     /* compare attribute and operator */
}  /* condcmp() */

/*--------------------------------------------------------------------*/

static int isect (RCOND *dst, RCOND *src)
{                               /* --- intersect two conditions */
                                /*     with a subset in one of them */
  VALID sn, dn;                 /* numbers of values in subsets */
  VALID *s, *d;                 /* to traverse value vectors */
  int   bits;                   /* bitflag rep. of a value set */

  assert(dst && src);           /* check the function arguments */

  /* --- only source contains a subset --- */
  if (dst->op == R_EQ) {        /* if dest. equal to some value */
    if (src->op == R_BF)        /* if bitflag rep. in source, */
      return (src->val.i & (1 << dst->val.i));   /* check bit */
    s = src->val.s; sn = *s++;  /* if vector rep. in source */
    return (bsearch(&dst->val.i, s, (size_t)sn,
            sizeof(VALID), valcmp) != NULL);
  }                             /* search for the value */
  if (dst->op == R_NE) {        /* if dest. not equal to some value */
    if (src->op == R_BF) {      /* if bitflag rep. in source, */
      dst->op    = R_BF;        /* remove value from the value set */
      dst->val.i = src->val.i & ~(1 << dst->val.i);
      return (dst->val.i != 0); /* return whether the value set */
    }                           /* without the value is not empty */
    s = src->val.s;             /* if vector rep. in source */
    return ((*s > 1) || (s[1] != dst->val.n));
  }                             /* check for remaining values */

  /* --- only destination contains a subset --- */
  if (src->op == R_EQ) {        /* if source equal to some value */
    if (dst->op == R_BF) {      /* if bitflag rep. in destination */
      dst->val.i &= 1 << src->val.n;     /* remove other values from */
      if (dst->val.i == 0) return 0; }   /* the set and check result */
    else {                      /* if vector rep. in destination */
      d = dst->val.s;           /* search the vector for the value */
      if (bsearch(&src->val.n, d+1, (size_t)*d,
                  sizeof(VALID), valcmp) == NULL) {
        *d = 0; return 0; }     /* if value not found, empty */
      free(d);                  /* the value vector and abort, */
    }                           /* otherwise delete the value vector */
    dst->op    = R_EQ;          /* replace the destination condition */
    dst->val.n = src->val.n;    /* by the source condition, which is */
    return 1;                   /* stronger, and return 'compatible' */
  }
  if (src->op == R_NE) {        /* if source not equal to some value */
    if (dst->op == R_BF) {      /* if bitflag rep. in destination */
      dst->val.i &= ~(1 << src->val.n);
      return (dst->val.i != 0); /* remove value from the set */
    }                           /* and check for an empty set */
    d  = dst->val.s;            /* search in the value vector */
    dn = (VALID)vid_bsearch(src->val.n, d+1, (size_t)*d);
    if (dn < 0) return 1;       /* if value not found, abort */
    if (*d-dn-1 > 0)            /* remove value from the vector */
      memmove(d+dn+1, d+dn+2, (size_t)(*d-dn-1) *sizeof(VALID));
    return (--(*d) > 0);        /* return whether the resulting */
  }                             /* value set is not empty */

  /* --- both conditions contain subsets --- */
  if ((src->op == R_BF)         /* if bitflag representation */
  ||  (dst->op == R_BF)) {      /* in source or in destination */
    if (dst->op == R_IN) {      /* if vector rep. in destination, */
      d = src->val.s;           /* traverse value vector of dest. */
      for (bits = 0, dn = *d++; --dn >= 0; )
        bits |= 1 << *d++;      /* set bits corresponding to values */
      dst->op = R_BF; dst->val.i = (DTINT)bits;
    }                           /* (convert dest. to bitflag rep.) */
    if (src->op == R_BF)        /* if bitflag rep. in source, */
      bits = (int)src->val.i;   /* get the bitflags */
    else {                      /* if vector rep. in source, */
      s = src->val.s;           /* traverse value vector of source */
      for (bits = 0, sn = *s++; --sn >= 0; )
        bits |= 1 << *s++;      /* set bits corresponding to values */
    }
    dst->val.i &= (DTINT)bits;  /* intersect bitflag representations */
    return (dst->val.i != 0);   /* return whether the intersection */
  }                             /* of the value sets is not empty */

  s = src->val.s; sn = *s++;    /* vectors in source and destination */
  d = dst->val.s; dn = *d++;    /* traverse both value vectors */
  if ((dn <= 0) || (sn <= 0)) { /* if either set is empty, */
    *--d = 0; return 0; }       /* the intersection is empty */
  while (1) {                   /* intersection loop */
    if      (*s < *d) {         /* source value not in destination */
      s++;       if (--sn <= 0) break; }  /* skip source value */
    else if (*s > *d) {         /* destination value not in source */
      *d++ = -1; if (--dn <= 0) break; }  /* mark dest. value */
    else {                      /* value in source and destination */
      s++, d++; if ((--dn <= 0) || (--sn <= 0)) break; }
  }                             /* skip both values */
  s = d = dst->val.s;           /* traverse destination again */
  for (dn = *s++ -= dn; --dn >= 0; s++)
    if (*s >= 0) *++d = *s;     /* group together remaining values, */
  s  = dst->val.s;              /* set new number of values, and */
  *s = (VALID)(d -s);           /* return whether the resulting */
  return (*s > 0);              /* value set is not empty */
}  /* isect() */

/*----------------------------------------------------------------------
  Rule Functions
----------------------------------------------------------------------*/

RULE* r_create (ATTSET *attset, ATTID maxcond)
{                               /* --- create a rule */
  RULE *rule;                   /* created rule */

  assert(attset && (maxcond >= 0)); /* check the function arguments */
  rule = (RULE*)malloc(sizeof(RULE)+(size_t)(maxcond-1)*sizeof(RCOND));
  if (!rule) return NULL;       /* allocate memory and */
  rule->attset   = attset;      /* initialize fields */
  rule->set      = NULL; rule->id = -1;
  rule->supp     = rule->conf = 0;
  rule->condvsz  = maxcond;
  rule->condcnt  =  0;
  rule->head.att = -1;          /* invalidate head attribute */
  rule->head.op  =  0;          /* and comparison operator */
  return rule;                  /* return the created rule */
}  /* r_create() */

/*--------------------------------------------------------------------*/

void r_delete (RULE *rule)
{                               /* --- delete a rule */
  assert(rule);                 /* check the function argument */
  if (rule->set)                /* remove rule from containing set */
    rs_rulerem(rule->set, rule->id);
  if (rule->head.op == R_IN)    /* delete value vetors */
    free(rule->head.val.s);     /* from the head and */
  r_condrem(rule, -1);          /* from all conditions */
  free(rule);                   /* delete rule body */
}  /* r_delete() */

/*--------------------------------------------------------------------*/

int r_copy (RULE *dst, RULE *src)
{                               /* --- copy a rule */
  ATTID i;                      /* loop variable */
  RCOND *sc, *dc;               /* to traverse the conditions */

  assert(src && dst             /* check the function arguments */
     && (dst->condvsz >= src->condcnt));
  if (dst->head.op == R_IN)     /* delete value vectors */
    free(dst->head.val.s);      /* from the head and */
  r_condrem(dst, -1);           /* from all conditions */
  if (condcopy(&dst->head, &src->head) != 0)
    return -1;                  /* copy the rule head */
  dst->supp    = src->supp;     /* copy rule support */
  dst->conf    = src->conf;     /* and confidence */
  dst->condcnt = src->condcnt;  /* traverse conditions */
  sc = src->conds; dc = dst->conds;
  for (i = src->condcnt; --i >= 0; dc++, sc++)
    if (condcopy(dc, sc) != 0) { dst->condcnt -= i+1; return -1; }
  return 0;                     /* copy conditions and return 'ok' */
}  /* r_copy() */

/*--------------------------------------------------------------------*/

int r_headset (RULE *rule, ATTID att, int op, RCVAL *val)
{                               /* --- set the head of a rule */
  void  *vec;                   /* buffer for value vector */
  RCVAL dummy;                  /* dummy for head deletion */

  assert(rule                   /* check the function arguments */
     && (att >= 0) && (att < as_attcnt(rule->attset)));
  dummy.n = 0;                  /* init. the dummy value */
  if (att < 0) { op = 0; val = &dummy; }
  vec = (rule->head.op == R_IN) ? rule->head.val.s : NULL;
  if (condset(&rule->head, att, op, val) != 0)
    return -1;                  /* set new rule head and */
  if (vec) free(vec);           /* delete value vector (if any) */
  return 0;                     /* and return 'ok' */
}  /* r_headset() */

/*--------------------------------------------------------------------*/

ATTID r_condadd (RULE *rule, ATTID att, int op, RCVAL *val)
{                               /* --- add a condition to a rule */
  ATTID i;                      /* loop variable for attributes */
  VALID k;                      /* loop variable for values */
  ATTID min, max;               /* min./max. condition to adapt */
  RCOND new;                    /* new condition (to add to the rule) */
  RCOND *cond, *dst;            /* to traverse conditions */
  int   cmp;                    /* result of value comparison */
  VALID *set;                   /* buffer for value subset */
  unsigned int bits;            /* buffer for bitflags */

  assert(rule                   /* check the function arguments */
     && (att >= 0) && (att < as_attcnt(rule->attset)));
  if (condset(&new, att, op, val) != 0)
    return -1;                  /* create new condition */

  /* --- check compatibility of new condition --- */
  min = -1; max = -2;           /* initialize condition indices */
  cond = rule->conds;           /* traverse conditions */
  for (i = 0; i < rule->condcnt; i++, cond++) {
    if (cond->att != att)       /* if condition refers to a */
      continue;                 /* different attribute, skip it */
    if ((new.op   >= R_IN)      /* if there is a subset operator */
    ||  (cond->op >= R_IN)) {   /* in one of the conditions */
      if (isect(&new, cond) == 0) break; }
    else {                      /* if there is no subset operator */
      cmp = (new.val.f > cond->val.f) ? R_GT
          : (new.val.f < cond->val.f) ? R_LT : R_EQ;
      if (cmp == R_EQ) {        /* if values in conditions are equal, */
        new.op &= cond->op;     /* combine condition operators */
        if (new.op <= 0) break; }      /* and check the result */
      else {                    /* if values in conditions differ */
        if (cond->op & cmp) {   /* if new value in same direction */
          if (!(new.op & cmp))  /* if new operator has opp. dir., */
            continue; }         /* condition merging is not possible */
        else {                  /* if new value in opp. direction */
          if (new.op & cmp)     /* if new operator has opp. dir., */
            break;              /* the conditions are incompatible */
          new.op  = cond->op;   /* if the new operator has same dir., */
          new.val = cond->val;  /* the old condition is stronger, */
        }                       /* so replace the new condition */
      }                         /* (the new condition is modified to */
    }                           /* represent the combined conditions) */
    cond->op |= MARK;           /* mark condition to replace */
    if (min < 0) min = i;       /* and note condition index for */
    max = i;                    /* later adaption of the conditions */
  }
  if (i < rule->condcnt) {      /* if new condition is not compatible */
    cond = rule->conds;         /* remove all condition markers */
    for (i = min; i <= max; i++) (cond++)->op &= ~MARK;
    if (new.op == R_IN) free(new.val.s);
    return -2;                  /* delete a value vector rep. */
  }                             /* and abort the function */

  /* --- simplify new condition --- */
  if (new.op == R_BF) {         /* if bitflag rep. in new condition, */
    for (k = 0, bits = (unsigned)new.val.i; bits; k++, bits >>= 1)
      if (bits & 1) break;      /* find the index of the first value */
    if (bits && !(bits >> 1)) { /* if there is only one value */
      new.op = R_EQ; new.val.n = k; }
  }                             /* simplify condition */
  if (new.op == R_IN) {         /* if vector rep. in new condition, */
    set = new.val.s;            /* check the value vector */
    if (*set == 1) {            /* if there is only one value, */
      new.op = R_EQ; new.val.n = set[1];
      free(set); }              /* simplify cond. and delete vector */
    else {                      /* if there is more than one value, */
      for (bits = 0, k = *set++; --k >= 0; ) {
        if (*set > MSBIT) break;/* traverse the value vector */
        bits |= (unsigned)(1 << *set++);
      }                         /* set bits corresponding to values */
      if (k < 0) {              /* if the conversion was successful, */
        free(new.val.s);        /* delete the value vector */
        new.op = R_BF; new.val.i = (DTINT)bits;
      }                         /* replace the condition by */
    }                           /* a subset test that uses */
  }                             /* a bitflag representation */

  /* --- add new condition --- */
  if (min < 0) {                /* if no adaptable condition found */
    if (rule->condcnt >= rule->condvsz)
      return -3;                /* check the number of conditions */
    rule->conds[rule->condcnt] = new;
    return rule->condcnt++;     /* insert new condition into the rule */
  }                             /* and return the condition index */

  /* --- adapt old condition(s) --- */
  cond = rule->conds +min;      /* get a pointer to the condition */
  cond->op &= ~MARK;            /* to adapt and remove the marker */
  if (new.op != R_NE) {         /* if to adapt condition, */
    if (cond->op == R_IN) free(cond->val.s);
    *cond = new;                /* delete an existing value vector */
  }                             /* and replace the condition */
  if (max <= min) return min;   /* if only one cond. to adapt, abort */
  dst = ++cond;                 /* traverse remaining conditions */
  for (i = rule->condcnt -min; --i > 0; cond++) {
    if (!(cond->op & MARK))     /* group together all */
      *dst++ = *cond;           /* unmarked conditions */
    else if ((cond->op & ~MARK) == R_IN)
      free(cond->val.s);        /* delete an existing value vector */
  }                             /* from the marked conditions */
  rule->condcnt = (ATTID)(dst -rule->conds);
  return min;                   /* set new number of conditions and */
}  /* r_condadd() */            /* return index of changed condition */

/*--------------------------------------------------------------------*/

void r_condrem (RULE *rule, ATTID index)
{                               /* --- remove a cond. from a rule */
  ATTID i;                      /* loop variable */
  RCOND *cond;                  /* to traverse the conditions */

  assert(rule                   /* check the function arguments */
     && (index < rule->condcnt));
  if (index < 0) {              /* if to remove all conditions, */
    cond = rule->conds;         /* delete value vectors */
    for (i = rule->condcnt; --i >= 0; cond++)
      if (cond->op == R_IN) free(cond->val.s);
    rule->condcnt = 0; }        /* clear condition counter */
  else {                        /* if to remove one condition */
    cond = rule->conds +index;  /* get the condition and delete */
    if (cond->op == R_IN) free(cond->val.s);    /* value vector */
    for (i = --rule->condcnt -index; --i >= 0; cond++)
      *cond = cond[1];          /* shift conditions to fill the gap */
  }                             /* left by the deleted condition */
}  /* r_condrem() */

/*--------------------------------------------------------------------*/

void r_condsort (RULE *rule)
{                               /* --- sort rule conditions */
  assert(rule);                 /* check the function argument */
  qsort(rule->conds, (size_t)rule->condcnt, sizeof(RCOND), condcmp);
}  /* r_condsort() */           /* sort the rule conditions */

/*--------------------------------------------------------------------*/

ATTID r_check (RULE *rule)
{                               /* --- check applicability of a rule */
  ATTID i;                      /* loop variable for conditions */
  VALID k;                      /* loop variable for values */
  int   t;                      /* attribute type */
  RCOND *cond;                  /* to traverse the conditions */
  ATT   *att;                   /* to traverse the attributes */
  INST  *val;                   /* to traverse the values */
  DTFLT f;                      /* buffer for comparisons */
  VALID *p;                     /* to traverse an identifier vector */

  assert(rule);                 /* check the function argument */
  for (cond = rule->conds, i = rule->condcnt; --i >= 0; cond++) {
    att = as_att(rule->attset, cond->att);
    val = att_inst(att);        /* traverse the conditions and */
    t   = att_type(att);        /* get the corresponding attribute */
    if (t == AT_NOM) {          /* if the attribute is nominal */
      if      (cond->op == R_EQ) {  /* equality to a value */
        if (cond->val.n != val->n) return i; }
      else if (cond->op == R_BF) {  /* bit flag represented set */
        if ((val->n >= 32) || !(cond->val.i & (1 << val->n)))
          return i; }           /* check whether the bit is set */
      else {                    /* identifier vector represented set */
        for (k = *(p = cond->val.s); --k >= 0; )
          if (*++p == val->n) break;
        if (k < 0) return i;    /* traverse the identifiers and */
      } }                       /* try to find the value */
    else {                      /* if the attribute is metric */
      if (t == AT_INT) f = (DTFLT)val->i;  /* get the value from */
      else             f =        val->f;  /* the attribute set */
      switch (cond->op) {       /* evaluate the comparison operator */
        case R_EQ: if (f != cond->val.f) return i; break;
        case R_LT: if (f >= cond->val.f) return i; break;
        case R_GT: if (f <= cond->val.f) return i; break;
        case R_LE: if (f >  cond->val.f) return i; break;
        case R_GE: if (f <  cond->val.f) return i; break;
        default  : if (f == cond->val.f) return i; break;
      }                         /* if the comparison fails, */
    }                           /* return the index of */
  }                             /* the condition that fails */
  return -1;                    /* return 'rule is applicable' */
}  /* r_check() */

/*--------------------------------------------------------------------*/
#ifdef RS_PARSE

static int valset (ATT *att, SCANNER *scan, RULE *rule)
{                               /* --- read a set of values */
  int   t, op;                  /* token value, comparison operator */
  VALID i, k;                   /* value identifier, loop variable */
  RCVAL val;                    /* to represent the condition value */
  VALID *set = NULL;            /* set of values read */

  assert(att && scan && rule);  /* check the function arguments */
  k  = att_valcnt(att);         /* get the number of values and */
  if (k <= 32) {                /* decide on the set representation */
    op  = R_BF; val.i = 0; }    /* if to use bit flags, clear bits */
  else {                        /* if to use a vector of values, */
    op  = R_IN;                 /* allocate an identifier vector */
    set = (VALID*)malloc((size_t)(k+1) *sizeof(VALID));
    if (!set) SCN_ERROR(scan, E_NOMEM);  /* create a set of values */
    set[0] = 0; val.s = set;    /* initialize the number of values */
  }                             /* (vector format: [cnt v1 v2 ... ]) */
  SCN_CHAR(scan, '{');          /* consume '{' */
  while (1) {                   /* value read loop */
    t = scn_token(scan);        /* check for an attribute value */
    if ((t != T_ID) && (t != T_NUM)) SCN_ERRVAL(scan, E_VALEXP);
    i = att_valid(att, scn_value(scan));
    if (i < 0)                       SCN_ERRVAL(scan, E_UNKVAL);
    SCN_NEXT(scan);             /* get, check, and consume the value */
    if (op == R_BF)             /* if the set is rep. by bit flags, */
      val.i |= 1 << i;          /* set the bit corresp. to the value */
    else {                      /* if the set is rep. by a vector, */
      for (k = *set; --k >= 0;) /* check whether the value exists */
        if (set[k+1] == i) break;
      if (k < 0) { k = *set++; set[k] = i; }
    }                           /* add only new values */
    if (scn_token(scan) != ',') /* add a condition to the rule */
      break;                    /* and check for another value */
    SCN_NEXT(scan);             /* consume ',' */
  }
  SCN_CHAR(scan, '}');          /* consume '}' */
  r_condadd(rule, att_id(att), op, &val);
  if (set) free(set);           /* add the condition, delete the */
  return 0;                     /* identifier vector, and return 'ok' */
}  /* valset() */

/*--------------------------------------------------------------------*/

static int getrule (ATTSET *attset, SCANNER *scan, RULE *rule)
{                               /* --- parse a rule */
  ATTID  id;                    /* attribute identifier */
  int    op;                    /* comparison operator */
  ATT    *att;                  /* attribute of a condition */
  RCVAL  val;                   /* attribute value */
  int    head = 1;              /* flag for the rule head */
  double supp, conf;            /* support and confidence */
  int    t;                     /* buffer for a token value */

  assert(attset && scan && rule);  /* check the function arguments */
  while (1) {                   /* condition read loop */
    t = scn_token(scan);        /* check for an attribute */
    if ((t != T_ID) && (t != T_NUM)) SCN_ERRVAL(scan, E_ATTEXP);
    id = as_attid(attset, scn_value(scan));
    if (id < 0)                      SCN_ERRVAL(scan, E_UNKATT);
    att = as_att(attset, id);   /* get the attribute */
    SCN_NEXT(scan);             /* consume the attribute name */
    t = scn_token(scan);        /* check for a comparison operator */
    if ((t != '=')   && (t != '<') && (t != '>')
    &&  (t != T_CMP) && (t != T_ID)) SCN_ERROR(scan, E_CHREXP, '=');
    for (op = R_IN +1; --op >= 0; )
      if (opsigns[op] && (strcmp(opsigns[op], scn_value(scan)) == 0))
        break;                  /* encode the comparison operator */
    if ((op < 0) || (head && (op != R_EQ)))
      SCN_ERROR(scan, E_CHREXP, '='); /* check for a valid comp. op. */
    if (att_type(att) == AT_NOM) {    /* if the attribute is nominal */
      if ((op != R_IN) && (op != R_EQ)) SCN_ERRVAL(scan, E_CMPOP);
      SCN_NEXT(scan);           /* consume the comparison operator */
      if (op == R_IN) {         /* if to read a set of values */
        t = valset(att, scan, rule); if (t) return t; }
      else {                    /* if to read a single value */
        t = scn_token(scan);    /* check for a nominal value */
        if ((t != T_ID) && (t != T_NUM)) SCN_ERRVAL(scan, E_VALEXP);
        val.n = att_valid(att, scn_value(scan));
        if (val.n < 0)                   SCN_ERRVAL(scan, E_UNKVAL);
        SCN_NEXT(scan);         /* get, check, and consume the value */
        if (head) r_headset(rule, id, op, &val);/* set the rule head */
        else      r_condadd(rule, id, op, &val);/* or add a condition */
      } }
    else {                      /* if the attribute is metric */
      if (op >= R_IN) SCN_ERRVAL(scan, E_CMPOP);
      SCN_NEXT(scan);           /* consume the comparison operator */
      SCN_NUM (scan);           /* check for a number */
      val.f = (float)atof(scn_value(scan));
      SCN_NEXT(scan);           /* get, check, and consume the number */
      if (head) r_headset(rule, id, op, &val); /* set the rule head */
      else      r_condadd(rule, id, op, &val); /* or add a condition */
    }
    if ((scn_token(scan) == ';') || (scn_token(scan) == '['))
      break;                    /* check for the end of the rule */
    if (head) { if (scn_token(scan) != T_LFT)
                  SCN_ERROR(scan, E_STREXP, "<-"); }
    else      { if (scn_token(scan) != '&')
                  SCN_ERROR(scan, E_CHREXP, '&');  }
    SCN_NEXT(scan);             /* consume implication or conjunction */
    head = 0;                   /* the first round reads the head, */
  }                             /* all following only conditions */
  if (scn_token(scan) == '[') { /* if support/confidence follows */
    SCN_NEXT(scan); t = 0;      /* consume '[' */
    if (scn_token(scan) == '~') { SCN_NEXT(scan); t = 1; }
    SCN_NUM(scan);              /* check for a number */
    supp = atof(scn_value(scan));
    conf = 0;                   /* get the support/confidence */
    SCN_NEXT(scan);             /* consume the number */
    if ((t == 0) && (scn_token(scan) == '%')) t = 2;
    if (t != 0) {               /* if there is no support value, */
      conf = supp; supp = 0;    /* set and check the confidence */
      if ((conf < 0) || (conf > 100)) {
        scn_back(scan); SCN_ERROR(scan, E_NUMBER); }
      if (t == 2) SCN_NEXT(scan); }  /* consume '%' */
    else if (scn_token(scan) == '/') {
      SCN_NEXT(scan);           /* consume the supp./conf. separator */
      if (scn_token(scan) == '~') { SCN_NEXT(scan); t = 1; }
      SCN_NUM(scan);            /* check for a number */
      conf = atof(scn_value(scan));
      if ((conf < 0) || (conf > 100)) SCN_ERROR(scan, E_NUMBER);
      SCN_NEXT(scan);           /* get, check and consume the conf. */
      if ((t == 0) && (scn_token(scan) == '%')) {
        SCN_NEXT(scan); t = 2; }
    }                           /* consume '%' */
    SCN_CHAR(scan, ']');        /* consume ']' */
    rule->supp = supp;          /* set support and confidence */
    rule->conf = (t == 2) ? 0.01 *conf : conf;
  }
  SCN_CHAR(scan, ';');          /* consume ';' */
  return 0;                     /* return 'ok' */
}  /* getrule() */

/*--------------------------------------------------------------------*/

RULE* r_parse (ATTSET *attset, SCANNER *scan)
{                               /* --- parse a rule */
  RULE *rule;                   /* created rule */

  assert(attset && scan);       /* check the function arguments */
  scn_setmsgs(scan, msgs, (int)(sizeof(msgs)/sizeof(*msgs)));
  scn_first(scan);              /* set messages, get first token */
  rule = r_create(attset, 2 *as_attcnt(attset));
  if (!rule) return NULL;       /* create a rule of maximum size */
  if (getrule(attset, scan, rule) != 0) {
    r_delete(rule); return NULL; }  /* parse a rule */
  rule = (RULE*)realloc(rule, sizeof(RULE)
                      +(size_t)(rule->condcnt-1) *sizeof(RCOND));
  rule->condvsz = rule->condcnt;/* try to shrink the rule */
  return rule;                  /* return the created rule */
}  /* r_parse() */

#endif
/*----------------------------------------------------------------------
  Rule Set Functions
----------------------------------------------------------------------*/

RULESET* rs_create (ATTSET *attset, RULE_DELFN delfn)
{                               /* --- create a ruleset */
  RULESET *set;                 /* created rule set */

  assert(attset && delfn);      /* check arguments */
  set = (RULESET*)malloc(sizeof(RULESET));
  if (!set) return NULL;        /* allocate memory and */
  set->attset = attset;         /* initialize fields */
  set->delfn  = delfn;
  set->size   = set->cnt = 0;
  set->rules  = NULL;
  return set;                   /* return the created rule set */
}  /* rs_create() */

/*--------------------------------------------------------------------*/

void rs_delete (RULESET *set, int delas)
{                               /* --- delete a ruleset */
  RULEID i;                     /* loop variable */
  RULE   **p;                   /* to traverse the rules */

  assert(set);                  /* check the function argument */
  for (p = set->rules +(i = set->cnt); --i >= 0; ) {
    (*--p)->set = NULL; (*p)->id = -1;
    set->delfn(*p);             /* remove and delete all rules */
  }                             /* and delete the rule vector */
  if (set->rules) free(set->rules);
  if (delas) as_delete(set->attset);
  free(set);                    /* delete the rule set body */
}  /* rs_delete() */

/*--------------------------------------------------------------------*/

int rs_ruleadd (RULESET *set, RULE *rule)
{                               /* --- add a rule to a ruleset */
  RULEID s;                     /* size of rule vector */
  RULE   **p;                   /* (new) rule vector */

  assert(set && rule);          /* check the function arguments */
  if (set->cnt >= set->size) {  /* if the rule vector is full */
    s = set->size +((set->size > BLKSIZE) ? (set->size >> 1) : BLKSIZE);
    p = (RULE**)realloc(set->rules, (size_t)s *sizeof(RULE*));
    if (!p) return -1;          /* allocate a (new) rule vector */
    set->rules = p; set->size = s;
  }                             /* set rule vector and its size */
  rule->set = set;              /* note rule set reference */
  rule->id  = set->cnt;         /* and set rule identifier */
  set->rules[set->cnt++] = rule;
  return 0;                     /* add the rule and return 'ok' */
}  /* rs_ruleadd() */

/*--------------------------------------------------------------------*/

RULE* rs_rulerem (RULESET *set, RULEID rid)
{                               /* --- remove a rule from a ruleset */
  RULEID i;                     /* loop variable */
  RULE   **p;                   /* to traverse the rule vector */
  RULE   *rule;                 /* buffer for removed rule */

  assert(set && (rid < set->cnt)); /* check function arguments */

  /* --- delete all rules --- */
  if (rid < 0) {                /* if no rule identifier given */
    for (p = set->rules +(i = set->cnt); --i >= 0; ) {
      (*--p)->set = NULL; (*p)->id = -1;
      set->delfn(*p);           /* remove and delete all rules */
    }
    if (set->rules) { free(set->rules); set->rules = NULL; }
    set->cnt = set->size = 0;   /* delete the rule vector */
    return NULL;                /* and abort the function */
  }

  /* --- delete one rule --- */
  rule = set->rules[rid];       /* note the rule to remove */
  p    = set->rules +rid;       /* traverse the successor rules */
  for (i = --set->cnt -rid; --i >= 0; ) {
    *p = p[1]; (*p++)->id--; }  /* shift rules and adapt identifiers */
  rule->set = NULL;             /* clear the rule set reference */
  rule->id  = -1;               /* and the rule identifier */
  return rule;                  /* and return the removed rule */
}  /* rs_rulerem() */

/*--------------------------------------------------------------------*/

void rs_ruleexg (RULESET *set, RULEID rid1, RULEID rid2)
{                               /* --- exchange two rules */
  RULE **p1, **p2, *rule;       /* exchange buffer */

  assert(set && (rid1 >= 0) && (rid1 < set->cnt)
             && (rid2 >= 0) && (rid2 < set->cnt));
  p1 = set->rules +rid1; rule = *p1;
  p2 = set->rules +rid2;        /* get pointers to attributes, */
  *p1 = *p2; (*p1)->id = rid1;  /* exchange them, and */
  *p2 = rule; rule->id = rid2;  /* set new identifiers */
}  /* rs_ruleexg() */

/*--------------------------------------------------------------------*/

void rs_rulemove (RULESET *set, RULEID off, RULEID cnt, RULEID pos)
{                               /* --- move some rules */
  RULE **p;                     /* to traverse rules */

  assert(set);                  /* check for a valid attribute set */
  if (pos > set->cnt)      pos = set->cnt;
  if (cnt > set->cnt -off) cnt = set->cnt -off;
  assert((cnt >= 0) && (off >= 0)
      && (pos >= 0) && ((pos <= off) || (pos >= off +cnt)));
  ptr_move(set->rules, (size_t)off, (size_t)cnt, (size_t)pos);
  if (pos <= off) {             /* move rules in vector */
    cnt += off; off = pos; pos = cnt; }
  p = set->rules +off;          /* set new rule identifiers */
  while (off < pos) (*p++)->id = off++;
}  /* rs_rulemove() */

/*--------------------------------------------------------------------*/
#if 0

int rs_rulecut (RULESET *dst, RULESET *src, int mode, ...)
{                               /* --- cut/copy rules */
  return 0;                     /* not implemented yet */
}  /* rs_rulecut() */

#endif
/*--------------------------------------------------------------------*/

void rs_sort (RULESET *set, RULE_CMPFN cmpfn, void *data)
{                               /* --- sort a ruleset */
  RULEID i;                     /* loop variable */
  RULE   **p;                   /* to traverse rules */

  assert(set && cmpfn);         /* check arguments */
  ptr_qsort(set->rules, (size_t)set->cnt, +1, (CMPFN*)cmpfn, data);
  p = set->rules +set->cnt;     /* sort rules and set new identifiers */
  for (i = set->cnt; --i >= 0; ) (*--p)->id = i;
}  /* rs_sort() */

/*--------------------------------------------------------------------*/

int rs_exec (RULESET *set)
{                               /* --- find first applicable rule */
  RULEID i;                     /* loop variable */

  assert(set);                  /* check the function argument */
  for (i = 0; i < set->cnt; i++)
    if (r_check(set->rules[i]) < 0) return i;
  return -1;                    /* traverse and check the rules */
}  /* rs_exec() */

/*--------------------------------------------------------------------*/
#ifdef RS_DESC

static void condout (DESC *desc, RCOND *cond, int op)
{                               /* --- print a condition */
  ATT   *att;                   /* attribute of the condition */
  VALID i;                      /* loop variable for values */
  VALID *val;                   /* to traverse attribute values */
  int   bit;                    /* bit index */
  int   len;                    /* length of attribute/value name */
  char  buf[8 *AS_MAXLEN +64];  /* output buffer */
  char *s = buf;                /* to traverse the output buffer */
  const char *t;                /* to traverse the operator sign */

  if      (op >  0) { *s++ = ' '; *s++ = '&';             *s++ = ' '; }
  else if (op == 0) { *s++ = ' '; *s++ = '<'; *s++ = '-'; *s++ = ' '; }
                                /* store logical operator (if any) */
  att  = as_att(desc->attset, cond->att);
  s   += scn_format(s, att_name(att), 0);
  *s++ = ' ';                   /* get and format attribute name */
  for (t = opsigns[cond->op]; *t; )
    *s++ = *t++;                /* store the condition operator */
  *s++ = ' ';                   /* and a separating blank */
  if (att_type(att) != AT_NOM)  /* if the attribute is metric, */
    s += sprintf(s, "%g", cond->val.f);     /* store its value */
  else if (cond->op >= R_IN) {  /* if subset test on nominal att., */
    *s++ = '{'; *s = '\0'; }    /* start an attribute value set */
  else {                        /* if simple nominal comparison */
    s += scn_format(s, att_valname(att, cond->val.n), 0);
  }                             /* get and format attribute value */
  if ((desc->pos > 4)           /* if the line would get too long, */
  &&  (desc->pos +(s -buf) >= desc->max)) {    /* start a new line */
    fputs((op > 0) ? "\n  " : "\n ", desc->file); desc->pos = 1; }
  fputs(buf, desc->file);       /* print output buffer contents */
  desc->pos += (int)(s -buf);   /* compute new position */
  if (cond->op < R_IN) return;  /* if this is no subset test, abort */

  s = NULL; buf[0] = ' ';       /* use string as a first value flag */
  if (cond->op == R_IN) {       /* if vector representation */
    val = cond->val.s;          /* traverse value vector */
    for (i = *val++; --i >= 0; val++) {
      if (s) {                  /* if this is not the first value, */
        fputc(',', desc->file); desc->pos++;} /* print a separator */
      len = (int)scn_format(s = buf+1, att_valname(att, *val), 0);
      if ((desc->pos > 4)       /* if the line would get too long, */
      &&  (desc->pos +len +3 > desc->max)) {   /* start a new line */
        fputs("\n    ", desc->file); desc->pos = 4; }
      fputs(buf, desc->file);   /* print attribute name */
      desc->pos += len +1;      /* and compute new position */
    } }
  else {                        /* if bitflag representation */
    for (bit = 1, i = 0; bit; i++, bit <<= 1) {
      if (!(cond->val.i & bit)) /* traverse bit flags */
        continue;               /* and skip cleared bits */
      if (s) {                  /* if this is not the first value, */
        fputc(',', desc->file); desc->pos++;} /* print a separator */
      len = (int)scn_format(s = buf+1, att_valname(att, i), 0);
      if ((desc->pos > 4)       /* if the line would get too long, */
      &&  (desc->pos +len +3 > desc->max)) {  /* start a new line */
        fputs("\n    ", desc->file); desc->pos = 4; }
      fputs(buf, desc->file);   /* print attribute name */
      desc->pos += len +1;      /* and compute new position */
    }
  }
  fputs(" }", desc->file);      /* terminate value set */
  desc->pos += 2;               /* and compute new position */
}  /* condout() */

/*--------------------------------------------------------------------*/

int rs_desc (RULESET *set, FILE *file, int mode, int maxlen)
{                               /* --- describe a rule set */
  RULEID i;                     /* loop variable for rules */
  ATTID  k;                     /* loop variable for conditions */
  int    len, l;                /* loop variables for comments */
  DESC   desc;                  /* description info */
  RULE   **rule;                /* to traverse the rule vector */
  RCOND  *cond;                 /* to traverse a condition vector */
  int    op;                    /* logical operator */
  char   buf[64];               /* output buffer */
  char   *s;                    /* to traverse the output buffer */

  assert(set && file);          /* check arguments */

  /* --- print header (as a comment) --- */
  if (mode & RS_TITLE) {        /* if the title flag is set */
    len = (maxlen > 0) ? maxlen -2 : 70;
    fputs("/*", file); for (l = len; --l >= 0; ) fputc('-', file);
    fprintf(file, "\n  rules\n");
    for (l = len; --l >= 0; ) fputc('-', file);
    fputs("*/\n", file);        /* print title header */
  }                             /* (as a comment) */
  desc.file   = file;           /* note the output file */
  desc.attset = set->attset;    /* and the attribute set */
  desc.max    = (maxlen <= 0) ? INT_MAX : maxlen;
  desc.pos    = 0;              /* initialize the position */

  /* --- print a list of rules --- */
  rule = set->rules;            /* traverse the rules */
  for (i = set->cnt; --i >= 0; rule++) {
    if ((*rule)->head.att < 0) {/* if there is no rule head, */
      fputc('#', file); desc.pos = 1; } /* print a special mark */
    else {                      /* if there is a rule head */
      condout(&desc, &(*rule)->head, -1);
      if (mode & RS_CONDLN) { fputs("\n ", file); desc.pos = 1; }
    }                           /* print head condition */
    op   = 0;                   /* set implication first */
    cond = (*rule)->conds;      /* traverse conditions */
    for (k = (*rule)->condcnt; --k > 0; cond++) {
      condout(&desc, cond, op);
      op = 1;                   /* print a condition, switch to and */
      if (mode & RS_CONDLN) { fputs("\n  ", file); desc.pos = 2; }
    }                           /* in line mode start a new line */
    if (k >= 0)                 /* print last condition (if any, */
      condout(&desc, cond, op); /* there could be no cond. at all) */
    if (mode & (RS_SUPP|RS_CONF)) {
      s = buf;                  /* if to print support or confidence */
      *s++ = ' '; *s++ = '[';   /* start additional information */
      if (mode & RS_SUPP)       /* if to print the support */
        s += sprintf(s, "%g", (*rule)->supp);
      if (mode & RS_CONF) {     /* if to print the confidence */
        if (mode & RS_SUPP) *s++ = '/';
        k = (*rule)->head.att;  /* check the head attribute */
        if ((k >= 0) && (att_type(as_att(set->attset, k)) != AT_NOM))
          s += sprintf(s, "~%g",    (*rule)->conf);
        else                    /* metric: print rmse as confidence */
          s += sprintf(s, "%.2f%%", (*rule)->conf *100);
      }                         /* nominal: print conf. in percent */
      *s++ = ']'; *s = '\0';    /* terminate additional information */
      k = (int)(s -buf);        /* and compute its length */
      if (desc.pos+k > desc.max)/* if the line would get too long, */
        fputs("\n  ", file);    /* start a new line and indent */
      fputs(buf, file);         /* print support and confidence */
    }
    fputs(";\n", file);         /* terminate the output line */
    desc.pos = 0;               /* and reset the position */
  }

  /* --- print additional information (as a comment) --- */
  if (mode & RS_INFO) {         /* if the add. info. flag is set */
    len = (maxlen > 0) ? maxlen -2 : 70;
    fputs("\n/*", file); for (l = len; --l >= 0; ) fputc('-', file);
    k = as_attcnt(set->attset);
    fprintf(file, "\n  number of attributes: %"ATTID_FMT, k);
    fprintf(file, "\n  number of rules     : %"RULEID_FMT"\n",set->cnt);
    for (l = len; --l >= 0; ) fputc('-', file);
    fputs("*/\n", file);        /* print add. rule set information */
  }                             /* (as a comment) */
  return ferror(file) ? -1 : 0; /* check for a write error */
}  /* rs_desc() */

#endif  /* #ifdef RS_DESC */
/*--------------------------------------------------------------------*/
#ifdef RS_PARSE

RULESET* rs_parse (ATTSET *attset, SCANNER *scan)
{                               /* --- parse a rule set */
  RULESET *set;                 /* created rule set */
  RULE    *rule;                /* to read/traverse the rules */
  int     err = 0;              /* error flag */

  assert(attset && scan);       /* check the function arguments */
  set = rs_create(attset, r_delete);
  if (!set) return NULL;        /* create a rule set */
  while (scn_token(scan) == T_ID) {   /* rule read loop */
    rule = r_parse(attset, scan);     /* parse a rule and */
    if (rule) rs_ruleadd(set, rule);  /* add it to the rule set */
    else { err = 1;             /* try to recover after an error */
      if (scn_recover(scan, ';', 0, 0, 0) == T_EOF) break; }
  }
  if (err) { rs_delete(set, 0); return NULL; }
  return set;                   /* return the created rule set */
}  /* rs_parse() */

#endif  /* #ifdef RS_PARSE */
