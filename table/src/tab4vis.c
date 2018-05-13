/*----------------------------------------------------------------------
  File    : tab4vis.c
  Contents: table utility functions for visualization programs
  Author  : Christian Borgelt
  History : 2001.11.08 file created from file lvq.c
            2013.08.08 adapted to definitions ATTID, VALID, WEIGHT etc.
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include "tab4vis.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
/* --- attribute set variables --- */
ATTSET  *attset   = NULL;       /* attribute set */
RANGE   *ranges   = NULL;       /* ranges of attribute values */
CCHAR   **nms_met = NULL;       /* names of metric attributes */
ATTID   *map_met  = NULL;       /* map metric atts. to attset ids */
ATTID   *inv_met  = NULL;       /* inverse map (attset id to index) */
ATTID   metcnt    = 0;          /* number of metric attributes */
CCHAR   **nms_nom = NULL;       /* names of nominal attributes */
ATTID   *map_nom  = NULL;       /* map nominal atts. to attset ids */
ATTID   *inv_nom  = NULL;       /* inverse map (attset id to index) */
ATTID   nomcnt    = 0;          /* number of nominal attributes */
SELATT  selatt    =             /* attribute selection information */
  { 0, { 0.0, 0.0 }, 0, { 0.0, 0.0 }, 0 };

/* --- data table variables --- */
TABLE   *table    = NULL;       /* data table */
DATAFMT datafmt   =             /* data format description */
  { 1, "\\n", " ,\\t", " \\t\\r", "?", "#" };

/* --- error message --- */
char    tv_msg[2*AS_MAXLEN+64]; /* buffer for an error message */

/* --- internal variables --- */
static TABREAD *tread = NULL;   /* table file reader */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

void tv_clean (void)
{                               /* --- clean up the learning data */
  if (tread)   { trd_delete(tread, 1); tread   = NULL; }
  if (table)   { tab_delete(table, 0); table   = NULL; }
  if (attset)  { as_delete(attset);    attset  = NULL; }
  if (nms_nom) { free((void*)nms_nom); nms_nom = NULL; }
  if (map_nom) { free(map_nom);        map_nom = inv_nom = NULL; }
  if (nms_met) { free((void*)nms_met); nms_met = NULL; }
  if (map_met) { free(map_met);        map_met = inv_met = NULL; }
  if (ranges)  { free(ranges);         ranges  = NULL; }
  metcnt = nomcnt = 0;          /* clean up data variables */
  memset(&selatt, 0, sizeof(selatt));
}  /* tv_clean() */

/*--------------------------------------------------------------------*/

static int error (int code, const char *msg)
{                               /* --- handle an error */
  if (!msg) tv_msg[0] = 0;      /* note the error message */
  else strncpy(tv_msg, msg, sizeof(tv_msg));
  tv_msg[sizeof(tv_msg)-1] = 0; /* terminate the error message */
  tv_clean();                   /* clear all variables */
  return code;                  /* return the error code */
}  /* error() */

/*--------------------------------------------------------------------*/

