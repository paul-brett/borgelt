/*----------------------------------------------------------------------
  File    : dtr.c
  Contents: decision and regression tree rule extraction
  Authors : Christian Borgelt
  History : 1998.05.26 file created
            1998.06.23 adapted to modified attset functions
            1998.09.03 interpretation of option -e changed
            1999.04.17 simplified using the new module 'io'
            2000.12.18 extended to regression trees
            2001.07.23 adapted to modified module scan
            2002.07.06 adapted to modified module rules (RS_INFO)
            2003.08.16 slight changes in error message output
            2004.11.09 execution time output added
            2011.07.28 adapted to modified attset and utility modules
            2013.08.23 adapted to definitions ATTID, VALID, TPLID etc.
            2014.10.24 changed from LGPL license to MIT license
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#ifndef AS_PARSE
#define AS_PARSE
#endif
#ifndef AS_DESC
#define AS_DESC
#endif
#include "attset.h"
#ifndef RS_DESC
#define RS_DESC
#endif
#include "rules.h"
#ifndef DT_PARSE
#define DT_PARSE
#endif
#ifndef DT_RULES
#define DT_RULES
#endif
#include "dtree.h"
#include "error.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "dtr"
#define DESCRIPTION "decision and regression tree rule extraction"
#define VERSION     "version 3.3 (2016.11.11)         " \
                    "(c) 1998-2016   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */
#define E_PARSE     (-9)        /* parse error */

#define SEC_SINCE(t)  ((double)(clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
const char *errmsgs[] = {       /* error messages */
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
  /*           -10 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR   *prgname;        /* program name for error messages */
static SCANNER *scan    = NULL; /* scanner (for decision tree) */
static DTREE   *dtree   = NULL; /* decision/regression tree */
static ATTSET  *attset  = NULL; /* attribute set */
static RULESET *ruleset = NULL; /* extracted rule set */
static FILE    *out     = NULL; /* output file */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (ruleset) rs_delete(ruleset, 0); \
  if (dtree)   dt_delete(dtree, 0);   \
  if (attset)  as_delete(attset);     \
  if (scan)    scn_delete(scan, 1);   \
  if (out && (out != stdout)) fclose(out);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

static int rulecmp (const RULE *r1, const RULE *r2, void *data)
{                               /* --- compare two rules */
  VALID c1 = r_headval(r1)->i;  /* get the classes */
  VALID c2 = r_headval(r2)->i;  /* of the two rules */
  if (c1 < c2) return -1;       /* and compare them */
  return (c1 > c2) ? 1 : 0;     /* return sign of their difference */
}  /* rulecmp() */

/*--------------------------------------------------------------------*/

int main (int argc, char* argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counter */
  char    *s;                   /* to traverse options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_dt   = NULL;      /* name of dec./reg. tree file */
  CCHAR   *fn_rs   = NULL;      /* name of rule set file */
  int     sort     = 1;         /* flag whether to sort conditions */
  int     maxlen   = 0;         /* maximal output line length */
  int     desc     = RS_TITLE|RS_INFO; /* rule set description mode */
  ATTID   m, h, z;              /* number of attributes, tree height */
  clock_t t;                    /* timer for measurements */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] dtfile rsfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-s       print support    of a rule\n");
    printf("-c       print confidence of a rule\n");
    printf("-d       print only one condition per line\n");
    printf("-t       do not sort conditions nor rules\n");
    printf("-l#      output line length (default: no limit)\n");
    printf("dtfile   file containing decision/regression "
                    "tree description\n");
    printf("rsfile   file to write rule set description to\n");
    return 0;                   /* print usage message */
  }                             /* and abort program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 's': desc  |= RS_SUPP;               break;
          case 'c': desc  |= RS_CONF;               break;
          case 'd': desc  |= RS_CONDLN;             break;
          case 't': sort   = 0;                     break;
          case 'l': maxlen = (int)strtol(s, &s, 0); break;
          default : error(E_OPTION, *--s);          break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_dt = s;       break;
        case  1: fn_rs = s;       break;
        default: error(E_ARGCNT); break;
      }                         /* note file names */
    }
  }
  if (optarg) error(E_OPTARG);  /* check option argument */
  if (k != 2) error(E_ARGCNT);  /* check number of arguments */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- read decision/regression tree --- */
  attset = as_create("domains", att_delete);
  if (!attset) error(E_NOMEM);  /* create an attribute set */
  scan = scn_create();          /* create a scanner */
  if (!scan)   error(E_NOMEM);  /* for the domain file */
  t = clock();                  /* start timer, open input file */
  if (scn_open(scan, NULL, fn_dt) != 0)
    error(E_FOPEN, scn_name(scan));
  fprintf(stderr, "reading %s ... ", scn_name(scan));
  if (as_parse(attset, scan, AT_ALL, 1) != 0)
    error(E_PARSE, scn_name(scan)); /* parse domain descriptions */
  dtree = dt_parse(attset, scan);   /* and the decision tree */
  if (!dtree || !scn_eof(scan, 1)) error(E_PARSE, scn_name(scan));
  scn_delete(scan, 1);          /* delete the scanner and */
  scan = NULL;                  /* clear the scanner variable */
  m    = as_attcnt(attset);     /* get the number of attributes, */
  h    = dt_height(dtree);      /* and the height of the tree */
  z    = dt_size(dtree);        /* and the number of nodes */
  fprintf(stderr, "[%"ATTID_FMT"+1 attribute(s)", m-1);
  fprintf(stderr, "/%"ATTID_FMT" level(s)",       h);
  fprintf(stderr, "/%"ATTID_FMT" node(s)]",       z);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- extract and write rule set --- */
  t = clock();                  /* start timer, open output file */
  if (strcmp(fn_rs, "-") == 0) fn_rs = "";
  if (fn_rs && *fn_rs) { out = fopen(fn_rs, "w"); }
  else                 { out = stdout; fn_rs = "<stdout>"; }
  fprintf(stderr, "writing %s ... ", fn_rs);
  if (!out) error(E_FOPEN, fn_rs);
  ruleset = dt_rules(dtree);    /* extract a rule set from the */
  if (!ruleset) error(E_NOMEM); /* decision/regression tree */
  if (sort) {                   /* if to sort the rules */
    for (i = rs_rulecnt(ruleset); --i >= 0; )
      r_condsort(rs_rule(ruleset, i));
    rs_sort(ruleset, rulecmp, NULL);
  }                             /* sort the conditions and the rules */
  if (as_desc(attset, out, AS_TITLE|AS_MARKED, maxlen) != 0)
    error(E_FWRITE, fn_rs);     /* describe attribute domains */
  fputc('\n', out);             /* leave one line empty */
  if (rs_desc(ruleset, out, desc, maxlen) != 0)
    error(E_FWRITE, fn_rs);     /* describe the ruleset */
  if (out && (((out == stdout) ? fflush(out) : fclose(out)) != 0))
    error(E_FWRITE, fn_rs);     /* close the output file and */
  out = NULL;                   /* print a success message */
  fprintf(stderr, "[%"RULEID_FMT" rule(s)]", rs_rulecnt(ruleset));
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
