/*----------------------------------------------------------------------
  File    : dtp.c
  Contents: decision and regression tree pruning
  Authors : Christian Borgelt
  History : 1997.08.04 file created
            1997.08.28 pessimistic pruning added
            1997.09.17 option '-a' (aligned output) added
            1998.01.11 null value characters (option -u) added
            1998.02.08 adapted to changed parse functions
            1998.02.09 adapted to changed order of pruning methods
            1998.06.23 adapted to modified attset functions
            1998.09.29 table reading simplified
            1998.10.20 output of relative class frequencies added
            1999.02.09 input from stdin, output to stdout added
            1999.04.17 simplified using the new module 'io'
            1999.04.30 log messages improved
            2000.12.17 option -q (balance class frequencies) added
            2000.12.18 extended to regression trees
            2001.07.23 adapted to modified module scanner
            2001.10.19 conf. level pruning for metric targets added
            2002.03.01 option -k (check largest branch) added
            2003.08.16 slight changes in error message output
            2004.11.09 execution time output added
            2007.02.13 adapted to modified module attset
            2011.07.28 adapted to modified attset and utility modules
            2013.08.23 adapted to definitions ATTID, VALID, TPLID etc.
            2014.10.24 changed from LGPL license to MIT license
            2015.11.12 node selection by frequency threshold added
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
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
#ifndef DT_PRUNE
#define DT_PRUNE
#endif
#ifndef DT_PARSE
#define DT_PARSE
#endif
#include "dtree.h"
#include "error.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "dtp"
#define DESCRIPTION "decision and regression tree pruning"
#define VERSION     "version 4.3 (2016.11.11)         " \
                    "(c) 1997-2016   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */
#define E_PARSE     (-9)        /* parse error */
#define E_BALANCE  (-10)        /* double assignment of stdin */
#define E_METHOD   (-11)        /* unknown pruning method */

#define SEC_SINCE(t)  ((double)(clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- method information --- */
  int  code;                    /* pruning method code */
  char *name;                   /* name of the method */
} MINFO;                        /* (method information) */

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
/* --- pruning methods --- */
static const MINFO pmtab[] = {
  { PM_NONE, "none" },          /* no pruning */
  { PM_PESS, "pess" },          /* pessimistic pruning */
  { PM_CLVL, "clvl" },          /* confidence level pruning */
  { -1,      NULL   }           /* sentinel */
};

/* --- error messages --- */
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
  /* E_BALANCE -10 */  "unknown balancing mode %c",
  /* E_METHOD  -11 */  "unknown pruning method %s",
  /*           -12 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR   *prgname;        /* program name for error messages */
static SCANNER *scan   = NULL;  /* scanner (for decision tree) */
static TABREAD *tread  = NULL;  /* table reader */
static ATTSET  *attset = NULL;  /* attribute set */
static TABLE   *table  = NULL;  /* data table */
static DTREE   *dtree  = NULL;  /* decision/regression tree */
static FILE    *out    = NULL;  /* output file */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (table)  tab_delete(table, 0); \
  if (dtree)  dt_delete (dtree, 0); \
  if (attset) as_delete (attset);   \
  if (tread)  trd_delete(tread, 1); \
  if (scan)   scn_delete(scan,  1); \
  if (out && (out != stdout)) fclose(out);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

static int code (const MINFO *tab, const char *name)
{                               /* --- get pruning method code */
  assert(tab && name);          /* check the function arguments */
  for ( ; tab->name; tab++)     /* look up name in table */
    if (strcmp(tab->name, name) == 0)
      return tab->code;         /* return the method code */
  return -1;                    /* or an error indicator */
}  /* code() */

/*--------------------------------------------------------------------*/

