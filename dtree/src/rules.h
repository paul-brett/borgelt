/*----------------------------------------------------------------------
  File    : rules.h
  Contents: rule and rule set management
  Author  : Christian Borgelt
  History : 1998.05.17 file created
            1998.05.26 functions r_supp and r_conf added
            1998.05.27 comparison operator added
            1998.05.31 operator coding changed
            1998.08.13 parameter delas added to function rs_delete
            1998.08.24 operators for value in subset added
            1998.08.26 function rs_ruleexg added
            2001.07.19 adapted to modified module scan
            2002.07.06 rule and rule set parsing functions added
            2004.08.12 adapted to new module parse
            2013.08.23 adapted to definitions ATTID, VALID, TPLID etc.
----------------------------------------------------------------------*/
#ifndef __RULES__
#define __RULES__
#ifdef RS_PARSE
#ifndef SCN_SCAN
#define SCN_SCAN
#endif
#include "scanner.h"
#endif
#include "attset.h"

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#ifndef RULEID
#define RULEID      int         /* rule identifier */
#endif

/*--------------------------------------------------------------------*/

#define int         1           /* to check definitions */
#define long        2           /* for certain types */
#define ptrdiff_t   3

/*--------------------------------------------------------------------*/

#if   RULEID==int
#ifndef RULEID_MAX
#define RULEID_MAX  INT_MAX     /* maximum dimension identifier */
#endif
#ifndef RULEID_SGN
#define RULEID_SGN  INT_MIN     /* sign of dimension identifier */
#endif
#ifndef RULEID_FMT
#define RULEID_FMT  "d"         /* printf format code for int */
#endif

#elif RULEID==long
#ifndef RULEID_MAX
#define RULEID_MAX  LONG_MAX    /* maximum dimension identifier */
#endif
#ifndef RULEID_SGN
#define RULEID_SGN  LONG_MIN    /* sign of dimension identifier */
#endif
#ifndef RULEID_FMT
#define RULEID_FMT  "ld"        /* printf format code for long */
#endif

#elif RULEID==ptrdiff_t
#ifndef RULEID_MAX
#define RULEID_MAX  PTRDIFF_MAX /* maximum dimension identifier */
#endif
#ifndef RULEID_SGN
#define RULEID_SGN  PTRDIFF_MIN /* sign of dimension identifier */
#endif
#ifndef RULEID_FMT
#  ifdef _MSC_VER
#  define RULEID_FMT  "Id"      /* printf format code for ptrdiff_t */
#  else
#  define RULEID_FMT  "td"      /* printf format code for ptrdiff_t */
#  endif                        /* MSC still does not support C99 */
#endif

#else
#error "RULEID must be either 'int', 'long' or 'ptrdiff_t'"
#endif

/*--------------------------------------------------------------------*/

#undef int                      /* remove preprocessor definitions */
#undef long                     /* needed for the type checking */
#undef ptrdiff_t

/*--------------------------------------------------------------------*/

/* --- comparison operators (r_headadd, r_condadd) --- */
#define R_EQ       0x0001       /* value is equal */
#define R_LT       0x0002       /* value is less */
#define R_GT       0x0004       /* value is greater */
#define R_LE       (R_LT|R_EQ)  /* value is less    or equal */
#define R_GE       (R_GT|R_EQ)  /* value is greater or equal */
#define R_NE       (R_LT|R_GT)  /* value is not equal */
#define R_IN       0x0008       /* value is in set */
#define R_BF       0x0009       /* value is in set (bitflags) */

/* --- rule set cut flags (rs_rulecut) --- */
#define RS_COPY    AS_COPY      /* copy source rules */
#define RS_ALL     AS_ALL       /* cut/copy all rules */
#define RS_RANGE   AS_RANGE     /* cut/copy a range of rules */

/* --- rule set description flags (rs_desc) --- */
#define RS_TITLE   0x0001       /* print a title as a comment */
#define RS_INFO    0x0002       /* print add. info. (as a comment) */
#define RS_SUPP    0x0004       /* print the rule support */
#define RS_CONF    0x0008       /* print the rule confidence */
#define RS_CONDLN  0x0010       /* one condition per line */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef union {                 /* --- instance --- */
  VALID n;                      /* identifier for nominal value */
  DTINT i;                      /* integer number */
  DTFLT f;                      /* floating-point number */
  VALID *s;                     /* set of nominal values */
} RCVAL;                        /* (rule condition value) */

typedef struct {                /* --- condition of a rule --- */
  ATTID att;                    /* attribute to test */
  int   op;                     /* comparison operator */
  RCVAL val;                    /* attribute value/value set */
} RCOND;                        /* (rule condition) */

