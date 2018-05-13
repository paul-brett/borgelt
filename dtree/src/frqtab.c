/*----------------------------------------------------------------------
  File    : frqtab.c
  Contents: frequency table management
  Author  : Christian Borgelt
  History : 1997.06.26 file created
            1997.07.29 first version completed
            1997.08.11 some functions changed to #define
            1997.08.25 functions ft_comb, ft_uncomb, and ft_dest added
            1997.09.18 bug in measure evaluation removed
            1997.09.24 function ft_alldst added
            1997.09.29 bug in function ft_comb removed
            1997.09.30 bug in evaluation with combined columns removed
            1998.02.09 order of evaluation measures changed
            1998.02.24 bug in function wevid() fixed
            1998.03.23 parameters added to evaluation functions
            1999.03.20 all 'float' fields/variables changed to 'double'
            1999.10.25 evaluation function wdiff() added
            2000.09.15 some assertions added
            2000.12.02 memory alloc. improved, function ft_copy added
            2000.12.03 table access optimized (concerning index -1)
            2001.03.02 evaluation measure FEM_INFGBAL added
            2001.05.26 computation of ln(\Gamma(n)) improved
            2001.09.26 bug in clean up in ft_create removed
            2002.01.02 measure FEM_SPCGBAL added, FEM_INFGBAL corrected
            2002.01.06 switched to sorting functions from arrays
            2002.01.11 measure FEM_CHI2NRM added
            2002.01.22 computations in wdiff() improved
            2002.01.31 computation of Gini index measures improved
            2002.02.02 BD and description length measures reprogrammed
            2002.02.04 quadratic information measures added
            2002.07.04 bug in function bdm() fixed (equiv. sample size)
            2006.04.27 bug in function quad() (FEM_QIGBAL) fixed
            2006.09.26 bug in function rdlen() fixed (sensitiv. param.)
            2009.04.16 modified relevance measure added
            2013.03.07 adapted to direction param. of sorting functions
            2013.08.23 adapted to preprocessor definition of DIMID
            2013.08.26 indexing system for frq_xy made more consistent
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "arrays.h"
#include "gamma.h"
#include "frqtab.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#ifdef FT_EVAL
#define M_PI         3.14159265358979323846  /* \pi   */
#define LN_2         0.69314718055994530942  /* ln(2) */
#define EPSILON      1e-12      /* to handle roundoff errors */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef double EVALFN (FRQTAB* ftab, int measure, double *params);

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
static const char* mnames[FEM_UNKNOWN+1] = {
  /* FEM_NONE      0 */ "no measure",
  /* FEM_INFGAIN   1 */ "information gain",
  /* FEM_INFGBAL   2 */ "balanced information gain",
  /* FEM_INFGR     3 */ "information gain ratio",
  /* FEM_INFSGR1   4 */ "symmetric information gain ratio 1",
  /* FEM_INFSGR2   5 */ "symmetric information gain ratio 2",
  /* FEM_QIGAIN    6 */ "quadratic information gain",
  /* FEM_QIGBAL    7 */ "balanced quadratic information gain",
  /* FEM_QIGR      8 */ "quadratic information gain ratio",
  /* FEM_QISGR1    9 */ "symmetric quadratic information gain ratio 1",
  /* FEM_QISGR2   10 */ "symmetric quadratic information gain ratio 2",
  /* FEM_GINI     11 */ "Gini index",
  /* FEM_GINISYM  12 */ "symmetric Gini index",
  /* FEM_GINIMOD  13 */ "modified Gini index",
  /* FEM_RELIEF   14 */ "relief measure",
  /* FEM_WDIFF    15 */ "sum of weighted differences",
  /* FEM_CHI2     16 */ "chi^2 measure",
  /* FEM_CHI2NRM  17 */ "normalized chi^2 measure",
  /* FEM_WEVID    18 */ "weight of evidence",
  /* FEM_RELEV    19 */ "relevance",
  /* FEM_RELMOD   20 */ "modified relevance",
  /* FEM_BDM      21 */ "Bayesian-Dirichlet / K2 metric",
  /* FEM_BDMOD    22 */ "modified Bayesian-Dirichlet / K2 metric",
  /* FEM_RDLREL   23 */ "reduction of description length (rel. freq.)",
  /* FEM_RDLABS   24 */ "reduction of description length (abs. freq.)",
  /* FEM_STOCO    25 */ "stochastic complexity",
  /* FEM_SPCGAIN  26 */ "specificity gain",
  /* FEM_SPCGAIN  27 */ "balanced specificity gain",
  /* FEM_SPCGR    28 */ "specificity gain ratio",
  /* FEM_SPCSGR1  29 */ "symmetric specificity gain ratio 1",
  /* FEM_SPCSGR2  30 */ "symmetric specificity gain ratio 2",
  /* FEM_UNKNOWN  31 */ "<unknown measure>",
};                              /* names of evaluation measures */

/*----------------------------------------------------------------------
  Auxiliary Functions
----------------------------------------------------------------------*/

static double nsp (double *dist, DIMID cnt)
{                               /* --- compute nonspecificity */
  double nsp  = 0;              /* nonspecificity */
  double prec = 0;              /* preceding frequency */
  double t;                     /* temporary buffer */

  assert(dist && (cnt >= 0));   /* check the function arguments */
  dbl_qsort(dist, (size_t)cnt, +1);
  for ( ; cnt > 1; cnt--) {     /* sort and traverse the frequencies */
    t = *dist -prec; prec = *dist++;
    if (t > 0) nsp += t *log((double)cnt);
  }                             /* calculate and return the */
  return nsp;                   /* nonspecificity of the distribution */
}  /* nsp() */

/*----------------------------------------------------------------------
  Evaluation Functions
----------------------------------------------------------------------*/

