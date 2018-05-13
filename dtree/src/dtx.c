/*----------------------------------------------------------------------
  File    : dtx.c
  Contents: decision and regression tree execution
  Authors : Christian Borgelt
  History : 1997.08.01 file created (as dtc.c)
            1997.08.04 first version completed
            1997.08.28 output file made optional
            1997.08.30 classification field name made variable
            1998.01.02 column/field alignment corrected
            1998.01.11 null value characters (option -u) added
            1998.02.08 adapted to changed parse functions
            1998.03.13 no header in output file (option -w) added
            1998.06.23 adapted to modified attset functions
            1998.09.03 support and confidence (options -s/-c) added
            1999.02.09 input from stdin, output to stdout added
            1999.04.17 simplified using the new module 'io'
            2000.12.17 bug in alignment of support and confidence fixed
            2000.12.18 extended to regression trees
            2001.07.23 adapted to modified module scan
            2002.09.18 bug concerning missing target fixed
            2003.02.02 bug in alignment in connection with -d fixed
            2003.04.23 missing AS_MARKED added for second reading
            2003.08.16 slight changes in error message output
            2004.11.09 execution time output added
            2007.02.13 adapted to modified module attset
            2007.08.23 option -x for non-occurring value weight added
            2007.09.20 bug in threshold evaluation fixed (res.conf)
            2011.07.29 adapted to modified attset and utility modules
            2011.12.15 processing without table reading improved
            2013.06.08 missing flag AS_MARKED added to table read mode
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
#include "attset.h"
#ifndef TAB_READ
#define TAB_READ
#endif
#include "table.h"
#ifndef DT_PARSE
#define DT_PARSE
#endif
#include "dtree.h"
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
#define PRGNAME     "dtx"
#define DESCRIPTION "decision and regression tree execution"
#define VERSION     "version 4.3 (2016.11.11)         " \
                    "(c) 1997-2016   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */
#define E_PARSE     (-9)        /* parse error */
#define E_TARGET   (-10)        /* missing target */
#define E_OUTPUT   (-11)        /* class in input or write output */

#define SEC_SINCE(t)  ((double)(clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- prediction result --- */
  ATT    *att;                  /* target attribute */
  int    type;                  /* type of the target attribute */
  int    bin;                   /* flag for binary target attribute */
  INST   pred;                  /* predicted value */
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
  /*           -12 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR    *prgname;       /* program name for error messages */
static SCANNER  *scan   = NULL; /* scanner (for decision tree) */
static TABREAD  *tread  = NULL; /* table reader */
static TABWRITE *twrite = NULL; /* table writer */
static ATTSET   *attset = NULL; /* attribute set */
static TABLE    *table  = NULL; /* data table */
static DTREE    *dtree  = NULL; /* decision/regression tree */
static RESULT   res     = {     /* prediction result */
  NULL, AT_NOM, 0,              /* target attribute and its type */
  {0}, "dt", 0, 6,              /* data for prediction column */
  0.0, NULL, 0, 1,              /* data for support    column */
  0.0, NULL, 0, 3,              /* data for confidence column */
  0 };                          /* error value (squared difference) */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (dtree)  dt_delete(dtree, 0);   \
  if (attset) as_delete(attset);     \
  if (table)  tab_delete(table,  0); \
  if (tread)  trd_delete(tread,  1); \
  if (twrite) twr_delete(twrite, 1); \
  if (scan)   scn_delete(scan,   1);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

