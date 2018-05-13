/*----------------------------------------------------------------------
  File    : attset.h
  Contents: attribute set management
  Author  : Christian Borgelt
  History : 1995.10.25 file created
            1995.11.03 functions as_write() and as_read() added
            1995.12.21 function att_valsort() added (sort values)
            1996.03.17 attribute types added (AT_NOM, AT_INT etc.)
            1996.07.01 functions att_valmin() and att_valmax() added
            1996.07.04 attribute weights added (floating-point)
            1996.07.24 definitions of null values added (NV_NOM etc.)
            1997.02.25 function as_info() added (add. information)
            1997.03.12 attribute markers added (for writing etc.)
            1997.03.28 function as_get/setwgt() added (inst. weight)
            1997.08.01 restricted describe added (using att. markers)
            1997.08.02 additional information output added (AS_INFO)
            1997.09.26 error code added to internal error information
            1998.02.08 function as_parse() transferred from parser.h
            1998.03.18 function att_info() added (add. information)
            1998.06.22 deletion function moved to function as_create()
            1998.06.23 major redesign, attribute functions introduced
            1998.08.19 typedef for attribute values (VAL) added
            1998.08.22 attribute set names and some functions added
            1998.08.30 params. 'map' and 'mapdir' added to att_valsort()
            1998.09.02 instance (current value) moved to attribute
            1998.09.06 second major redesign completed
            1998.09.12 deletion function parameter changed to ATT
            1998.09.14 attribute selection in as_write/as_desc improved
            1998.09.17 attribute selection improvements completed
            1998.09.24 parameter 'map' added to function att_convert()
            1988.09.25 function as_attaddm() added (add multiple atts.)
            1998.11.25 fucntions att_valcopy() and as_attcopy() added
            1998.11.29 functions att_clone() and as_clone() added
            1999.02.04 long int changed to int (assuming 32 bit system)
            1999.04.17 definitions of AS_NOXATT and AS_NOXVAL added
            2000.11.22 function sc_form() moved to module scan/escape
            2001.05.13 definition of AS_NONULL added (no null values)
            2004.08.12 adapted to new module parse (desc. parsing)
            2007.02.13 adapted to redesigned module tabread
            2007.02.17 attribute directions added (DIR_IN, DIR_OUT etc.)
            2010.10.07 adapted to modified module tabread
            2010.10.08 function as_write() adapted to module tabwrite
            2010.11.15 functions as_save() and as_restore() added
            2010.12.10 function as_errmsg() added (read error messages)
            2011.02.09 parameter 'mand' added to function as_parse()
            2013.07.18 definitions of ATTID, VALID, DTINT, DTFLT added
            2013.07.19 utility functions for conversions added
            2013.07.26 parameter 'dir' added to function att_valsort()
            2013.08.29 function as_target() added (target detection)
            2015.08.01 function as_attperm() added (permute attributes)
----------------------------------------------------------------------*/
#ifndef __ATTSET__
#define __ATTSET__
#include <stddef.h>
#include <stdint.h>
#ifdef AS_DESC
#include <stdio.h>
#endif
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#ifdef AS_READ
#include "tabread.h"
#endif
#ifdef AS_WRITE
#include "tabwrite.h"
#endif
#ifdef AS_PARSE
#define SCN_SCAN
#endif
#include "scanner.h"

#ifdef _MSC_VER
#ifndef INFINITY
#define INFINITY    (DBL_MAX+DBL_MAX)
#endif
#ifndef NAN
#define NAN         (INFINITY-INFINITY)
#endif
#ifndef isnan
#define isnan(x)    _isnan(x)
#endif                          /* check for 'not a number' */
#endif                          /* MSC still does not support C99 */

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#ifdef EXTDATA

#ifndef ATTID
#define ATTID       ptrdiff_t   /* attribute identifier */
#endif

#ifndef VALID
#define VALID       ptrdiff_t   /* attribute value identifier */
#endif

#ifndef DTINT
#define DTINT       ptrdiff_t   /* data type: integer */
#endif

#ifndef DTFLT
#define DTFLT       double      /* data type: floating-point */
#endif

#ifndef WEIGHT
#define WEIGHT      double      /* attribute or instance weight */
#endif

#else

#ifndef ATTID
#define ATTID       int         /* attribute identifier */
#endif