typedef struct {                /* --- rule --- */
  ATTSET      *attset;          /* underlying attribute set */
  struct rset *set;             /* containing rule set (if any) */
  RULEID      id;               /* identifier (index in rule set) */
  double      supp;             /* support of the rule */
  double      conf;             /* confidence of the rule */
  RCOND       head;             /* head/consequent of the rule */
  ATTID       condvsz;          /* size of condition vector */
  ATTID       condcnt;          /* number of condition in vector */
  RCOND       conds[1];         /* condition vector */
} RULE;                         /* (rule) */

typedef void RULE_DELFN (RULE *rule);  /* rule functions */
typedef int  RULE_CMPFN (const RULE *r1, const RULE *r2, void *data);

typedef struct rset {           /* --- set of rules --- */
  ATTSET      *attset;          /* underlying attribute set */
  RULE_DELFN  *delfn;           /* rule deletion function */
  RULEID      size;             /* size of rule vector */
  RULEID      cnt;              /* number of rules in vector */
  RULE        **rules;          /* rule vector */
} RULESET;                      /* (rule set) */

/*----------------------------------------------------------------------
  Rule Functions
----------------------------------------------------------------------*/
extern RULE*    r_create    (ATTSET *attset, ATTID maxcond);
extern void     r_delete    (RULE *rule);
extern int      r_copy      (RULE *dst,  RULE *src);
extern ATTSET*  r_attset    (RULE *rule);
extern double   r_setsupp   (RULE *rule, double supp);
extern double   r_getsupp   (RULE *rule);
extern double   r_setconf   (RULE *rule, double conf);
extern double   r_getconf   (RULE *rule);
extern void     r_clear     (RULE *rule);

extern int      r_headset   (RULE *rule, ATTID att, int op, RCVAL *val);
extern ATTID    r_headatt   (RULE *rule);
extern int      r_headop    (RULE *rule);
extern RCVAL*   r_headval   (RULE *rule);

extern ATTID    r_condcnt   (RULE *rule);
extern ATTID    r_condadd   (RULE *rule, ATTID att, int op, RCVAL *val);
extern void     r_condrem   (RULE *rule, ATTID index);
extern ATTID    r_condatt   (RULE *rule, ATTID index);
extern int      r_condop    (RULE *rule, ATTID index);
extern RCVAL*   r_condval   (RULE *rule, ATTID index);
extern void     r_condsort  (RULE *rule);

extern ATTID    r_check     (RULE *rule);

#ifdef RS_PARSE
extern RULE*    r_parse     (ATTSET *attset, SCANNER *scan);
#endif

/*----------------------------------------------------------------------
  Rule Set Functions
----------------------------------------------------------------------*/
extern RULESET* rs_create   (ATTSET  *attset, RULE_DELFN delfn);
extern void     rs_delete   (RULESET *set, int delas);
extern ATTSET*  rs_attset   (RULESET *set);

extern int      rs_ruleadd  (RULESET *set, RULE *rule);
extern RULE*    rs_rulerem  (RULESET *set, RULEID rid);
extern void     rs_ruleexg  (RULESET *set, RULEID rid1, RULEID rid2);
extern void     rs_rulemove (RULESET *set,
                             RULEID offs, RULEID cnt, RULEID pos);
extern int      rs_rulecut  (RULESET *dst, RULESET *src, int mode, ...);
extern RULE*    rs_rule     (RULESET *set, RULEID rid);
extern RULEID   rs_rulecnt  (RULESET *set);

extern void     rs_sort     (RULESET *set, RULE_CMPFN cmpfn,void *data);
extern RULEID   rs_exec     (RULESET *set);

#ifdef RS_DESC
extern int      rs_desc     (RULESET *set, FILE *file,
                             int mode, int maxlen);
#endif
#ifdef RS_PARSE
extern RULESET* rs_parse    (ATTSET *attset, SCANNER *scan);
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define r_attset(r)      ((r)->attset)
#define r_setsupp(r,s)   ((r)->supp = (s))
#define r_getsupp(r)     ((r)->supp)
#define r_setconf(r,c)   ((r)->conf = (c))
#define r_getconf(r)     ((r)->conf)
#define r_clear(r)       ((r)->conf = (r)->supp = 0)

#define r_headatt(r)     ((r)->head.att)
#define r_headop(r)      ((r)->head.op)
#define r_headval(r)     (&(r)->head.val)

#define r_condcnt(r)     ((r)->condcnt)
#define r_condatt(r,i)   ((r)->conds[i].att)
#define r_condop(r,i)    ((r)->conds[i].op)
#define r_condval(r,i)   (&(r)->conds[i].val)

#define rs_attset(s)     ((s)->attset)
#define rs_rule(s,i)     ((s)->rules[i])
#define rs_rulecnt(s)    ((s)->cnt)

#endif