static double info (FRQTAB *ftab, int measure, double *params)
{                               /* --- Shannon information measures */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double s_x, s_y, s_xy;        /* sums for entropy computation */
  double info, t;               /* information gain (ratio), buffer */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  s_x = s_y = s_xy = 0;         /* initialize the sums */
  for (y = 0; y < ftab->ycnt; y++)
    if (fy[y] > 0) s_y += fy[y] *log(fy[y]);
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] <= 0) continue;   /* skip empty and combined columns */
    s_x += fx[x] *log(fx[x]);   /* process the column distribution */
    for (t = 0, y = 0; y < ftab->ycnt; y++)
      if (fxy[x][y] > 0) t += fxy[x][y] *log(fxy[x][y]);
    s_xy += t;                  /* process columns individually and */
  }                             /* sum the results (higher accuracy) */
  t = ftab->known; t *= log(t); /* compute N *log(N) only once */
  s_x  = t -s_x;                /* N H_x  = -N sum_x  p_x  *log(p_x)  */
  s_y  = t -s_y;                /* N H_y  = -N sum_y  p_y  *log(p_y)  */
  s_xy = t -s_xy;               /* N H_xy = -N sum_xy p_xy *log(p_xy) */
  info = s_x +s_y -s_xy;        /* compute information gain *N *ln(2) */
  switch (measure & 0xff) {     /* evaluate the measure code */
    case FEM_INFGBAL: info /= log((double)ftab->xcnt)
                                * ftab->known;     break;
    case FEM_INFGR  : if (s_x      <= 0) return 0;
                      info /= s_x;                 break;
    case FEM_INFSGR1: if (s_xy     <= 0) return 0;
                      info /= s_xy;                break;
    case FEM_INFSGR2: if (s_x +s_y <= 0) return 0;
                      info /= s_x +s_y;            break;
    default:          info /= LN_2 *ftab->known;   break;
  }                             /* form requested entropy ratio */
  if (measure & FEF_WGTD) return info *(ftab->known/ftab->frq);
  return info;                  /* return the information measure */
}  /* info() */

/*--------------------------------------------------------------------*/

static double quad (FRQTAB *ftab, int measure, double *params)
{                               /* --- quadratic information measures */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double s_x, s_y, s_xy;        /* sum of squared frequencies */
  double quad, t;               /* information gain (ratio), buffer */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  s_y = s_x = s_xy = 0;         /* initialize the sums */
  for (y = 0; y < ftab->ycnt; y++)
    s_y += fy[y] *fy[y];        /* compute sum_y N(y)^2 */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] <= 0) continue;   /* skip empty and combined columns */
    s_x += fx[x] *fx[x];        /* process the column distribution */
    for (t = 0, y = 0; y < ftab->ycnt; y++)
      t += fxy[x][y] *fxy[x][y];/* process columns individually and */
    s_xy += t;                  /* sum the results (higher accuracy) */
  }                             /* (compute sum_xy N(x,y)^2) */
  t = ftab->known; t *= t;      /* compute N^2 only once */
  s_x  = t -s_x;                /* N^2/2 H^2_i  = N^2 -sum_i  N_i^2  */
  s_y  = t -s_y;                /* N^2/2 H^2_j  = N^2 -sum_j  N_j^2  */
  s_xy = t -s_xy;               /* N^2/2 H^2_ij = N^2 -sum_ij N_ij^2 */
  quad = s_x +s_y -s_xy;        /* compute information gain *N *ln(2) */
  switch (measure & 0xff) {     /* evaluate the measure code */
  case FEM_QIGBAL: quad /= t *(1 -1.0/(double)ftab->xcnt); break;
    case FEM_QIGR  : if (s_x      <= 0) return 0;
                     quad /= s_x;                          break;
    case FEM_QISGR1: if (s_xy     <= 0) return 0;
                     quad /= s_xy;                         break;
    case FEM_QISGR2: if (s_x +s_y <= 0) return 0;
                     quad /= s_x +s_y;                     break;
    default:         quad /= 0.5 *t;                       break;
  }                             /* form requested entropy ratio */
  if (measure & FEF_WGTD) return quad *(ftab->known/ftab->frq);
  return quad;                  /* return the information measure */
}  /* quad() */

/*--------------------------------------------------------------------*/

static double gini (FRQTAB *ftab, int measure, double *params)
{                               /* --- Gini index/relief measure */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double s_x, s_y, s_xy;        /* sum of squared frequencies */
  double w_xy, w_yx;            /* weighted sum of squared freq. */
  double gini, t;               /* Gini index / relief measure */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  s_y = s_xy = w_yx = 0;        /* initialize the sums */
  for (y = 0; y < ftab->ycnt; y++)
    s_y += fy[y] *fy[y];        /* compute sum_y N(y)^2 */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] <= 0) continue;   /* skip empty and combined columns */
    for (t = 0, y = 0; y < ftab->ycnt; y++)
      t += fxy[x][y] *fxy[x][y];/* process columns individually */
    s_xy += t;                  /* and sum the results */
    w_yx += t / fx[x];          /* compute sum_xy N(x,y)^2 and */
  }                             /* sum_x 1/N(x) sum_y N(x,y)^2 */
  if ((measure & 0xff) == FEM_GINI) {
    return (w_yx -s_y /ftab->known)
         / ((measure & FEF_WGTD) ? ftab->frq : ftab->known);
  }                             /* compute and return the Gini index */

  s_x = w_xy = 0;               /* initialize the sums */
  for (x = 0; x < ftab->xcnt; x++)
    s_x += fx[x] *fx[x];        /* compute sum_x N(x)^2 */
  if (((measure & 0xff) != FEM_GINIMOD)
  &&  ((measure & 0xff) != FEM_RELIEF)) {
    for (y = 0; y < ftab->ycnt; y++) {
      if (fy[y] <= 0) continue; /* skip empty rows (vanishing freq.) */
      for (t = 0, x = 0; x < ftab->xcnt; x++)
        if (fx[x] > 0) t += fxy[x][y] *fxy[x][y];
      w_xy += t / fy[y];        /* process columns individually */
    }                           /* and sum the results */
  }                             /* (sum_y 1/N(y) sum_x N(x,y)^2) */

  t = ftab->known; t *= t;      /* compute N^2 */
  switch (measure & 0xff) {     /* evaluate the measure code */
    case FEM_GINIMOD:           /* modified Gini index */
      if (s_x <= 0) return 0;   /* check the denominator */
      gini = s_xy / s_x - s_y / t;                     break;
    case FEM_RELIEF:            /* relief measure */
      if ((s_y <= 0) || (t -s_y <= 0)) return 0;
      gini = s_xy / s_y - (s_x -s_xy) / (t -s_y);      break;
    default:                    /* symmetric Gini index */
      t = 2*t -s_x -s_y;        /* compute and */
      if (t <= 0) return 0;     /* check the denominator */
      gini = ((w_xy +w_yx) *ftab->known -s_x -s_y) /t; break;
  }                             /* compute requested measure */
  if (measure & FEF_WGTD) return gini *(ftab->known/ftab->frq);
  return gini;                  /* return Gini index/relief measure */
}  /* gini() */