#ifndef VALID
#define VALID       int         /* attribute value identifier */
#endif

#ifndef DTINT
#define DTINT       int         /* data type: integer */
#endif

#ifndef DTFLT
#define DTFLT       float       /* data type: floating-point */
#endif

#ifndef WEIGHT
#define WEIGHT      float       /* attribute or instance weight */
#endif

#endif
/*--------------------------------------------------------------------*/

#define int         1           /* to check definitions */
#define long        2           /* for certain types */
#define ptrdiff_t   3
#define float       4
#define double      5

/*--------------------------------------------------------------------*/

#if   ATTID==int
#ifndef ATTID_MAX
#define ATTID_MAX   INT_MAX     /* maximum attribute identifier */
#endif
#ifndef ATTID_FMT
#define ATTID_FMT   "d"         /* printf format code for int */
#endif
#ifndef aid_qsort
#define aid_qsort   int_qsort
#endif

#elif ATTID==long
#ifndef ATTID_MAX
#define ATTID_MAX   LONG_MAX    /* maximum attribute identifier */
#endif
#ifndef ATTID_FMT
#define ATTID_FMT   "ld"        /* printf format code for long */
#endif
#ifndef aid_qsort
#define aid_qsort   lng_qsort
#endif

#elif ATTID==ptrdiff_t
#ifndef ATTID_MAX
#define ATTID_MAX   PTRDIFF_MAX /* maximum attribute identifier */
#endif
#ifndef ATTID_FMT
#  ifdef _MSC_VER
#  define ATTID_FMT "Id"        /* printf format code for ptrdiff_t */
#  else
#  define ATTID_FMT "td"        /* printf format code for ptrdiff_t */
#  endif                        /* MSC still does not support C99 */
#endif
#ifndef aid_qsort
#define aid_qsort   dif_qsort
#endif

#else
#error "ATTID must be either 'int', 'long' or 'ptrdiff_t'"
#endif

/*--------------------------------------------------------------------*/

#if   VALID==int
#ifndef VALID_MAX
#define VALID_MAX   INT_MAX     /* maximum value identifier */
#endif
#ifndef VALID_FMT
#define VALID_FMT   "d"         /* printf format code for int */
#endif
#ifndef vid_qsort
#define vid_qsort   int_qsort
#endif

#elif VALID==long
#ifndef VALID_MAX
#define VALID_MAX   LONG_MAX    /* maximum value identifier */
#endif
#ifndef VALID_FMT
#define VALID_FMT   "ld"        /* printf format code for long */
#endif
#ifndef vid_qsort
#define vid_qsort   lng_qsort
#endif

#elif VALID==ptrdiff_t
#ifndef VALID_MAX
#define VALID_MAX   PTRDIFF_MAX /* maximum value identifier */
#endif
#ifndef VALID_FMT
#  ifdef _MSC_VER
#  define VALID_FMT "Id"        /* printf format code for ptrdiff_t */
#  else
#  define VALID_FMT "td"        /* printf format code for ptrdiff_t */
#  endif                        /* MSC still does not support C99 */
#endif
#ifndef vid_qsort
#define vid_qsort   dif_qsort
#endif

#else
#error "VALID must be either 'int', 'long' or 'ptrdiff_t'"
#endif

/*--------------------------------------------------------------------*/

#if   DTINT==int
#ifndef DTINT_MIN
#define DTINT_MIN   (-INT_MAX)  /* minimum integer number */
#endif
#ifndef DTINT_MAX
#define DTINT_MAX   (+INT_MAX)  /* maximum integer number */
#endif
#ifndef DTINT_FMT
#define DTINT_FMT   "d"         /* printf format code for int */
#endif
#ifndef dti_qsort
#define dti_qsort   int_qsort
#endif

#elif DTINT==long
#ifndef DTINT_MIN
#define DTINT_MIN   (-LONG_MAX) /* minimum integer number */
#endif
#ifndef DTINT_MAX
#define DTINT_MAX   (+LONG_MAX) /* maximum integer number */
#endif
#ifndef DTINT_FMT
#define DTINT_FMT   "ld"        /* printf format code for long */
#endif
#ifndef dti_qsort
#define dti_qsort   lng_qsort
#endif