int tv_load (CCHAR *fname, double addfrac)
{                               /* --- load a data table */
  int    r;                     /* result of a function call */
  int    t;                     /* attribute type, read mode */
  ATTID  k, n;                  /* number of attributes */
  DTINT  i;                     /* buffer for an integer value */
  DTFLT  f;                     /* buffer for a float value */
  ATT    *att;                  /* to traverse the attributes */
  RANGE  *rng;                  /* to traverse attribute value ranges */
  double d;                     /* difference between max and min */

  assert(fname && (addfrac >= 0));  /* check the function arguments */
  tv_clean();                   /* delete existing table, maps etc. */

  /* --- create attribute set and load data table --- */
  attset = as_create("domains", att_delete);
  if (!attset) return E_NOMEM;  /* create attribute set and table */
  table = tab_create("table", attset, tpl_delete);
  if (!table)  return error(E_NOMEM, NULL);
  tread = trd_create();         /* create a table file reader */
  if (!tread)  return error(E_NOMEM, NULL);
  trd_allchs(tread, datafmt.recseps, datafmt.fldseps,
                    datafmt.blanks,  datafmt.nullchs, datafmt.comment);
  if (trd_open(tread, NULL, fname) != 0) /* open the input file */
    return error(E_FOPEN, trd_name(tread));
  t = ((datafmt.names) ? AS_ATT : AS_DFLT) | AS_MARKED;
  r = tab_read(table, tread, t);/* read the table file */
  if (r < 0) return error(-r, tab_errmsg(table, NULL, 0));
  trd_delete(tread, 1);         /* delete the table file reader */
  tread = NULL;                 /* and clear the corresp. variable */

  /* --- determine attribute types --- */
  n = as_attcnt(attset);        /* get number of attributes and */
  for (k = 0; k < n; k++)       /* determine types automatically */
    tab_colconv(table, k, AT_AUTO);

  /* --- collect metric attributes --- */
  nms_met = (CCHAR**)malloc((size_t) n    *sizeof(CCHAR*));
  map_met = (ATTID*) malloc((size_t)(n+n) *sizeof(ATTID));
  ranges  = (RANGE*) malloc((size_t) n    *sizeof(RANGE));
  if (!nms_met || !map_met || !ranges) {
    tv_clean(); return E_NOMEM;}/* create name and map vectors */
  inv_met = map_met +n;         /* and array for ranges of values */
  for (k = 0; k < n; k++) inv_met[k] = -1;
  selatt.h_att = selatt.v_att = -1;
  rng = ranges;                 /* initialize attribute indices */
  for (k = metcnt = 0; k < n; k++) {
    att = as_att(attset, k);    /* traverse the attributes */
    t   = att_type(att);        /* and get the attribute type */
    if ((t != AT_INT) && (t != AT_FLT))
      continue;                 /* skip non-metric attributes */
    if      (selatt.h_att < 0)  /* note the first  metric attribute */
      selatt.h_att = metcnt;    /* for the horizontal direction */
    else if (selatt.v_att < 0)  /* note the second metric attribute */
      selatt.v_att = metcnt;    /* for the vertical direction */
    nms_met[metcnt] = att_name(att);
    map_met[metcnt] = k;        /* note the attribute name */
    inv_met[k]      = metcnt++; /* and the attribute identifier */
    if (t == AT_INT) {          /* if attribute is integer valued */
      i = att_valmin(att)->i;   /* get and set minimum value */
      rng->min = (i <= DTINT_MIN) ?  INFINITY : (double)i;
      i = att_valmax(att)->i;   /* get and set maximum value */
      rng->max = (i >= DTINT_MAX) ? -INFINITY : (double)i; }
    else {                      /* if attribute is real valued */
      f = att_valmin(att)->f;   /* get and set minimum value */
      rng->min = (f <= DTFLT_MIN) ?  INFINITY : (double)f;
      f = att_valmax(att)->f;   /* get and set maximum value */
      rng->max = (f >= DTFLT_MAX) ? -INFINITY : (double)f;
    }                           /* (set initial range of values ) */
    d = rng->max -rng->min;     /* get difference between max and min */
    rng->min -= 0.5*addfrac *d; /* and adapt the range of values */
    rng->max += 0.5*addfrac *d; /* (add a frame around the points) */
    if      (rng->min >  rng->max) { rng->min  = 0; rng->max  = 1; }
    else if (rng->min == rng->max) { rng->min -= 1; rng->max += 1; }
    rng++;                      /* check and adapt final range */
  }                             /* and go to the next attribute */
  if (selatt.v_att < 0) selatt.v_att = selatt.h_att;
  selatt.h_rng = ranges[selatt.h_att];
  selatt.v_rng = ranges[selatt.v_att];

  /* --- collect nominal attributes --- */
  nms_nom = (CCHAR**)malloc((size_t)(n+1) *sizeof(CCHAR*));
  map_nom = (ATTID*) malloc((size_t)(n+n) *sizeof(ATTID));
  if (!nms_nom || !map_nom) {   /* create name and map vectors */
    tv_clean(); return E_NOMEM; }
  inv_nom = map_nom +n;         /* get array for inverse map */
  for (k = 0; k < n; k++) inv_nom[k] = -1;
  for (k = nomcnt = 0; k < n; k++) {
    att = as_att(attset, k);    /* traverse the attributes */
    if ((att_type(att) != AT_NOM) || (att_valcnt(att) <= 0))
      continue;                 /* skip non-nominal attributes */
    nms_nom[nomcnt] = att_name(att);
    map_nom[nomcnt] = k;        /* note the attribute name */
    inv_nom[k]      = nomcnt++; /* and the attribute identifier */
  }
  nms_nom[nomcnt] = "<none>";   /* add an empty entry */
  selatt.c_att = nomcnt-1;      /* set the alleged class attribute */

  return 0;                     /* return 'ok' */
}  /* tv_load() */