/*--------------------------------------------------------------------*/

static double wdiff (FRQTAB *ftab, int measure, double *params)
{                               /* --- weighted sum of differences */
  DIMID  x, y, m;               /* loop variables, integer exponent */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double e, s, t;               /* exponent of difference, buffers */
  double sum;                   /* sum of weighted differences */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  e = (params) ? *params : 1.0; /* get the sensitivity parameter */
  if      (e <= 0.0) e = 1.0;   /* an exponent less than 0 is useless */
  if      (e == 1.0) m = 1;     /* (exponent of absolute difference) */
  else if (e == 2.0) m = 2;     /* and create a flag to distinguish */
  else               m = 0;     /* the two simplest special cases */
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  sum = 0;                      /* initialize the sum */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] <= 0) continue;   /* skip empty and combined columns */
    s = 0; y = ftab->ycnt;      /* traverse the row distribution */
    switch (m) {                /* distinguish special cases */
      case  1: while (--y >= 0){/* if exponent for difference is 1 */
                 t  = fx[x] *fy[y] -fxy[x][y] *ftab->known;
                 s += fxy[x][y] *fabs(t);
               } break;         /* sum weighted absolute differences */
      case  2: while (--y >= 0){/* if exponent for difference is 2 */
                 t  = fx[x] *fy[y] -fxy[x][y] *ftab->known;
                 s += fxy[x][y] *t *t;
               } break;         /* sum weighted squared differences */
      default: while (--y >= 0){/* if any other exponent */
                 t  = fx[x] *fy[y] -fxy[x][y] *ftab->known;
                 s += fxy[x][y] *pow(fabs(t), e);
               } break;         /* raise the differences to */
    }                           /* the given power and sum them */
    sum += s;                   /* sum the results for the columns */
  }
  t = ftab->known;              /* normalize the sum */
  return sum /(pow(t*t, e) *((measure & FEF_WGTD) ? ftab->frq : t));
}  /* wdiff() */                /* return sum of weighted differences */

/*--------------------------------------------------------------------*/

static double chi2 (FRQTAB *ftab, int measure, double *params)
{                               /* --- chi^2 measure */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double s, t, p;               /* temporary buffers */
  double chi2;                  /* chi^2 measure */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  chi2 = 0;                     /* initialize the chi^2 value */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] <= 0) continue;   /* skip empty and combined columns */
    s = 0;                      /* traverse the rows of the column */
    for (y = 0; y < ftab->ycnt; y++) {
      if (fy[y] <= 0) continue; /* traverse non-empty rows */
      p = fx[x] *fy[y];         /* compute (N(x)N(y) - N N(x,y))^2 */
      t = p -fxy[x][y] *ftab->known; /*  / (N(x)N(y))              */
      s += t *t /p;             /* and sum these values */
    }
    chi2 += s;                  /* sum the results for the columns */
  }                             /* (results in chi^2 *N) */
  if ((measure & 0xff) == FEM_CHI2NRM) {
    t = (double)(ftab->xcnt-1) *(double)(ftab->ycnt-1);
    if (t > 0) chi2 /= t;       /* normalize the chi^2 measure by */
  }                             /* dividing by the degrees of freedom */
  return chi2 /((measure & FEF_WGTD) ? ftab->frq : ftab->known);
}  /* chi2() */                 /* return the chi^2 measure */

/*--------------------------------------------------------------------*/

static double wevid (FRQTAB *ftab, int measure, double *params)
{                               /* --- weight of evidence */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double inner, outer;          /* inner and outer sum */
  double a, b, z;               /* temporary buffers */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  outer = 0;                    /* initialize the outer sum */
  for (x = 0; x < ftab->xcnt; x++) {
    if (*--fx <= 0) continue;   /* skip empty and combined columns */
    inner = 0;                  /* traverse the rows of the column */
    for (y = 0; y < ftab->ycnt; y++) {
      z = fy[y] *fxy[x][y];     /* compute the common term */
      a = fxy[x][y] *ftab->known -z;  /* and the terms of */
      b = fx[x] *fy[y]           -z;  /* the odds fraction */
      if ((a >= EPSILON) && (b >= EPSILON))
        inner += fy[y] *fabs(log(a/b));
    }                           /* compute the inner sum */
    outer += fx[x] *inner;      /* compute the outer sum */
  }
  outer /= LN_2 *ftab->known;   /* scale and return the result */
  return outer /((measure & FEF_WGTD) ? ftab->frq : ftab->known);
}  /* wevid() */