static void predict (double thresh, double weight)
{                               /* --- classify the current tuple */
  INST *inst;                   /* to access the target instance */

  assert(dtree);                /* check for a dec./reg. tree */
  dt_exec(dtree, NULL, weight, &res.pred, &res.supp, &res.conf);
  inst = att_inst(res.att);     /* execute decision/regression tree */
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
  CCHAR   *fn_dt   = NULL;      /* name of dec./reg. tree file */
  CCHAR   *fn_out  = NULL;      /* name of output file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  double  thresh   = 0.5;       /* classification threshold */
  double  novwgt   = 1e-12;     /* weight for non-occurring values */
  int     mode     = AS_ATT|AS_MARKED; /* table file read  mode */
  int     mout     = AS_ATT;           /* table file write mode */
  double  errs     = 0.0;       /* number of misclassifications */
  TUPLE   *tpl;                 /* to traverse the data tuples */
  ATTID   m, h, z;              /* number of attributes */
  TPLID   n;                    /* number of data tuples */
  double  w, u;                 /* weight of data tuples */
  clock_t t;                    /* timer for measurements */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] dtfile [-d|-h hdrfile] "
                     "tabfile [outfile]\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
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
    printf("-x#      weight for non-occurring values        "
                    "(default: %g)\n", novwgt);
    printf("         (negative: treat values as if they were null)\n");
    printf("-a       align fields in output table           "
                    "(default: single separator)\n");
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
    printf("dtfile   file containing decision/regression "
                    "tree description\n");
    printf("-d       use default header "
                    "(attribute names = field numbers)\n");
    printf("-h       read table header  "
                    "(attribute names) from hdrfile\n");
    printf("hdrfile  file containing table header "
                    "(attribute names)\n");
    printf("tabfile  table file to read "
                    "(attribute names in first record)\n");
    printf("outfile  file to write output table to (optional)\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 'p': optarg = &res.col_pred; break;
          case 's': optarg = &res.col_supp; break;
          case 'c': optarg = &res.col_conf; break;
          case 'o': res.dig_pred = (int)strtol(s, &s, 0); break;
          case 'y': res.dig_supp = (int)strtol(s, &s, 0); break;
          case 'z': res.dig_conf = (int)strtol(s, &s, 0); break;
          case 't': thresh = strtod(s, &s); break;
          case 'x': novwgt = strtod(s, &s); break;
          case 'a': mout  |=  AS_ALIGN;     break;
          case 'w': mout  &= ~AS_ATT;       break;
          case 'r': optarg = &recseps;      break;
          case 'f': optarg = &fldseps;      break;
          case 'b': optarg = &blanks;       break;
          case 'u': optarg = &nullchs;      break;
          case 'C': optarg = &comment;      break;
          case 'n': mout  |= AS_WEIGHT;
                    mode  |= AS_WEIGHT;     break;
          case 'd': mode  |= AS_DFLT;       break;
          case 'h': optarg = &fn_hdr;       break;
          default : error(E_OPTION, *--s);  break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_dt  = s;      break;
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
  i = (!fn_dt || !*fn_dt) ? 1 : 0;
  if  (fn_tab && !*fn_tab) i++;
  if  (fn_hdr && !*fn_hdr) i++; /* check assignments of stdin: */
  if (i > 1) error(E_STDIN);    /* stdin must not be used twice */
  if ((mout & AS_ATT) && (mout & AS_ALIGN))
    mout |= AS_ALNHDR;          /* set align to header flag */
  if (fn_out) mout |= AS_MARKED|AS_INFO1|AS_RDORD;
  else        mout  = 0;        /* set up the table write mode */
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

  /* --- get target attribute --- */
  res.att  = dt_target(dtree);  /* get the target attribute */
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
    twrite = twr_create();      /* create a table writer and */
    if (!twrite) error(E_NOMEM);/* configure the characters */
    twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
    t = clock();                /* start timer, open output file */
    if (twr_open(twrite, NULL, fn_out) != 0)
      error(E_FOPEN, twr_name(twrite));
    fprintf(stderr, "writing %s ... ", twr_name(twrite));
    if ((mout & AS_ATT)         /* write a table header */
    &&  (as_write(attset, twrite, mout, infout) != 0))
      error(E_FWRITE, twr_name(twrite));
    mout = AS_INST | (mout & ~AS_ATT);
    m += (res.col_supp ? 2 : 1) +(res.col_conf ? 1 : 0);
    for (i = 0; i < n; i++) {   /* traverse the tuples */
      tpl = tab_tpl(table, i);  /* get the next tuple and */
      tpl_toas(tpl);            /* copy it to the attribute set */
      predict(thresh, novwgt);  /* compute prediction for target */
      u = as_getwgt(attset);    /* get the tuple weight and */
      errs += res.err *u;       /* count the classification errors */
      if (as_write(attset, twrite, mout, infout) != 0)
        error(E_FWRITE, twr_name(twrite));
    } }                         /* write the current tuple */
  else {                        /* if to process tuples directly */
    k = as_read(attset, tread, mode); /* read/generate table header */
    if (k < 0) error(-k, as_errmsg(attset, NULL, 0));
    if (!fn_out && (att_getmark(res.att) < 0))
      error(E_OUTPUT);          /* check for output to produce */
    if (fn_out) {               /* if to write an output file */
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
      predict(thresh, novwgt);  /* compute prediction for target */
      w    += u = as_getwgt(attset);  /* sum the tuple weights and */
      errs += res.err *u;       /* count the classification errors */
      if (twrite                /* write the current tuple */
      && (as_write(attset, twrite, mout, infout) != 0))
        error(E_FWRITE, twr_name(twrite));
      k = as_read(attset, tread, mode);
    }                           /* try to read the next tuple */
    if (k < 0) error(-k, as_errmsg(attset, NULL, 0));
    trd_delete(tread, 1);       /* delete the table reader */
    tread = NULL;               /* and clear the variable */
    m = as_attcnt(attset);      /* get the number of attributes */
  }
  if (twrite) {                 /* if an output file was written */
    if (twr_close(twrite) != 0) error(E_FWRITE, twr_name(twrite));
    twr_delete(twrite, 1);      /* close the output file and */
    twrite = NULL;              /* delete the table writer */
  }                             /* print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- print error statistics --- */
  if (att_getmark(res.att) >= 0) {       /* if the target is present */
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