#elif DTINT==ptrdiff_t
#ifndef DTINT_MIN
#define DTINT_MIN   (-PTRDIFF_MAX)  /* minimum integer number */
#endif
#ifndef DTINT_MAX
#define DTINT_MAX   (+PTRDIFF_MAX)  /* maximum integer number */
#endif
#ifndef DTINT_FMT
#  ifdef _MSC_VER
#  define DTINT_FMT "Id"        /* printf format code for ptrdiff_t */
#  else
#  define DTINT_FMT "td"        /* printf format code for ptrdiff_t */
#  endif                        /* MSC still does not support C99 */
#endif
#ifndef dti_qsort
#define dti_qsort   dif_qsort
#endif

#else
#error "DTINT must be either 'int', 'long' or 'ptrdiff_t'"
#endif

/*--------------------------------------------------------------------*/

#if   DTFLT==float
#ifndef DTFLT_MIN
#define DTFLT_MIN   (-FLT_MAX)  /* minimum floating-point number */
#endif
#ifndef DTFLT_MAX
#define DTFLT_MAX   (+FLT_MAX)  /* maximum floating-point number */
#endif
#ifndef DTFLT_FMT
#define DTFLT_FMT   "g"         /* printf format code for float */
#endif
#ifndef dtf_qsort
#define dtf_qsort   flt_qsort
#endif

#elif DTFLT==double
#ifndef DTFLT_MIN
#define DTFLT_MIN   (-DBL_MAX)  /* minimum floating-point number */
#endif
#ifndef DTFLT_MAX
#define DTFLT_MAX   (+DBL_MAX)  /* maximum floating-point number */
#endif
#ifndef DTFLT_FMT
#define DTFLT_FMT   "g"         /* printf format code for double */
#endif
#ifndef dtf_qsort
#define dtf_qsort   dbl_qsort
#endif

#else
#error "DTFLT must be either 'float' or 'double'"
#endif

/*--------------------------------------------------------------------*/

#if   WEIGHT==float
#ifndef WGT_MIN
#define WGT_MIN     (-FLT_MAX)  /* minimum weight */
#endif
#ifndef WGT_MAX
#define WGT_MAX     (+FLT_MAX)  /* maximum weight */
#endif
#ifndef WGT_FMT
#define WGT_FMT     "g"         /* printf format code for float */
#endif
#ifndef wgt_qsort
#define wgt_qsort   flt_qsort
#endif

#elif WEIGHT==double
#ifndef WGT_MIN
#define WGT_MIN     (-DBL_MAX)  /* minimum weight */
#endif
#ifndef WGT_MAX
#define WGT_MAX     (+DBL_MAX)  /* maximum weight */
#endif
#ifndef WGT_FMT
#define WGT_FMT     "g"         /* printf format code for double */
#endif
#ifndef wgt_qsort
#define wgt_qsort   dbl_qsort
#endif

#else
#error "WEIGHT must be either 'float' or 'double'"
#endif

/*--------------------------------------------------------------------*/

#undef int                      /* remove preprocessor definitions */
#undef long                     /* needed for the type checking */
#undef ptrdiff_t
#undef float
#undef double

/*--------------------------------------------------------------------*/

#ifndef SIZE_FMT
#  ifdef _MSC_VER
#  define SIZE_FMT  "Iu"        /* printf format code for size_t */
#  else
#  define SIZE_FMT  "zu"        /* printf format code for size_t */
#  endif                        /* MSC still does not support C99 */
#endif

/*--------------------------------------------------------------------*/
#define CCHAR      const char   /* abbreviation */
#define CINST      const INST   /* ditto */

/* --- attribute types --- */
#define AT_NOM     0x0001       /* nominal (finite set of values) */
#define AT_INT     0x0002       /* integer number */
#define AT_FLT     0x0004       /* floating-point number */
#define AT_ALL     0x0007       /* all types (for function as_parse) */
#define AT_AUTO    (-1)         /* automatic type conversion */

/* --- null values --- */
#define NV_NOM     (-1)         /* null nominal value */
#define NV_INT     (DTINT_MIN-1)/* null integer value */
#define NV_FLT     ((DTFLT)NAN) /* null floating-point value */
#define isnone(i)  ((i) < 0)    /* check for a null value */
#define isnull(i)  ((i) < DTINT_MIN)
/*      isnan(f)   whether floating-point value is NaN */

