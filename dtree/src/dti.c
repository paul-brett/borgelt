/*----------------------------------------------------------------------
  File    : dti.c
  Contents: decision and regression tree induction
  Authors : Christian Borgelt
  History : 1997.08.08 file created
            1997.09.08 class checks corrected
            1997.09.17 option '-a' (aligned output) added
            1998.01.11 null value characters (option -u) added
            1998.02.08 adapted to changed parse functions
            1998.02.09 adapted to changed order of evaluation measures
            1998.03.30 bug in class check removed
            1998.06.23 adapted to modified attset functions
            1998.09.29 table reading simplified
            1998.10.20 output of relative class frequencies added
            1999.02.09 input from stdin, output to stdout added
            1999.04.17 simplified using the new module 'io'
            1999.04.30 log messages improved
            1999.10.25 bug in initialization of 'minval' fixed
            1999.10.29 evaluation measure FEM_WDIFF added
            2000.12.18 extended to regression trees
            2001.03.02 evaluation measure FEM_INFGBAL added
            2001.07.23 adapted to modified module scan
            2002.02.11 evaluation measures coded with names
            2002.02.02 adapted to modified list of evaluation measures
            2002.02.04 quadratic information measures added
            2003.08.16 slight changes in error message output
            2004.05.26 measure selection made more flexible
            2004.07.21 option -x added (attribute evaluation)
            2004.09.13 trivial pruning of grown tree made optional
            2004.11.09 execution time output added
            2007.02.13 adapted to modified module attset
            2007.10.10 evaluation of attribute directions added
            2009.04.16 modified relevance measure added
            2011.07.28 adapted to modified attset and utility modules
            2013.08.23 adapted to definitions ATTID, VALID, TPLID etc.
            2013.08.29 adapted to new function as_target()
            2014.10.24 changed from LGPL license to MIT license
            2015.09.30 forward selection of attributes based on AUC
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include "arrays.h"
#ifndef AS_READ
#define AS_READ
#endif
#ifndef AS_PARSE
#define AS_PARSE
#endif
#ifndef AS_DESC
#define AS_DESC
#endif
#include "attset.h"
#ifndef TAB_READ
#define TAB_READ
#endif
#include "table.h"
#ifndef DT_GROW
#define DT_GROW
#endif
#include "dtree.h"
#include "error.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "dti"
#define DESCRIPTION "decision and regression tree induction"
#define VERSION     "version 4.3 (2016.11.11)         " \
                    "(c) 1997-2016   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION     (-6)       /* unknown option */
#define E_OPTARG     (-7)       /* missing option argument */
#define E_ARGCNT     (-8)       /* wrong number of arguments */
#define E_PARSE      (-9)       /* parse error on domain file */
#define E_ATTCNT    (-10)       /* no usable attributes found */
#define E_UNKTRG    (-11)       /* unknown  target attribute */
#define E_MULTRG    (-12)       /* multiple target attributes */
#define E_CLSBIN    (-13)       /* class attribute is not binary */
#define E_BALANCE   (-14)       /* unknown balancing mode */
#define E_MEASURE   (-15)       /* unknown selection measure */
#define E_MINCNT    (-16)       /* invalid minimal number of tuples */

#define SEC_SINCE(t)  ((double)(clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- measure information --- */
  int  code;                    /* measure code */
  char *name;                   /* name of the measure */
} MINFO;                        /* (measure information) */

