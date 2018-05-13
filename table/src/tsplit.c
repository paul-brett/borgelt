/*----------------------------------------------------------------------
  File    : tsplit.c
  Contents: program to split table into subtables
  Author  : Christian Borgelt
  History : 1998.02.24 file created
            1998.09.12 adapted to modified module attset
            1998.09.25 table reading simplified
            1999.02.06 arbitrary sample size made possible
            1999.02.07 input from stdin, output to stdout added
            1999.02.12 default header handling improved
            1999.04.17 simplified using the new module 'io'
            2001.07.14 adapted to modified module tabread
            2003.08.16 slight changes in error message output
            2007.02.13 adapted to redesigned module attset
            2010.10.08 adapted to modules tabread and tabwrite
            2010.12.12 adapted to a generic error reporting function
            2011.01.05 adapted to functions tab_read() and tab_write()
            2013.07.21 adapted to definitions ATTID, VALID, WEIGHT etc.
            2013.07.26 adapted to additional parameter of tab_sort()
            2014.10.24 changed from LGPL license to MIT license
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#ifndef TAB_READ
#define TAB_READ
#endif
#ifndef TAB_WRITE
#define TAB_WRITE
#endif
#include "table.h"
#include "random.h"
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
#define PRGNAME     "tsplit"
#define DESCRIPTION "split a table into subtables"
#define VERSION     "version 2.2 (2015.12.03)         " \
                    "(c) 1998-2015   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */
#define E_UNKATT    (-9)        /* unknown attribute */
#define E_EMPTAB   (-10)        /* empty table */
#define E_SMLTAB   (-11)        /* table too small for sample */