/* --- attribute directions --- */
#define DIR_NONE   0            /* no direction */
#define DIR_ID     1            /* identifier */
#define DIR_IN     2            /* input  attribute */
#define DIR_OUT    4            /* output attribute */

/* --- cut/copy/read/write flags --- */
#define AS_ALL     0x0000       /* cut/copy all      atts./values */
#define AS_RANGE   0x0010       /* cut/copy range of atts./values */
#define AS_MARKED  0x0020       /* cut/copy marked   atts./values */

/* --- read/write flags --- */
#define AS_INST    0x0000       /* read/write instances */
#define AS_ATT     0x0001       /* read/write attributes */
#define AS_DFLT    0x0002       /* create default attribute names */
#define AS_NOXATT  0x0004       /* do not extend set of attributes */
#define AS_NOXVAL  0x0008       /* do not extend set of (nom.) values */
#define AS_NOEXT   (AS_NOXATT|AS_NOXVAL)   /* do not extend either */
#define AS_NONULL  0x0100       /* do not accept null values */
#define AS_RDORD   0x0200       /* write fields in read order */
#define AS_ALIGN   0x0400       /* align fields (pad with blanks) */
#define AS_ALNHDR  0x0800       /* align fields respecting a header */
#define AS_WEIGHT  0x1000       /* last field contains inst. weight */
#define AS_NONEG   0x2000       /* inst. weight must not be negative */
#define AS_INFO1   0x4000       /* write add. info. 1 (before weight) */
#define AS_INFO2   0x8000       /* write add. info. 2 (after  weight) */
/* also applicable: AS_ALL, AS_RANGE, AS_MARKED */

/* --- description flags --- */
#define AS_TITLE   0x0001       /* title with att. set name (comment) */
#define AS_IVALS   0x0002       /* intervals for numeric attributes */
#define AS_DIRS    0x0004       /* print attribute directions */
/* also applicable: AS_ALL, AS_RANGE, AS_MARKED, AS_WEIGHT */

/* --- sizes --- */
#define AS_MAXLEN    1024       /* maximum name length (<= MAX_INT) */

/* --- error codes --- */
#define E_NONE          0       /* no error */
#define E_NOMEM       (-1)      /* not enough memory */
#define E_FOPEN       (-2)      /* file open failed */
#define E_FREAD       (-3)      /* file read failed */
#define E_FWRITE      (-4)      /* file write failed */
#define E_STDIN       (-5)      /* double assignment of stdin */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef union {                 /* --- instance --- */
  VALID n;                      /* identifier for nominal value */
  DTINT i;                      /* integer number */
  DTFLT f;                      /* floating-point number */
} INST;                         /* (instance) */

typedef struct val {            /* --- attribute value --- */
  VALID      id;                /* identifier (index in attribute) */
  size_t     hash;              /* hash value of value name */
  struct val *succ;             /* successor in hash bucket */
  char       name[1];           /* value name */
} VAL;                          /* (attribute value) */

typedef int VAL_CMPFN (const char *name1, const char *name2);

typedef struct att {            /* --- attribute --- */
  char   *name;                 /* attribute name */
  int    type;                  /* attribute type, e.g. AT_NOM */
  int    dir;                   /* direction,      e.g. DIR_IN */
  WEIGHT wgt;                   /* weight, e.g. to indicate relevance */
  ATTID  mark;                  /* mark,   e.g. to indicate usage */
  int    read;                  /* read flag (used e.g. in as_read) */
  int    sd2p;                  /* significant digits to print */
  size_t size;                  /* size of value array */
  VALID  cnt;                   /* number of values in array */
  VAL    **vals;                /* value array (nominal attributes) */
  VAL    **htab;                /* hash table for values */
  INST   min, max;              /* minimal and maximal value/id */
  int    attwd[2];              /* attribute name widths */
  int    valwd[2];              /* maximum of value name widths */
  INST   inst;                  /* attribute instance (current value) */
  ATTID  id;                    /* identifier (index in att. set) */
  size_t hash;                  /* hash value of attribute name */
  struct attset *set;           /* containing attribute set (if any) */
  struct att    *succ;          /* successor in hash bucket */
} ATT;                          /* (attribute) */

typedef void ATT_DELFN (ATT *att);
typedef void ATT_APPFN (ATT *att, void *data);

