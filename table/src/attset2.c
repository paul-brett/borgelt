/*----------------------------------------------------------------------
  File    : attset2.c
  Contents: attribute set management, read/write and parse functions
  Author  : Christian Borgelt
  History : 1995.10.26 file created
            1995.11.03 functions as_read() and as_write() added
            1995.11.25 functions as_read() and as_write() extended
            1995.11.29 output restricted to maxlen characters per line
            1996.02.22 functions as_read() and as_write() redesigned
            1997.02.26 default attribute name generation added
            1997.08.01 restricted read/write/describe added
            1997.08.02 additional information output added
            1998.01.04 scan functions moved to module tabread
            1998.01.06 read/write functions made optional
            1998.01.10 variable null value characters added
            1998.03.06 alignment of last field removed
            1998.03.13 adding values from function as_read() simplified
            1998.08.23 attribute creation and deletion functions added
            1998.09.01 several assertions added
            1998.09.06 second major redesign completed
            1998.09.14 attribute selection in as_write() improved
            1998.09.17 attribute selection improvements completed
            1999.02.04 long int changed to int (assume 32 bit system)
            1999.02.12 attribute reading in mode AS_DFLT improved
            1999.03.23 attribute name existence check made optional
            2000.02.03 error reporting bug in function as_read() fixed
            2000.11.22 functions scn_format() and scn_fmtlen() exported
            2001.05.13 acceptance of null values made optional
            2001.06.23 module split into two files (optional functions)
            2001.10.04 bug in value range reading removed
            2002.01.22 parser functions moved to a separate file
            2002.05.31 bug in field reporting in as_read() fixed
            2003.07.22 bug in as_write() (output of NV_FLT) fixed
            2004.05.21 semantics of null value reading changed
            2006.01.17 bug in function as_write() fixed (RDORD/RANGE)
            2006.10.06 adapted to improved function trd_next()
            2007.02.13 adapted to redesigned module tabread
            2007.02.17 directions added, weight description modified
            2010.10.07 adapted to modified module tabread
            2010.10.08 function as_write() adapted to module tabwrite
            2010.12.20 functions as_vread(), as_vwrite(), as_vdesc()
            2011.02.08 reading and writing of zero attributes added
            2013.07.18 adapted to definitions ATTID, VALID, DTINT, DTFLT
            2013.08.14 reading and writing of empty tuples added
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "attset.h"
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
#define BLKSIZE        16       /* block size for arrays */