typedef struct {                /* --- prediction for AUC comp. --- */
  double out;                   /* computed prediction */
  double trg;                   /* actual target value */
} PRED;                         /* (prediction pair) */

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
/* --- measures for nominal targets --- */
static const MINFO nomtab[] = {
  { FEM_NONE,    "none"    },   /* no measure */
  { FEM_INFGAIN, "infgain" },   /* information gain */
  { FEM_INFGBAL, "infgbal" },   /* balanced information gain */
  { FEM_INFGR,   "infgr"   },   /* information gain ratio */
  { FEM_INFSGR1, "infsgr1" },   /* sym. information gain ratio 1 */
  { FEM_INFSGR2, "infsgr2" },   /* sym. information gain ratio 2 */
  { FEM_QIGAIN,  "qigain"  },   /* quadratic information gain */
  { FEM_QIGBAL,  "qigbal"  },   /* balanced quad. information gain */
  { FEM_QIGR,    "qigr"    },   /* quadratic information gain ratio */
  { FEM_QISGR1,  "qisgr1"  },   /* sym. quad. info. gain ratio 1 */
  { FEM_QISGR2,  "qisgr2"  },   /* sym. quad. info. gain ratio 2 */
  { FEM_GINI,    "gini"    },   /* gini index */
  { FEM_GINISYM, "ginisym" },   /* symmetric gini index */
  { FEM_GINIMOD, "ginimod" },   /* modified gini index */
  { FEM_RELIEF,  "relief"  },   /* relief measure */
  { FEM_WDIFF,   "wdiff"   },   /* weighted differences */
  { FEM_CHI2,    "chi2"    },   /* chi^2 measure */
  { FEM_CHI2NRM, "chi2nrm" },   /* normalized chi^2 measure */
  { FEM_WEVID,   "wevid"   },   /* weight of evidence */
  { FEM_RELEV,   "relev"   },   /* relevance */
  { FEM_RELMOD,  "relmod"  },   /* modified relevance */
  { FEM_BDM,     "bdm"     },   /* Bayesian-Dirichlet / K2 metric */
  { FEM_BDMOD,   "bdmod"   },   /* modified BD / K2 metric */
  { FEM_RDLREL,  "rdlrel"  },   /* red. of description length 1 */
  { FEM_RDLABS,  "rdlabs"  },   /* red. of description length 2 */
  { FEM_STOCO,   "stoco"   },   /* stochastic complexity */
  { FEM_SPCGAIN, "spcgain" },   /* specificity gain */
  { FEM_SPCGBAL, "spcgbal" },   /* balanced specificity gain */
  { FEM_SPCGR,   "spcgr"   },   /* specificity gain ratio */
  { FEM_SPCSGR1, "spcsgr1" },   /* sym. specificity gain ratio 1 */
  { FEM_SPCSGR2, "spcsgr2" },   /* sym. specificity gain ratio 2 */
  { -1,          NULL      }    /* sentinel */
};

/* --- measures for metric targets --- */
static const MINFO mettab[] = {
  { VEM_NONE,    "none"    },   /* no measure */
  { VEM_SSE,     "sse"     },   /* sum of squared errors */
  { VEM_MSE,     "mse"     },   /* mean squared error */
  { VEM_RMSE,    "rmse"    },   /* square root of mean squared error */
  { VEM_VAR,     "var"     },   /* variance (unbiased estimator) */
  { VEM_SDEV,    "sd"      },   /* standard deviation (from variance) */
  { -1,          NULL      }    /* sentinel */
};

