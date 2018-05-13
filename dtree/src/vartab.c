/*----------------------------------------------------------------------
  File    : vartab.c
  Contents: variation table management
  Author  : Christian Borgelt
  History : 2000.09.14 file created
            2000.12.17 first version completed
            2013.08.23 adapted to preprocessor definition of DIMID
            2013.08.26 indexing system for data made more consistent
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "vartab.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define EPSILON     1e-12       /* to handle roundoff errors */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
#ifdef VT_EVAL
typedef double EVALFN (VARTAB* vtab, int measure, double *params);

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
static const char* mnames[VEM_UNKNOWN+1] = {
  /* VEM_NONE    0 */ "no measure",
  /* VEM_SSE     1 */ "reduction of sum of squared errors",
  /* VEM_MSE     2 */ "reduction of mean squared error",
  /* VEM_RMSE    3 */ "reduction of square root of mean squared error",
  /* VEM_VAR     4 */ "reduction of variance (unbiased estimator)",
  /* VEM_SDEV    5 */ "reduction of standard deviation (from variance)",
  /* VEM_UNKNOWN 6 */ "<unknown measure>",
};                              /* names of evaluation measures */

/*----------------------------------------------------------------------
  Evaluation Functions
----------------------------------------------------------------------*/

static double sse (VARTAB *vtab, int measure, double *params)
{                               /* --- sum of squared errors */
  DIMID  i;                     /* loop variable */
  double red;                   /* reduction of sse */

  assert(vtab);                 /* check the function arguments */
  if (vtab->known <= 0)         /* if there are too few cases, */
    return 0;                   /* abort the function */
  for (red = vtab->sse, i = 0; i < vtab->cnt; i++)
    red -= vtab->data[i].sse;   /* subtract sums of squared errors */
  if (measure & VEF_WGTD) red *= vtab->known/vtab->frq;
  return red;                   /* return the (weighted) reduction */
}  /* sse() */

/*--------------------------------------------------------------------*/

static double mse (VARTAB *vtab, int measure, double *params)
{                               /* --- mean squared error */
  DIMID  i;                     /* loop variable */
  double red;                   /* reduction of mse */

  assert(vtab);                 /* check the function arguments */
  if (vtab->known <= 0)         /* if there are too few cases, */
    return 0;                   /* abort the function */
  for (red = 0, i = 0; i < vtab->cnt; i++)
    red += vtab->data[i].sse;   /* sum the sums of the squared errors */
  red = vtab->sse /vtab->frq    /* compute the reduction */
      - red /vtab->known;       /* of the mean squared error */
  if (measure & VEF_WGTD) red *= vtab->known/vtab->frq;
  return red;                   /* return the (weighted) reduction */
}  /* mse() */

/*--------------------------------------------------------------------*/

static double rmse (VARTAB *vtab, int measure, double *params)
{                               /* --- root of mean squared error */
  DIMID  i;                     /* loop variable */
  double red;                   /* reduction of rmse */

  assert(vtab);                 /* check the function arguments */
  if (vtab->known <= 0)         /* if there are too few cases, */
    return 0;                   /* abort the function */
  for (red = 0, i = 0; i < vtab->cnt; i++)
    red += sqrt(vtab->data[i].sse *vtab->data[i].frq);
                                /* red = \sum frq *\sqrt{sse/frq} */
  red = sqrt(vtab->sse /vtab->frq)
      - red /vtab->known;       /* compute the rmse reduction */
  if (measure & VEF_WGTD) red *= vtab->known/vtab->frq;
  return red;                   /* return the (weighted) reduction */
}  /* rmse() */

/*--------------------------------------------------------------------*/

static double var (VARTAB *vtab, int measure, double *params)
{                               /* --- variance */
  DIMID  i;                     /* loop variable */
  double var, frq;              /* global variance, buffer */
  double red;                   /* reduction of variance */

  assert(vtab);                 /* check the function arguments */
  if (vtab->known <= 0)         /* if there are too few cases, */
    return 0;                   /* abort the function */
  frq = vtab->frq -1;           /* compute the global variance */
  var = (frq > 0) ? vtab->sse /frq : 0;
  for (red = 0, i = 0; i < vtab->cnt; i++) {
    frq  = vtab->data[i].frq-1; /* traverse the table columns */
    red += vtab->data[i].frq    /* weighted sum of the variances */
         * ((frq > 0) ? vtab->data[i].sse /frq : var);
  }
  red = var -red /vtab->known;  /* reduction of variance */
  if (measure & VEF_WGTD) red *= vtab->known/vtab->frq;
  return red;                   /* return the (weighted) reduction */
}  /* var() */