#define SEC_SINCE(t)  ((double)(clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
static const char *errmsgs[] = {   /* error messages */
  /* E_NONE      0 */  "no error",
  /* E_NOMEM    -1 */  "not enough memory",
  /* E_FOPEN    -2 */  "cannot open file %s",
  /* E_FREAD    -3 */  "read error on file %s",
  /* E_FWRITE   -4 */  "write error on file %s",
  /* E_STDIN    -5 */  "double assignment of standard input",
  /* E_OPTION   -6 */  "unknown option -%c",
  /* E_OPTARG   -7 */  "missing option argument",
  /* E_ARGCNT   -8 */  "wrong number of arguments",
  /* E_UNKATT   -9 */  "invalid attribute '%s'",
  /* E_EMPTAB  -10 */  "table is empty",
  /* E_SMLTAB  -11 */  "table is too small for sample",
  /*           -12 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR    *prgname;       /* program name for error messages */
static TABREAD  *tread  = NULL; /* table reader */
static TABWRITE *twrite = NULL; /* table writer */
static ATTSET   *attset = NULL; /* attribute set */
static TABLE    *table  = NULL; /* data table */
static char     fn_out[1024];   /* output file name */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (table)  tab_delete(table, 0);  \
  if (attset) as_delete (attset);    \
  if (tread)  trd_delete(tread,  1); \
  if (twrite) twr_delete(twrite, 1);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counters */
  char    *s;                   /* to traverse the options */
  CCHAR   **optarg = NULL;      /* to get option arguments */
  CCHAR   *fn_hdr  = NULL;      /* name of header file */
  CCHAR   *fn_tab  = NULL;      /* name of table  file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  CCHAR   *attname = NULL;      /* name of attribute to base split on */
  CCHAR   *pattern = "%i.tab";  /* output file name pattern */
  int     mode     = AS_ATT;    /* table file read  mode */
  int     mout     = AS_ATT;    /* table file write mode */
  int     shuffle  =  0;        /* flag for tuple shuffling */
  TPLID   sample   =  0;        /* flag for drawing a sample */
  ATTID   colid    = -1;        /* column identifier */
  double  tabcnt   =  0;        /* number of tables */
  int     tabid    =  0;        /* table identifier */
  long    seed;                 /* random number seed */
  TPLID   size, first, tplid;   /* table size and tuple index */
  VALID   prev, val;            /* (previous) column value */
  double  off, x, y;            /* offset, tuple weight and buffer */
  int     one_in_n;             /* flag for one in n selection */
  int     done;                 /* completion flag */
  TUPLE   *tpl;                 /* tuple to traverse table */
  TPLID   n;                    /* number of data tuples */
  double  w;                    /* weight of data tuples */
  clock_t t;                    /* timer for measurement */

  prgname = argv[0];            /* get program name for error msgs. */
  seed    = (long)time(NULL);   /* and get a default seed value */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] [-d|-h hdrfile] tabfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-c#      name of attribute to base split on     "
                    "(default: none)\n");
    printf("         (stratified sampling if the option -t "
                    "is also given)\n");
    printf("-x       shuffle tuples before split operation  "
                    "(default: as in input)\n");
    printf("-s#      seed value for random number generator "
                    "(default: time)\n");
    printf("-t#      number of subtables to split into      "
                    "(default: number of values)\n");
    printf("-p#      draw a sample with # tuples "
                    "(one output table)\n");
    printf("-z#      start value for table identifier       "
                    "(default: %d)\n", tabid);
    printf("-o#      name pattern for output file(s)        "
                    "(default: \"%s\")\n", pattern);
    printf("-a       align fields of output table(s)        "
                    "(default: do not align)\n");
    printf("-w       do not write attribute names to output files\n");
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
    printf("-n       number of tuple occurences in last field\n");
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
      while (1) {               /* traverse options */
        switch (*s++) {         /* evaluate option */
          case 'c': optarg  = &attname;                break;
          case 'x': shuffle = 1;                       break;
          case 's': seed    =        strtol(s, &s, 0); break;
          case 't': tabcnt  =        strtod(s, &s);    break;
          case 'p': sample  = (TPLID)strtol(s, &s, 0); break;
          case 'o': optarg  = &pattern;                break;
          case 'a': mout   |= AS_ALIGN;                break;
          case 'w': mout   &= ~AS_ATT;                 break;
          case 'r': optarg  = &recseps;                break;
          case 'f': optarg  = &fldseps;                break;
          case 'b': optarg  = &blanks;                 break;
          case 'u': optarg  = &nullchs;                break;
          case 'C': optarg  = &comment;                break;
          case 'n': mout   |= AS_WEIGHT;
                    mode   |= AS_WEIGHT;               break;
          case 'd': mode   |= AS_DFLT;                 break;
          case 'h': optarg  = &fn_hdr;                 break;
          default : error(E_OPTION, *--s);             break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_tab = s;      break;
        default: error(E_ARGCNT); break;
      }                         /* note filenames */
    }
  }
  if (optarg) error(E_OPTARG);  /* check option argument */
  if (k != 1) error(E_ARGCNT);  /* check number of arguments */
  if (fn_hdr && (strcmp(fn_hdr, "-") == 0))
    fn_hdr = "";                /* convert "-" to "" for consistency */
  i = ( fn_hdr && !*fn_hdr) ? 1 : 0;
  if  (!fn_tab || !*fn_tab) i++;/* check assignments of stdin: */
  if (i > 1) error(E_STDIN);    /* stdin must not be used twice */
  if ((mout & AS_ALIGN) && (mout & AS_ATT))
    mout |= AS_ALNHDR;          /* set align to header flag */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- read data table --- */
  attset = as_create("domains", att_delete);
  if (!attset) error(E_NOMEM);  /* create an attribute set */
  tread = trd_create();         /* create a table reader and */
  if (!tread)  error(E_NOMEM);  /* configure the characters */
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
  }                             /* remove the attribute flags */
  table = tab_create("table", attset, tpl_delete);
  if (!table) error(E_NOMEM);   /* create a data table */
  t = clock();                  /* start timer, open input file */
  if (trd_open(tread, NULL, fn_tab) != 0)
    error(E_FOPEN, trd_name(tread));
  fprintf(stderr, "reading %s ... ", trd_name(tread));
  if (!fn_hdr                   /* if the attribute names are in the */
  && (mode & (AS_ATT|AS_DFLT))) {     /* table file or default names */
    k = tab_read(table, tread, mode | TAB_ONE);
    if (k < 0) error(-k, tab_errmsg(table, NULL, 0));
    mode &= ~(AS_ATT|AS_DFLT);  /* read the first table record */
  }                             /* and remove the attribute flag */
  if (attname) {                /* if an attribute name is given */
    colid = as_attid(attset, attname);
    if (colid < 0) error(E_UNKATT, attname);
  }                             /* get the attribute identifier */
  k = tab_read(table, tread, mode);
  if (k < 0) error(-k, tab_errmsg(table, NULL, 0));
  trd_delete(tread, 1);         /* read the table body and */
  tread = NULL;                 /* delete the table reader */
  n = tab_tplcnt(table);        /* get the number of tuples */
  w = tab_tplwgt(table);        /* and print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", tab_attcnt(table));
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].", SEC_SINCE(t));
  if (n <= 0)      error(E_EMPTAB);
  if (n <  sample) error(E_SMLTAB);
  fputc('\n', stderr);          /* check the table size */

  /* --- split table --- */
  if (shuffle) {                /* if the shuffle flag is set, */
    rseed((unsigned)seed);      /* init. random number generator */
    tab_shuffle(table, 0, n, drand);
  }                             /* shuffle tuples in table */
  if (colid >= 0)               /* sort table w.r.t. given column */
    tab_sort(table, 0, n, +1, tpl_cmp1, &colid);
  if (sample > 0)               /* compute equiv. number of tables */
    tabcnt = ((double)n /(double)sample) *(1 +1e-12);
  one_in_n = ((colid < 0) || (tabcnt > 0));
  if (tabcnt <= 0) tabcnt = 1;  /* get tuple selection mode */
  twrite = twr_create();        /* create a table writer and */
  if (!twrite) error(E_NOMEM);  /* configure the characters */
  twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
  val   = NV_NOM;               /* clear current and get first value */
  prev  = (colid >= 0) ? tpl_colval(tab_tpl(table, 0), colid)->n : val;
  size  = n;                    /* note number of tuples in table, */
  first = tplid = 0;            /* initialize tuple and table index */
  for (done = 0; !done; ) {     /* table write loop */
    if (!*pattern) *fn_out = 0; /* get (next) output file name */
    else snprintf(fn_out, sizeof(fn_out), pattern, tabid++);
    t = clock();                /* start timer, open output file */
    if (twr_open(twrite, NULL, fn_out) != 0)
      error(E_FOPEN, twr_name(twrite));
    fprintf(stderr, "writing %s ... ", twr_name(twrite));
    if ((mout & AS_ATT)         /* write the attribute names */
    &&  (as_write(attset, twrite, mout) != 0))
      error(E_FWRITE, twr_name(twrite));
    k = AS_INST | (mout & ~AS_ATT);
    w = 0; n = 0;               /* initialize tuple counter */
    if (one_in_n) {             /* if to select every n-th tuple */
      tplid = 0; off = (double)first;   /* get next tuple offset */
      if ((++first >= tabcnt) || sample)
        done = 1;               /* if last table, set done flag */
      while (tplid < size) {    /* while not at end of table */
        tpl = tab_tpl(table, tplid++);
        x   = tpl_getwgt(tpl);  /* get next tuple and its weight */
        if (x <= off) {         /* if offset is larger than weight, */
          off -= x; continue; } /* skip this tuple (do not select) */
        tpl_toas(tpl);          /* transfer tuple to attribute set */
        y = ceil((x -off) /tabcnt);   /* compute and set weight and */
        as_setwgt(attset, (WEIGHT)y); /* write instantiation (tuple) */
        if (as_write(attset, twrite, k) != 0)
          error(E_FWRITE, twr_name(twrite));
        n++; w += y;            /* count the written tuple */
        off = fmod(off +y *tabcnt -x, tabcnt);
      } }                       /* compute next tuple offset */
    else {                      /* if to split according to values */
      while (tplid < size) {    /* while not all tuples processed */
        tpl = tab_tpl(table, tplid);      /* get next tuple and */
        val = tpl_colval(tpl, colid)->n;  /* its column value */
        if (val != prev) break; /* if next value reached, abort */
        tpl_toas(tpl);          /* transfer tuple to attribute set */
        if (as_write(attset, twrite, k) != 0)
          error(E_FWRITE, twr_name(twrite));
        w += as_getwgt(attset); /* write instantiation (tuple) */
        n++; tplid++;           /* and increment tuple counter */
      }                         /* and tuple index */
      if (tplid >= size) done = 1;
      prev = val;               /* check for completion and */
    }                           /* note the current column value */
    if ((twr_file(twrite) == stdout) && !done)
      printf("\n");             /* separate tables by an empty line */
    if (twr_close(twrite) != 0) /* close the output file and */
      error(E_FWRITE, fn_out);  /* print a success message */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s),", tab_attcnt(table));
    fprintf(stderr, " %"TPLID_FMT, n);
    if (w != (double)n) fprintf(stderr, "/%g", w);
    fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));
  }                             /* while not all tables written */
  twr_delete(twrite, 1);        /* delete the table writer */
  twrite = NULL;                /* clear the writer variable */

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