typedef struct attset {         /* --- attribute set --- */
  char      *name;              /* name of attribute set */
  size_t    size;               /* size of attribute array */
  ATTID     cnt;                /* number of attributes in array */
  ATT       **atts;             /* attribute array */
  ATT       **htab;             /* hash table for attributes */
  ATT_DELFN *delfn;             /* attribute deletion function */
  WEIGHT    wgt;                /* weight (of current instantiation) */
  int       sd2p;               /* significant digits to print */
  ATTID     fldsize;            /* size of field array */
  ATTID     fldcnt;             /* number of fields */
  ATTID     *flds;              /* field array for as_read() */
  int       err;                /* error code (file reading) */
  CCHAR     *str;               /* error string (field name) */
  void      *trd;               /* table reader (untyped) */
} ATTSET;                       /* (attribute set) */

typedef void AS_DELFN (ATTSET *set);
#ifdef AS_WRITE
typedef void INFOUTFN (ATTSET *set, TABWRITE *twr, int mode);
#endif

/*----------------------------------------------------------------------
  Utility Functions
----------------------------------------------------------------------*/
extern DTFLT   asu_int2flt (DTINT x);
extern DTINT   asu_flt2int (DTFLT x);
extern DTINT   asu_str2int (const char *s);
extern DTFLT   asu_str2flt (const char *s);
extern WEIGHT  asu_str2wgt (const char *s);

/*----------------------------------------------------------------------
  Attribute Functions
----------------------------------------------------------------------*/
extern ATT*    att_create  (const char *name, int type);
extern ATT*    att_clone   (const ATT *att);
extern void    att_delete  (ATT *att);
extern int     att_rename  (ATT *att, const char *name);
extern int     att_convert (ATT *att, int type, INST *map);
extern int     att_cmp     (const ATT *att1, const ATT *att2);

extern CCHAR*  att_name    (const ATT *att);
extern int     att_type    (const ATT *att);
extern int     att_width   (const ATT *att, int scform);
extern int     att_setdir  (ATT *att, int dir);
extern int     att_getdir  (const ATT *att);
extern WEIGHT  att_setwgt  (ATT *att, WEIGHT wgt);
extern WEIGHT  att_getwgt  (const ATT *att);
extern ATTID   att_setmark (ATT *att, ATTID mark);
extern ATTID   att_getmark (const ATT *att);
extern int     att_setsd2p (ATT *att, int sd2p);
extern int     att_getsd2p (const ATT *att);
extern INST*   att_inst    (ATT *att);
extern ATTSET* att_attset  (ATT *att);
extern ATTID   att_id      (const ATT *att);

/*----------------------------------------------------------------------
  Attribute Value Functions
----------------------------------------------------------------------*/
extern int     att_valadd  (ATT *att, CCHAR *name, INST *inst);
extern void    att_valrem  (ATT *att, VALID valid);
extern void    att_valexg  (ATT *att, VALID valid1, VALID valid2);
extern void    att_valmove (ATT *att, VALID off, VALID cnt, VALID pos);
extern int     att_valcut  (ATT *dst, ATT *src, int mode, ...);
extern int     att_valcopy (ATT *dst, const ATT *src, int mode, ...);
extern void    att_valsort (ATT *att, int dir, VAL_CMPFN cmpfn,
                            VALID *map, int mapdir);

extern VALID   att_valid   (const ATT *att, const char *name);
extern CCHAR*  att_valname (const ATT *att, VALID valid);
extern VALID   att_valcnt  (const ATT *att);
extern int     att_valwd   (ATT *att, int scform);
extern CINST*  att_valmin  (const ATT *att);
extern CINST*  att_valmax  (const ATT *att);

/*----------------------------------------------------------------------
  Attribute Set Functions
----------------------------------------------------------------------*/
extern ATTSET* as_create   (const char *name, ATT_DELFN delfn);
extern ATTSET* as_clone    (const ATTSET *set);
extern void    as_delete   (ATTSET *set);
extern int     as_rename   (ATTSET *set, const char *name);
extern int     as_cmp      (const ATTSET *set1, const ATTSET *set2);

