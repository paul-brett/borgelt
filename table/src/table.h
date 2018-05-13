/*----------------------------------------------------------------------
  File    : table.h
  Contents: tuple and table management
  Author  : Christian Borgelt
  History : 1996.03.06 file created
            1996.03.07 function tab_copy() and tuple functions added
            1996.07.23 tuple and table management redesigned
            1997.02.26 functions tab_sort() and tab_search() added
            1997.02.27 function tab_reduce() added (comb. equal tuples)
            1997.03.28 function tpl_get/setwgt() added, INSTFN added
            1997.03.29 several functions added or extended
            1997.08.07 function tab_tplexg() added (exchange tuples)
            1998.01.29 function tpl_fromas() added (tuple creation)
            1998.02.03 functions tab_sort and tab_secsort merged
            1998.02.24 function tab_shuffle() added (shuffle tuples)
            1998.05.29 get/set functions split (were combined before)
            1998.06.22 deletion function moved to function tab_create()
            1998.07.27 function tab_tplmove() added
            1998.08.16 several functions added
            1998.09.25 first step of major redesign completed
            1998.10.01 table names added (string identifier)
            1998.11.29 functions tpl_clone() and tab_clone() added
            1999.02.04 long int changed to int (assume 32 bit system)
            1999.02.13 function tab_balance() added (balance classes)
            1999.03.12 function tab_opc() added (one point coverages)
            1999.03.15 one point coverage functions transf. from opc.c
            1999.03.16 function tpl_hash() added (compute hash value)
            1999.04.03 parameter 'cloneas' added to function tab_clone()
            1999.04.17 function tab_getwgt() added (get table weight)
            1999.10.21 definition of flag TAB_NORM (for tab_opc()) added
            2003.01.16 functions tpl_compat(), tab_poss(), tab_possx()
            2003.07.22 function tab_colnorm() added (normalize range)
            2007.01.17 tab_colnorm() returns offset and scaling factor
            2007.01.18 tab_coltlin(), tab_coltype(), tpl_coltype() added
            2010.12.31 function tab_tplwgt() added (total tuple weight)
            2011.02.03 functions tpl_attcnt() and tab_attcnt() added
            2013.07.18 definition TPLID added, adapted to ATTID etc.
            2013.07.26 parameter dir added to function tab_sort()
            2013.08.20 function tpl_sqrdist() transferred from distmat.c
            2013.08.23 extended tuple weight added (for dtree package)
            2013.09.05 return values for tab_reduce() and tab_balance()
            2015.08.01 function tab_colperm() added (permute columns)
            2015.08.05 parameter 'intmul' added to tab_balance()
----------------------------------------------------------------------*/
#ifndef __TABLE__
#define __TABLE__
#include <stddef.h>
#include <stdint.h>
#include "fntypes.h"
#if defined TAB_READ  && !defined AS_READ
#define AS_READ
#endif
#if defined TAB_WRITE && !defined AS_WRITE
#define AS_WRITE
#endif
#include "attset.h"

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#ifndef TPLID
#define TPLID       int         /* tuple identifier */
#endif

/*--------------------------------------------------------------------*/

#define int         1           /* to check definitions */
#define long        2           /* for certain types */
#define ptrdiff_t   3

/*--------------------------------------------------------------------*/

#if   TPLID==int
#ifndef TPLID_MAX
#define TPLID_MAX   INT_MAX     /* maximum tuple identifier */
#endif
#ifndef TPLID_FMT
#define TPLID_FMT   "d"         /* printf format code for int */
#endif
#ifndef tid_qsort
#define tid_qsort   int_qsort
#endif

#elif TPLID==long
#ifndef TPLID_MAX
#define TPLID_MAX   LONG_MAX    /* maximum tuple identifier */
#endif
#ifndef TPLID_FMT
#define TPLID_FMT   "ld"        /* printf format code for long */
#endif
#ifndef tid_qsort
#define tid_qsort   lng_qsort
#endif

