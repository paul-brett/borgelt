/*----------------------------------------------------------------------
  File    : frqtab.h
  Contents: frequency table management
  Author  : Christian Borgelt
  History : 1997.06.23 file created
            1997.07.29 first version completed
            1997.08.11 some functions changed to #define
            1997.08.25 functions ft_comb(), ft_uncomb(), ft_dest() added
            1997.09.24 function ft_alldst() added
            1998.02.09 order of evaluation measures changed
            1999.02.23 parameter 'params' added to function ft_eval()
            1999.03.30 all 'float' fields/variables changed to 'double'
            1999.10.25 evaluation measure FEM_WDIFF added
            2000.12.03 table access optimized (concerning index -1)
            2000.12.06 evaluation function made an option
            2000.12.21 functions ft_xcnt() and ft_ycnt() added
            2001.03.02 evaluation measure FEM_INFGBAL added
            2002.01.02 measure FEM_SPCGBAL added, FEM_INFGBAL corrected
            2002.01.11 measure FEM_CHI2NRM added
            2002.02.02 FEM_K2MET etc. replaced by FEM_BDM and FEM_BDMOD
            2002.02.04 quadratic information measures added
            2009.04.16 modified relevance measure added
            2013.08.23 preprocessor definition of type DIMID added
            2013.08.26 indexing system for frq_xy made more consistent
----------------------------------------------------------------------*/
#ifndef __FRQTAB__
#define __FRQTAB__
#include <float.h>

#ifdef _MSC_VER
#ifndef strtoll
#define strtoll     _strtoi64   /* string to long long int */
#endif
#endif                          /* MSC still does not support C99 */

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

#ifdef FT_EVAL
/* --- evaluation measures --- */
#define FEM_NONE      0         /* no measure */
#define FEM_INFGAIN   1         /* information gain */
#define FEM_INFGBAL   2         /* balanced information gain */
#define FEM_INFGR     3         /* information gain ratio */
#define FEM_INFSGR1   4         /* sym. information gain ratio 1 */
#define FEM_INFSGR2   5         /* sym. information gain ratio 2 */
#define FEM_QIGAIN    6         /* quadratic information gain */
#define FEM_QIGBAL    7         /* balanced quad. information gain */
#define FEM_QIGR      8         /* quadratic information gain ratio */
#define FEM_QISGR1    9         /* sym. quad. info. gain ratio */
#define FEM_QISGR2   10         /* sym. quad. info. gain ratio */
#define FEM_GINI     11         /* gini index */
#define FEM_GINISYM  12         /* symmetric gini index */
#define FEM_GINIMOD  13         /* modified gini index */
#define FEM_RELIEF   14         /* relief measure */
#define FEM_WDIFF    15         /* sum of weighted differences */
#define FEM_CHI2     16         /* chi^2 measure */
#define FEM_CHI2NRM  17         /* chi^2 measure */
#define FEM_WEVID    18         /* weight of evidence */
#define FEM_RELEV    19         /* relevance */
#define FEM_RELMOD   20         /* modified relevance */
#define FEM_BDM      21         /* Bayesian-Dirichlet / K2 metric */
#define FEM_BDMOD    22         /* modified BD / K2 metric */
#define FEM_RDLREL   23         /* red. of desc. length (rel. freq.) */
#define FEM_RDLABS   24         /* red. of desc. length (abs. freq.) */
#define FEM_STOCO    25         /* stochastic complexity */
#define FEM_SPCGAIN  26         /* specificity gain */
#define FEM_SPCGBAL  27         /* balanced specificity gain */
#define FEM_SPCGR    28         /* specificity gain ratio */
#define FEM_SPCSGR1  29         /* sym. specificity gain ratio 1 */
#define FEM_SPCSGR2  30         /* sym. specificity gain ratio 2 */
#define FEM_UNKNOWN  31         /* unknown measure */

/* --- evaluation flags --- */
#define FEF_WGTD     0x8000     /* weight with frac. of known values */

#endif
/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- frequency table --- */
  DIMID  xsize, ysize;          /* size of table (columns/rows) */
  DIMID  xcnt,  ycnt;           /* current number of columns/rows */
  DIMID  *dsts;                 /* destinations of combined columns */
  double *buf_x;                /* buffer for internal use */
  double *buf_y;                /* ditto */
  double *buf_xy;               /* ditto */
  double frq;                   /* total number of cases */
  double known;                 /* number of cases with a known value */
  double *frq_x;                /* marginal x frequencies */
  double *frq_y;                /* marginal y frequencies */
  double *frq_z[1];             /* freqs. for unknown x (frq_xy[-1]) */
  double *frq_xy[1];            /* table of joint frequencies */
} FRQTAB;                       /* (frequency table) */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/
extern FRQTAB* ft_create (DIMID xsize, DIMID ysize);
extern void    ft_delete (FRQTAB *ftab);
extern void    ft_init   (FRQTAB *ftab, DIMID xcnt, DIMID ycnt);
extern void    ft_copy   (FRQTAB *dst, const FRQTAB *src);

extern void    ft_set    (FRQTAB *ftab, DIMID x, DIMID y, double frq);
extern void    ft_add    (FRQTAB *ftab, DIMID x, DIMID y, double frq);
extern void    ft_marg   (FRQTAB *ftab);
extern void    ft_addwm  (FRQTAB *ftab, DIMID x, DIMID y, double frq);
extern void    ft_move   (FRQTAB *ftab, DIMID xsrc, DIMID xdst, DIMID y,
                          double frq);

extern void    ft_comb   (FRQTAB *ftab, DIMID xsrc, DIMID xdst);
extern void    ft_uncomb (FRQTAB *ftab, DIMID xsrc);
extern DIMID   ft_dest   (FRQTAB *ftab, DIMID x);
extern void    ft_alldst (FRQTAB *ftab, DIMID *dsts);

extern DIMID   ft_xcnt   (FRQTAB *ftab);
extern DIMID   ft_ycnt   (FRQTAB *ftab);
extern double  ft_frq_xy (FRQTAB *ftab, DIMID x, DIMID y);
extern double  ft_frq_x  (FRQTAB *ftab, DIMID x);
extern double  ft_frq_y  (FRQTAB *ftab, DIMID y);
extern double  ft_frq    (FRQTAB *ftab);
extern double  ft_known  (FRQTAB *ftab);

#ifdef FT_EVAL
extern double  ft_eval   (FRQTAB *ftab, int measure, double *params);
extern CCHAR*  ft_mname  (int measure);
#endif
#ifndef NDEBUG
extern void    ft_show   (FRQTAB *ftab);
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define ft_set(t,x,y,f)  ((t)->frq_xy[x][y]  = (f))
#define ft_add(t,x,y,f)  ((t)->frq_xy[x][y] += (f))
#define ft_xcnt(t)       ((t)->xcnt)
#define ft_ycnt(t)       ((t)->ycnt)
#define ft_frq_x(t,x)    ((t)->frq_x [x])
#define ft_frq_y(t,y)    ((t)->frq_y [y])
#define ft_frq_xy(t,x,y) ((t)->frq_xy[x][y])
#define ft_frq(t)        ((t)->frq)
#define ft_known(t)      ((t)->known)

#endif