extern CCHAR*  as_name     (const ATTSET *set);
extern void    as_setdir   (ATTSET *set, int dir);
extern void    as_setmark  (ATTSET *set, ATTID mark);
extern void    as_save     (ATTSET *set, ATTID *marks);
extern void    as_restore  (ATTSET *set, ATTID *marks);
extern WEIGHT  as_setwgt   (ATTSET *set, WEIGHT wgt);
extern WEIGHT  as_getwgt   (const ATTSET *set);
extern int     as_setsd2p  (ATTSET *set, int sd2p);
extern int     as_getsd2p  (const ATTSET *set);

extern int     as_attadd   (ATTSET *set, ATT *att);
extern int     as_attaddm  (ATTSET *set, ATT **att, ATTID cnt);
extern ATT*    as_attrem   (ATTSET *set, ATTID attid);
extern void    as_attexg   (ATTSET *set, ATTID attid1, ATTID attid2);
extern void    as_attmove  (ATTSET *set, ATTID off,ATTID cnt,ATTID pos);
extern void    as_attperm  (ATTSET *set, ATTID *perm);
extern int     as_attcut   (ATTSET *dst, ATTSET *src, int mode, ...);
extern int     as_attcopy  (ATTSET *dst, const ATTSET *src,
                            int mode, ...);
extern ATTID   as_attid    (const ATTSET *set, const char *name);
extern int     as_attname  (const ATTSET *set, ATTID attid);
extern ATT*    as_att      (ATTSET *set, int attid);
extern ATTID   as_attcnt   (const ATTSET *set);
extern ATTID   as_target   (ATTSET *set, const char *name, int dirs);

extern void    as_apply    (ATTSET *set, ATT_APPFN appfn, void *data);
#ifdef AS_READ
extern CCHAR*  as_errmsg   (ATTSET *set, char *buf, size_t size);
extern int     as_read     (ATTSET *set, TABREAD  *trd, int mode, ...);
extern int     as_vread    (ATTSET *set, TABREAD  *trd, int mode,
                            va_list *args);
#endif
#ifdef AS_WRITE
extern int     as_write    (ATTSET *set, TABWRITE *twr, int mode, ...);
extern int     as_vwrite   (ATTSET *set, TABWRITE *twr, int mode,
                            va_list *args);
#endif
#ifdef AS_DESC
extern int     as_desc     (ATTSET *set, FILE *file, int mode,
                            int maxlen, ...);
extern int     as_vdesc    (ATTSET *set, FILE *file, int mode,
                            int maxlen, va_list *args);
#endif
#ifdef AS_PARSE
extern int     as_parse    (ATTSET *set, SCANNER *scan,
                            int types, int mand);
#endif
#ifndef NDEBUG
extern void    as_stats    (const ATTSET *set);
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define att_name(a)        ((CCHAR*)(a)->name)
#define att_type(a)        ((a)->type)
#define att_width(a,s)     ((a)->attwd[(s) ? 1 : 0])
#define att_setdir(a,d)    ((a)->dir  = (d))
#define att_getdir(a)      ((a)->dir)
#define att_setmark(a,m)   ((a)->mark = (m))
#define att_getmark(a)     ((a)->mark)
#define att_setwgt(a,w)    ((a)->wgt  = (w))
#define att_getwgt(a)      ((a)->wgt)
#define att_setsd2p(a,n)   ((a)->sd2p = (n))
#define att_getsd2p(a)     ((a)->sd2p)
#define att_inst(a)        (&(a)->inst)
#define att_attset(a)      ((a)->set)
#define att_id(a)          ((a)->id)

/*--------------------------------------------------------------------*/
#define att_valname(a,i)   ((CCHAR*)(a)->vals[i]->name)
#define att_valcnt(a)      ((a)->cnt)
#define att_valmin(a)      ((CINST*)&(a)->min)
#define att_valmax(a)      ((CINST*)&(a)->max)

/*--------------------------------------------------------------------*/
#define as_name(s)         ((CCHAR*)(s)->name)
#define as_getwgt(s)       ((s)->wgt)
#define as_setwgt(s,w)     ((s)->wgt  = (w))
#define as_getsd2p(s)      ((s)->sd2p)
#define as_setsd2p(s,n)    ((s)->sd2p = (n))

#define as_att(s,i)        ((s)->atts[i])
#define as_attcnt(s)       ((s)->cnt)
#define as_attname(s,i)    att_name((s)->atts[i])

#endif
