/*----------------------------------------------------------------------
  File    : rsx.c
  Contents: rule set execution
  Authors : Christian Borgelt
  History : 2002.07.06 file created from file dtx.c
            2003.04.23 missing AS_MARKED added for second reading
            2003.08.16 slight changes in error message output
            2004.11.09 execution time output added
            2007.02.13 adapted to modified module attset
            2011.07.28 adapted to modified attset and utility modules
            2011.12.15 processing without table reading improved
            2013.08.23 adapted to definitions ATTID, VALID, TPLID etc.
            2014.10.24 changed from LGPL license to MIT license
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#ifndef AS_READ
#define AS_READ
#endif
#ifndef AS_WRITE
#define AS_WRITE
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
#ifndef RS_PARSE
#define RS_PARSE
#endif
#ifndef RS_DESC
#define RS_DESC
#endif
#include "rules.h"
#include "error.h"
#ifdef STORAGE
#include "storage.h"
#endif

#ifdef _MSC_VER
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif                          /* MSC still does not support C99 */

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "rsx"
#define DESCRIPTION "rule set execution"
#define VERSION     "version 2.1 (2014.10.24)         " \
                    "(c) 2002-2014   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */
#define E_PARSE     (-9)        /* parse error */
#define E_TARGET   (-10)        /* missing target (head attribute) */
#define E_OUTPUT   (-11)        /* class in input or write output */
#define E_EMPTY    (-12)        /* empty rule set */
#define E_HEAD     (-13)        /* more than one head attribute */

#define SEC_SINCE(t)  ((double)(clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- prediction result --- */
  ATT   *att;                   /* target attribute */
  int    type;                  /* type of the target attribute */
  int    bin;                   /* flag for binary target attribute */
  RULE  *rule;                  /* rule used for prediction */
  RCVAL  pred;                  /* predicted value */
  CCHAR *col_pred;              /* name   of prediction column */
  int    cwd_pred;              /* width  of prediction column */
  int    dig_pred;              /* digits of prediction column */
  double supp;                  /* support of prediction */
  CCHAR *col_supp;              /* name   of support    column */
  int    cwd_supp;              /* width  of support    column */
  int    dig_supp;              /* digits of support    column */
  double conf;                  /* confidence of prediction */
  CCHAR *col_conf;              /* name   of confidence column */
  int    cwd_conf;              /* width  of confidence column */
  int    dig_conf;              /* digits of confidence column */
  double err;                   /* error value (squared difference) */
} RESULT;                       /* (prediction result) */

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
  /* E_TARGET  -10 */  "missing target '%s' in file %s",
  /* E_OUTPUT  -11 */  "must have target as input or write output",
  /* E_EMPTY   -12 */  "rule set is empty",
  /* E_HEAD    -13 */  "more than one head attribute in rule set",
  /*           -14 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR    *prgname;       /* program name for error messages */
static SCANNER  *scan   = NULL; /* scanner */
static TABREAD  *tread  = NULL; /* table reader */
static TABWRITE *twrite = NULL; /* table writer */
static ATTSET   *attset = NULL; /* attribute set */
static TABLE    *table  = NULL; /* data table */
static RULESET  *rulset = NULL; /* rule set to execute */
static FILE     *out    = NULL; /* output file for rule set */
static RESULT   res     = {     /* prediction result */
  NULL, AT_NOM, 0, NULL,        /* target attribute and its type */
  {0}, "rs", 0, 6,              /* data for prediction column */
  0.0, NULL, 0, 1,              /* data for support    column */
  0.0, NULL, 0, 3,              /* data for confidence column */
  0 };                          /* error value (squared difference) */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (rulset) rs_delete(rulset,  0); \
  if (attset) as_delete(attset);     \
  if (table)  tab_delete(table,  0); \
  if (tread)  trd_delete(tread,  1); \
  if (twrite) twr_delete(twrite, 1); \
  if (scan)   scn_delete(scan,   1); \
  if (out && (out != stdout)) fclose(out);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