/*--------------------------------------------------------------------*/

static double relev (FRQTAB *ftab, int measure, double *params)
{                               /* --- relevance */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double max, t;                /* (maximal) frequency ratio */
  double relev = 0, sum;        /* relevance measure, buffer */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] <= 0) continue;   /* skip empty and combined columns */
    max = sum = 0;              /* traverse the rows of the column */
    for (y = 0; y < ftab->ycnt; y++) {
      if (fxy[x][y] <= 0) continue;   /* skip zero frequencies */
      t = fxy[x][y] / fy[y];    /* compute the frequency ratio */
      if (t <= max) sum += t;
      else        { sum += max; max = t; }
    }                           /* sum ratios with the exception */
    relev += sum;               /* of the/one maximal ratio */
  }
  relev = 1 -relev /(double)(ftab->ycnt-1);
  if (measure & FEF_WGTD) return relev *(ftab->known/ftab->frq);
  return relev;                 /* return the relevance measure */
}  /* relev() */

/*--------------------------------------------------------------------*/

static double relmod (FRQTAB *ftab, int measure, double *params)
{                               /* --- modified relevance */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double max, t;                /* (maximal) frequency ratio */
  double relev = 0, sum;        /* relevance measure, buffer */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] <= 0) continue;   /* skip empty and combined columns */
    sum = max = t = 0;          /* traverse the rows of the column */
    for (y = 0; y < ftab->ycnt; y++) {
      if (fxy[x][y] <= 0) continue; /* skip zero frequencies */
      if (fxy[x][y] <= max) sum += fxy[x][y] /fy[y];
      else                { sum += t; t = (max = fxy[x][y]) /fy[y]; }
    }                           /* sum ratios with the exception of */
    relev += sum;               /* the or one for the majority class */
  }
  relev = 1 -relev /(double)(ftab->ycnt-1);
  if (measure & FEF_WGTD) return relev *(ftab->known/ftab->frq);
  return relev;                 /* return the relevance measure */
}  /* relmod() */

/*--------------------------------------------------------------------*/

static double bdm (FRQTAB *ftab, int measure, double *params)
{                               /* --- Bayesian-Dirichlet metric */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double p, s, a;               /* (sum of) prior(s), sensitivity */
  double bdm, r, t, z;          /* Bayesian-Dirichlet metric, buffers */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  a = (params && (params[0]+1 > 0)) ? params[0]+1 : 1;
  a *= a;                       /* get the sensitivity parameter */
  p = (params && (params[1] !=  0)) ? params[1]   : 1;
  if (p < 0)                    /* if given (negative sign), */
    p /= (double)-ftab->ycnt;   /* get the prior/equiv. sample size */
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  for (r = 0, y = 0; y < ftab->ycnt; y++)
    r += logGamma(fy[y] *a +p); /* process the class distribution */
  s  = (double) ftab->ycnt *p;  /* and compute the reference value */
  r += (double)-ftab->ycnt *logGamma(p)
     + (logGamma(s) -logGamma(ftab->known *a +s));
  if (params && (params[1] < 0))/* adapt prior for l. equiv. version */
    s = (double)ftab->ycnt *(p /= (double)ftab->xcnt);
  z = logGamma(s) -(double)ftab->ycnt *logGamma(p);
  bdm = 0;                      /* init. Bayesian-Dirichlet measure */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] < 0) continue;    /* skip empty and combined columns */
    for (t = 0, y = 0; y < ftab->ycnt; y++)
      t += logGamma(fxy[x][y] *a +p); /* process a cond. distribution */
    bdm += t +z -logGamma(fx[x] *a +s);
  }                             /* sum the results over the columns */
  bdm -= r;                     /* and return the weighted BD metric */
  return bdm /(LN_2 *((measure & FEF_WGTD) ? ftab->frq : ftab->known));
}  /* bdm() */

/*--------------------------------------------------------------------*/

static double bdmod (FRQTAB *ftab, int measure, double *params)
{                               /* --- modified BD / K2 metric 2 */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double p, a;                  /* prior and sensitivity */
  double bdm, r, t, z;          /* Bayesian-Dirichlet metric, buffers */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  a = (params && (params[0] >= 0)) ? params[0] : 0;
                                /* get the sensitivity parameter */
  p = (params && (params[1] != 0)) ? params[1] : 1;
  if (p < 0)                    /* if given (negative sign), */
    p /= (double)-ftab->ycnt;   /* get the prior/equiv. sample size */
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  for (r = 0, y = 0; y < ftab->ycnt; y++) {
    z = fy[y] *a +p; r += logGamma(z +fy[y]) -logGamma(z); }
  z  = ftab->known *a +(double)ftab->ycnt *p; /* process the */
  r += logGamma(z) -logGamma(z +ftab->known); /* class distribution */
  if (params && (params[1] < 0))/* adapt the prior for the */
    p /= (double)ftab->xcnt;    /* likelihood equivalent version */
  bdm = 0;                      /* init. Bayesian-Dirichlet measure */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] < 0) continue;    /* skip empty and combined columns */
    for (t = 0, y = 0; y < ftab->ycnt; y++) {
      z = fxy[x][y] *a +p; t += logGamma(z +fxy[x][y]) -logGamma(z); }
    z = fx[x] *a +(double)ftab->ycnt *p; /* process a cond. distrib. */
    bdm += t +(logGamma(z) -logGamma(z +fx[x]));
  }                             /* sum the results over the columns */
  bdm -= r;                     /* and return the weighted BD metric */
  return bdm /(LN_2 *((measure & FEF_WGTD) ? ftab->frq : ftab->known));
}  /* bdmod() */