/* --- error messages --- */
static const char *errmsgs[] = {
  /* E_NONE      0 */  "no error",
  /* E_NOMEM    -1 */  "not enough memory",
  /* E_FOPEN    -2 */  "cannot open file %s",
  /* E_FREAD    -3 */  "read error on file %s",
  /* E_FWRITE   -4 */  "write error on file %s",
  /* E_STDIN    -5 */  "double assignment of standard input",
  /* E_OPTION   -6 */  "unknown option -%c",
  /* E_OPTARG   -7 */  "missing option argument",
  /* E_ARGCNT   -8 */  "wrong number of arguments",
  /* E_PARSE    -9 */  "parse error(s) on file %s",
  /* E_ATTCNT  -10 */  "no (usable) attributes (need at least 1)",
  /* E_UNKTRG  -11 */  "unknown target attribute '%s'",
  /* E_MULTRG  -12 */  "multiple target attributes",
  /* E_CLSBIN  -13 */  "class attribute '%s' is not binary",
  /* E_BALANCE -14 */  "unknown balancing mode %c",
  /* E_MEASURE -15 */  "unknown attribute selection measure %s",
  /* E_MINCNT  -16 */  "invalid minimal number of tuples %g",
  /*           -17 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR   *prgname;        /* program name for error messages */
static SCANNER *scan   = NULL;  /* scanner (for domain file) */
static TABREAD *tread  = NULL;  /* table reader */
static ATTSET  *attset = NULL;  /* attribute set */
static TABLE   *table  = NULL;  /* (training) data table */
static TABLE   *valid  = NULL;  /* validation data table */
static DTREE   *dtree  = NULL;  /* decision/regression tree */
static FILE    *out    = NULL;  /* decision tree output file */

/*----------------------------------------------------------------------
  Forward Selection of Attributes
----------------------------------------------------------------------*/

static int cleanup (int errcode, PRED *preds, DTREE *dtree)
{                               /* --- clean up after an error */
  if (dtree) dt_delete(dtree, 0);
  if (preds) free(preds);       /* free all allocated memory */
  return errcode;               /* return the error code */
}  /* cleanup() */

/*--------------------------------------------------------------------*/

static int predcmp (const void *a, const void *b, void *data)
{                               /* --- compare two predictions */
  if (((PRED*)a)->out > ((PRED*)b)->out) return +1;
  if (((PRED*)a)->out < ((PRED*)b)->out) return -1;
  return 0;                     /* return sign of difference */
}  /* predcmp() */              /* of prediction values */

/*--------------------------------------------------------------------*/

static void printtime (void)
{                               /* --- report current date and time */
  time_t    t;                  /* current time */
  struct tm *tm;                /* current time as a time struct */
  char      buf[64];            /* for formating the current time */

  t  = time(NULL);              /* get the current time and */
  tm = localtime(&t);           /* turn it into a time struct */
  if (!tm) return;              /* check for correct conversion */
  strftime(buf, sizeof(buf)-1, "%F %T", tm);
  fprintf(stderr, "%s ", buf);  /* format and print current time */
}  /* printtime() */

/*--------------------------------------------------------------------*/

static int fwdsel (TABLE *table, TABLE *valid, ATTID trgid,
                   int measure, double *params, double minval,
                   ATTID maxht, double mincnt, int flags,
                   ATTID maxsel, int verbose)
{                               /* --- forward selection of atts. */
  ATTSET  *attset;              /* underlying attribute set */
  TPLID   j, n;                 /* loop variables for tuples */
  TUPLE   *tpl;                 /* to traverse the tuples */
  ATTID   i, k, m;              /* loop variables for attributes */
  ATT     *att;                 /* to traverse the attributes */
  PRED    *preds;               /* decision tree predictions */
  INST    res;                  /* result of prediction */
  double  p;                    /* prediction confidence/probability */
  TPLID   neg, pos, sum;        /* negative and positive counters */
  double  amin, amax, aavg;     /* area under the ROC curve (AUC) */
  double  bmin, bmax, bavg;     /* best minimum/maximum/average AUC */
  ATTID   bid;                  /* index of best attribute */
  DTREE   *dtree = NULL;        /* decision tree classifier */
  clock_t t;                    /* for time measurements */

  assert(table                  /* check the function arguments */
  &&     valid && (trgid >= 0));
  attset = tab_attset(table);   /* get the underlying attribute set */
  m = as_attcnt(attset);        /* an get the number of attributes */
  as_setmark(attset, -1);       /* unmark all attributes */
  att = as_att(attset, trgid);  /* get and mark */
  att_setmark(att, 0);          /* the target attribute */
  assert(att_valcnt(att) <= 2); /* check for at most two classes */
  if (att_valcnt(att) < 2) return 0;
  n = tab_tplcnt(valid);        /* get number of validation tuples */
  preds = (PRED*)malloc((size_t)n *sizeof(PRED));
  if (!preds) return E_NOMEM;   /* create a prediction array */

  t = clock();                  /* start timer, print log message */
  fprintf(stderr, "selecting attributes ...\n");
  bmin = bmax = bavg = -1.0;    /* clear the best attribute value */
  for (k = 0; k < m-1; k++) {   /* forward selection of attributes */
    bid = -1;                   /* clear the best attribute */
    for (i = 0; i < m; i++) {   /* traverse the attributes */
      att = as_att(attset, i);  /* skip target and selected attribs. */
      if (att_getmark(att) >= 0) continue;
      att_setmark(att, 0);      /* mark the attribute as selected */

      /* --- decision tree induction --- */
      dtree = dt_grow(table, trgid, measure, params, minval,
                      maxht, mincnt, flags);
      if (!dtree) return cleanup(E_NOMEM, preds, dtree);

      /* --- evaluate model on validation data --- */
      for (j = 0; j < n; j++) { /* traverse the validation tuples */
        tpl = tab_tpl(valid, j);/* (assess the model performance) */
        preds[j].trg = (double)tpl_colval(tpl, trgid)->n;
        dt_exec(dtree, tpl, 1e-12, &res, NULL, NULL);
        preds[j].out = dt_conf(dtree, 1);
      }                         /* collect confidence of prediction */
      obj_qsort(preds, (size_t)n, sizeof(PRED), +1, predcmp, NULL);
      neg  = pos  = sum = 0;    /* initialize the case counters */
      amin = amax = 0;          /* and the area under the ROC curve */
      p = preds[0].out;         /* get the first prediction */
      for (j = n; --j >= 0; ) { /* traverse the cases (in reverse) */
        if (preds[j].out != p){ /* if the prediction changes */
          p     = preds[j].out; /* note the new prediction value */
          amin += (double)neg *(double)sum;
          sum  += pos;          /* add current rectangle to AUC */
          amax += (double)neg *(double)sum;
          neg   = pos = 0;      /* and reinitialize the counters */
        }                       /* (for next prediction value) */
        if (preds[j].trg > 0) pos += 1;
        else                  neg += 1;
      }                         /* update the case counters */
      amin += (double)neg *(double)sum;
      sum  += pos;              /* add last rectangle to AUC */
      amax += (double)neg *(double)sum;
      amin /= p = (double)sum *(double)(n -sum);
      amax /= p;                /* normalize by rectangle area */
      amin = 2.0 *amin -1.0;    /* compute area above diagonal */
      amax = 2.0 *amax -1.0;    /* (minimum/maximum/average) */
      aavg = 0.5 *amin +0.5 *amax;
      if (verbose) {            /* if verbose reporting requested */
        printtime();            /* print the current date and time */
        fprintf(stderr, "% .6f [% .6f,% .6f] %s\n",
                aavg, amin, amax, att_name(att));
      }                         /* print minimum and maximum AUC */
      if ((aavg > bavg) || (maxsel < -2)) {  /* update best att. */
        bavg = aavg; bmin = amin; bmax = amax; bid = i; }

      /* --- clean up the variables --- */
      dt_delete(dtree, 0); dtree = NULL;
      att_setmark(att, -1);     /* unmark the current attribute */
      if (maxsel < -1) break;   /* abort the loop if to select */
    }                           /* the attributes in a fixed order */

    if (bid < 0) break;         /* if no attribute selected, abort */
    printtime();                /* print the current date and time */
    att = as_att(attset, bid);  /* get and report best attribute */
    fprintf(stderr, "%3"ATTID_FMT": % .6f [% .6f,% .6f] %s\n",
            k+1, bavg, bmin, bmax, att_name(att));
    att_setwgt (att, (float)bavg);   /* note AUC value in weight */
    att_setmark(att, k+1);      /* mark best attribute with step */
    if ((k >= maxsel-1) && (maxsel >= 0)) { k++; break; }
  }                             /* check for maximum number of atts. */
  if ((maxsel > -3)             /* if to do an actual selection */
  &&  (bavg   <= 0)) {          /* and result no better than random */
    as_setmark(attset, -1);     /* remove all selected attributes */
    att_setmark(as_att(attset, trgid), 0);
  }
  fprintf(stderr, "attribute selection [%"ATTID_FMT" attribute(s)]", k);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
  free(preds);                  /* delete the prediction array */
  return 0;                     /* return 'ok' */
}  /* fwdsel() */

/*----------------------------------------------------------------------
  Main Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (dtree)  dt_delete(dtree,  0); \
  if (valid)  tab_delete(valid, 0); \
  if (table)  tab_delete(table, 0); \
  if (attset) as_delete(attset);    \
  if (tread)  trd_delete(tread, 1); \
  if (scan)   scn_delete(scan,  1); \
  if (out && (out != stdout)) fclose(out);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

static void help (void)
{                               /* --- print help on sel. measures */
  int i;                        /* loop variable */

  fprintf(stderr, "\n");        /* terminate startup message */
  printf("\nattribute selection measures (option -e#)\n");
  printf("measures for nominal target attributes:\n");
  printf("  name       measure\n");
  for (i = 0; nomtab[i].name; i++)
    printf("  %-9s  %s\n", nomtab[i].name, ft_mname(nomtab[i].code));
  printf("\nMeasures wdiff, bdm, bdmod, rdlrel, rdlabs "
         "take a sensitivity\n"
         "parameter (-z#, default: 0, i.e. normal sensitivity)\n");
  printf("Measures bdm and bdmod take a prior (-p#, positive number)\n"
         "or an equivalent sample size (-p#, negative number).\n");
  printf("\nmeasures for metric target attributes:\n");
  printf("  name       measure\n");
  for (i = 0; mettab[i].name; i++)
    printf("  %-9s  %s\n", mettab[i].name, vt_mname(mettab[i].code));
  exit(0);                      /* print a list of selection measures */
}  /* help() */                 /* and abort the program */