static void predict (double thresh)
{                               /* --- classify the current tuple */
  RULEID r;                     /* index of first applicable rule */
  INST   *inst;                 /* to access the target instance */

  assert(rulset);               /* check for a rule set */
  r = rs_exec(rulset);          /* find the first applicable rule */
  if (r >= 0) {                 /* if some rule is applicable, */
    res.rule = rs_rule(rulset, r); /* get and execute the rule */
    res.pred = *r_headval(res.rule);
    res.supp = r_getsupp(res.rule);
    res.conf = r_getconf(res.rule); }
  else {                        /* if no rule is applicable */
    res.rule = NULL;            /* use the rule variable as a flag */
    if      (res.type == AT_NOM) res.pred.n = NV_NOM;
    else if (res.type == AT_INT) res.pred.i = NV_INT;
    else                         res.pred.f = NV_FLT;
    res.supp = res.conf = 0;    /* set an unknown prediction and */
  }                             /* clear the support and confidence */
  inst = att_inst(res.att);     /* get the target attribute */
  if (res.type == AT_NOM) {     /* if the target attribute is nominal */
    if (res.bin) {              /* if the target attribute is binary */
      if (((res.pred.i == 0) && (1.0 -res.conf >= thresh))
      ||  ((res.pred.i == 1) && (     res.conf <  thresh))) {
         res.pred.i = 1 -res.pred.i; res.conf = 1.0 -res.conf; }
    }                           /* adapt the classification result */
    res.err = (!isnone(inst->n) && (res.pred.n != inst->n)) ? 1 : 0; }
  else {                        /* if the target att. is metric */
    if (res.type == AT_INT) {   /* if it is integer-valued */
      res.pred.i = (DTINT)(res.pred.f +0.5);
      res.err    = !isnull(inst->i) ? res.pred.i -inst->i : 0; }
    else {                      /* if it is real-valued */
      res.err    = !isnan (inst->f) ? res.pred.f -inst->f : 0;
    }                           /* compute diff. to the true value */
    res.err *= res.err;         /* square the difference */
  }                             /* to compute the error */
  if      (res.conf > 1.0) res.conf = 1.0;
  else if (res.conf < 0.0) res.conf = 0.0;
}  /* predict() */

/*--------------------------------------------------------------------*/

static void infout (ATTSET *set, TABWRITE *twrite, int mode)
{                               /* --- write additional information */
  int n, k;                     /* character counters */

  assert(set && twrite);        /* check the function arguments */
  if (mode & AS_ATT) {          /* if to write the header */
    twr_puts(twrite, res.col_pred); /* write prediction column name */
    if ((mode & AS_ALIGN)       /* if to align the column */
    && ((mode & AS_WEIGHT) || res.col_supp || res.col_conf)) {
      n = (int)strlen(res.col_pred);
      k = att_valwd(res.att, 0);
      res.cwd_pred = k = ((mode & AS_ALNHDR) && (n > k)) ? n : k;
      if (k > n) twr_pad(twrite, (size_t)(k-n));
    }                           /* compute width of class column */
    if (res.col_supp) {         /* if to write a class support */
      twr_fldsep(twrite);       /* write a field separator and */
      twr_puts(twrite, res.col_supp);       /* the column name */
      if ((mode & AS_ALIGN)     /* if to align the column */
      && ((mode & AS_WEIGHT) || res.col_conf)) {
        n = (int)strlen(res.col_supp);
        k = res.dig_supp +3;    /* compute width of support column */
        res.cwd_supp = k = ((mode & AS_ALNHDR) && (n > k)) ? n : k;
        if (k > n) twr_pad(twrite, (size_t)(k-n));
      }                         /* pad with blanks if requested */
    }
    if (res.col_conf) {         /* if to write a class confidence */
      twr_fldsep(twrite);       /* write a field separator and */
      twr_puts(twrite, res.col_conf);       /* the column name */
      if ((mode & AS_ALIGN)     /* if to align the column */
      &&  (mode & AS_WEIGHT)) {
        n = (int)strlen(res.col_conf);
        k = res.dig_conf +3;    /* compute width of conf. column */
        res.cwd_conf = k = ((mode & AS_ALNHDR) && (n > k)) ? n : k;
        if (k > n) twr_pad(twrite, (size_t)(k-n));
      }                         /* pad with blanks if requested */
    } }
  else {                        /* if to write a normal record */
    n = (res.type == AT_NOM)    /* get the class value */
      ? twr_printf(twrite, "%s", att_valname(res.att, res.pred.n))
      : twr_printf(twrite, "%.*g", res.dig_pred, res.pred.f);
    if (res.cwd_pred > n) twr_pad(twrite, (size_t)(res.cwd_pred-n));
    if (res.col_supp) {         /* if to write a class probability */
      twr_fldsep(twrite);       /* write separator and probability */
      n = twr_printf(twrite, "%.*g", res.dig_supp, res.supp);
      if (res.cwd_supp > n) twr_pad(twrite, (size_t)(res.cwd_supp-n));
    }                           /* if to align, pad with blanks */
    if (res.col_conf) {         /* if to write a class probability */
      twr_fldsep(twrite);       /* write separator and probability */
      n = twr_printf(twrite, "%.*g", res.dig_conf, res.conf);
      if (res.cwd_conf > n) twr_pad(twrite, (size_t)(res.cwd_conf-n));
    }                           /* if to align, pad with blanks */
  }
}  /* infout() */