/*--------------------------------------------------------------------*/

static double rdlen (FRQTAB *ftab, int measure, double *params)
{                               /* --- reduction of desc. length */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */
  double lGy;                   /* buffer for log_2(Gamma(ycnt)) */
  double a;                     /* sensitivity parameter */
  double dat, mod, s, t;        /* description lengths, buffer */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  lGy = logGamma((double)ftab->ycnt); /* note \ln\Gamma(#classes)) */
  a   = (params && (params[0]+1 > 0)) ? params[0]+1 : 1;
  dat = mod = 0;                /* get the sensitivity parameter */
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */

  /* --- relative frequency coding --- */
  if ((measure & 0xff) == FEM_RDLREL) {
    for (x = 0; x < ftab->xcnt; x++) {
      if (fx[x] <= 0) continue; /* skip empty and combined columns */
      s = 0;                    /* process a conditional distribution */
      for (y = 0; y < ftab->ycnt; y++)
        if (fxy[x][y] > 0) s += fxy[x][y] *log(fxy[x][y]);
      dat -= fx[x] *log(fx[x]) -s; /* compute and sum cond. entropy */
      mod -= logGamma(fx[x] +(double)ftab->ycnt)
           - logGamma(fx[x] +1) -lGy;
    }                           /* sum conditional model terms */
    s = 0;                      /* process the row distribution */
    for (y = 0; y < ftab->ycnt; y++)
      if (fy[y] > 0) s += fy[y] *log(fy[y]);
    dat += ftab->known *log(ftab->known) -s;
    mod += logGamma(ftab->known +(double)ftab->ycnt)
         - logGamma(ftab->known +1) -lGy;
  }                             /* compute red. of desc. lengths */

  /* --- absolute frequency coding --- */
  else {                        /* ((measure & 0xff) == FEM_RDLABS) */
    for (x = 0; x < ftab->xcnt; x++) {
      if (fx[x] <= 0) continue; /* skip empty and combined columns */
      s = 0;                    /* process a conditional distribution */
      for (y = 0; y < ftab->ycnt; y++)
        s += logGamma(fxy[x][y]+1);/* compute data description length */
      dat -= (t = logGamma(fx[x] +1)) -s;       /* and sum the result */
      mod -= logGamma(fx[x] +(double)ftab->ycnt) -t -lGy;
    }                           /* sum model description lengths */
    s = 0;                      /* process the row distribution */
    for (y = 0; y < ftab->ycnt; y++)
      s += logGamma(fy[y] +1);  /* compute data description length */
    dat += (t = logGamma(ftab->known+1)) -s;
    mod += logGamma(ftab->known +(double)ftab->ycnt) -t -lGy;
  }                             /* compute red. of desc. lengths */

  return (dat +mod/a)           /* return the weighted measure */
       / (LN_2 *((measure & FEF_WGTD) ? ftab->frq : ftab->known));
}  /* rdlen() */

/*--------------------------------------------------------------------*/

static double stoco (FRQTAB *ftab, int measure, double *params)
{                               /* --- stochastic complexity */
  DIMID  x;                     /* loop variable */
  double *fx;                   /* to traverse the frequencies */
  double stc;                   /* stochastic complexity */

  assert(ftab);                 /* check the function argument */
  if (ftab->known < EPSILON) return 0;
  stc = log(0.5 *ftab->known);  /* compute log(n../2) */
  fx  = ftab->frq_x;            /* get the column distribution */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] > 0) stc -= log(0.5 *fx[x]);
  }                             /* compute (r_y -1)/2 *[ log(n../2)  */
  stc *= 0.5*(double)(ftab->ycnt-1); /* - sum_{j=1}{r_x} log(n.j/2)] */
  stc -= (double)(ftab->xcnt-1) *(log(pow(M_PI,0.5*(double)ftab->ycnt))
                                    - logGamma(0.5*(double)ftab->ycnt));
  stc /= LN_2 *ftab->known;     /* scale and normalize length */
  stc += info(ftab, FEM_INFGAIN, NULL); /* add the information gain */
  if (measure & FEF_WGTD) return stc *(ftab->known/ftab->frq);
  return stc;                   /* return the stochastic complexity */
}  /* stoco() */

/*--------------------------------------------------------------------*/

static double spec (FRQTAB *ftab, int measure, double *params)
{                               /* --- specificity measures */
  DIMID  x, y;                  /* loop variables */
  double *fx, **fxy;            /* to traverse the frequencies */
  double *bx, *by, *bxy;        /* to traverse the buffers */
  double s_x, s_y, s_xy;        /* nonspecificities */
  double spec;                  /* specificity gain (ratio) */

  assert(ftab);                 /* check the function argument */
  if ((ftab->xcnt <= 1) || (ftab->ycnt <= 1)
  ||  (ftab->known < EPSILON)) return 0;
  bx  = memset(ftab->buf_x, 0, (size_t)ftab->xcnt *sizeof(double));
  by  = memset(ftab->buf_y, 0, (size_t)ftab->ycnt *sizeof(double));
  bxy = ftab->buf_xy;           /* clear and get the buffers */
  fx  = ftab->frq_x;            /* get the column distribution */
  fxy = ftab->frq_xy;           /* and the joint distribution */
  for (x = 0; x < ftab->xcnt; x++) {
    if (fx[x] <= 0) continue;   /* skip empty and combined columns */
    for (y = 0; y < ftab->ycnt; y++) {
      if (fxy[x][y] > 0)    *bxy++ = fxy[x][y];
      if (fxy[x][y] > by[y]) by[y] = fxy[x][y];
      if (fxy[x][y] > bx[x]) bx[x] = fxy[x][y];
    }                           /* compute the joint and marginal */
  }                             /* possibility distributions */
  s_x  = nsp(bx, ftab->xcnt);   /* compute the */
  s_y  = nsp(by, ftab->ycnt);   /* nonspecificities */
  s_xy = nsp(ftab->buf_xy, (DIMID)(bxy -ftab->buf_xy));
  spec = s_x +s_y -s_xy;        /* compute the specificity gain */
  switch (measure & 0xff) {     /* evaluate the measure code */
    case FEM_SPCGBAL: spec /= log((double)ftab->xcnt); break;
    case FEM_SPCGR  : if (s_x      <= 0) return 0;
                      spec /= s_x;                     break;
    case FEM_SPCSGR1: if (s_xy     <= 0) return 0;
                      spec /= s_xy;                    break;
    case FEM_SPCSGR2: if (s_x +s_y <= 0) return 0;
                      spec /= s_x +s_y;                break;
    default:          spec /= LN_2;                    break;
  }                             /* form requested nonspec. ratio */
  if (measure & FEF_WGTD)       /* check for a weighted measure */
    return spec *(ftab->known/ftab->frq);
  return spec;                  /* return the specificity measure */
}  /* spec() */

