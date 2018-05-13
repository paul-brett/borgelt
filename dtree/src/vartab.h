/*----------------------------------------------------------------------
  File    : vartab.h
  Contents: variation table management
  Author  : Christian Borgelt
  History : 2000.09.12 file created
            2013.08.23 preprocessor definition of type DIMID added
            2013.08.26 indexing system for data made more consistent
----------------------------------------------------------------------*/
#ifndef __VARTAB__
#define __VARTAB__
#include <float.h>

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#ifndef DIMID
#define DIMID       int         /* dimension identifier */
#endif

/*--------------------------------------------------------------------*/

#define int         1           /* to check definitions */
#define long        2           /* for certain types */
#define ptrdiff_t   3

/*--------------------------------------------------------------------*/

#if   DIMID==int
#ifndef DIMID_MAX
#define DIMID_MAX   INT_MAX     /* maximum dimension identifier */
#endif
#ifndef DIMID_SGN
#define DIMID_SGN   INT_MIN     /* sign of dimension identifier */
#endif
#ifndef DIMID_FMT
#define DIMID_FMT   "d"         /* printf format code for int */
#endif
#ifndef strtodim
#define strtodim(s,p)   (int)strtol(s,p,0)
#endif

#elif DIMID==long
#ifndef DIMID_MAX
#define DIMID_MAX   LONG_MAX    /* maximum dimension identifier */
#endif
#ifndef DIMID_SGN
#define DIMID_SGN   LONG_MIN    /* sign of dimension identifier */
#endif
#ifndef DIMID_FMT
#define DIMID_FMT   "ld"        /* printf format code for long */
#endif
#ifndef strtodim
#define strtodim(s,p)   strtol(s,p,0)
#endif

#elif DIMID==ptrdiff_t
#ifndef DIMID_MAX
#define DIMID_MAX   PTRDIFF_MAX /* maximum dimension identifier */
#endif
#ifndef DIMID_SGN
#define DIMID_SGN   PTRDIFF_MIN /* sign of dimension identifier */
#endif
#ifndef DIMID_FMT
#  ifdef _MSC_VER
#  define DIMID_FMT "Id"        /* printf format code for ptrdiff_t */
#  else
#  define DIMID_FMT "td"        /* printf format code for ptrdiff_t */
#  endif                        /* MSC still does not support C99 */
#endif
#ifndef strtodim
#define strtodim(s,p)   (ptrdiff_t)strtoll(s,p,0)
#endif

#else
#error "DIMID must be either 'int', 'long' or 'ptrdiff_t'"
#endif

/*--------------------------------------------------------------------*/

#undef int                      /* remove preprocessor definitions */
#undef long                     /* needed for the type checking */
#undef ptrdiff_t

/*--------------------------------------------------------------------*/

#define CCHAR      const char   /* abbreviation */

#ifdef VT_EVAL
/* --- evaluation measures --- */
#define VEM_NONE      0         /* no measure */
#define VEM_SSE       1         /* sum of squared errors */
#define VEM_MSE       2         /* mean squared error */
#define VEM_RMSE      3         /* square root of mean squared error */
#define VEM_VAR       4         /* variance (unbiased estimator) */
#define VEM_SDEV      5         /* standard deviation (from variance) */
#define VEM_UNKNOWN   6         /* unknown evaluation measure */

/* --- evaluation flags --- */
#define VEF_WGTD    0x8000      /* weight with frac. of known values */

#endif
/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- variation data --- */
  double  frq;                  /* number of cases */
  double  sum;                  /* sum of values */
  double  ssv;                  /* sum of squared values */
  double  mean;                 /* mean value */
  double  sse;                  /* sum of squared errors */
} VARDATA;                      /* (variation data) */

typedef struct {                /* --- variation table --- */
  DIMID   size;                 /* size of table (number of colums) */
  DIMID   cnt;                  /* current number of columns */
  DIMID   *dsts;                /* destinations of combined columns */
  double  known;                /* number of cases with known values */
  double  frq;                  /* total frquency (number of cases) */
  double  sum;                  /* sum of values */
  double  ssv;                  /* sum of squared values */
  double  mean;                 /* mean value */
  double  sse;                  /* sum of squared errors */
  VARDATA null[1];              /* variation data for unknown x */
  VARDATA data[1];              /* variation data per column */
} VARTAB;                       /* (variation table) */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/
extern VARTAB* vt_create  (DIMID size);
extern void    vt_delete  (VARTAB *vtab);
extern void    vt_init    (VARTAB *vtab, DIMID cnt);
extern void    vt_copy    (VARTAB *dst, const VARTAB *src);

extern void    vt_add     (VARTAB *vtab, DIMID x, double y, double frq);
extern void    vt_calc    (VARTAB *vtab);
extern void    vt_move    (VARTAB *vtab, DIMID xsrc, DIMID xdst,
                           double y, double frq);

extern void    vt_comb    (VARTAB *vtab, DIMID xsrc, DIMID xdst);
extern void    vt_uncomb  (VARTAB *vtab, DIMID xsrc);
extern DIMID   vt_dest    (VARTAB *vtab, DIMID x);
extern void    vt_alldst  (VARTAB *vtab, DIMID *dsts);

extern DIMID   vt_colcnt  (VARTAB *vtab);
extern double  vt_colfrq  (VARTAB *vtab, DIMID x);
extern double  vt_colsum  (VARTAB *vtab, DIMID x);
extern double  vt_colssv  (VARTAB *vtab, DIMID x);
extern double  vt_colmean (VARTAB *vtab, DIMID x);
extern double  vt_colsse  (VARTAB *vtab, DIMID x);
extern double  vt_frq     (VARTAB *vtab);
extern double  vt_sum     (VARTAB *vtab);
extern double  vt_ssv     (VARTAB *vtab);
extern double  vt_mean    (VARTAB *vtab);
extern double  vt_sse     (VARTAB *vtab);
extern double  vt_known   (VARTAB *vtab);

#ifdef VT_EVAL
extern double  vt_eval   (VARTAB *vtab, int measure, double *params);
extern CCHAR*  vt_mname  (int measure);
#endif
#ifndef NDEBUG
extern void    vt_show   (VARTAB *vtab);
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define vt_colcnt(t)      ((t)->cnt)
#define vt_colfrq(t,x)    ((t)->data[x].frq)
#define vt_colsum(t,x)    ((t)->data[x].sum)
#define vt_colssv(t,x)    ((t)->data[x].ssv)
#define vt_colmean(t,x)   ((t)->data[x].mean)
#define vt_colsse(t,x)    ((t)->data[x].sse)
#define vt_frq(t)         ((t)->frq)
#define vt_sum(t)         ((t)->sum)
#define vt_ssv(t)         ((t)->ssv)
#define vt_mean(t)        ((t)->mean)
#define vt_sse(t)         ((t)->sse)
#define vt_known(t)       ((t)->known)

#endif