/*--------------------------------------------------------------------*/

static int code (const MINFO *tab, const char *name)
{                               /* --- get measure code */
  assert(tab && name);          /* check the function arguments */
  for ( ; tab->name; tab++)     /* look up name in table */
    if (strcmp(tab->name, name) == 0)
      return tab->code;         /* return the measure code */
  return -1;                    /* or an error indicator */
}  /* code() */

/*--------------------------------------------------------------------*/

static int read_table (ATTSET *attset, TABREAD *tread,
                       CCHAR *fn_tab, CCHAR *fn_hdr, int mode,
                       TABLE **tab)
{                               /* --- read a table file */
  int     k;                    /* return value of read function */
  ATTID   m;                    /* number of attributes */
  TPLID   n;                    /* number of data tuples */
  double  w;                    /* weight of data tuples */
  clock_t t;                    /* for time measurements */

  /* --- read table header --- */
  if (fn_hdr) {                 /* if a header file is given */
    t = clock();                /* start timer, open input file */
    if (trd_open(tread, NULL, fn_hdr) != 0) return E_FOPEN;
    fprintf(stderr, "reading %s ... ", trd_name(tread));
    k = as_read(attset, tread, (mode & ~AS_DFLT) | AS_ATT);
    if (k < 0) return -k;       /* read table header */
    trd_close(tread);           /* and close the file */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s)]", as_attcnt(attset));
    fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
    mode &= ~(AS_ATT|AS_DFLT);  /* print a success message and */
  }                             /* remove the attribute flag */

  /* --- read table --- */
  t = clock();                  /* start timer, open input file */
  if (trd_open(tread, NULL, fn_tab) != 0) return E_FOPEN;
  fprintf(stderr, "reading %s ... ", trd_name(tread));
  *tab = tab_create("table", attset, tpl_delete);
  if (!*tab) return E_NOMEM;    /* create a data table */
  k = tab_read(*tab, tread, mode);
  if (k < 0) return -k;         /* read the table file */
  trd_close(tread);             /* and close the file */
  m = tab_attcnt(*tab);         /* get the number of attributes */
  n = tab_tplcnt(*tab);         /* and the number of data tuples */
  w = tab_tplwgt(*tab);         /* and print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  return 0;                   /* return 'ok' */
}  /* read_table() */