/* --- error codes --- */
#define E_DUPATT     (-16)      /* duplicate attribute */
#define E_MISATT     (-17)      /* missing   attribute */
#define E_FLDCNT     (-18)      /* wrong number of fields/columns */
#define E_EMPFLD     (-19)      /* field/column is empty */
#define E_VALUE      (-20)      /* invalid attribute value */

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
#ifdef AS_READ
static const char *errmsgs[] = {   /* error messages */
  /* E_NONE      0 */  "no error",
  /* E_NOMEM    -1 */  "not enough memory",
  /* E_FOPEN    -2 */  "cannot open file %s",
  /* E_FREAD    -3 */  "read error on file %s",
  /* E_FWRITE   -4 */  "write error on file %s",
  /* E_STDIN    -5 */  "double assignment of standard input",
  /*     -6 to -15 */  NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL,
  /* E_DUPATT  -16 */  "#duplicate attribute '%s'",
  /* E_MISATT  -17 */  "#missing attribute '%s'",
  /* E_FLDCNT  -18 */  "#wrong number of fields/columns",
  /* E_EMPFLD  -19 */  "#field/column is empty",
  /* E_VALUE   -20 */  "#invalid attribute value '%s'",
  /*           -21 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static char msgbuf[2*AS_MAXLEN+64];  /* buffer for error messages */

/*----------------------------------------------------------------------
  Read/Write Functions
----------------------------------------------------------------------*/

const char* as_errmsg (ATTSET *set, char *buf, size_t size)
{                               /* --- get last (read) error message */
  int        i;                 /* index of error message */
  size_t     k = 0;             /* buffer for header length */
  const char *msg;              /* error message (format) */
  TABREAD    *trd;              /* table reader (typed) */

  assert(set                    /* check the function arguments */
  &&    (!buf || (size > 0)));  /* if none given, get internal buffer */
  if (!buf) { buf = msgbuf; size = sizeof(msgbuf); }
  i = (set->err < 0) ? -set->err : 0;
  assert(i < (int)(sizeof(errmsgs)/sizeof(char*)));
  msg = errmsgs[i];             /* get the error message (format) */
  trd = (TABREAD*)set->trd;     /* and the table reader */
  assert(msg);                  /* check for a proper message */
  if (*msg == '#') { msg++;     /* if message needs a header */
    k = (size_t)snprintf(buf, size, "%s:%"SIZE_FMT"(%"SIZE_FMT"): ",
                         TRD_FPOS(trd));
    if (k >= size) k = size-1;  /* print the input file name and */
  }                             /* the record and field number */
  snprintf(buf+k, size-k, msg,  /* format the error message */
           (-i == E_MISATT) ? set->str : trd_field(trd));
  return buf;                   /* return the error message */
}  /* as_errmsg() */

/*--------------------------------------------------------------------*/

int as_read (ATTSET *set, TABREAD *trd, int mode, ...)
{                               /* --- read attributes/instances */
  int     r;                    /* buffer for result */
  va_list args;                 /* list of variable arguments */

  assert(set && trd);           /* check the function arguments */
  va_start(args, mode);         /* start variable argument evaluation */
  r = as_vread(set, trd, mode, &args); /* read attributes/instances */
  va_end(args);                 /* end variable argument evaluation */
  return r;                     /* return the read result */
}  /* as_read() */

/*--------------------------------------------------------------------*/

int as_vread (ATTSET *set, TABREAD *trd, int mode, va_list *args)
{                               /* --- read attributes/instances */
  int    r;                     /* buffer for function results */
  int    d;                     /* delimiter type */
  ATTID  k;                     /* loop variable */
  ATTID  off, cnt, end;         /* range of attributes */
  char   *name;                 /* attribute name */
  ATTID  attid;                 /* attribute identifier */
  ATT    *att;                  /* to traverse the attributes */
  ATTID  size;                  /* field array size */
  ATTID  *fld;                  /* pointer to/in field array */
  WEIGHT wgt;                   /* instantiation weight */
  INST   *inst;                 /* dummy instance */
  char   dflt[64];              /* buffer for default name */

  assert(set && args);          /* check the function arguments */
  if (!trd) {                   /* if no table reader is given */
    if (set->flds) { free(set->flds); set->flds = NULL; }
    set->fldcnt = set->fldsize = 0;
    return 0;                   /* delete the field map */
  }                             /* and abort the function */
  inst = (mode & AS_NOXVAL) ? (void*)1 : NULL;
  set->trd = trd;               /* get instance and note reader */

  /* --- get range of attributes --- */
  if (mode & AS_RANGE) {        /* if an index range is given, */
    off = va_arg(*args, ATTID); /* get offset to first value */
    cnt = va_arg(*args, ATTID); /* and number of values */
    k   = set->cnt -off;        /* check and adapt the */
    if (cnt > k) cnt = k; }     /* number of attributes */
  else {                        /* if no index range is given, */
    off = 0;                    /* get the full index range */
    cnt = set->cnt;             /* (cover all attributes) */
  }                             /* afterwards check range params. */
  assert((off >= 0) && (cnt >= 0));
  end = off +cnt;               /* get end index of att. range */

  /* --- read attributes --- */
  if (mode & (AS_ATT|AS_DFLT)){ /* if to read attributes */
    for (k = 0; k < set->cnt; k++)
      set->atts[k]->read = 0;   /* clear all read flags */
    size = set->fldsize;        /* get size of field array */
    cnt  = set->fldcnt = 0;     /* initialize field counter */
    do {                        /* traverse the table fields */
      d = trd_read(trd);        /* read the next field */
      if (d <= TRD_ERR) return set->err = E_FREAD;
      if (d <= TRD_EOF) break;  /* check for end of file */
      if (trd_field(trd)[0] == '\0') {
        if      (mode & AS_DFLT) { /* do nothing */ }
        else if ((cnt <= 0) && (d == TRD_REC)) break;
        else return set->err = E_EMPFLD;
      }                         /* handle an empty field */
      if ((mode & AS_WEIGHT) && (d == TRD_REC))
        break;                  /* skip weight in last field */
      if (cnt >= size) {        /* if the field array is full */
        size += (size > BLKSIZE) ? (size >> 1) : BLKSIZE;
        fld   = (ATTID*)realloc(set->flds, (size_t)size *sizeof(ATTID));
        if (!fld) return set->err = E_NOMEM;
        set->flds    = fld;     /* resize the field array */
        set->fldsize = size;    /* and set the new array */
      }                         /* as well as its size */
      if (mode & AS_DFLT)       /* in default mode create a name */
        sprintf(name = dflt, "%"ATTID_FMT, cnt+1);
      else      name = trd_field(trd);
      attid = as_attid(set, name);  /* get the attribute id */
      if (attid >= 0) {         /* if the attribute exists, */
        att = set->atts[attid]; /* get attribute and check flag */
        if (att->read) return set->err = E_DUPATT;
        if ((attid <  off)      /* if the attribute */
        ||  (attid >= end)      /* is out of range */
        ||  ((mode & AS_MARKED) /* or if in marked mode and */
        &&   (att->mark < 0)))  /* the attribute is not marked, */
          attid     = -1;       /* skip this attribute, */
        else {                  /* otherwise (if to read attribute) */
          att->read = -1;       /* set the read flag */
        } }                     /* if the attribute does not exist */
      else if (mode & AS_NOXATT)/* if not to extend the att. set, */
        att = NULL;             /* invalidate the attribute */
      else {                    /* if to extend the attribute set */
        att = att_create(name, AT_NOM);
        if (!att || (as_attadd(set, att) != 0)) {
          if (att) att_delete(att);
          return set->err = E_NOMEM;
        }                       /* create a new attribute, */
        attid     = att->id;    /* add it to the attribute set, */
        att->read = -1;         /* note its attribute identifier */
      }                         /* and set its read flag */
      set->flds[cnt++] = attid; /* set field mapping */
      if ((mode & AS_DFLT)      /* if to use a default header, */
      &&  (attid >= 0)) {       /* set the attribute value read */
        name = trd_field(trd);  /* get the value name */
        if (name[0] == '\0') {  /* if the value is null, check mode */
          if (mode & AS_NONULL) return set->err = E_VALUE;
          if      (att->type == AT_FLT) att->inst.f = NV_FLT;
          else if (att->type == AT_INT) att->inst.i = NV_INT;
          else                          att->inst.n = NV_NOM; }
        else {                  /* if the value is not null */
          r = att_valadd(att, name, inst);
          if (r >=  0) continue;/* add the value to the attribute */
          if (r >= -1) return set->err = E_NOMEM;
          else         return set->err = E_VALUE;
        }                       /* if the value cannot be added, */
      }                         /* abort with an error code */
    } while (d != TRD_REC);     /* while not at end of record */
    for (k = off; k < end; k++) {
      att = set->atts[k];       /* traverse the attributes, */
      if (att->read) continue;  /* but skip read attributes */
      if (!(mode & AS_MARKED)   /* check for unread attributes */
      ||  (att->mark > 0)) {    /* that have to be read (marked) */
        set->str = att_name(att); return set->err = E_MISATT; }
      if ((mode & AS_MARKED)    /* if an unread attribute is marked */
      &&  (att->mark == 0))     /* as optional (read if present), */
        att->mark = -1;         /* adapt the attribute marker */
    }                           /* to indicate that it is missing */
    set->fldcnt = cnt;          /* set number of fields */
    return set->err = 0;        /* and return 'ok' */
  }                             /* (end of read attributes) */

  /* --- read instances --- */
  cnt = set->fldcnt;            /* get number of fields and */
  if (cnt > 0) fld = set->flds; /* a pointer to the field array */
  else       { fld = NULL; cnt = set->cnt; }
  if (cnt <= 0) {               /* if no attributes to read */
    d = trd_read(trd);          /* check for end of file */
    if (d <= TRD_EOF) return set->err = 1;
    return set->err = (d <= TRD_ERR) ? E_FREAD : E_FLDCNT;
  }
  d = TRD_FLD;                  /* set default delimiter type */
  for (k = 0; (k < cnt) && (d == TRD_FLD); k++) {
    d = trd_read(trd);          /* read the next value */
    if  (d <= TRD_ERR)              return set->err = E_FREAD;
    if ((d <= TRD_EOF) && (k <= 0)) return set->err = 1;
    attid = (fld) ? *fld++ : k; /* get attribute identifier */
    if ((attid < off) || (attid >= end))
      continue;                 /* skip attributes out of range */
    att = set->atts[attid];     /* get attribute */
    if ((mode & AS_MARKED)      /* if in marked mode and */
    &&  (att->mark < 0))        /* attribute is not marked, */
      continue;                 /* skip this field */
    name = trd_field(trd);      /* get the value name */
    if (name[0] == '\0') {      /* if the value is null, check mode */
      if (mode & AS_NONULL) return set->err = E_VALUE;
      if      (att->type == AT_FLT) att->inst.f = NV_FLT;
      else if (att->type == AT_INT) att->inst.i = NV_INT;
      else                          att->inst.n = NV_NOM; }
    else {                      /* if the value is not null */
      r = att_valadd(att, name, inst);
      if (r >=  0) continue;    /* add the value to the attribute */
      if (r >= -1) return set->err = E_NOMEM;
      else         return set->err = E_VALUE;
    }                           /* if the value cannot be added, */
  }  /* for (k = 0; .. */       /* abort with an error code */
  if (!(mode & AS_WEIGHT))      /* if there is no weight field, */
    set->wgt = 1.0;             /* set instantiation weight to 1 */
  else if (d != TRD_FLD)        /* if no weight field is available, */
    return set->err = E_FLDCNT; /* abort with failure */
  else {                        /* if weight field is available */
    d = trd_read(trd);          /* read the weight field */
    if (d <= TRD_ERR) return set->err = E_FREAD;
    name = trd_field(trd);      /* get the weight string and */
    wgt  = asu_str2wgt(name);   /* convert weight to floating point */
    if (errno || isnan(wgt) || ((mode & AS_NONEG) && (wgt < 0)))
      return set->err = E_VALUE;/* check the weight value and */
    set->wgt = wgt;             /* set the instantiation weight */
  }
  if ((k < cnt) || (d == TRD_FLD))
    return set->err = E_FLDCNT; /* check the number of fields */
  return set->err = 0;          /* return 'ok' */
}  /* as_vread() */

#endif  /* #ifdef AS_READ */
/*--------------------------------------------------------------------*/
#ifdef AS_WRITE

int as_write (ATTSET *set, TABWRITE *twr, int mode, ...)
{                               /* --- read attributes/instances */
  int     r;                    /* buffer for result */
  va_list args;                 /* list of variable arguments */

  assert(set && twr);           /* check the function arguments */
  va_start(args, mode);         /* start variable argument evaluation */
  r = as_vwrite(set, twr, mode, &args); /* write attributes/instances */
  va_end(args);                 /* end variable argument evaluation */
  return r;                     /* return the write result */
}  /* as_write() */

/*--------------------------------------------------------------------*/

int as_vwrite (ATTSET *set, TABWRITE *twr, int mode, va_list *args)
{                               /* --- write attributes/instances */
  int        i;                 /* loop variable, buffer */
  ATTID      k;                 /* loop variable */
  ATTID      off, cnt, end;     /* range of attributes */
  ATT        *att;              /* to traverse attributes */
  ATTID      attid;             /* attribute identifier */
  ATTID      *fld;              /* pointer to/in field array */
  const char *name;             /* name of attribute/value */
  INFOUTFN   *infout = 0;       /* add. info. output function */
  char       buf[64];           /* buffer for attribute values */

  assert(set && twr && args);   /* check the function arguments */

  /* --- get range of attributes --- */
  if (mode & AS_RANGE) {        /* if an index range is given, */
    off = va_arg(*args, ATTID); /* get offset to first value */
    cnt = va_arg(*args, ATTID); /* and number of values */
    k   = set->cnt -off;        /* check and adapt the */
    if (cnt > k) cnt = k; }     /* number of attributes */
  else {                        /* if no index range given, */
    off = 0;                    /* get the full index range */
    cnt = set->cnt;             /* (cover all attributes) */
  }                             /* afterwards check range params. */
  assert((off >= 0) && (cnt >= 0));
  end = off +cnt;               /* get end of range of attributes */
  if (mode & (AS_INFO1|AS_INFO2))  /* get additional information */
    infout = va_arg(*args, INFOUTFN*);        /* output function */

  /* --- write attributes/values --- */
  if (!(mode & AS_RDORD)        /* if not to write in read order */
  ||  (set->fldcnt <= 0))       /* or no read order is available, */
    fld = NULL;                 /* clear field pointer */
  else {                        /* if to write in read order, */
    fld = set->flds;            /* get field pointer */
    cnt = set->fldcnt;          /* and number of fields */
  }
  name = NULL;                  /* clear name as a first field flag */
  for (k = 0; k < cnt; k++) {   /* traverse fields/attributes */
    attid = (fld) ? fld[k] : (k +off);      /* get next index */
    if ((attid < off) || (attid >= end))
      continue;                 /* skip attributes out of range */
    att = set->atts[attid];     /* get attribute */
    if ((mode & AS_MARKED)      /* if in marked mode and */
    &&  (att->mark < 0))        /* attribute is not marked, */
      continue;                 /* skip this attribute */
    if (name)                   /* if not the first field, */
      twr_fldsep(twr);          /* print field separator */
    if      (mode & AS_ATT)     /* if to write attributes, */
      name = att->name;         /* get attribute name */
    else if (att->type == AT_INT) { /* if integer value */
      if (isnull(att->inst.i))  /* if value is null, */
        name = twr_nvname(twr); /* set null value character */
      else {                    /* if value is known */
        sprintf(buf, "%"DTINT_FMT, att->inst.i); name = buf;
      } }                       /* format value */
    else if (att->type == AT_FLT) { /* if floating point value */
      if (isnan(att->inst.f))   /* if value is null, */
        name = twr_nvname(twr); /* set null value character */
      else {                    /* if value is known */
        sprintf(buf, "%.*"DTFLT_FMT, att->sd2p, att->inst.f);
        name = buf;             /* format value */
      } }
    else {                      /* otherwise (nominal value) */
      if (isnone(att->inst.n))  /* if value is null, */
        name = twr_nvname(twr); /* set null value character */
      else                      /* if value is known */
        name = att->vals[att->inst.n]->name;
    }                           /* get name of attribute value */
    twr_puts(twr, name);        /* write attribute/value name */
    if ((mode & (AS_ALIGN|AS_ALNHDR))   /* if to align fields and */
    &&  ((k < cnt-1)            /* not on the last field to write */
    ||   (mode & (AS_INFO1|AS_WEIGHT|AS_INFO2)))) {
      i = att_valwd(att, 0);    /* get width of widest value */
      if ((mode & AS_ALNHDR) && (att->attwd[0] > i))
        i = att->attwd[0];      /* adapt with width of att. name and */
      i -= (int)strlen(name);   /* subtract width of current value */
      while (--i >= 0) twr_blank(twr);
    }                           /* pad the field with blanks */
  }                             /* (write normal fields) */

  /* --- write weight and additional information --- */
  k = (name) ? 1 : 0;           /* get number of written fields */
  if (mode & AS_INFO1) {        /* if to write additional information */
    if (k++ > 0) twr_fldsep(twr);  /* write field separator */
    infout(set, twr, mode);     /* call function to write add. info. */
  }
  if (mode & AS_WEIGHT) {       /* if weight output requested */
    if (k++ > 0) twr_fldsep(twr);  /* write field separator */
    if (mode & AS_ATT) twr_putc(twr, '#');
    else twr_printf(twr, "%.*"WGT_FMT, set->sd2p, set->wgt);
  }                             /* write counter field */
  if (mode & AS_INFO2) {        /* if to write additional information */
    if (k++ > 0) twr_fldsep(twr);  /* write field separator */
    infout(set, twr, mode);     /* call function to write add. info. */
  }
  twr_recsep(twr);              /* terminate the record written */
  return twr_error(twr);        /* check for a write error */
}  /* as_write() */

#endif  /* #ifdef AS_WRITE */