/*--------------------------------------------------------------------*/

static double sdev (VARTAB *vtab, int measure, double *params)
{                               /* --- standard deviation */
  DIMID  i;                     /* loop variable */
  double sdev, frq;             /* global standard deviation, buffer */
  double red;                   /* reduction of standard deviation */

  assert(vtab);                 /* check the function arguments */
  if (vtab->known <= 0)         /* if there are too few cases, */
    return 0;                   /* abort the function */
  frq  = vtab->frq -1;          /* compute the global std. deviation */
  sdev = (frq > 0) ? sqrt(vtab->sse /frq) : 0;
  for (red = 0, i = 0; i < vtab->cnt; i++) {
    frq  = vtab->data[i].frq-1; /* traverse the table columns */
    red += vtab->data[i].frq    /* weighted sum of the std. devs. */
         * ((frq > 0) ? sqrt(vtab->data[i].sse /frq) : sdev);
  }
  red = sdev -red /vtab->known; /* reduction of standard deviation */
  if (measure & VEF_WGTD) red *= vtab->known/vtab->frq;
  return red;                   /* return the (weighted) reduction */
}  /* sdev() */

/*----------------------------------------------------------------------
  Table of Evaluation Functions
----------------------------------------------------------------------*/
static EVALFN *evalfn[] = {     /* evaluation functions */
  (EVALFN*)0,                   /* VEM_NONE  0 */
  sse,                          /* VEM_SSE   1 */
  mse,                          /* VEM_MSE   2 */
  rmse,                         /* VEM_RMSE  3 */
  var,                          /* VEM_VAR   4 */
  sdev,                         /* VEM_SDEV  5 */
};

#endif  /* #ifdef VT_EVAL */
/*----------------------------------------------------------------------
  Main Functions
----------------------------------------------------------------------*/

VARTAB* vt_create (DIMID size)
{                               /* --- create a variance table */
  VARTAB *vtab;                 /* created variance table */

  if (size < 1) size = 1;       /* check the function argument */
  vtab = (VARTAB*)malloc(sizeof(VARTAB)
                       +(size_t)(size-1) *sizeof(VARDATA));
  if (!vtab) return NULL;       /* create a variance table and */
  vtab->size = size;            /* set its size (number of columns) */
  vtab->cnt  = 0;
  vtab->dsts = (DIMID*)malloc((size_t)(size+1) *sizeof(DIMID));
  if (!vtab->dsts) { free(vtab); return NULL; }
  vtab->dsts++;                 /* create column destination table */
  return vtab;                  /* and return the created structure */
}  /* vt_create() */

/*--------------------------------------------------------------------*/

void vt_delete (VARTAB *vtab)
{                               /* --- delete a variance table */
  assert(vtab);                 /* check the function argument */
  free(vtab->dsts -1);          /* delete the column dest. table */
  free(vtab);                   /* and the base structure */
}  /* vt_delete() */

/*--------------------------------------------------------------------*/

void vt_init (VARTAB *vtab, DIMID cnt)
{                               /* --- initialize a variance table */
  DIMID i;                      /* loop variable */

  assert(vtab && (cnt <= vtab->size)); /* check function arguments */
  if (cnt >= 0) vtab->cnt = cnt;/* note the number of columns */
  memset(vtab->data-1, 0, (size_t)(vtab->cnt+1) *sizeof(VARDATA));
  for (i = -1; i < cnt; i++)    /* clear the statistics and */
    vtab->dsts[i] = -1;         /* the column destination references */
}  /* vt_init() */

/*--------------------------------------------------------------------*/

void vt_copy (VARTAB *dst, const VARTAB *src)
{                               /* --- copy a frequency table */
  assert(dst && src             /* check the function arguments */
  &&    (dst->size >= src->cnt));
  dst->cnt   = src->cnt;        /* copy the current size of the table */
  dst->frq   = src->frq;        /* copy the total number of cases, */
  dst->known = src->known;      /* the number with a known value, */
  dst->sum   = src->sum;        /* and the global statistics */
  dst->ssv   = src->ssv;
  dst->mean  = src->mean;
  dst->sse   = src->sse;
  memcpy(dst->data-1,  src->data-1,  /* copy column statistics */
        (size_t)(dst->cnt+1) *sizeof(VARDATA));
  memcpy(dst->dsts-1,  src->dsts-1,  /* copy destination references */
        (size_t)(dst->cnt+1) *sizeof(int));
}  /* vt_copy() */