/*--------------------------------------------------------------------*/

int main (int argc, char* argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counter, buffer */
  char    *s;                   /* to traverse options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_dom  = NULL;      /* name of domain file */
  CCHAR   *fn_hdr  = NULL;      /* name of table header file */
  CCHAR   *fn_tab  = NULL;      /* name of table file */
  CCHAR   *fn_val  = NULL;      /* name of validation data file */
  CCHAR   *fn_dt   = NULL;      /* name of dec./reg. tree file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  CCHAR   *trgname = NULL;      /* name of the target attribute */
  ATTID   trgid    = -1;        /* id/index of target column */
  CCHAR   *mname   = NULL;      /* name of att. selection measure */
  int     measure  = 3;         /* attribute selection measure */
  int     wgtd     = FEF_WGTD;  /* flag for weighted measure */
  double  params[] = { 0, 0 };  /* selection measure parameters */
  double  minval   = -INFINITY; /* minimal value of selection measure */
  WEIGHT  mincnt   = 2.0F;      /* minimal number of cases */
  ATTID   maxht    = ATTID_MAX; /* maximal height of tree */
  ATTID   maxsel   = -1;        /* maximum number of atts. to select */
  int     verbose  = 0;         /* flag for verbose reporting */
  int     mode     = AS_ATT|AS_NOXATT; /* table file read mode */
  int     flags    = 0;         /* flags, e.g. DF_SUBSET */
  int     balance  = 0;         /* flag for balancing class freqs. */
  int     maxlen   = 0;         /* maximal output line length */
  int     desc     = 0;         /* description mode */
  const   MINFO *ntab;          /* table of measure names */
  ATT     *att;                 /* to traverse the attributes */
  ATTID   *perm;                /* permutation of attributes */
  double  *evals;               /* attribute evaluations */
  double  *cuts;                /* cut values of metric attributes */
  ATTID   size;                 /* number of nodes in dec./reg. tree */
  ATTID   m, r, z, c, used;     /* number of attributes (in tree) */
  TPLID   n;                    /* number of data tuples */
  double  w;                    /* weight of data tuples */
  clock_t t;                    /* timer for measurements */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] domfile [-d|-h hdrfile] "
                     "tabfile dtfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-c#      target attribute name                  "
                    "(default: last attribute)\n");
    printf("-q#      balance class frequencies (weight tuples)\n");
    printf("         (l: lower, b: boost, s: shift tuple weights;\n");
    printf("          uppercase letters: use integer factors)\n");
    printf("-e#      attribute selection measure            "
                    "(default: infgr/rmse)\n");
    printf("-!       print a list of available "
                    "attribute selection measures\n");
    printf("-z#      sensitivity parameter                  "
                    "(default: %g)\n", params[0]);
    printf("         (for measures wdiff, bdm, bdmod, "
                    "rdlen1, rdlen2)\n");
    printf("-p#      prior (positive) or "
                    "equivalent sample size (negative)\n");
    printf("         (for measures bdm, bdmod)\n");
    printf("-w       do not weight measure with fraction "
                    "of known values\n");
    printf("-i#      minimal value of selection measure     "
                    "(default: no limit)\n");
    printf("-t#      maximal height of the tree             "
                    "(default: no limit)\n");
    printf("-k#      minimal tuples in two branches         "
                    "(default: %g)\n", mincnt);
    printf("-j       split nominal attributes "
                    "one value against rest\n");
    printf("-s       try to form subsets on nominal attributes\n");
    printf("-B       enforce binary subsets splits (with -s)\n");
    printf("-g       do not do basic pruning of grown tree\n");
    printf("-v#      file name of validation data set       "
                    "(default: none)\n");
    printf("         (for a forward selection of attributes,\n");
    printf("         based on AUC, for only two classes)\n");
    printf("-m#      maximum number of attributes to select "
                    "(default: no limit)\n");
    printf("-V       print verbose progress messages\n");
    printf("-x       only evaluate attributes w.r.t. target\n");
    printf("-l#      output line length                     "
                    "(default: no limit)\n");
    printf("-a       align values of test attributes        "
                    "(default: do not align)\n");
    printf("-R       print relative frequencies (in percent)\n");
    printf("-r#      record     separators                  "
                    "(default: \"\\n\")\n");
    printf("-f#      field      separators                  "
                    "(default: \" \\t,\")\n");
    printf("-b#      blank      characters                  "
                    "(default: \" \\t\\r\")\n");
    printf("-u#      null value characters                  "
                    "(default: \"?*\")\n");
    printf("-C#      comment    characters                  "
                    "(default: \"#\")\n");
    printf("-n       number of tuple occurrences in last field\n");
    printf("domfile  file containing domain descriptions\n");
    printf("-d       use default header "
                    "(attribute names = field numbers)\n");
    printf("-h       read table header  "
                    "(attribute names) from hdrfile\n");
    printf("hdrfile  file containing table header "
                    "(attribute names)\n");
    printf("tabfile  table file to read "
                    "(attribute names in first record)\n");
    printf("dtfile   file to write induced "
                    "decision/regression tree to\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* remaining option characters: o A[D-Q][S-U][W-Z] */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case '!': help();                              break;
          case 'c': optarg    = &trgname;                break;
          case 'q': balance   = (*s) ? *s++ : 0;         break;
          case 'e': optarg    = &mname;                  break;
          case 'z': params[0] =         strtod(s, &s);   break;
          case 'p': params[1] =         strtod(s, &s);   break;
          case 'i': minval    =         strtod(s, &s);   break;
          case 'w': wgtd      = 0;                       break;
          case 'k': mincnt    = (WEIGHT)strtod(s, &s);   break;
          case 'j': flags    |= DT_1INN;                 break;
          case 's': flags    |= DT_SUBSET;               break;
          case 'B': flags    |= DT_BINARY;               break;
          case 'g': flags    |= DT_NOPRUNE;              break;
          case 'v': optarg    = &fn_val;                 break;
          case 'm': maxsel    =   (int)strtol(s, &s, 0); break;
          case 'V': verbose   = 1;                       break;
          case 'x': flags    |= DT_EVAL;                 break;
          case 't': maxht     = (ATTID)strtol(s, &s, 0); break;
          case 'l': maxlen    =   (int)strtol(s, &s, 0); break;
          case 'a': desc     |= DT_ALIGN;                break;
          case 'R': desc     |= DT_REL;                  break;
          case 'b': optarg    = &blanks;                 break;
          case 'f': optarg    = &fldseps;                break;
          case 'r': optarg    = &recseps;                break;
          case 'u': optarg    = &nullchs;                break;
          case 'C': optarg    = &comment;                break;
          case 'n': mode     |= AS_WEIGHT;               break;
          case 'd': mode     |= AS_DFLT;                 break;
          case 'h': optarg    = &fn_hdr;                 break;
          default : error(E_OPTION, *--s);               break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_dom = s;      break;
        case  1: fn_tab = s;      break;
        case  2: fn_dt  = s;      break;
        default: error(E_ARGCNT); break;
      }                         /* note file names */
    }
  }
  if (optarg) error(E_OPTARG);  /* check the option argument */
  if (k != 3) error(E_ARGCNT);  /* and the number of arguments */
  if (fn_hdr && (strcmp(fn_hdr, "-") == 0))
    fn_hdr = "";                /* convert "-" to "" for consistency */
  i = ( fn_hdr && !*fn_hdr) ? 1 : 0;
  if  (!fn_dom || !*fn_dom) i++;
  if  (!fn_tab || !*fn_tab) i++;/* check assignments of stdin: */
  if (i > 1) error(E_STDIN);    /* stdin must not be used twice */
  if (mincnt < 0) error(E_MINCNT, mincnt);
  if ((        balance  !=  0)  && (tolower(balance) != 'l')
  &&  (tolower(balance) != 'b') && (tolower(balance) != 's'))
    error(E_BALANCE, balance);  /* check the balancing mode */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- parse domain descriptions --- */
  attset = as_create("domains", att_delete);
  if (!attset) error(E_NOMEM);  /* create an attribute set */
  scan = scn_create();          /* create a scanner */
  if (!scan)   error(E_NOMEM);  /* for the domain file */
  t = clock();                  /* start timer, open input file */
  if (scn_open(scan, NULL, fn_dom) != 0)
    error(E_FOPEN, scn_name(scan));
  fprintf(stderr, "reading %s ... ", scn_name(scan));
  if ((as_parse(attset, scan, AT_ALL, 1) != 0)
  ||  !scn_eof(scan, 1))        /* parse domain descriptions */
    error(E_PARSE, scn_name(scan));
  scn_delete(scan, 1);          /* delete the scanner and */
  scan = NULL;                  /* clear the scanner variable */
  m    = as_attcnt(attset);     /* get the number of attributes */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s)]", m);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
  if (m <= 0) error(E_ATTCNT);  /* check for at least one attribute */

  /* --- determine id of target attribute --- */
  trgid = as_target(attset, trgname, 1);
  if (trgid < 0) error(trgname ? E_UNKTRG : E_MULTRG, trgname);

  /* --- translate measure --- */
  att = as_att(attset, trgid);  /* get the target attribute */
  if (att_type(att) == AT_NOM){ /* if nominal target attribute */
    if (!mname) mname = "infgr";/* set default measure name and */
    ntab = nomtab; }            /* get the nominal name table */
  else {                        /* if metric target attribute */
    if (!mname) mname = "rmse"; /* set default measure name and */
    ntab = mettab;              /* get the metric name table, */
  }                             /* then get the measure code */
  s = strchr(mname, ':');       /* check for a double measure */
  if (!s)                       /* if only a single measure name, */
    measure = code(ntab,mname); /* simply get the measure code */
  else {                        /* if a double measure name */
    *s = 0;          measure = code(ntab, mname);
    if (measure < 0) measure = code(ntab, s+1);
    *s = ':';                   /* check both measure names */
  }                             /* (nominal and metric target) */
  if (measure < 0) error(E_MEASURE, mname);
  measure |= wgtd;              /* add flag for weighted measure */

  /* --- read data table(s) --- */
  tread = trd_create();         /* create a table reader and */
  if (!tread) error(E_NOMEM);   /* configure the characters */
  trd_allchs(tread, recseps, fldseps, blanks, nullchs, comment);
  k = read_table(attset, tread, fn_tab, fn_hdr, mode, &table);
  if (k == E_NOMEM) error(E_NOMEM);
  if (k == E_FOPEN) error(E_FOPEN, trd_name(tread));
  if (k >  0)       error(k, as_errmsg(attset, NULL, 0));
  if (fn_val) {                 /* if validation data is given */
    k = read_table(attset, tread, fn_val, fn_hdr, mode, &valid);
    if (k == E_NOMEM) error(E_NOMEM);
    if (k == E_FOPEN) error(E_FOPEN, trd_name(tread));
    if (k >  0)       error(k, as_errmsg(attset, NULL, 0));
  }                             /* read the valdation data */
  trd_delete(tread, 1);         /* read the table body and */
  tread = NULL;                 /* delete the table reader */

  /* --- check target attribute --- */
  if (fn_val && ((att_type(att) != AT_NOM) || (att_valcnt(att) > 2)))
    error(E_CLSBIN, att_name(att));

  /* --- reduce and balance table --- */
  t = clock();                  /* start timer, print log message */
  fprintf(stderr, "reducing%s table ... ",
                  (balance) ? " and balancing" : "");
  n = tab_reduce(table);        /* reduce table for speed up */
  w = tab_tplwgt(table);        /* and get the total weight */
  if (balance                   /* if the balance flag is set */
  && (att_type(as_att(attset, trgid)) == AT_NOM)) {
      k = (tolower(balance) == 'l') ? -2
        : (tolower(balance) == 'b') ? -1 : 0;
      i = (tolower(balance) != balance);
      w = tab_balance(table, trgid, k, NULL, i);
    if (w < 0) error(E_NOMEM);  /* balance the class frequencies */
  }                             /* and get the new weight sum */
  fprintf(stderr, "[%"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- forward selection of attributes --- */
  if (valid) {                  /* if to select attributes */
    fwdsel(table, valid, trgid, measure, params, minval,
           maxht, mincnt, flags, maxsel, verbose);
    /* The result of the attribute selection is represented  */
    /* by attribute markers in the underlying attribute set. */
    tab_delete(valid, 0);       /* delete the validation data */
    valid = NULL;               /* and clear the variable */
    perm  = (ATTID*)malloc((size_t)m *sizeof(ATTID));
    if (!perm) error(E_NOMEM);  /* create a permutation array */
    for (r = m; --r >= 0; ) {   /* traverse the attributes */
      z = att_getmark(as_att(attset, r));
      if      (z > 0) perm[z-1] = r;
      else if (z < 0) perm[--m] = r;
    }                           /* collect destinations of columns */
    perm[--m] = trgid;          /* store target attribute and */
    trgid = m;                  /* get the new target index */
    tab_colperm(table, perm);   /* move selected attributes */
    free(perm);                 /* to the front of the table */
  }                             /* and delete permutation array */

  /* --- grow (final) decision/regression tree --- */
  t = clock();                  /* start timer, print log message */
  fprintf(stderr, "growing %s tree ... ",
          (att_type(att) == AT_NOM) ? "decision" : "regression");
  if (flags & DT_EVAL) maxht = 2;
  dtree = dt_grow(table, trgid, measure, params, minval,
                  maxht, mincnt, flags);
  if (!dtree) error(E_NOMEM);   /* grow decision/regression tree */
  used  = dt_attchk(dtree);     /* mark occuring attributes */
  maxht = dt_height(dtree);     /* get the height of the tree */
  size  = dt_size(dtree);       /* and the number of nodes */
  fprintf(stderr, "[%"ATTID_FMT"+1 attribute(s)", used-1);
  fprintf(stderr, "/%"ATTID_FMT" level(s)",       maxht);
  fprintf(stderr, "/%"ATTID_FMT" node(s)]",       size);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- write decision/regression tree --- */
  t = clock();                  /* start timer, open output file */
  if (strcmp(fn_dt, "-") == 0) fn_dt = "";
  if (fn_dt && *fn_dt) { out = fopen(fn_dt, "w"); }
  else                 { out = stdout; fn_dt = "<stdout>"; }
  fprintf(stderr, "writing %s ... ", fn_dt);
  if (!out) error(E_FOPEN, fn_dt);
  if (flags & DT_EVAL) {        /* if attributes were evaluated */
    used = m;                   /* all attributes are 'used' */
    for (i = k = 0, c = 0; c < m; c++) {
      if (c == trgid) continue; /* traverse the attributes */
      i = (int)strlen(att_name(as_att(attset, c)));
      if (i > k) k = i;         /* determine the maximum width */
    }                           /* of an attribute name */
    evals = dt_evals(dtree);    /* get the attribute evaluations */
    cuts  = dt_cuts (dtree);    /* and the cut values (if metric) */
    for (k += 2, c = 0; c < m; c++) {
      if (c == trgid) continue; /* traverse the attributes, */
      att = as_att(attset, c);  /* but skip the target attribute */
      i   = fprintf(out, "%s", att_name(att));
      if (i < 0) break;         /* print the attribute name */
      while (++i <= k) fputc(' ', out);
      fprintf(out, "%g", evals[c]);
      if (att_type(att) != AT_NOM) fprintf(out, " (%g)", cuts[c]);
      fputc('\n', out);         /* print the attribute evaluation */
    } }                         /* and, if necessary, a cut value */
  else {                        /* if a decision tree has been grown */
    if (as_desc(attset, out, AS_TITLE|AS_MARKED|AS_IVALS, maxlen) != 0)
      error(E_FWRITE, fn_dt);   /* describe attribute domains */
    fputc('\n', out);           /* leave one line empty */
    if (dt_desc(dtree, out, DT_TITLE|DT_INFO|desc, maxlen) != 0)
      error(E_FWRITE, fn_dt);   /* describe decision/regression tree */
  }
  if (out && (((out == stdout) ? fflush(out) : fclose(out)) != 0))
    error(E_FWRITE, fn_dt);     /* close the output file and */
  out = NULL;                   /* print a success message */
  fprintf(stderr, "[%"ATTID_FMT"+1 attribute(s)", used-1);
  if (!(flags & DT_EVAL)) {     /* if not only evaluation */
    fprintf(stderr, "/%"ATTID_FMT" level(s)", maxht);
    fprintf(stderr, "/%"ATTID_FMT" node(s)", size);
  }                             /* print tree size information */
  fprintf(stderr, "] done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