#elif TPLID==ptrdiff_t
#ifndef TPLID_MAX
#define TPLID_MAX   PTRDIFF_MAX /* maximum tuple identifier */
#endif
#ifndef TPLID_FMT
#  ifdef _MSC_VER
#  define TPLID_FMT "Id"        /* printf format code for ptrdiff_t */
#  else
#  define TPLID_FMT "td"        /* printf format code for ptrdiff_t */
#  endif                        /* MSC still does not support C99 */
#endif
#ifndef tid_qsort
#define tid_qsort   dif_qsort
#endif

#else
#error "TPLID must be either 'int', 'long' or 'ptrdiff_t'"
#endif

/*--------------------------------------------------------------------*/

#undef int                      /* remove preprocessor definitions */
#undef long                     /* needed for the type checking */
#undef ptrdiff_t

/*--------------------------------------------------------------------*/

/* --- cut/copy flags --- */
#define TAB_ALL     AS_ALL       /* cut/copy all      columns/tuples */
#define TAB_RANGE   AS_RANGE     /* cut/copy range of columns/tuples */
#define TAB_MARKED  AS_MARKED    /* cut/copy marked   columns/tuples */

/* --- read/write flags --- */
#define TAB_ONE     (AS_MARKED << 1)  /* read only one record */

/* --- one point coverage flags --- */
#define TAB_COND    0x0000       /* compute condensed form */
#define TAB_FULL    0x0001       /* fully expand null values */
#define TAB_NORM    0x0002       /* normalize one point coverages */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- tuple --- */
  ATTSET       *attset;         /* underlying attribute set */
  struct table *table;          /* containing table (if any) */
  TPLID        id;              /* identifier (index in table) */
  TPLID        mark;            /* mark,   e.g. to indicate usage */
  WEIGHT       wgt;             /* weight, e.g. number of occurrences */
  WEIGHT       xwgt;            /* extended weight */
  INST         cols[1];         /* columns (holding attribute values) */
} TUPLE;                        /* (tuple) */

typedef void TPL_DELFN (TUPLE *tpl);
typedef void TPL_APPFN (TUPLE *tpl, void *data);
typedef int  TPL_SELFN (const TUPLE *tpl, void *data);
typedef int  TPL_CMPFN (const TUPLE *t1, const TUPLE *t2, void *data);

typedef struct table {          /* --- table --- */
  char         *name;           /* table name */
  ATTSET       *attset;         /* underlying attribute set */
  TPLID        size;            /* size of tuple array */
  TPLID        cnt;             /* number of tuples */
  TUPLE        **tpls;          /* tuple array */
  TUPLE        *buf;            /* buffer for a tuple */
  TPL_DELFN    *delfn;          /* tuple deletion function */
  double       wgt;             /* total tuple weight */
} TABLE;                        /* (table) */

/*----------------------------------------------------------------------
  Tuple Functions
----------------------------------------------------------------------*/
extern TUPLE*  tpl_create  (ATTSET *attset, int fromas);
extern TUPLE*  tpl_clone   (const TUPLE *tpl);
extern void    tpl_copy    (TUPLE *dst, const TUPLE *src);
extern void    tpl_delete  (TUPLE *tpl);
extern int     tpl_cmp     (const TUPLE *tpl1, const TUPLE *tpl2,
                            void *data);
extern int     tpl_cmpx    (const TUPLE *tpl1, const TUPLE *tpl2,
                            void *data);
extern int     tpl_cmp1    (const TUPLE *tpl1, const TUPLE *tpl2,
                            void *data);
extern double  tpl_sqrdist (const TUPLE *tpl1, const TUPLE *tpl2);

extern ATTSET* tpl_attset  (TUPLE *tpl);
extern ATTID   tpl_attcnt  (const TUPLE *tpl);
extern ATT*    tpl_colatt  (TUPLE *tpl, ATTID colid);
extern INST*   tpl_colval  (TUPLE *tpl, ATTID colid);
extern int     tpl_coltype (TUPLE *tpl, ATTID colid);
extern ATTID   tpl_colcnt  (const TUPLE *tpl);
extern TPLID   tpl_getmark (const TUPLE *tpl);
extern TPLID   tpl_setmark (TUPLE *tpl, TPLID mark);
extern WEIGHT  tpl_getwgt  (const TUPLE *tpl);
extern WEIGHT  tpl_setwgt  (TUPLE *tpl, WEIGHT wgt);
extern WEIGHT  tpl_incwgt  (TUPLE *tpl, WEIGHT wgt);
extern WEIGHT  tpl_getxwgt (const TUPLE *tpl);
extern WEIGHT  tpl_setxwgt (TUPLE *tpl, WEIGHT wgt);
extern WEIGHT  tpl_incxwgt (TUPLE *tpl, WEIGHT wgt);
extern WEIGHT  tpl_mulxwgt (TUPLE *tpl, WEIGHT wgt);
extern TABLE*  tpl_table   (TUPLE *tpl);
extern TPLID   tpl_id      (const TUPLE *tpl);