/*--------------------------------------------------------------------*/

void vt_add (VARTAB *vtab, DIMID x, double y, double frq)
{                               /* --- add to a variance table */
  VARDATA *p;                   /* table column referred to */

  assert(vtab                   /* check the function arguments */
  &&    (x >= -1) && (x < vtab->cnt));
  p = vtab->data +x;            /* get the column's variation data */
  p->frq += frq;                /* adapt the column's statistics */
  p->sum += frq *= y;
  p->ssv += frq *= y;
}  /* vt_add() */

/*--------------------------------------------------------------------*/

void vt_calc (VARTAB *vtab)
{                               /* --- calculate estimates */
  DIMID   i;                    /* loop variable */
  VARDATA *p;                   /* to traverse the variation data */

  assert(vtab);                 /* check the function argument */
  vtab->frq = vtab->sum = vtab->ssv = 0;
  for (i = -1; i < vtab->cnt; i++) {
    p = vtab->data +i;          /* traverse the table columns */
    vtab->frq += p->frq;        /* and sum the statistics */
    vtab->sum += p->sum;        /* of the columns, then */
    vtab->ssv += p->ssv;        /* compute the aggregates */
  }
  vtab->known = vtab->frq - vtab->data[-1].frq;
  vtab->mean  = (vtab->frq > 0) ? vtab->sum /vtab->frq : 0;
  vtab->sse   = vtab->ssv -vtab->mean *vtab->sum;
  for (i = 0; i < vtab->cnt; i++) {
    p = vtab->data +i;          /* traverse the table columns again */
    p->mean = (p->frq > 0) ? p->sum /p->frq : vtab->mean;
    p->sse  = p->ssv -p->mean *p->sum;
  }                             /* compute the column aggregates */
}  /* vt_calc() */

/*--------------------------------------------------------------------*/

void vt_move (VARTAB *vtab, DIMID xsrc, DIMID xdst, double y,double frq)
{                               /* --- move value/frequency */
  VARDATA *src, *dst;           /* source and destination column */

  assert(vtab                   /* check the function arguments */
  &&    (xsrc >= 0) && (xsrc < vtab->cnt)
  &&    (xdst >= 0) && (xdst < vtab->cnt));
  src = vtab->data +xsrc;       /* check the indices and */
  dst = vtab->data +xdst;       /* get the column data */
  src->frq -= frq;      dst->frq += frq;  /* move the value */
  src->sum -= frq *= y; dst->sum += frq;  /* and its frequency */
  src->ssv -= frq *= y; dst->ssv += frq;
  src->mean = (src->frq > 0) ? src->sum /src->frq : vtab->mean;
  src->sse  = src->ssv -src->mean *src->sum;
  dst->mean = (dst->frq > 0) ? dst->sum /dst->frq : vtab->mean;
  dst->sse  = dst->ssv -dst->mean *dst->sum;
}  /* vt_move() */              /* recompute column aggregates */

/*--------------------------------------------------------------------*/

void vt_comb (VARTAB *vtab, DIMID xsrc, DIMID xdst)
{                               /* --- combine two columns */
  VARDATA *src, *dst;           /* source and destination column */

  assert(vtab                   /* check the function arguments */
  &&    (xsrc >= 0) && (xsrc < vtab->cnt)
  &&    (xdst >= 0) && (xdst < vtab->cnt)
  &&    (vtab->dsts[xsrc] < 0));
  vtab->dsts[xsrc] = xdst;      /* note reference to destination */
  src = vtab->data +xsrc;       /* get the source and */
  dst = vtab->data +xdst;       /* the destination column */
  dst->frq += src->frq;         /* update the statistics */
  dst->sum += src->sum;         /* of the destination column */
  dst->ssv += src->ssv;         /* and recompute the aggregates */
  dst->mean = (dst->frq > 0) ? dst->sum /dst->frq : vtab->mean;
  dst->sse  = dst->ssv -dst->mean *dst->sum;
}  /* vt_comb() */

/*--------------------------------------------------------------------*/