/*----------------------------------------------------------------------
  Table of Evaluation Functions
----------------------------------------------------------------------*/
static EVALFN *evalfn[] = {     /* evaluation functions */
  /* FEM_NONE      0 */ (EVALFN*)0,
  /* FEM_INFGAIN   1 */ info,
  /* FEM_INFGBAL   2 */ info,
  /* FEM_INFGR     3 */ info,
  /* FEM_INFSGR1   4 */ info,
  /* FEM_INFSGR2   5 */ info,
  /* FEM_QIGAIN    6 */ quad,
  /* FEM_QIGBAL    7 */ quad,
  /* FEM_QIGR      8 */ quad,
  /* FEM_QISGR1    9 */ quad,
  /* FEM_QISGR2   10 */ quad,
  /* FEM_GINI     11 */ gini,
  /* FEM_GINISYM  12 */ gini,
  /* FEM_GINIMOD  13 */ gini,
  /* FEM_RELIEF   14 */ gini,
  /* FEM_WDIFF    15 */ wdiff,
  /* FEM_CHI2     16 */ chi2,
  /* FEM_CHI2NRM  17 */ chi2,
  /* FEM_WEVID    18 */ wevid,
  /* FEM_RELEV    19 */ relev,
  /* FEM_RELMOD   20 */ relmod,
  /* FEM_BDM      21 */ bdm,
  /* FEM_BDMOD    22 */ bdmod,
  /* FEM_RDLEN1   23 */ rdlen,
  /* FEM_RDLEN2   24 */ rdlen,
  /* FEM_STOCO    25 */ stoco,
  /* FEM_SPCGAIN  26 */ spec,
  /* FEM_SPCGBAL  27 */ spec,
  /* FEM_SPCGR    28 */ spec,
  /* FEM_SPCSGR1  29 */ spec,
  /* FEM_SPCSGR2  30 */ spec,
  /* FEM_UNKNOWN  31 */ (EVALFN*)0,
};

#endif  /* #ifdef FT_EVAL */
/*----------------------------------------------------------------------
  Main Functions
----------------------------------------------------------------------*/

FRQTAB* ft_create (DIMID xsize, DIMID ysize)
{                               /* --- create a frequency table */
  DIMID  i;                     /* loop variable, temporary buffer */
  size_t z;                     /* allocation size */
  FRQTAB *ftab;                 /* created frequency table */
  double *p;                    /* to traverse the table elements */

  if (xsize < 1) xsize = 1;     /* check and correct */
  if (ysize < 1) ysize = 1;     /* the function arguments */
  z = (size_t)xsize *(size_t)ysize +(size_t)xsize+(size_t)ysize;
  ftab = (FRQTAB*)malloc(sizeof(FRQTAB)
                       +(size_t)xsize *sizeof(double*));
  if (!ftab) return NULL;       /* allocate frequency table body */
  ftab->xsize = xsize++;        /* and set the number of classes and */
  ftab->ysize = ysize++;        /* the maximal number of att. values */
  ftab->dsts  = (DIMID*)malloc((size_t)xsize *sizeof(DIMID));
  if (!ftab->dsts) { free(ftab); return NULL; }
  ftab->dsts++;                 /* allocate destination vector */
  z += (size_t)xsize *(size_t)ysize +(size_t)xsize+(size_t)ysize;
  p  = (double*)malloc(z *sizeof(double));
  if (!p) { free(ftab->dsts-1); free(ftab); return NULL; }
  ftab->frq_x    = p += 1;      /* allocate table and buffer */
  ftab->frq_y    = p += xsize;  /* elements and organize them */
  ftab->frq_z[0] = p += ysize;
  for (i = 0; i < xsize-1; i++)
    ftab->frq_xy[i] = p += ysize;
  ftab->buf_x  = p += ysize -1;
  ftab->buf_y  = p += xsize -1;
  ftab->buf_xy = p += ysize -1;
  return ftab;                  /* return the created structure */
}  /* ft_create() */

/*--------------------------------------------------------------------*/

void ft_delete (FRQTAB* ftab)
{                               /* --- delete a frequency table */
  assert(ftab);                 /* check the function argument */
  if (ftab->frq_x) free(ftab->frq_x-1);
  if (ftab->dsts)  free(ftab->dsts -1);
  free(ftab);                   /* delete table entries, vectors, */
}  /* ft_delete() */            /* and the table body */

/*--------------------------------------------------------------------*/