extern void    tpl_toas    (TUPLE *tpl);
extern void    tpl_fromas  (TUPLE *tpl);
extern ATTID   tpl_nullcnt (const TUPLE *tpl);
extern int     tpl_isect   (TUPLE *res, TUPLE *tpl1, const TUPLE *tpl2);
extern int     tpl_compat  (const TUPLE *tpl1, const TUPLE *tpl2);
extern size_t  tpl_hash    (TUPLE *tpl);

#ifndef NDEBUG
extern void    tpl_show    (TUPLE *tpl, TPL_APPFN showfn, void *data);
#endif

/*----------------------------------------------------------------------
  Table Functions
----------------------------------------------------------------------*/
extern TABLE*  tab_create  (const char *name, ATTSET *attset,
                            TPL_DELFN delfn);
extern TABLE*  tab_clone   (const TABLE *tab, int cloneas);
extern void    tab_delete  (TABLE *tab, int delas);
extern int     tab_rename  (TABLE *tab, const char *name);
extern int     tab_cmp     (const TABLE *tab1, const TABLE *tab2,
                            TPL_CMPFN cmpfn, void *data);

extern CCHAR*  tab_name    (const TABLE *tab);
extern ATTSET* tab_attset  (TABLE *tab);
extern ATTID   tab_attcnt  (const TABLE *tab);
extern TUPLE*  tab_buf     (TABLE *tab);

extern TPLID   tab_reduce  (TABLE *tab);
extern int     tab_opc     (TABLE *tab, int mode);
extern WEIGHT  tab_poss    (TABLE *tab, TUPLE *tpl);
extern void    tab_possx   (TABLE *tab, TUPLE *tpl, double res[]);

extern double  tab_balance (TABLE *tab, ATTID colid,
                            double wgtsum, double *fracs, int intmul);
extern double  tab_getwgt  (const TABLE *tab, TPLID off, TPLID cnt);
extern void    tab_shuffle (TABLE *tab, TPLID off, TPLID cnt,
                            RANDFN randfn);
extern void    tab_sort    (TABLE *tab, TPLID off, TPLID cnt, int dir,
                            TPL_CMPFN cmpfn, void *data);
extern TPLID   tab_search  (TABLE *tab, TPLID off, TPLID cnt,
                            TUPLE *tpl, TPL_CMPFN cmpfn, void *data);
extern TPLID   tab_group   (TABLE *tab, TPLID off, TPLID cnt,
                            TPL_SELFN selfn, void *data);
extern void    tab_apply   (TABLE *tab, TPLID off, TPLID cnt,
                            TPL_APPFN appfn, void *data);
extern void    tab_fill    (TABLE *tab, TPLID tploff, TPLID tplcnt,
                                        ATTID coloff, ATTID colcnt);
extern ATTID   tab_join    (TABLE *dst, TABLE *src,
                            ATTID *dcis, ATTID *scis, ATTID cnt);
#ifdef TAB_READ
extern CCHAR*  tab_errmsg  (TABLE *tab, char *buf, size_t size);
extern int     tab_read    (TABLE *tab, TABREAD *trd, int mode, ...);
extern int     tab_vread   (TABLE *tab, TABREAD *trd, int mode,
                            va_list *args);
#endif
#ifdef TAB_WRITE
extern int     tab_write   (TABLE *tab, TABWRITE *twr, int mode, ...);
extern int     tab_vwrite  (TABLE *tab, TABWRITE *twr, int mode,
                            va_list *args);
#endif
#ifndef NDEBUG
extern void    tab_show    (const TABLE *tab, TPLID off, TPLID cnt,
                            TPL_APPFN show, void *data);