void vt_uncomb (VARTAB *vtab, DIMID xsrc)
{                               /* --- uncombine a column */
  DIMID   xdst;                 /* index of destination column */
  VARDATA *src, *dst;           /* source and destination column */

  assert(vtab                   /* check the function arguments */
  &&    (xsrc >= 0) && (xsrc < vtab->cnt));
  xdst = vtab->dsts[xsrc];      /* get the first destination column */
  if (xdst < 0) return;         /* if column is not combined, abort */
  vtab->dsts[xsrc] = -1;        /* clear the destination reference */
  src = vtab->data +xsrc;       /* get the source */
  do {                          /* value/frequency remove loop */
    dst = vtab->data +xdst;     /* get the destination column */
    dst->frq -= src->frq;       /* and update the statistics */
    dst->sum -= src->sum;       /* of the destination column, */
    dst->ssv -= src->ssv;       /* then recompute the aggregates */
    dst->mean = (dst->frq > 0) ? dst->sum /dst->frq : vtab->mean;
    dst->sse  = dst->ssv -dst->mean *dst->sum;
    xdst = vtab->dsts[xdst];    /* get the next destination column, */
  } while (xdst >= 0);          /* while there is another */
}  /* vt_uncomb() */

/*--------------------------------------------------------------------*/

DIMID vt_dest (VARTAB *vtab, DIMID x)
{                               /* --- get destination of a column */
  DIMID t;                      /* temporary buffer */

  assert(vtab                   /* check the function arguments */
  &&    (x >= 0) && (x < vtab->cnt));
  while ((t = vtab->dsts[x]) >= 0) x = t;
  return x;                     /* follow references and return id */
}  /* vt_dest() */

/*--------------------------------------------------------------------*/

void vt_alldst (VARTAB *vtab, DIMID *dsts)
{                               /* --- get all column destinations */
  DIMID i;                      /* loop variable */
  DIMID x, t;                   /* temporary buffers */

  assert(vtab && dsts);         /* check the function arguments */
  for (i = 0; i < vtab->cnt; i++) {
    x = i;                      /* default: index of source column */
    while ((t = vtab->dsts[x]) >= 0) x = t;
    *dsts++ = x;                /* follow references and */
  }                             /* set id of destination column */
}  /* vt_alldst() */

/*--------------------------------------------------------------------*/
#ifdef VT_EVAL

double vt_eval (VARTAB *vtab, int measure, double *params)
{                               /* --- evaluate variance table */
  int m = measure & ~VEF_WGTD;  /* flagless measure */

  assert(vtab);                 /* check the function arguments */
  if ((m <= VEM_NONE) || (m >= VEM_UNKNOWN))
    return 0;                   /* check the measure code */
  return evalfn[m](vtab, measure, params);
}  /* vt_eval() */              /* return the evaluation result */

/*--------------------------------------------------------------------*/

const char* vt_mname (int measure)
{                               /* --- get name of evaluation measure */
  static char buf[64];          /* buffer for measure name */
  int m = measure & ~VEF_WGTD;  /* index in names vector */

  if ((m < VEM_NONE) || (m > VEM_UNKNOWN))
    m = VEM_UNKNOWN;            /* check measure code */
  strcpy(buf, (measure & VEF_WGTD) ? "weighted " : "");
  strcat(buf, mnames[m]);       /* build measure name */
  return buf;                   /* and return it */
}  /* vt_mname() */

#endif
/*--------------------------------------------------------------------*/
#ifndef NDEBUG

void vt_show (VARTAB *vtab)
{                               /* --- show variance table */
  DIMID   i;                    /* loop variable */
  VARDATA *p;                   /* to traverse the variance data */

  assert(vtab);                 /* check the function arguments */
  for (i = -1; i < vtab->cnt; i++) {
    p = vtab->data +i;          /* traverse the table columns */
    if (i >= 0) printf("%2"DIMID_FMT": ", i);
    else        printf("uv: "); /* print column indicator */
    printf("%5.1f %5.1f %7.1f ", p->frq, p->sum, p->ssv);
    printf("%5.1f %7.1f\n", p->mean, p->sse);
  }                             /* print column statistics */
  printf("tt: %5.1f %5.1f %7.1f ", vtab->frq, vtab->sum, vtab->ssv);
  printf("%5.1f %7.1f\n", vtab->mean, vtab->sse);
}  /* vt_show() */              /* print total statistics */

#endif