int main (int argc, char* argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counter */
  char    *s;                   /* to traverse options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_dt   = NULL;      /* name of dec./reg. tree file */
  CCHAR   *fn_out  = NULL;      /* name of output file */
  CCHAR   *fn_hdr  = NULL;      /* name of table header file */
  CCHAR   *fn_tab  = NULL;      /* name of table file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  CCHAR   *mname   = NULL;      /* name of pruning method */
  int     method   = 2;         /* pruning method */
  double  param    = 0.5;       /* pruning parameter */
  ATTID   maxht    = ATTID_MAX; /* maximal height of tree */
  int     chklb    = 0;         /* whether to check largest branch */
  double  thsel    = 0;         /* threshold for node selection */
  int     mode     = AS_ATT|AS_NOXATT;     /* table file read mode */
  int     balance  = 0;         /* flag for balancing class freqs. */
  int     maxlen   = 0;         /* maximal output line length */
  int     desc     = 0;         /* description mode */
  ATTID   m, h, z;              /* number of attributes */
  TPLID   n;                    /* number of data tuples */
  double  w;                    /* weight of data tuples */
  clock_t t;                    /* timer for measurements */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] dtfile outfile "
                     "[[-d|-h hdrfile] tabfile]\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-m#      pruning method                         "
                    "(default: clvl)\n");
    printf("         (none: no pruning, pess: pessimistic, "
                    "clvl: confidence level)\n");
    printf("-p#      pruning parameter                      "
                    "(default: %g)\n", param);
    printf("-q#      balance class frequencies (weight tuples)\n");
    printf("         (l: lower, b: boost, s: shift tuple weights;\n");
    printf("          uppercase letters: use integer factors)\n");
    printf("-t#      maximal height of the tree             "
                    "(default: no limit)\n");
    printf("-k       check largest branch "
                    "(can be very time consuming)\n");
    printf("-s#      threshold for node selection           "
                    "(default: none)\n");
    printf("         (percentage; only for two classes "
                    "and no tabfile)\n");
    printf("-l#      output line length                     "
                    "(default: no limit)\n");
    printf("-a       align values of test attributes        "
                    "(default: do not align)\n");
    printf("-v       print relative frequencies (in percent)\n");
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
    printf("dtfile   file containing decision/regression "
                    "tree description\n");
    printf("outfile  file to write pruned "
                    "decision/regression tree to\n");
    printf("-d       use default header "
                    "(attribute names = field numbers)\n");
    printf("-h       read table header  "
                    "(attribute names) from hdrfile\n");
    printf("hdrfile  file containing table header "
                    "(attribute names)\n");
    printf("tabfile  table file to read "
                    "(attribute names in first record)\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 'm': optarg  = &mname;                  break;
          case 'p': param   =        strtod(s, &s);    break;
          case 'q': balance = (*s) ? *s++ : 0;         break;
          case 't': maxht   = (ATTID)strtol(s, &s, 0); break;
          case 'k': chklb   = 1;                       break;
          case 's': thsel   = 0.01 * strtod(s, &s);    break;
          case 'a': desc   |= DT_ALIGN;                break;
          case 'v': desc   |= DT_REL;                  break;
          case 'l': maxlen  =   (int)strtol(s, &s, 0); break;
          case 'b': optarg  = &blanks;                 break;
          case 'f': optarg  = &fldseps;                break;
          case 'r': optarg  = &recseps;                break;
          case 'u': optarg  = &nullchs;                break;
          case 'C': optarg  = &comment;                break;
          case 'n': mode   |= AS_WEIGHT;               break;
          case 'd': mode   |= AS_DFLT;                 break;
          case 'h': optarg  = &fn_hdr;                 break;
          default : error(E_OPTION, *--s);             break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_dt  = s;      break;
        case  1: fn_out = s;      break;
        case  2: fn_tab = s;      break;
        default: error(E_ARGCNT); break;
      }                         /* note file names */
    }
  }
  if (optarg) error(E_OPTARG);  /* check the option argument and */
  if ((k < 2) || (k > 3)) error(E_ARGCNT); /* the number of args */
  if (fn_hdr && (strcmp(fn_hdr, "-") == 0))
    fn_hdr = "";                /* convert "-" to "" for consistency */
  i = (!fn_dt || !*fn_dt) ? 1 : 0;
  if  (fn_tab && !*fn_tab) i++;
  if  (fn_hdr && !*fn_hdr) i++; /* check assignments of stdin: */
  if (i > 1) error(E_STDIN);    /* stdin must not be used twice */
  if ((        balance  !=  0)  && (tolower(balance) != 'l')
  &&  (tolower(balance) != 'b') && (tolower(balance) != 's'))
    error(E_BALANCE, balance);  /* check the balancing mode */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- translate pruning method --- */
  if (!mname) mname = "clvl";   /* set default method name */
  method = code(pmtab, mname);  /* and get the method code */
  if (method < 0) error(E_METHOD, method);

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

  if (fn_tab) {                 /* if a data table is given */
    /* --- read table header --- */
    tread = trd_create();       /* create a table reader and */
    if (!tread) error(E_NOMEM); /* configure the characters */
    trd_allchs(tread, recseps, fldseps, blanks, nullchs, comment);
    if (fn_hdr) {               /* if a header file is given */
      t = clock();              /* start timer, open input file */
      if (trd_open(tread, NULL, fn_hdr) != 0)
        error(E_FOPEN, trd_name(tread));
      fprintf(stderr, "reading %s ... ", trd_name(tread));
      k = as_read(attset, tread, (mode & ~AS_DFLT) | AS_ATT);
      if (k < 0) error(-k, as_errmsg(attset, NULL, 0));
      trd_close(tread);         /* read table header, close file */
      fprintf(stderr, "[%"ATTID_FMT" attribute(s)]", as_attcnt(attset));
      fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
      mode &= ~(AS_ATT|AS_DFLT);/* print a success message and */
    }                           /* remove the attribute flag */

    /* --- read table --- */
    t = clock();                /* start timer, open input file */
    if (trd_open(tread, NULL, fn_tab) != 0)
      error(E_FOPEN, trd_name(tread));
    fprintf(stderr, "reading %s ... ", trd_name(tread));
    table = tab_create("table", attset, tpl_delete);
    if (!table) error(E_NOMEM); /* create a data table */
    k = tab_read(table, tread, mode);
    if (k < 0) error(-k, tab_errmsg(table, NULL, 0));
    trd_delete(tread, 1);       /* read the table body and */
    tread = NULL;               /* delete the table reader */
    m = tab_attcnt(table);      /* get the number of attributes */
    n = tab_tplcnt(table);      /* and the number of data tuples */
    w = tab_tplwgt(table);      /* and print a success message */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
    fprintf(stderr, " %"TPLID_FMT, n);
    if (w != (double)n) fprintf(stderr, "/%g", w);
    fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

    /* --- reduce and balance table --- */
    t = clock();                /* start timer, print log message */
    fprintf(stderr, "reducing%s table ... ",
                    (balance) ? " and balancing" : "");
    n = tab_reduce(table);      /* reduce table for speed up */
    z = dt_trgid(dtree);        /* get the target id/index */
    if (balance                 /* if the balance flag is set */
    && (att_type(as_att(attset, z)) == AT_NOM)) {
      k = (tolower(balance) == 'l') ? -2
        : (tolower(balance) == 'b') ? -1 : 0;
      i = (tolower(balance) != balance);
      w = tab_balance(table, z, k, NULL, i);
      if (w < 0) error(E_NOMEM);/* balance the class frequencies */
    }                           /* and get the new weight sum */
    fprintf(stderr, "[%"TPLID_FMT, n);
    if (w != (double)n) fprintf(stderr, "/%g", w);
    fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));
  }

  /* --- prune decision/regression tree --- */
  t = clock();                  /* start the timer */
  fprintf(stderr, "pruning tree ... ");     /* print a log message */
  if (dt_prune(dtree, method, param, maxht, chklb, thsel, table) != 0)
    error(E_NOMEM);             /* prune the decision/regression tree */
  m = dt_attchk(dtree);         /* mark the occurring attributes */
  h = dt_height(dtree);         /* get the height of the tree */
  z = dt_size(dtree);           /* and the number of nodes */
  fprintf(stderr, "[%"ATTID_FMT"+1 attribute(s)", m-1);
  fprintf(stderr, "/%"ATTID_FMT" level(s)",       h);
  fprintf(stderr, "/%"ATTID_FMT" node(s)]",       z);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- write decision/regression tree --- */
  t = clock();                  /* start timer, open output file */
  if (strcmp(fn_out, "-") == 0) fn_out = "";
  if (fn_out && *fn_out) { out = fopen(fn_out, "w"); }
  else                   { out = stdout; fn_out = "<stdout>"; }
  fprintf(stderr, "writing %s ... ", fn_out);
  if (!out) error(E_FOPEN, fn_out);
  if (as_desc(attset, out, AS_TITLE|AS_MARKED|AS_IVALS, maxlen) != 0)
    error(E_FWRITE, fn_out);    /* describe attribute domains */
  fputc('\n', out);             /* leave one line empty */
  if (dt_desc(dtree, out, DT_TITLE|DT_INFO|desc, maxlen) != 0)
    error(E_FWRITE, fn_out);    /* describe decision/regression tree */
  if (out && (((out == stdout) ? fflush(out) : fclose(out)) != 0))
    error(E_FWRITE, fn_out);    /* close the output file and */
  out = NULL;                   /* print a success message */
  fprintf(stderr, "[%"ATTID_FMT"+1 attribute(s)", m-1);
  fprintf(stderr, "/%"ATTID_FMT" level(s)",       h);
  fprintf(stderr, "/%"ATTID_FMT" node(s)]",       z);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