#endif

/*----------------------------------------------------------------------
  Table Column Functions
----------------------------------------------------------------------*/
extern int     tab_coladd  (TABLE *tab, ATT *att, int fill);
extern int     tab_coladdm (TABLE *tab, ATT **att, ATTID cnt);
extern void    tab_colrem  (TABLE *tab, ATTID colid);
extern int     tab_colconv (TABLE *tab, ATTID colid, int type);
extern void    tab_colnorm (TABLE *tab, ATTID colid,
                            double exp, double sdev,
                            double *pscl, double *poff);
extern void    tab_coltlin (TABLE *tab, ATTID colid,
                            double scl, double off);
extern void    tab_colexg  (TABLE *tab, ATTID colid1, ATTID colid2);
extern void    tab_colmove (TABLE *tab, ATTID off,ATTID cnt,ATTID pos);
extern void    tab_colperm (TABLE *tab, ATTID *perm);
extern int     tab_colcut  (TABLE *dst, TABLE *src, int mode, ...);
extern int     tab_colcopy (TABLE *dst, const TABLE *src, int mode,...);
extern int     tab_coltype (TABLE *tab, ATTID colid);
extern ATT*    tab_col     (TABLE *tab, ATTID colid);
extern ATTID   tab_colcnt  (const TABLE *tab);

/*----------------------------------------------------------------------
  Table Tuple Functions
----------------------------------------------------------------------*/
extern int     tab_tpladd  (TABLE *tab, TUPLE *tpl);
extern int     tab_tpladdm (TABLE *tab, TUPLE **tpls, TPLID cnt);
extern TUPLE*  tab_tplrem  (TABLE *tab, TPLID tplid);
extern void    tab_tplexg  (TABLE *tab, TPLID tplid1, TPLID tplid2);
extern void    tab_tplmove (TABLE *tab, TPLID off,TPLID cnt,TPLID pos);
extern int     tab_tplcut  (TABLE *dst, TABLE *src, int mode, ...);
extern int     tab_tplcopy (TABLE *dst, const TABLE *src, int mode,...);
extern TUPLE*  tab_tpl     (TABLE *tab, TPLID tplid);
extern TPLID   tab_tplcnt  (const TABLE *tab);
extern double  tab_tplwgt  (const TABLE *tab);

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define tpl_attset(t)      ((t)->attset)
#define tpl_attcnt(t)      as_attcnt((t)->attset)
#define tpl_colatt(t,i)    as_att((t)->attset, i)
#define tpl_colval(t,i)    ((t)->cols +(i))
#define tpl_coltype(t,i)   att_type(tpl_colatt(t, i))
#define tpl_colcnt(t)      as_attcnt((t)->attset)
#define tpl_getmark(t)     ((t)->mark)
#define tpl_setmark(t,m)   ((t)->mark  = (m))
#define tpl_getwgt(t)      ((t)->wgt)
#define tpl_getxwgt(t)     ((t)->xwgt)
#define tpl_setxwgt(t,w)   ((t)->xwgt  = (w))
#define tpl_incxwgt(t,w)   ((t)->xwgt += (w))
#define tpl_mulxwgt(t,w)   ((t)->xwgt *= (w))
#define tpl_table(t)       ((t)->table)
#define tpl_id(t)          ((t)->id)

/*--------------------------------------------------------------------*/
#define tab_name(t)        ((t)->name)
#define tab_attset(t)      ((t)->attset)
#define tab_attcnt(t)      as_attcnt((t)->attset)
#define tab_buf(t)         ((t)->buf)
#define tab_errmsg(t,b,s)  as_errmsg((t)->attset, b, s)

#define tab_coladd(t,c,i)  tab_coladdm(t, &(c), (i) ? -1 : 1)
#define tab_coltype(t,i)   att_type(tab_col(t, i))
#define tab_col(t,i)       as_att((t)->attset, i)
#define tab_colcnt(t)      as_attcnt((t)->attset)

#define tab_tpl(t,i)       ((t)->tpls[i])
#define tab_tplcnt(t)      ((t)->cnt)
#define tab_tplwgt(t)      ((t)->wgt)
#endif