void ft_init (FRQTAB *ftab, DIMID xcnt, DIMID ycnt)
{                               /* --- initialize a frequency table */
  DIMID x;                      /* loop variable */

  assert(ftab                   /* check the function arguments */
  &&    (xcnt <= ftab->xsize) && (ycnt <= ftab->ysize));
  if (xcnt >= 0) ftab->xcnt = xcnt; /* note the number of */
  if (ycnt >= 0) ftab->ycnt = ycnt; /* columns and rows */
  for (x = -1; x < xcnt; x++) { /* traverse the columns */
    memset(ftab->frq_xy[x]-1, 0, (size_t)(ftab->ycnt+1)*sizeof(double));
    ftab->dsts[x] = -1;         /* clear the joint frequencies and */
  }                             /* the column destination references */
}  /* ft_init() */

/*--------------------------------------------------------------------*/

void ft_copy (FRQTAB *dst, const FRQTAB *src)
{                               /* --- copy a frequency table */
  DIMID x;                      /* loop variable */

  assert(dst && src             /* check the function arguments */
  &&    (dst->xsize >= src->xcnt)
  &&    (dst->ysize >= src->ycnt));
  dst->xcnt  = src->xcnt;       /* copy the current size */
  dst->ycnt  = src->ycnt;       /* of the frequency table */
  dst->frq   = src->frq;        /* copy the total number of cases */
  dst->known = src->known;      /* and the number with known values */
  memcpy(dst->frq_x-1, src->frq_x-1, /* copy marginals for columns */
        (size_t)(dst->xcnt+1) *sizeof(double));
  memcpy(dst->frq_y-1, src->frq_y-1, /* copy marginals for rows */
        (size_t)(dst->ycnt+1) *sizeof(double));
  memcpy(dst->dsts-1,  src->dsts-1,  /* copy destination references */
        (size_t)(dst->xcnt+1) *sizeof(int));
  for (x = -1; x < dst->xcnt; x++)   /* copy joint frequencies */
    memcpy(dst->frq_xy[x]-1, src->frq_xy[x]-1,
          (size_t)(dst->ycnt+1) *sizeof(double));
}  /* ft_copy() */

/*--------------------------------------------------------------------*/

void ft_marg (FRQTAB *ftab)
{                               /* --- marginalize a frequency table */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse the frequencies */

  assert(ftab);                 /* check the function arguments */
  ftab->known = 0;              /* clear the total for knowns */
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  memset(fx-1, 0, (size_t)(ftab->xcnt+1) *sizeof(double));
  memset(fy-1, 0, (size_t)(ftab->ycnt+1) *sizeof(double));
  for (x = 0; x < ftab->xcnt; x++) {
    if (ftab->dsts[x] >= 0) {   /* skip combined columns and */
      fx[x] = -1; continue; }   /* invalidate their marginals */
    for (y = 0; y < ftab->ycnt; y++) {
      fy[y] += fxy[x][y];       /* marginalize for known y */
      fx[x] += fxy[x][y];       /* and         for known x */
    }                           /* (sum rows and columns) */
    fy[-1] = fxy[x][-1];        /* marginalize for unknown y */
    ftab->known += fx[x];       /* compute total for known values */
  }
  for (y = 0; y < ftab->ycnt; y++)
    fx[-1] += fxy[-1][y];       /* marginalize for unknown x */
  ftab->frq = ftab->known       /* compute the overall total */
            + fx [-1]           /* (y known, x unknown) */
            + fy [-1]           /* (x known, y unknown) */
            + fxy[-1][-1];      /* (both x and y unknown) */
}  /* ft_marg() */

/*--------------------------------------------------------------------*/

void ft_addwm (FRQTAB *ftab, DIMID x, DIMID y, double frq)
{                               /* --- add frequency with marginals */
  assert(ftab                   /* check the function arguments */
  &&    (x >= -1) && (x <= ftab->xcnt)
  &&    (y >= -1) && (y <= ftab->ycnt));
  ftab->frq_xy[x][y] += frq;    /* add the frequency */
  ftab->frq_x [x]    += frq;    /* to the joint and */
  ftab->frq_y [y]    += frq;    /* to the marginal tables */
}  /* ft_addwm() */

/*--------------------------------------------------------------------*/

void ft_move (FRQTAB *ftab, DIMID xsrc, DIMID xdst, DIMID y, double frq)
{                               /* --- move frequency */
  assert(ftab                   /* check the function arguments */
  &&    (xsrc >= 0) && (xsrc < ftab->xcnt)
  &&    (xdst >= 0) && (xdst < ftab->xcnt)
  &&    (y    >= 0) && (y    < ftab->ycnt));
  ftab->frq_xy[xsrc][y] -= frq;  /* move the frequency */
  ftab->frq_xy[xdst][y] += frq;
  ftab->frq_x [xsrc]    -= frq;
  ftab->frq_x [xdst]    += frq;
}  /* ft_move() */

/*--------------------------------------------------------------------*/

void ft_comb (FRQTAB *ftab, DIMID xsrc, DIMID xdst)
{                               /* --- combine two columns */
  DIMID  i;                     /* loop variable */
  double *src, *dst;            /* to traverse the frequencies */
  double marg;                  /* marginal frequency */

  assert(ftab                   /* check the function arguments */
  &&    (xsrc >= 0) && (xsrc < ftab->xcnt)
  &&    (xdst >= 0) && (xdst < ftab->xcnt)
  &&    (xsrc != xdst) && (ftab->dsts[xsrc] < 0));
  ftab->dsts[xsrc] = xdst;      /* note reference to destination */
  marg = ftab->frq_x[xsrc];     /* note the marginal source frequency */
  ftab->frq_x[xsrc] = -1;       /* and invalidate it */
  src  = ftab->frq_xy[xsrc];    /* get the source column to combine */
  do {                          /* traverse destination columns */
    dst = ftab->frq_xy[xdst];   /* get next destination column */
    for (i = 0; i <= ftab->ycnt; i++)
      dst[i] += src[i];         /* add the source frequencies */
    ftab->frq_x[xdst] += marg;  /* sum marginal frequencies */
    xdst = ftab->dsts[xdst];    /* get next destination column index */
  } while (xdst >= 0);          /* loop, while next column found */
}  /* ft_comb() */