/*--------------------------------------------------------------------*/

int main (int argc, char* argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counter */
  char    *s;                   /* to traverse options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_hdr  = NULL;      /* name of table header file */
  CCHAR   *fn_tab  = NULL;      /* name of table file */
  CCHAR   *fn_rs   = NULL;      /* name of rule set file */
  CCHAR   *fn_out  = NULL;      /* name of output file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  double  thresh   = 0.5;       /* classification threshold */
  int     mode     = AS_ATT;    /* table file read  mode */
  int     mout     = AS_ATT;    /* table file write mode */
  double  errs     = 0.0;       /* number of misclassifications */
  int     adapt    = 0;         /* flag for rule set adaptation */
  int     maxlen   = 0;         /* maximal output line length */
  RULE    *rule;                /* to traverse the rules */
  ATTID   m, c;                 /* number of attributes */
  TPLID   n, r;                 /* number of data tuples */
  RULEID  x, y;                 /* number of rules, loop variable */
  double  w, u;                 /* weight of data tuples */
  clock_t t;                    /* timer for measurements */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] rsfile [-d|-h hdrfile] "
                     "tabfile [outfile]\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-x       adapt rule set support/confidence      "
                    "(default: classify cases)\n");
    printf("-p#      prediction field name                  "
                    "(default: \"%s\")\n", res.col_pred);
    printf("-o#      significant digits for prediction      "
                    "(default: %d)\n", res.dig_pred);
    printf("-s#      support    field name                  "
                    "(default: no support)\n");
    printf("-y#      significant digits for support         "
                    "(default: %d)\n", res.dig_supp);
    printf("-c#      confidence/probability field name      "
                    "(default: no confidence)\n");
    printf("-z#      significant digits for confidence      "
                    "(default: %d)\n", res.dig_conf);
    printf("-t#      probability threshold                  "
                    "(default: %g)\n", thresh);
    printf("         (only for problems with just two classes)\n");
    printf("-a       align fields of output table           "
                    "(default: do not align)\n");
    printf("-w       do not write field names to output file\n");
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
    printf("rsfile   file containing rule set description\n");
    printf("-d       use default header "
                    "(attribute names = field numbers)\n");
    printf("-h       read table header  "
                    "(attribute names) from hdrfile\n");
    printf("hdrfile  file containing table header "
                    "(attribute names)\n");
    printf("tabfile  table file to read "
                    "(attribute names in first record)\n");
    printf("outfile  file to write output table/rule set to "
                    "(optional)\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 'x': adapt  = 1;                     break;
          case 'p': optarg = &res.col_pred;         break;
          case 's': optarg = &res.col_supp;         break;
          case 'c': optarg = &res.col_conf;         break;
          case 'o': res.dig_pred = (int)strtol(s, &s, 0); break;
          case 'y': res.dig_supp = (int)strtol(s, &s, 0); break;
          case 'z': res.dig_conf = (int)strtol(s, &s, 0); break;
          case 't': thresh = strtod(s, &s);         break;
          case 'a': mout  |= AS_ALIGN;              break;
          case 'w': mout  &= ~AS_ATT;               break;
          case 'r': optarg = &recseps;              break;
          case 'f': optarg = &fldseps;              break;
          case 'b': optarg = &blanks;               break;
          case 'u': optarg = &nullchs;              break;
          case 'C': optarg = &comment;              break;
          case 'n': mout  |= AS_WEIGHT;
                    mode  |= AS_WEIGHT;             break;
          case 'l': maxlen = (int)strtol(s, &s, 0); break;
          case 'd': mode  |= AS_DFLT;               break;
          case 'h': optarg = &fn_hdr;               break;
          default : error(E_OPTION, *--s);          break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_rs  = s;      break;
        case  1: fn_tab = s;      break;
        case  2: fn_out = s;      break;
        default: error(E_ARGCNT); break;
      }                         /* note file names */
    }
  }
  if (optarg) error(E_OPTARG);  /* check the option argument and */
  if ((k < 2) || (k > 3)) error(E_ARGCNT); /* the number of args */
  if (fn_hdr && (strcmp(fn_hdr, "-") == 0))
    fn_hdr = "";                /* convert "-" to "" for consistency */
  i = (!fn_rs || !*fn_rs) ? 1 : 0;
  if  (fn_tab && !*fn_tab) i++;
  if  (fn_hdr && !*fn_hdr) i++; /* check assignments of stdin: */
  if (i > 1) error(E_STDIN);    /* stdin must not be used twice */
  if ((mout & AS_ATT) && (mout & AS_ALIGN))
    mout |= AS_ALNHDR;          /* set align to header flag */
  if (fn_out) mout |= AS_MARKED|AS_INFO1|AS_RDORD;
  else        mout  = 0;        /* set up the table write mode */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- read rule set --- */
  attset = as_create("domains", att_delete);
  if (!attset) error(E_NOMEM);  /* create an attribute set */
  scan = scn_create();          /* create a scanner */
  if (!scan)   error(E_NOMEM);  /* for the domain file */
  t = clock();                  /* start timer, open input file */
  if (scn_open(scan, NULL, fn_rs) != 0)
    error(E_FOPEN, scn_name(scan));
  fprintf(stderr, "reading %s ... ", scn_name(scan));
  if ((as_parse(attset, scan, AT_ALL, 1) != 0)
  ||  !(rulset = rs_parse(attset, scan))
  ||  !scn_eof(scan, 1))        /* parse the rule set */
    error(E_PARSE, scn_name(scan));
  scn_delete(scan, 1);          /* delete the scanner and */
  scan = NULL;                  /* clear the scanner variable */
  y = rs_rulecnt(rulset);       /* check whether the rule set */
  if (y < 0) error(E_EMPTY);    /* contains at least one rule */
  c = r_headatt(rs_rule(rulset, 0));
  for (x = y; --x >= 0; ) {     /* check for a unique head attribute */
    if (r_headatt(rs_rule(rulset, x)) != c) break; }
  if (x >= 0) error(E_HEAD);    /* (otherwise execution is imposs.) */
  m = as_attcnt(attset);        /* get the number of attributes */
  fprintf(stderr, "[%"ATTID_FMT"+1 attribute(s), ", m-1);
  fprintf(stderr, " %"RULEID_FMT" rule(s)]",        y);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
  if (adapt)                    /* if to adapt support and confidence */
    for (x = 0; x < y; x++)     /* clear support and confidence */
      r_clear(rs_rule(rulset, x));

  /* --- get target attribute --- */
  res.att  = as_att(attset, c); /* get the target attribute */
  res.type = att_type(res.att); /* and its type (and binary flag) */
  res.bin  = (res.type == AT_NOM) && (att_valcnt(res.att) == 2);
  as_setmark(attset, 1);        /* mark all attributes */
  att_setmark(res.att, 0);      /* except the target attribute */

  /* --- read table header --- */
  tread = trd_create();         /* create a table reader and */
  if (!tread) error(E_NOMEM);   /* configure the characters */
  trd_allchs(tread, recseps, fldseps, blanks, nullchs, comment);
  if (fn_hdr) {                 /* if a header file is given */
    t = clock();                /* start timer, open input file */
    if (trd_open(tread, NULL, fn_hdr) != 0)
      error(E_FOPEN, trd_name(tread));
    fprintf(stderr, "reading %s ... ", trd_name(tread));
    k = as_read(attset, tread, (mode & ~AS_DFLT) | AS_ATT);
    if (k < 0) error(-k, as_errmsg(attset, NULL, 0));
    trd_close(tread);           /* read table header, close file */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s)]", as_attcnt(attset));
    fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
    mode &= ~(AS_ATT|AS_DFLT);  /* print a success message and */
  }                             /* remove the attribute flag */

  /* --- process table body --- */
  t = clock();                  /* start timer, open input file */
  if (trd_open(tread, NULL, fn_tab) != 0)
    error(E_FOPEN, trd_name(tread));
  fprintf(stderr, "reading %s ... ", trd_name(tread));
  if (mout & AS_ALIGN) {        /* if to align the output columns */
    table = tab_create("table", attset, tpl_delete);
    if (!table) error(E_NOMEM); /* read the data table */
    k = tab_read(table, tread, mode);
    if (k < 0) error(-k, tab_errmsg(table, NULL, 0));
    trd_delete(tread, 1);       /* delete the table reader */
    tread = NULL;               /* and clear the variable */
    m = tab_attcnt(table);      /* get the number of attributes */
    n = tab_tplcnt(table);      /* and the number of tuples */
    w = tab_tplwgt(table);      /* and print a success message */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
    fprintf(stderr, " %"TPLID_FMT, n);
    if (w != (double)n) fprintf(stderr, "/%g", w);
    fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));
    if (!adapt) {               /* if not to adapt the rule set */
      twrite = twr_create();    /* create a table writer and */
      if (!twrite) error(E_NOMEM);   /* configure characters */
      twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
      t = clock();              /* start timer, open output file */
      if (twr_open(twrite, NULL, fn_out) != 0)
        error(E_FOPEN, twr_name(twrite));
      fprintf(stderr, "writing %s ... ", twr_name(twrite));
      if ((mout & AS_ATT)       /* write a table header */
      &&  (as_write(attset, twrite, mout, infout) != 0))
        error(E_FWRITE, twr_name(twrite));
      mout = AS_INST | (mout & ~AS_ATT);
    }                           /* switch to instance output */
    for (r = 0; r < n; r++) {   /* traverse the tuples */
      tpl_toas(tab_tpl(table, r));
      predict(thresh);          /* compute prediction for target */
      u = as_getwgt(attset);    /* get the tuple weight and */
      errs += res.err *u;       /* count the classification errors */
      if (!adapt) {             /* if to predict the target */
        if (as_write(attset, twrite, mout, infout) != 0)
          error(E_FWRITE, twr_name(twrite)); }
      else if (res.rule) {      /* if to adapt the rule set */
        r_setsupp(res.rule, res.supp +u);
        r_setconf(res.rule, res.conf +res.err);
      }                         /* update support and confidence */
    } }
  else {                        /* if to process tuples directly */
    k = as_read(attset, tread, mode); /* read/generate table header */
    if (k < 0) error(-k, as_errmsg(attset, NULL, 0));
    if (!fn_out && (att_getmark(res.att) < 0))
      error(E_OUTPUT);          /* check for output to produce */
    if (fn_out && !adapt) {     /* if to write an output file */
      twrite = twr_create();    /* create a table writer and */
      if (!twrite) error(E_NOMEM);   /* configure characters */
      twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
      t = clock();              /* start timer, open output file */
      if (twr_open(twrite, NULL, fn_out) != 0)
        error(E_FOPEN, twr_name(twrite));
      if ((mout & AS_ATT)       /* write a table header */
      &&  (as_write(attset, twrite, mout, infout) != 0))
        error(E_FWRITE, twr_name(twrite));
      mout = AS_INST | (mout & ~AS_ATT);
    }                           /* remove the attribute flag */
    i = mode; mode = (mode & ~(AS_DFLT|AS_ATT)) | AS_INST;
    if (i & AS_ATT)             /* if not done yet, read first tuple */
      k = as_read(attset, tread, mode);
    for (w = 0, n = 0; k == 0; n++) {
      predict(thresh);          /* compute prediction for target */
      w    += u = as_getwgt(attset);   /* sum the tuple weights */
      errs += res.err *u;       /* count the misclassifications */
      if (twrite) {             /* if to predict the target */
        if (as_write(attset, twrite, mout, infout) != 0)
          error(E_FWRITE, twr_name(twrite)); }
      else if (res.rule) {      /* if to adapt the rule set */
        r_setsupp(res.rule, res.supp +u);
        r_setconf(res.rule, res.conf +res.err);
      }                         /* update support and confidence */
      k = as_read(attset, tread, mode);
    }                           /* try to read the next tuple */
    if (k < 0) error(-k, as_errmsg(attset, NULL, 0));
    trd_delete(tread, 1);       /* delete the table reader */
    tread = NULL;               /* and clear the variable */
  }
  if (twrite) {                 /* if an output file was written */
    if (twr_close(twrite) != 0) error(E_FWRITE, twr_name(twrite));
    twr_delete(twrite, 1);      /* close the output file and */
    twrite = NULL;              /* delete the table writer */
  }
  m = as_attcnt(attset)+1;      /* print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- write the adapted rule set --- */
  if (adapt) {                  /* if to adapt support and confidence */
    t = clock();                /* start the timer */
    fprintf(stderr, "updating rule set ... ");
    for (x = 0; x < y; x++) {   /* traverse the rules */
      rule = rs_rule(rulset, x);
      u    = r_getsupp(rule);   /* get and check */
      if (u <= 0) continue;     /* the new support of each rule */
      u = r_getconf(rule) /u;   /* compute and set new confidence */
      r_setconf(rule, (res.type == AT_NOM) ? u : sqrt(u));
    }                           /* compute and set the new confidence */
    m = as_attcnt(attset);      /* get the number of attributes */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
    fprintf(stderr, " %"RULEID_FMT" rule(s)]",     y);
    fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
    t = clock();                /* start timer, open output file */
    if (fn_out && (strcmp(fn_out, "-") == 0)) fn_out = "";
    if (fn_out && *fn_out) { out = fopen(fn_out, "w"); }
    else                   { out = stdout; fn_out = "<stdout>"; }
    if (!out) error(E_FOPEN, fn_out);
    fprintf(stderr, "writing %s ... ", fn_out);
    mout = RS_TITLE|RS_INFO|RS_CONF|RS_SUPP;
    if (as_desc(attset, out, AS_TITLE|AS_MARKED, maxlen) != 0)
      error(E_FWRITE, fn_out);  /* describe attribute domains */
    fputc('\n', out);           /* leave one line empty */
    if (rs_desc(rulset, out, mout, maxlen) != 0)
      error(E_FWRITE, fn_out);  /* describe the rule set */
    if (out && (((out == stdout) ? fflush(out) : fclose(out)) != 0))
      error(E_FWRITE, fn_out);  /* close the output file and */
    out = NULL;                 /* print a success message */
    m = as_attcnt(attset);      /* get the number of attributes */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
    fprintf(stderr, " %"RULEID_FMT" rule(s)]",     y);
    fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
  }                             /* print a success message */

  /* --- print error statistics --- */
  else if (att_getmark(res.att) >= 0) {  /* if the target is present */
    if (res.type != AT_NOM) {   /* if the target attribute is metric */
      fprintf(stderr, "sse: %g", errs);
      if (w > 0) {              /* if there was at least one tuple, */
        errs /= w;              /* compute mean squared error */
        fprintf(stderr, ", mse: %g, rmse: %g", errs, sqrt(errs));
      } }                       /* print some error measures */
    else {                      /* if the target attribute is nominal */
      fprintf(stderr, "%g error(s) ", errs);
      fprintf(stderr, "(%.2f%%)", (w > 0) ? 100*(errs/w) : 0);
    }                           /* print number of misclassifications */
    fputc('\n', stderr);        /* terminate the error statistics */
  }

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