/*--------------------------------------------------------------------*/

void ft_uncomb (FRQTAB *ftab, DIMID xsrc)
{                               /* --- uncombine a column */
  DIMID  i, xdst;               /* loop variable, buffer */
  double *src, *dst;            /* to traverse the frequencies */
  double marg = 0;              /* marginal frequency */

  assert(ftab                   /* check the function arguments */
  &&    (xsrc >= 0) && (xsrc < ftab->xcnt) && (ftab->dsts[xsrc] >= 0));
  xdst = ftab->dsts[xsrc];      /* get the first destination column */
  if (xdst < 0) return;         /* if column is not combined, abort */
  ftab->dsts[xsrc] = -1;        /* clear reference to destination */
  src = ftab->frq_xy[xsrc];     /* get the combined source column */
  for (i = 0; i < ftab->ycnt; i++)
    marg += src[i];             /* sum the joint frequencies */
  ftab->frq_x[xsrc] = marg;     /* and (re)store this marginal */
  do {                          /* traverse destination columns */
    dst = ftab->frq_xy[xdst];   /* get next destination column */
    for (i = 0; i <= ftab->ycnt; i++)
      dst[i] -= src[i];         /* subtract the source frequencies */
    ftab->frq_x[xdst] -= marg;  /* get pointer to dest. marginal */
    xdst = ftab->dsts[xdst];    /* get next destination column index */
  } while (xdst >= 0);          /* loop, while next column found */
}  /* ft_uncomb() */

 /*-------------------------------------------------------------------*/

DIMID ft_dest (FRQTAB *ftab, DIMID x)
{                               /* --- get destination of a column */
  DIMID t;                      /* temporary buffer */

  assert(ftab                   /* check the function arguments */
  &&    (x >= 0) && (x < ftab->xcnt));
  while ((t = ftab->dsts[x]) >= 0) x = t;
  return x;                     /* follow references and return id */
}  /* ft_dest() */

/*--------------------------------------------------------------------*/

void ft_alldst (FRQTAB *ftab, DIMID *dsts)
{                               /* --- get all column destinations */
  DIMID i;                      /* loop variable */
  DIMID x, t;                   /* temporary buffers */

  assert(ftab && dsts);         /* check the function arguments */
  for (i = 0; i < ftab->xcnt; i++) {
    x = i;                      /* default: index of source column */
    while ((t = ftab->dsts[x]) >= 0) x = t;
    dsts[i] = x;                /* follow references and */
  }                             /* set id of destination column */
}  /* ft_alldst() */

/*--------------------------------------------------------------------*/
#ifdef FT_EVAL

double ft_eval (FRQTAB *ftab, int measure, double *params)
{                               /* --- evaluate a frequency table */
  int m = measure & 0xff;       /* flagless measure */

  assert(ftab);                 /* check the function arguments */
  if ((m <= FEM_NONE) || (m >= FEM_UNKNOWN))
    return 0;                   /* check the measure code */
  return evalfn[m](ftab, measure, params);
}  /* ft_eval() */              /* return the evaluation result */

/*--------------------------------------------------------------------*/

const char* ft_mname (int measure)
{                               /* --- get name of evaluation measure */
  static char buf[64];          /* buffer for measure name */
  int m = measure & 0xff;       /* index in names vector */

  if ((m < FEM_NONE) || (m > FEM_UNKNOWN))
    m = FEM_UNKNOWN;            /* check measure code */
  strcpy(buf, (measure & FEF_WGTD) ? "weighted " : "");
  strcat(buf, mnames[m]);       /* build measure name */
  return buf;                   /* and return it */
}  /* ft_mname() */

#endif
/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void ft_show (FRQTAB *ftab)
{                               /* --- show a frequency table */
  DIMID  x, y;                  /* loop variables */
  double *fx, *fy, **fxy;       /* to traverse frequencies */

  assert(ftab);                 /* check the function argument */
  fx  = ftab->frq_x;            /* get the column distribution, */
  fy  = ftab->frq_y;            /* the row distribution and */
  fxy = ftab->frq_xy;           /* the joint distribution */
  printf("%6.1f |", fxy[-1][-1]);  /* print the frequency of */
  for (x = 0; x < ftab->xcnt; x++) /* 1) unknown y (x unknown) */
    printf(" %6.1f", fxy[x][-1]);  /* 2) unknown y (x known) */
  printf(" | %6.1f\n", fy[-1]);    /* 3) marginal  (x known) */
  for (x = 0; x < ftab->xcnt; x++) printf("-------");
  printf("-----------------\n");/* print a separator */

  for (y = 0; y < ftab->ycnt; y++) {
    printf("%6.1f |", fxy[-1][y]); /* print frequency of unknown x */
    for (x = 0; x < ftab->xcnt; x++)  /* traverse the columns and */
      printf(" %6.1f", fxy[x][y]); /* print the joint frequencies */
    printf(" | %6.1f\n", fy[y]);   /* and the row marginal */
  }
  for (x = 0; x < ftab->xcnt; x++) printf("-------");
  printf("-----------------\n");/* print separator */

  printf("%6.1f |", fx[-1]);    /* print marginal for unknown x */
  for (x = 0; x < ftab->xcnt; x++) /* traverse marginal frequencies */
    printf(" %6.1f", fx[x]);       /* for knowns and print them */
  printf(" | %6.1f\n", ftab->known);
}  /* ft_show() */              /* finally print the total freq. */

#endif
