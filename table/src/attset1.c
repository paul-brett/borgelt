/*----------------------------------------------------------------------
  File    : attset1.c
  Contents: attribute set management, base functions
  Author  : Christian Borgelt
  History : 1995.10.26 file created
            1995.12.21 function att_valsort() added
            1996.03.17 attribute types added (function att_type())
            1996.07.04 attribute weights added (att_set/getwgt())
            1997.02.26 default attribute name generation added
            1997.03.12 attribute marks added (att_set/getmark())
            1997.05.27 function att_convert() added (type conversion)
            1997.08.30 removal of field array made possible
            1998.06.22 deletion function moved to function as_create()
            1998.06.23 major redesign, attribute functions introduced
            1998.08.23 attribute creation and deletion functions added
            1998.08.30 params. 'map' and 'mapdir' added to att_valsort()
            1998.09.01 several assertions added (safety checks)
            1998.09.06 second major redesign completed
            1998.09.12 deletion function parameter changed to ATT
            1998.09.24 parameter 'map' added to function att_convert()
            1988.09.25 function as_attaddm() added (add multiple)
            1998.11.25 functions att_valcopy() and as_attcopy() added
            1998.11.29 functions att_clone() and as_clone() added
            1999.02.04 long int changed to int (assume 32 bit system)
            2000.11.22 functions scn_format() and scn_fmtlen() exported
            2001.06.23 module split into two files (optional functions)
            2001.07.16 return code of as_attadd set to 1 if att. exists
            2004.05.21 bug concerning null value output fixed
            2005.11.19 cast from object to function pointer removed
            2007.02.13 adapted to redesigned module tabread
            2010.08.07 adapted to redesigned module tabread
            2010.11.15 functions as_save() and as_restore() added
            2011.02.01 functions as_setdir() and as_setmark() added
            2013.03.07 adapted to direction param. of sorting functions
            2013.07.18 adapted to definitions ATTID, VALID, DTINT, DTFLT
            2013.07.19 utility functions for conversions added
            2013.07.26 parameter 'dir' added to function att_valsort()
            2013.09.03 removed check for new value for int and float
            2015.08.01 function as_attperm() added (permute attributes)
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "arrays.h"
#include "attset.h"
#ifdef STORAGE
#include "storage.h"
#endif

#ifdef _MSC_VER
#ifndef strtoll
#define strtoll     _strtoi64   /* string to long long */
#endif                          /* MSC still does not support C99 */
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/

#define int         1           /* to check definitions */
#define long        2           /* for certain types */
#define ptrdiff_t   3

/*--------------------------------------------------------------------*/

#if   DTINT==int
#define   XINT      long int
#define   strtoi    strtol

#elif DTINT==long
#define   XINT      long int
#define   strtoi    strtol

#elif DTINT==ptrdiff_t
#if PTRDIFF_MAX <= LONG_MAX
  #define XINT      long int
  #define strtoi    strtol
#else
  #define XINT      long long int
  #define strtoi    strtoll
#endif

#else
#error "DTINT must be either 'int', 'long' or 'ptrdiff_t'"
#endif

/*--------------------------------------------------------------------*/

#undef int                      /* remove preprocessor definitions */
#undef long                     /* needed for the type checking */
#undef ptrdiff_t

/*--------------------------------------------------------------------*/

#define BLKSIZE       16        /* block size for arrays */

#define DEL_FLDS(s)             /* delete the fields array */ \
  if ((s)->flds) { free((s)->flds); (s)->flds = NULL; \
                   (s)->fldcnt = (s)->fldsize = 0; }

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct { VAL_CMPFN *cmpfn; } VALCMP;

/*----------------------------------------------------------------------
  Auxiliary Functions
----------------------------------------------------------------------*/

static int length (const char *s)
{                               /* --- compute bounded string length */
  int k;                        /* character counter */
  for (k = 0; (k < AS_MAXLEN) && s[k]; k++);
  return k;                     /* determine the bounded length */
}  /* length() */               /* of the string (max. AS_MAXLEN) */

/*--------------------------------------------------------------------*/

static char* copy (char *d, const char *s)
{                               /* --- length-bounded string copy */
  int k;                        /* character counter */
  for (k = 0; (k < AS_MAXLEN) && s[k]; k++) d[k] = s[k];
  d[k] = '\0';                  /* copy string (max. AS_MAXLEN chs.) */
  return d;                     /* and return the destination */
}  /* copy() */

/*--------------------------------------------------------------------*/

static size_t hash (const char *s)
{                               /* --- hash function */
  int    k;                     /* character counter */
  size_t h = 0;                 /* hash value */
  for (k = 0; (k < AS_MAXLEN) && s[k]; k++)
    h = h *251 +(size_t)(unsigned char)s[k];
  return h;                     /* compute and return the hash value */
}  /* hash() */

/*--------------------------------------------------------------------*/

static int valcmp (const void *p1, const void *p2, void *data)
{                               /* --- compare two attribute values */
  return (((VALCMP*)data)->cmpfn)(((const VAL*)p1)->name,
                                  ((const VAL*)p2)->name);
}  /* valcmp() */

/*----------------------------------------------------------------------
  Utility Functions (for conversions)
----------------------------------------------------------------------*/

DTFLT asu_int2flt (DTINT x)
{                               /* --- convert int to floating point */
  return (isnull(x)) ? NV_FLT : (DTFLT)x;
}  /* asu_int2flt() */

/*--------------------------------------------------------------------*/

DTINT asu_flt2int (DTFLT x)
{                               /* --- convert floating point to int */
  if (isnan(x))             return NV_INT;
  if (x < (DTFLT)DTINT_MIN) return DTINT_MIN;
  if (x > (DTFLT)DTINT_MAX) return DTINT_MAX;
  if (x >= 0)               return (DTINT)(x+0.5);
  else                      return (DTINT)(x-0.5);
}  /* asu_flt2int() */

/*--------------------------------------------------------------------*/

DTINT asu_str2int (const char *s)
{                               /* --- convert string to integer */
  char *e;                      /* end point for conversion */
  XINT i;                       /* result of conversion */

  assert(s);                    /* check the function arguments */
  errno = 0;                    /* clear the error indicator and */
  i = strtoi(s, &e, 10);        /* convert value to floating point */
  if (errno) return NV_INT;     /* check for a successful conversion */
  if (*e || (e == s) || (i < DTINT_MIN) || (i > DTINT_MAX)) {
    errno = ERANGE; return NV_INT; }
  return (DTINT)i;              /* return the converted value */
}  /* asu_str2int() */

/*--------------------------------------------------------------------*/

DTFLT asu_str2flt (const char *s)
{                               /* --- convert string to float */
  char   *e;                    /* end point for conversion */
  double f;                     /* result of conversion */

  assert(s);                    /* check the function arguments */
  errno = 0;                    /* clear the error indicator and */
  f = strtod(s, &e);            /* convert value to floating point */
  if (errno) return NV_FLT;     /* check for a successful conversion */
  if (*e || (e == s) || (f < DTFLT_MIN) || (f > DTFLT_MAX)) {
    errno = ERANGE; return NV_FLT; }
  return (DTFLT)f;              /* return the converted value */
}  /* asu_str2flt() */

/*--------------------------------------------------------------------*/

WEIGHT asu_str2wgt (const char *s)
{                               /* --- convert string to float */
  char   *e;                    /* end point for conversion */
  double f;                     /* result of conversion */

  assert(s);                    /* check the function arguments */
  errno = 0;                    /* clear the error indicator and */
  f = strtod(s, &e);            /* convert value to floating point */
  if (errno) return (WEIGHT)NAN;/* check for a successful conversion */
  if (*e || (e == s) || (f < WGT_MIN) || (f > WGT_MAX)) {
    errno = ERANGE; return (WEIGHT)NAN; }
  return (WEIGHT)f;             /* return the converted value */
}  /* asu_str2wgt() */

/*----------------------------------------------------------------------
  Attribute Functions
----------------------------------------------------------------------*/

static int att_resize (ATT *att, VALID size)
{                               /* --- resize value array */
  VALID k;                      /* loop variable, buffer */
  VAL   **val;                  /* to traverse values */
  VAL   **p;                    /* to traverse hash bins */

  assert(att);                  /* check the function argument */
  k = (VALID)att->size;         /* get current array size */
  if (size > 0) {               /* if to enlarge the array */
    if (k >= size) return 0;    /* if array is large enough, abort */
    k += (k > BLKSIZE) ? k >> 1 : BLKSIZE;
    if (k >  size) size = k; }  /* compute new array size */
  else {                        /* if to shrink the array */
    size = att->cnt << 1;       /* get maximally tolerable size */
    if (size < BLKSIZE) size = BLKSIZE;
    if (k <= size) return 0;    /* if array is small enough, abort */
    size = att->cnt +(att->cnt >> 1);
    if (size < BLKSIZE) size = BLKSIZE;
  }                             /* compute new array size */
  val = (VAL**)realloc(att->vals, 2*(size_t)size *sizeof(VAL*));
  if (!val) return -1;          /* resize the value array */
  att->vals = val;              /* set the new value array */
  att->htab = val +size;        /* and the new hash table */
  att->size = (size_t)size;     /* clear the hash bins */
  memset(att->htab, 0, att->size *sizeof(VAL*));
  for (k = 0; k < att->cnt; k++) { /* traverse the values */
    p = att->htab +val[k]->hash % att->size;
    val[k]->succ = *p;          /* get corresponding hash bin and */
    *p = val[k];                /* insert value into hash table */
  }                             /* (at the head of the bin list) */
  return 0;                     /* return 'ok' */
}  /* att_resize() */

/*--------------------------------------------------------------------*/

ATT* att_create (const char *name, int type)
{                               /* --- create an attribute */
  ATT *att;                     /* created attribute */

  assert(name && *name);        /* check the function argument */
  att = (ATT*)malloc(sizeof(ATT));
  if (!att) return NULL;        /* allocate memory for attribute */
  att->attwd[1] = length(name); /* and attribute name */
  att->name = (char*)malloc((size_t)(att->attwd[1]+1) *sizeof(char));
  if (!att->name) { free(att); return NULL; }
  copy(att->name, name);        /* copy attribute name and */
  att->hash = hash(att->name);  /* compute its hash value */
  att->dir      = DIR_IN;       /* initialize the fields */
  att->wgt      = 1.0;          /* (with default values) */
  att->htab     = att->vals = NULL;
  att->mark     = 0;
  att->read     = 0;
  att->sd2p     = 6;            /* (set default behavior of %g) */
  att->size     = 0;
  att->cnt      = 0;
  att->attwd[0] = (int)scn_fmtlen(att->name, NULL);
  att->valwd[0] = att->valwd[1] = 1;
  att->id       = -1;           /* attribute is not part of a set and */
  att->succ     = NULL;         /* thus does not have an identifier */
  att->set      = NULL;
  if      (type == AT_INT) {    /* if attribute is integer, */
    att->type   = AT_INT;       /* note the type (integer), */
    att->min.i  = DTINT_MAX;    /* initialize minimal */
    att->max.i  = DTINT_MIN;    /* and maximal value */
    att->inst.i = NV_INT; }     /* and clear instance */
  else if (type == AT_FLT) {    /* if attribute is floating point, */
    att->type   = AT_FLT;       /* note the type (float), */
    att->min.f  = DTFLT_MAX;    /* initialize minimal */
    att->max.f  = DTFLT_MIN;    /* and maximal value */
    att->inst.f = NV_FLT; }     /* and clear instance */
  else {                        /* if attribute is nominal, */
    att->type   = AT_NOM;       /* note the type (nominal), */
    att->min.n  =  0;           /* initialize minimal */
    att->max.n  = -1;           /* and maximal value */
    att->inst.n = NV_NOM;       /* and clear instance */
  }
  return att;                   /* return created attribute */
}  /* att_create() */

/*--------------------------------------------------------------------*/

ATT* att_clone (const ATT *att)
{                               /* --- clone an attribute */
  ATT *clone;                   /* created clone */

  assert(att);                  /* check the function arguments */
  clone = att_create(att->name, att->type);
  if (!clone) return NULL;      /* create a new attribute */
  if (att_valcopy(clone, att, AS_ALL) != 0) {
    att_delete(clone); return NULL; }
  clone->dir  = att->dir;       /* copy all attribute values */
  clone->wgt  = att->wgt;       /* and all other information */
  clone->mark = att->mark;      /* into the clone */
  clone->sd2p = att->sd2p;
  clone->inst = att->inst;
  return clone;                 /* return the created clone */
}  /* att_clone() */

/*--------------------------------------------------------------------*/

void att_delete (ATT *att)
{                               /* --- delete an attribute */
  VALID k;                      /* loop variable */

  assert(att && att->name);     /* check the function argument */
  if (att->set)                 /* if there is a containing set, */
    as_attrem(att->set,att->id);/* remove the attribute from it */
  if (att->vals) {              /* if there are attribute values */
    for (k = 0; k < att->cnt; k++) free(att->vals[k]);
    free(att->vals);            /* traverse and delete the values */
  }                             /* and delete the value array */
  free(att->name);              /* delete the attribute name */
  free(att);                    /* and the attribute body */
}  /* att_delete() */

/*--------------------------------------------------------------------*/

int att_rename (ATT *att, const char *name)
{                               /* --- rename an attribute */
  int    n;                     /* (bounded) length of attribute name */
  size_t h;                     /* hash value of new attribute name */
  char   *s;                    /* new name for attribute object */
  ATT    *a, **p;               /* to traverse a hash bin list */

  assert(att && name && *name); /* check the function arguments */
  h = hash(name);               /* compute the name's hash value */
  if (att->set) {               /* if attribute is contained in a set */
    for (a = att->set->htab[h % att->set->size]; a; a = a->succ)
      if (strncmp(name, a->name, AS_MAXLEN) == 0)
        return (a == att) ? 0 : -2; /* check for another attribute */
  }                                 /* with the same name */
  n = length(name);             /* get the (bounded) name length */
  s = (char*)realloc(att->name, (size_t)(n+1) *sizeof(char));
  if (!s) return -1;            /* reallocate memory for the new name */
  att->name = copy(s, name);    /* copy the attribute name (bounded) */
  att->attwd[0] = (int)scn_fmtlen(att->name, NULL);
  att->attwd[1] = n;            /* determine the name widths */
  att->hash     = h;            /* set the new hash value */
  if (att->set) {               /* if attribute is contained in a set */
    p = att->set->htab +att->hash % att->set->size;
    while (*p != att) p = &(*p)->succ;
    *p = att->succ;             /* remove attribute from hash table */
    p = att->set->htab +h         % att->set->size;
    att->succ = *p; *p = att;   /* insert attribute into hash table */
  }                             /* (based on the new name) */
  return 0;                     /* return 'ok' */
}  /* att_rename() */

/*--------------------------------------------------------------------*/

int att_convert (ATT *att, int type, INST *map)
{                               /* --- convert attribute to new type */
  VALID k;                      /* loop variable for values */
  DTINT i;                      /* integer value */
  DTFLT f;                      /* floating point value */

  assert(att);                  /* check the function argument */
  if ((att->type == AT_INT)     /* if to convert integer */
  &&  (type      == AT_FLT)) {  /* to floating point */
    att->type   = AT_FLT;       /* set new attribute type */
    att->min.f  = (DTFLT)att->min.i; /* adapt minimal and */
    att->max.f  = (DTFLT)att->max.i; /* maximal value */
    att->inst.f = asu_int2flt(att->inst.i);
    return 0;                   /* convert attribute instance */
  }                             /* and return 'ok' */
  if ((att->type == AT_FLT)     /* if to convert floating point */
  &&  (type      == AT_INT)) {  /* to integer */
    att->type   = AT_INT;       /* set new attribute type */
    att->min.i  = asu_flt2int(att->min.f);  /* convert minimal */
    att->max.i  = asu_flt2int(att->max.f);  /* and maximal value */
    att->inst.i = asu_flt2int(att->inst.f);
    return 0;                   /* convert attribute instance */
  }                             /* and return 'ok' */
  if (att->type != AT_NOM) {    /* if attribute is not nominal */
    if (type != AT_NOM) return -1;
    att->type   = AT_NOM;       /* if to convert to nominal */
    att->min.n  =  0;           /* clear minimal and */
    att->max.n  = -1;           /* maximal value and */
    att->inst.n = NV_NOM;       /* attribute instance */
    att->valwd[0] = att->valwd[1] = 0;
    return 0;                   /* clear value widths */
  }                             /* and return 'ok' */
  if (type == AT_AUTO) {        /* if automatic type determination */
    type = (att->cnt > 0) ? AT_INT : AT_NOM;
    for (k = 0; k < att->cnt; k++) {       /* traverse values and */
      i = asu_str2int(att->vals[k]->name); /* try to convert to int */
      if (errno) { type = AT_FLT; break; }
    }                           /* on failure, switch to float */
    for (k = 0; k < att->cnt; k++) {       /* traverse values and */
      f = asu_str2flt(att->vals[k]->name); /* try to convert to int */
      if (errno) { type = AT_NOM; break; }
    }                           /* if conversion was not successful, */
  }                             /* attribute must stay nominal */
  if      (type == AT_INT) {    /* if to convert to integer */
    att->min.i = DTINT_MAX;     /* initialize the */
    att->max.i = DTINT_MIN;     /* range of values */
    for (k = 0; k < att->cnt; k++) {       /* traverse values and */
      i = asu_str2int(att->vals[k]->name); /* convert to integer */
      if (errno || isnull(i)) { if (map) map[k].i = NV_INT; continue; }
      if (map) map[k].i = i;    /* if a map is requested, set it */
      if (i < att->min.i) att->min.i = i;
      if (i > att->max.i) att->max.i = i;
    }                           /* adapt range of values */
    if (isnone(att->inst.n))    /* if the current value is null, */
      att->inst.i = NV_INT;     /* set instance to null integer */
    else {                      /* if the current value is known, */
      i = asu_str2int(att->vals[att->inst.n]->name);
      att->inst.i = (errno || isnull(i)) ? NV_INT : i;
    } }                         /* adapt attribute instance */
  else if (type == AT_FLT) {    /* if to convert to floating point */
    att->min.f = DTFLT_MAX;     /* initialize the */
    att->max.f = DTFLT_MIN;     /* range of values */
    for (k = 0; k < att->cnt; k++) {       /* traverse values and */
      f = asu_str2flt(att->vals[k]->name); /* convert to float */
      if (errno || isnan(f)) { if (map) map[k].f = NV_FLT; continue; }
      if (map) map[k].f = f;    /* if a map is requested, set it */
      if (f < att->min.f) att->min.f = f;
      if (f > att->max.f) att->max.f = f;
    }                           /* adapt range of values */
    if (isnone(att->inst.n))    /* if the current value is null, */
      att->inst.f = NV_FLT;     /* set instance to null float */
    else {                      /* if the current value is known */
      f = asu_str2flt(att->vals[att->inst.n]->name);
      att->inst.f = (errno || isnan(f)) ? NV_FLT : f;
    } }                         /* adapt attribute instance */
  else                          /* if no correct new type given or */
    return -1;                  /* no conversion possible, abort */
  if (att->vals) {              /* if there are attribute values */
    for (k = 0; k < att->cnt; k++) free(att->vals[k]);
    free(att->vals);            /* traverse and delete the values */
    att->htab = att->vals = NULL;   /* and delete the value array */
  }
  att->type = type;             /* set the new attribute type */
  att->size = 0;                /* clear the array size */
  att->cnt  = 0;                /* and the value counter */
  return 0;                     /* return 'ok' */
}  /* att_convert() */

/*--------------------------------------------------------------------*/

int att_cmp (const ATT *att1, const ATT *att2)
{                               /* --- compare two attributes */
  VALID k;                      /* loop variable */

  assert(att1 && att2);         /* check the function arguments */
  if (att1->type != att2->type) /* compare attribute types and */
    return -1;                  /* if they are not equal, abort */
  if (att1->type == AT_INT) {   /* if attribute is integer */
    return ((att1->min.i != att2->min.i)
    ||      (att1->max.i != att2->max.i));
  }                             /* compare range of values */
  if (att1->type == AT_FLT) {   /* if attribute is floating point */
    return ((att1->min.f != att2->min.f)
    ||      (att1->max.f != att2->max.f));
  }                             /* compare range of values */
  if (att1->cnt != att2->cnt)   /* if attribute is nominal, */
    return -1;                  /* compare number of values */
  for (k = 0; k < att1->cnt; k++)
    if (strcmp(att1->vals[k]->name, att2->vals[k]->name) != 0)
      return -1;                /* if a value differs, abort */
  return 0;                     /* otherwise return 'equal' */
}  /* att_cmp() */

/*----------------------------------------------------------------------
  Attribute Value Functions
----------------------------------------------------------------------*/

int att_valadd (ATT *att, const char *name, INST *inst)
{                               /* --- add a value to an attribute */
  DTINT  i;                     /* integer value */
  DTFLT  f;                     /* floating point value */
  VAL    *val;                  /* created nominal value */
  VAL    **p;                   /* to traverse values in hash bin */
  int    w;                     /* value name width */
  size_t h;                     /* hash value of value name */

  assert(att);                  /* check the function arguments */

  /* --- integer attribute --- */
  if (att->type == AT_INT) {    /* if attribute is integer-valued */
    if (!name) {                /* if no value name given, */
      if (!inst) {              /* if no instance given */
        att->min.i = DTINT_MIN; /* set the maximal */
        att->max.i = DTINT_MAX; /* range of values */
        return 0;               /* and abort the function */
      }                         /* if an instance is given, */
      i = inst->i; w = 0; }     /* get the value from the instance */
    else {                      /* if a value name is given, */
      i = asu_str2int(name);    /* convert name to integer */
      if (errno) return -2;     /* if the conversion failed, abort */
      w = length(name);         /* get the length of the value name */
    }
    if (isnull(i)) return -2;   /* check for a null value */
    #ifdef AS_CHKXNUM           /* if to check numeric range ext. */
    if (name && inst && ((i < att->min.i) || (i > att->max.i)))
      return -3;                /* check for a new value */
    #endif
    if (i < att->min.i) att->min.i = i;  /* update minimal */
    if (i > att->max.i) att->max.i = i;  /* and maximal value */
    att->inst.i = i;                     /* and set instance */
    if ((att->valwd[0] > 0) && (w > att->valwd[0]))
      att->valwd[0] = att->valwd[1] = w;
    return 0;                   /* adapt the value widths */
  }                             /* and return 'ok' */

  /* --- float attribute --- */
  if (att->type == AT_FLT) {    /* if attribute is float */
    if (!name) {                /* if no value name given */
      if (!inst) {              /* if no instance given */
        att->min.f = DTFLT_MIN; /* otherwise set the */
        att->max.f = DTFLT_MAX; /* maximal range of values */
        return 0;               /* and abort the function */
      }                         /* if an instance is given, */
      f = inst->f; w = 0; }     /* get the value from the instance */
    else {                      /* if a value name is given, */
      f = asu_str2flt(name);    /* convert name to float value */
      if (errno) return -2;     /* if the conversion failed, abort */
      w = length(name);         /* get the length of the name */
    }
    if (isnan(f)) return -2;    /* check for a null value */
    #ifdef AS_CHKXNUM           /* if to check numeric range ext. */
    if (name && inst && ((f < att->min.f) || (f > att->max.f)))
      return -3;                /* check for a new value */
    #endif
    if (f < att->min.f) att->min.f = f;  /* update minimal */
    if (f > att->max.f) att->max.f = f;  /* and maximal value */
    att->inst.f = f;                     /* and set instance */
    if ((att->valwd[0] > 0) && (w > att->valwd[0]))
      att->valwd[0] = att->valwd[1] = w;
    return 0;                   /* adapt the value widths */
  }                             /* and return 'ok' */

  /* --- nominal attribute --- */
  assert(name && *name);        /* check for a valid value name */
  if (att_resize(att, att->cnt+1) != 0)
    return -1;                  /* resize the value array */
  h = hash(name);               /* compute the name's hash value and */
  p = att->htab +h % att->size; /* traverse the corresp. hash bin */
  for (val = *p; val; val = val->succ) {
    if (strncmp(name, val->name, AS_MAXLEN) == 0) {
      att->inst.n = val->id; return 1; }
  }                             /* if name already exists, abort */
  if (inst) return -3;          /* if not to extend the domain, abort */
  w   = length(name);           /* get (bounded) value name length */
  val = (VAL*)malloc(sizeof(VAL) +(size_t)w *sizeof(char));
  if (!val) return -1;          /* allocate memory for a value */
  copy(val->name, name);        /* copy name and set hash value */
  val->hash = h;                /* set value identifier and instance */
  val->id   = att->inst.n = att->max.n = att->cnt;
  val->succ = *p; *p = val;     /* insert value into the hash table */
  att->vals[att->cnt++] = val;  /* and the value array */
  if (att->valwd[0] > 0) {      /* if value name widths are valid */
    if (w > att->valwd[1]) att->valwd[1] = w;
    w = (int)scn_fmtlen(val->name, NULL);
    if (w > att->valwd[0]) att->valwd[0] = w;
  }                             /* update maximal value widths */
  return 0;                     /* return 'ok' */
}  /* att_valadd() */

/*--------------------------------------------------------------------*/

void att_valrem (ATT *att, VALID valid)
{                               /* --- remove an attribute value */
  VALID k;                      /* loop variable */
  VAL   *val;                   /* value to remove */
  VAL   **p;                    /* to traverse the hash bin */

  assert(att                    /* check the function arguments */
  &&    (att->type == AT_NOM) && (valid < att->cnt));

  /* --- remove all attribute values --- */
  if (valid < 0) {              /* if no value identifier given */
    if (!att->vals) return;     /* if there are no values, abort */
    for (k = 0; k < att->cnt; k++) free(att->vals[k]);
    free(att->vals);            /* traverse and delete the values */
    att->htab   = att->vals = NULL; /* and delete the value array */
    att->size   =  0;           /* clear the array size */
    att->cnt    =  0;           /* and the value counter */
    att->min.n  =  0;           /* clear the identifier range */
    att->max.n  = -1;           /* (minimum and maximum value id) */
    att->inst.n = NV_NOM;       /* and the instance (current value) */
    return;                     /* abort the function */
  }

  /* --- remove one attribute value --- */
  val = att->vals[valid];       /* get the value to remove */
  p   = att->htab +val->hash % att->size;
  while (*p != val) p = &(*p)->succ;
  *p = val->succ;               /* remove value from hash table */
  free(val);                    /* and delete the value body */
  att->max.n = --att->cnt -1;   /* adapt maximal value identifier */
  for (k = valid; k < att->cnt; k++) {
    att->vals[k] = val = att->vals[k+1];
    val->id = k;                /* shift the following values */
  }                             /* and adapt their identifiers */
  att->valwd[0] = 0;            /* invalidate value widths */
  if      (att->inst.n >  valid) att->inst.n--;
  else if (att->inst.n == valid) att->inst.n = NV_NOM;
  att_resize(att, 0);           /* adapt instance (current value) */
}  /* att_valrem() */           /* and try to shrink the value array */

/*--------------------------------------------------------------------*/

void att_valexg (ATT *att, VALID valid1, VALID valid2)
{                               /* --- exchange two attribute values */
  VAL *val, **p, **q;           /* values to exchange, buffer */

  assert(att && (att->type == AT_NOM) /* check function arguments */
  &&    (valid1 >= 0) && (valid1 < att->cnt)
  &&    (valid2 >= 0) && (valid2 < att->cnt));
  p  = att->vals +valid1; val = *p;
  q  = att->vals +valid2;       /* get pointers to values, */
  *p = *q; (*p)->id = valid1;   /* exchange them, and */
  *q = val; val->id = valid2;   /* set new identifiers */
  if      (att->inst.n == valid1) att->inst.n = valid2;
  else if (att->inst.n == valid2) att->inst.n = valid1;
}  /* att_valexg() */           /* adapt instance (current value) */

/*--------------------------------------------------------------------*/

void att_valmove (ATT *att, VALID off, VALID cnt, VALID pos)
{                               /* --- move some attribute values */
  VAL *curr;                    /* current value (instance) */

  assert(att && (att->type == AT_NOM));  /* check function arguments */
  if (pos > att->cnt)      pos = att->cnt;  /* check and adapt */
  if (off > att->cnt)      off = att->cnt;  /* parameters */
  if (cnt > att->cnt -off) cnt = att->cnt -off;
  assert((cnt >= 0) && (off >= 0)
  &&     (pos >= 0) && ((pos <= off) || (pos >= off +cnt)));
  curr = (isnone(att->inst.n))  /* note instance (current value) */
       ? NULL : att->vals[att->inst.n];
  ptr_move(att->vals, (size_t)off, (size_t)cnt, (size_t)pos);
  if (pos <= off) {             /* move values in array */
    cnt += off; off = pos; pos = cnt; }
  for ( ; off < pos; off++)     /* set the new value identifiers and */
    att->vals[off]->id = off;   /* adapt instance (current value) */
  if (curr) att->inst.n = curr->id;
}  /* att_valmove() */

/*--------------------------------------------------------------------*/

int att_valcut (ATT *dst, ATT *src, int mode, ...)
{                               /* --- cut some attribute values */
  VALID   n;                    /* loop variables */
  VALID   off, cnt;             /* range of values to cut */
  VAL     *val, *h, **p;        /* to traverse values and hash bins */
  va_list args;                 /* list of variable arguments */

  assert( src                   /* check the function arguments */
  &&    (!dst || (dst->type == src->type)));

  /* --- numeric attributes --- */
  if (src->type != AT_NOM) {    /* if attribute is numeric */
    if (dst) {                  /* if there is a destination */
      if (src->type == AT_INT){ /* if integer attribute */
        if (src->min.i < dst->min.i) dst->min.i = src->min.i;
        if (src->max.i > dst->max.i) dst->max.i = src->max.i; }
      else {                    /* if float attribute */
        if (src->min.f < dst->min.f) dst->min.f = src->min.f;
        if (src->max.f > dst->max.f) dst->max.f = src->max.f;
      }                         /* adapt range of values */
      if ((dst->valwd[0] > 0) && (src->valwd[0] > dst->valwd[0]))
        dst->valwd[0] = dst->valwd[1] = src->valwd[0];
    }                           /* adapt the value widths */
    if (src->type == AT_INT) {  /* if integer attribute */
      src->min.i  = DTINT_MAX;  /* clear the range of values */
      src->max.i  = DTINT_MIN;  /* and the attribute instance */
      src->inst.i = NV_INT; }   /* (current value) */
    else {                      /* if float attribute */
      src->min.f  = DTFLT_MAX;  /* clear the range of values */
      src->max.f  = DTFLT_MIN;  /* and the attribute instance */
      src->inst.f = NV_FLT;     /* (current value) */
    }
    src->valwd[0] = src->valwd[1] = 0;
    return 0;                   /* clear the value widths */
  }                             /* and return 'ok' */

  /* --- nominal attributes: get range of values --- */
  if (mode & AS_RANGE) {        /* if an index range is given */
    va_start(args, mode);       /* start variable argument evaluation */
    off = va_arg(args, VALID);  /* get offset to first value */
    cnt = va_arg(args, VALID);  /* and number of values */
    va_end(args);               /* end variable argument evaluation */
    n = src->cnt -off;          /* check and adapt the */
    if (cnt > n) cnt = n; }     /* number of values */
  else {                        /* if no index range is given, */
    off = 0;                    /* get the full index range */
    cnt = src->cnt;             /* (cover all values) */
  }                             /* afterward check range params. */
  assert((off >= 0) && (cnt >= 0));
  if (cnt <= 0) return 0;       /* if range is empty, abort */
  if (dst && (att_resize(dst, dst->cnt +cnt) != 0))
    return -1;                  /* resize array and hash table */
  n = src->inst.n;              /* adapt instance (current value) */
  if (n >= off) src->inst.n = (n-cnt >= off) ? n-cnt : NV_NOM;
  cnt += off;                   /* get end index of value range */

  /* --- cut source values --- */
  for (n = off; off < cnt; off++) {
    val = src->vals[off];       /* traverse the range of values */
    p = src->htab +val->hash % src->size;
    while (*p != val) p = &(*p)->succ;
    *p = val->succ;             /* remove value from the hash bin */
    if (!dst) {                 /* if there is no destination, */
      free(val); continue; }    /* simply delete the value */
    for (h = dst->htab[val->hash % dst->size]; h; h = h->succ)
      if (strcmp(val->name, h->name) == 0)
        break;                  /* search value in destination */
    if (h) {                    /* if value is in destination, */
      free(val); continue; }    /* simply delete it */
    p = dst->htab +val->hash % dst->size;
    val->succ = *p; *p = val;   /* insert value into hash table */
    dst->vals[dst->cnt] = val;  /* and value array of destination */
    val->id = dst->cnt++;       /* set the new value identifier */
  }                             /* (in the destination attribute) */
  while (off < src->cnt) {      /* traverse the remaining values */
    src->vals[n] = val = src->vals[off++];
    val->id = n++;              /* shift remaining values left/down */
  }                             /* and set their new identifiers */
  src->cnt   = n;               /* adapt number of values */
  src->max.n = n-1;             /* and maximal identifier */
  src->valwd[0] = 0;            /* invalidate value widths and */
  att_resize(src, 0);           /* try to shrink the value array */
  if (dst) {                    /* if there is a destination */
    dst->max.n = dst->cnt-1;    /* adapt maximal value identifier */
    dst->valwd[0] = 0;          /* and invalidate the value widths */
    att_resize(dst, 0);         /* try to shrink the value array */
  }
  return 0;                     /* return 'ok' */
}  /* att_valcut() */

/*--------------------------------------------------------------------*/

int att_valcopy (ATT *dst, const ATT *src, int mode, ...)
{                               /* --- copy some attribute values */
  VALID   n;                    /* loop variable, buffer */
  VALID   off, cnt;             /* range of values to copy */
  VAL     *val, *h, **d;        /* to traverse values and hash bins */
  va_list args;                 /* list of variable arguments */

  assert(src && dst             /* check the function arguments */
  &&    (dst->type == src->type));

  /* --- numeric attributes --- */
  if (src->type != AT_NOM) {    /* if attribute is numeric */
    if (src->type == AT_INT) {  /* if integer attribute */
      if (src->min.i < dst->min.i) dst->min.i = src->min.i;
      if (src->max.i > dst->max.i) dst->max.i = src->max.i; }
    else {                      /* if float attribute */
      if (src->min.f < dst->min.f) dst->min.f = src->min.f;
      if (src->max.f > dst->max.f) dst->max.f = src->max.f;
    }                           /* adapt range of values */
    if ((dst->valwd[0] > 0) && (src->valwd[0] > dst->valwd[0]))
      dst->valwd[0] = dst->valwd[1] = src->valwd[0];
    return 0;                   /* adapt the value widths */
  }                             /* and return 'ok' */

  /* --- nominal attributes: get range of values --- */
  if (mode & AS_RANGE) {        /* if an index range is given */
    va_start(args, mode);       /* start variable argument evaluation */
    off = va_arg(args, VALID);  /* get offset to first value */
    cnt = va_arg(args, VALID);  /* and number of values */
    va_end(args);               /* end variable argument evaluation */
    n = src->cnt -off;          /* check and adapt the */
    if (cnt > n) cnt = n; }     /* number of values */
  else {                        /* if no index range is given, */
    off = 0;                    /* get the full index range */
    cnt = src->cnt;             /* (cover all values) */
  }                             /* afterward check range params. */
  assert((off >= 0) && (cnt >= 0));
  if (cnt <= 0) return 0;       /* if range is empty, abort */
  if (att_resize(dst, dst->cnt +cnt) != 0)
    return -1;                  /* resize array and hash table */
  cnt += off;                   /* get end index of value range */

  /* --- copy source values --- */
  for (d = dst->vals +(n = dst->cnt); off < cnt; off++) {
    val = src->vals[off];       /* traverse the range of values */
    for (h = dst->htab[val->hash % dst->size]; h; h = h->succ)
      if (strcmp(val->name, h->name) == 0)
        break;                  /* search value in destination */
    if (h) continue;            /* if value already exists, skip it */
    *d = (VAL*)malloc(sizeof(VAL) +strlen(val->name) *sizeof(char));
    if (!*d) break;             /* allocate memory for a new value */
    strcpy((*d)->name, val->name);
    (*d)->hash = val->hash;     /* copy value name and hash value */
    (*d++)->id = n++;           /* and set the value identifier */
  }
  if (off < cnt) {              /* if an error occured */
    while (--n >= dst->cnt) free(*--d);
    return -1;                  /* delete all copied values */
  }                             /* and abort the function */

  /* --- insert values into destination --- */
  while (dst->cnt < n) {        /* traverse the copied values */
    val = dst->vals[dst->cnt++];
    d = dst->htab +val->hash % dst->size;
    val->succ = *d; *d = val;   /* insert values into hash table */
  }                             /* of the containing attribute */
  dst->max.n = n-1;             /* adapt maximal value identifier */
  dst->valwd[0] = 0;            /* and invalidate the value widths */
  att_resize(dst, 0);           /* try to shrink the value array */
  return 0;                     /* return 'ok' */
}  /* att_valcopy() */

/*--------------------------------------------------------------------*/

void att_valsort (ATT *att, int dir, VAL_CMPFN cmpfn,
                  VALID *map, int mapdir)
{                               /* --- sort attribute values */
  VALID  k;                     /* loop variable */
  VAL    *curr;                 /* current value of attribute */
  VALCMP vc;                    /* comparison function object */

  assert(att && (att->type == AT_NOM));  /* check function arguments */
  curr = (att->inst.n >= 0)     /* note instance (current value) */
       ? att->vals[att->inst.n] : NULL;
  vc.cmpfn = cmpfn;             /* sort the attribute values */
  ptr_qsort(att->vals, (size_t)att->cnt, dir, valcmp, (void*)&vc);
  if (map) {                    /* if an identifier map is requested */
    if (mapdir < 0)             /* if backward map (i.e. new -> old) */
      for (k = 0; k < att->cnt; k++) map[k] = att->vals[k]->id;
    else                        /* if forward  map (i.e. old -> new) */
      for (k = 0; k < att->cnt; k++) map[att->vals[k]->id] = k;
  }                             /* (build identifier map) */
  for (k = 0; k < att->cnt; k++)
    att->vals[k]->id = k;       /* set the new value identifiers */
  if (curr) att->inst.n = curr->id;
}  /* att_valsort() */          /* adapt instance (current value) */

/*--------------------------------------------------------------------*/

int att_valwd (ATT *att, int scform)
{                               /* --- determine widths of values */
  VALID  k;                     /* loop variable */
  size_t w, x;                  /* value name widths */

  assert(att);                  /* check the function argument */
  if (att->valwd[0] <= 0) {     /* if the value widths are invalid */
    att->valwd[0] = att->valwd[1] = 0;
    for (k = 0; k < att->cnt; k++) {
      w = scn_fmtlen(att->vals[k]->name, &x);
      if ((int)w > att->valwd[0]) att->valwd[0] = (int)w;
      if ((int)x > att->valwd[1]) att->valwd[1] = (int)x;
    }                           /* determine maximal widths and */
  }                             /* return width of widest value */
  return att->valwd[(scform) ? 1 : 0];
}  /* att_valwd() */

/*--------------------------------------------------------------------*/

VALID att_valid (const ATT *att, const char *name)
{                               /* --- get the identifier of a value */
  VAL *val;                     /* to traverse hash bin */

  assert(att                    /* check the function arguments */
  &&     name && (att->type == AT_NOM));
  if (att->cnt <= 0) return NV_NOM;
  for (val = att->htab[hash(name) % att->size]; val; val = val->succ)
    if (strncmp(name, val->name, AS_MAXLEN) == 0)
      break;                    /* if value found, abort loop */
  return (val) ? val->id : NV_NOM;
}  /* att_valid() */            /* return value identifier */

/*----------------------------------------------------------------------
  Attribute Set Functions
----------------------------------------------------------------------*/

static int as_resize (ATTSET *set, ATTID size)
{                               /* --- resize attribute array */
  ATTID k;                      /* loop variable */
  ATT   **att;                  /* to traverse attributes */
  ATT   **p;                    /* to traverse hash bins */

  assert(set);                  /* check the function argument */
  k = (ATTID)set->size;         /* get current array size */
  if (size > 0) {               /* if to enlarge the array */
    if (k >= size) return 0;    /* if array is large enough, abort */
    k += (k > BLKSIZE) ? k >> 1 : BLKSIZE;
    if (k >  size) size = k; }  /* compute new array size */
  else {                        /* if to shrink the array */
    size = set->cnt << 1;       /* get maximal tolerable size */
    if (size < BLKSIZE) size = BLKSIZE;
    if (k <= size) return 0;    /* if array is small enough, abort */
    size = set->cnt +(set->cnt >> 1);
    if (size < BLKSIZE) size = BLKSIZE;
  }                             /* compute new array size */
  att = (ATT**)realloc(set->atts, 2*(size_t)size *sizeof(ATT*));
  if (!att) return -1;          /* resize attribute array */
  set->atts = att;              /* set the new attribute array */
  set->htab = att +size;        /* and the new hash table */
  set->size = (size_t)size;     /* clear the hash bins bins */
  memset(set->htab, 0, set->size *sizeof(ATT*));
  for (k = 0; k < set->cnt; k++) { /* traverse the attributes */
    p = set->htab +att[k]->hash % set->size;
    att[k]->succ = *p;          /* get pointer to hash bin and */
    *p = att[k];                /* insert attribute into hash table */
  }                             /* (at the head of the bin list) */
  return 0;                     /* return 'ok' */
}  /* as_resize() */

/*--------------------------------------------------------------------*/

ATTSET* as_create (const char *name, ATT_DELFN delfn)
{                               /* --- create an attribute set */
  ATTSET *set;                  /* created attribute set */

  assert(name && *name && delfn);  /* check the function arguments */
  set = (ATTSET*)malloc(sizeof(ATTSET));
  if (!set) return NULL;        /* allocate memory for an att. set */
  set->name = (char*)malloc((size_t)(length(name)+1) *sizeof(char));
  if (!set->name) { free(set); return NULL; }
  copy(set->name, name);        /* copy attribute set name */
  set->size   = 0;              /* initialize the fields */
  set->cnt    = 0;
  set->sd2p   = 6;              /* (set default behavior of %g) */
  set->htab   = set->atts = NULL;
  set->delfn  = delfn;
  set->wgt    = 1.0;
  set->fldcnt = set->fldsize = 0;
  set->flds   = NULL;           /* clear field map */
  set->err    = 0;
  set->str    = NULL;
  set->trd    = NULL;
  return set;                   /* return created attribute set */
}  /* as_create() */

/*--------------------------------------------------------------------*/

ATTSET* as_clone (const ATTSET *set)
{                               /* --- clone an attribute set */
  ATTSET *clone;                /* created clone */

  assert(set);                  /* check the function argument */
  clone = as_create(set->name, set->delfn);
  if (!clone) return NULL;      /* create a new attribute set */
  if (as_attcopy(clone, set, AS_ALL) != 0) {
    as_delete(clone); return NULL; }
  clone->sd2p = set->sd2p;      /* copy all attributes and */
  clone->wgt  = set->wgt;       /* all other information */
  if (set->flds) {              /* if there is a field array */
    clone->flds = (ATTID*)malloc((size_t)set->fldsize *sizeof(ATTID));
    if (!clone->flds) { as_delete(clone); return NULL; }
    memcpy(clone->flds, set->flds, (size_t)set->fldcnt *sizeof(ATTID));
  }                             /* create and copy the field array */
  return clone;                 /* return created clone */
}  /* as_clone() */

/*--------------------------------------------------------------------*/

void as_delete (ATTSET *set)
{                               /* --- delete an attribute set */
  ATTID k;                      /* loop variable */
  ATT   **p;                    /* to traverse the attribute array */

  assert(set);                  /* check the function argument */
  if (set->atts) {              /* if there are attributes, */
    for (p = set->atts +(k = set->cnt); --k >= 0; ) {
      (*--p)->set = NULL; (*p)->id = -1; set->delfn(*p); }
    free(set->atts);            /* delete attributes, array, */
  }                             /* hash table, and field map */
  if (set->flds) free(set->flds);
  free(set->name);              /* delete attribute set name */
  free(set);                    /* and attribute set body */
}  /* as_delete() */

/*--------------------------------------------------------------------*/

int as_rename (ATTSET *set, const char *name)
{                               /* --- rename an attribute set */
  char *s;                      /* new name for attribute */

  assert(set && name && *name); /* check the function arguments */
  s = (char*)realloc(set->name, (size_t)(length(name)+1) *sizeof(char));
  if (!s) return -1;            /* reallocate memory block */
  set->name = copy(s, name);    /* and copy the new name */
  return 0;                     /* return 'ok' */
}  /* as_rename() */

/*--------------------------------------------------------------------*/

int as_cmp (const ATTSET *set1, const ATTSET *set2)
{                               /* --- compare two attribute sets */
  ATTID k;                      /* loop variable, attribute index */

  assert(set1 && set2);         /* check the function arguments */
  if (set1->cnt != set2->cnt)   /* compare number of attributes */
    return 1;                   /* and abort if they differ */
  for (k = 0; k < set1->cnt; k++)    /* traverse all attributes */
    if (att_cmp(set1->atts[k], set2->atts[k]) != 0)
      return 1;                 /* if an attribute differs, abort */
  return 0;                     /* otherwise return 'equal' */
}  /* as_cmp() */

/*--------------------------------------------------------------------*/

void as_setdir (ATTSET *set, int dir)
{                               /* --- set the attribute directions */
  ATTID k;                      /* loop variable */

  assert(set);                  /* check the function arguments */
  for (k = set->cnt; --k >= 0;) /* traverse the attributes */
    set->atts[k]->dir = dir;    /* and set their directions */
}  /* as_setdir() */

/*--------------------------------------------------------------------*/

void as_setmark (ATTSET *set, ATTID mark)
{                               /* --- set the attribute markers */
  ATTID k;                      /* loop variable */

  assert(set);                  /* check the function arguments */
  for (k = set->cnt; --k >= 0;) /* traverse the attributes */
    set->atts[k]->mark = mark;  /* and set their directions */
}  /* as_setmark() */

/*--------------------------------------------------------------------*/

void as_save (ATTSET *set, ATTID *marks)
{                               /* --- save the attribute markers */
  ATTID k;                      /* loop variable */

  assert(set && marks);         /* check the function arguments */
  for (k = 0; k < set->cnt; k++)/* copy markers to given array */
    marks[k] = set->atts[k]->mark;
}  /* as_save() */

/*--------------------------------------------------------------------*/

void as_restore (ATTSET *set, ATTID *marks)
{                               /* --- restore the attribute markers */
  ATTID k;                      /* loop variable */

  assert(set && marks);         /* check the function arguments */
  for (k = 0; k < set->cnt; k++)/* copy markers to given array */
    set->atts[k]->mark = marks[k];
}  /* as_restore() */

/*--------------------------------------------------------------------*/

int as_attadd (ATTSET *set, ATT *att)
{                               /* --- add one attribute */
  ATT **p, *h;                  /* to traverse the hash bin */

  assert(set && att);           /* check the function arguments */
  if (as_resize(set, set->cnt +1) != 0)
    return -1;                  /* resize the attribute array */
  p = set->htab +att->hash % set->size;
  for (h = *p; h; h = h->succ)  /* traverse the hash bin list */
    if (strcmp(att->name, h->name) == 0)
      return 1;                 /* if name already exists, abort */
  if (att->set)                 /* remove attribute from old set */
    as_attrem(att->set, att->id);
  att->succ = *p; *p = att;     /* insert attribute into hash table */
  set->atts[set->cnt] = att;    /* and attribute array */
  att->id   = set->cnt++;       /* set attribute identifier */
  att->set  = set;              /* and containing attribute set */
  return 0;                     /* return 'ok' */
}  /* as_attadd() */

/*--------------------------------------------------------------------*/

int as_attaddm (ATTSET *set, ATT **atts, ATTID cnt)
{                               /* --- add several attributes */
  ATTID k;                      /* loop variable */
  ATT   *att;                   /* to traverse the attributes */
  ATT   **p, *h;                /* to traverse the hash bin */

  assert(set && atts && (cnt >= 0));  /* check function arguments */
  if (as_resize(set, set->cnt +cnt) != 0)
    return -1;                  /* resize the attribute array */
  for (k = 0; k < cnt; k++) {   /* traverse new attributes */
    att = atts[k];              /* get next attribute */
    for (h = set->htab[att->hash % set->size]; h; h = h->succ)
      if (strcmp(att->name, h->name) == 0)
        return -2;              /* if name already exists, */
  }                             /* abort the function */
  for (k = 0; k < cnt; k++) {   /* traverse new attributes again */
    att = atts[k];              /* get next attribute and */
    if (att->set)               /* remove it from old set */
      as_attrem(att->set, att->id);
    p = set->htab +att->hash % set->size;
    att->succ = *p; *p = att;   /* insert attribute into hash table */
    set->atts[set->cnt] = att;  /* and attribute array */
    att->id  = set->cnt++;      /* set attribute identifier */
    att->set = set;             /* and containing attribute set */
  }
  return 0;                     /* return 'ok' */
}  /* as_attaddm() */

/*--------------------------------------------------------------------*/

ATT* as_attrem (ATTSET *set, ATTID attid)
{                               /* --- remove an attribute */
  ATTID k;                      /* loop variable */
  ATT   *att;                   /* attribute to remove */
  ATT   **p;                    /* to traverse the hash bin */

  assert(set && (attid < set->cnt));  /* check function arguments */

  /* --- remove all attributes --- */
  if (attid < 0) {              /* if no attribute identifier given */
    if (!set->atts) return NULL;/* if there are no attributes, abort */
    for (p = set->atts +(attid = set->cnt); --attid >= 0; ) {
      (*--p)->set = NULL; (*p)->id = -1; set->delfn(*p); }
    free(set->atts);            /* delete all attributes and */
    set->atts = set->htab = NULL;  /* delete attribute array */
    set->size = 0;              /* clear the array size */
    set->cnt  = 0;              /* and the attribute counter */
    return NULL;                /* abort the function */
  }                             /* (no attribute can be returned) */

  /* --- remove one attribute --- */
  att = set->atts[attid];       /* get the attribute to remove */
  p   = set->htab +att->hash % set->size;
  while (*p != att) p = &(*p)->succ;
  *p = att->succ;               /* remove attribute from hash table */
  att->set = NULL;              /* clear reference to containing set */
  att->id  = -1;                /* and attribute identifier */
  for (k = attid; k < set->cnt; k++) {
    set->atts[k] = att = set->atts[k+1];
    att->id = k;                /* shift the following values */
  }                             /* and adapt their identifiers */
  as_resize(set, 0);            /* try to shrink attribute array */
  DEL_FLDS(set);                /* and delete the field map */
  return att;                   /* return removed attribute */
}  /* as_attrem() */

/*--------------------------------------------------------------------*/

void as_attexg (ATTSET *set, ATTID attid1, ATTID attid2)
{                               /* --- exchange two attributes */
  ATT *att, **p, **q;           /* attributes to exchange, buffer */

  assert(set                    /* check the function arguments */
  &&    (attid1 >= 0) && (attid1 < set->cnt)
  &&    (attid2 >= 0) && (attid2 < set->cnt));
  p = set->atts +attid1; att = *p;
  q = set->atts +attid2;        /* get pointers to attributes, */
  *p = *q; (*p)->id = attid1;   /* exchange them, and */
  *q = att; att->id = attid2;   /* set new identifiers */
  DEL_FLDS(set);                /* delete field map */
}  /* as_attexg() */

/*--------------------------------------------------------------------*/

void as_attmove (ATTSET *set, ATTID off, ATTID cnt, ATTID pos)
{                               /* --- move some attributes */
  assert(set);                  /* check the function argument */
  if (pos > set->cnt)      pos = set->cnt;  /* check and adapt */
  if (off > set->cnt)      off = set->cnt;  /* parameters */
  if (cnt > set->cnt -off) cnt = set->cnt -off;
  assert((cnt >= 0) && (off  >= 0)
  &&     (pos >= 0) && ((pos <= off) || (pos >= off +cnt)));
  ptr_move(set->atts, (size_t)off, (size_t)cnt, (size_t)pos);
  if (pos <= off) {             /* move attributes in array */
    cnt += off; off = pos; pos = cnt; }
  for ( ; off < pos; off++)     /* traverse affected attributes */
    set->atts[off]->id = off;   /* and set their new identifiers */
  DEL_FLDS(set);                /* delete field map */
}  /* as_attmove() */

/*--------------------------------------------------------------------*/

void as_attperm (ATTSET *set, ATTID *perm)
{                               /* --- permute attributes */
  ATTID i;                      /* loop variable for attributes */
  ATT   **p;                    /* to traverse hash bins */

  assert(set && perm);          /* check the function arguments */
  memcpy(set->htab, set->atts, (size_t)set->cnt *sizeof(ATT*));
  for (i = 0; i < set->cnt; i++)/* permute the attributes */
    set->atts[i] = set->htab[perm[i]];
  memset(set->htab, 0, set->size *sizeof(ATT*));
  for (i = 0; i < set->cnt; i++) { /* traverse the attributes */
    set->atts[i]->id = i;       /* set the new attribute identifier */
    p = set->htab +set->atts[i]->hash % set->size;
    set->atts[i]->succ = *p;    /* get pointer to hash bin and */
    *p = set->atts[i];          /* insert attribute into hash table */
  }                             /* (rebuild the hash table) */
}  /* as_attperm() */

/*--------------------------------------------------------------------*/

int as_attcut (ATTSET *dst, ATTSET *src, int mode, ...)
{                               /* --- cut/copy attributes */
  ATTID   n;                    /* loop variable, buffer */
  ATTID   off, cnt;             /* range of attributes to cut */
  ATT     *att, *h, **p;        /* to traverse atts. and hash bins */
  va_list args;                 /* list of variable arguments */

  assert(src);                  /* check the function arguments */

  /* --- get range of attributes --- */
  if (mode & AS_RANGE) {        /* if an index range is given, */
    va_start(args, mode);       /* start variable argument evaluation */
    off = va_arg(args, ATTID);  /* get offset to first attribute */
    cnt = va_arg(args, ATTID);  /* and number of attributes */
    va_end(args);               /* end variable argument evaluation */
    n = src->cnt -off;          /* check and adapt the */
    if (cnt > n) cnt = n; }     /* number of attributes */
  else {                        /* if no index range is given, */
    off = 0;                    /* get the full index range */
    cnt = src->cnt;             /* (cover all attributes) */
  }                             /* afterward check range params. */
  assert((off >= 0) && (cnt >= 0));
  if (cnt <= 0) return 0;       /* if range is empty, abort */
  if (dst && (as_resize(dst, dst->cnt +cnt) != 0))
    return -1;                  /* resize array and hash table */
  cnt += off;                   /* get end index of attribute range */

  /* --- cut source attributes --- */
  for (n = off; off < cnt; off++) {
    att = src->atts[off];       /* traverse the range of attributes */
    if ((mode & AS_MARKED)      /* if in marked mode and attribute */
    &&  (att->mark < 0)) {      /* is not marked, merely shift it */
      src->atts[att->id = n++] = att; continue; }
    p = src->htab +att->hash % src->size;
    while (*p != att) p = &(*p)->succ;
    *p = att->succ;             /* remove att. from the hash table */
    att->set = dst;             /* set/clear the att. set reference */
    if (!dst) {                 /* if no dest., simply delete att. */
      att->id = -1; src->delfn(att); continue; }
    for (h = dst->htab[att->hash % dst->size]; h; h = h->succ)
      if (strcmp(att->name, h->name) == 0)
        break;                  /* search attribute in destination */
    if (h) {                    /* if att. is in dest., delete it */
      att->set = NULL; att->id = -1; src->delfn(att); continue; }
    p = dst->htab +att->hash % dst->size;
    att->succ = *p; *p = att;   /* insert attribute into hash table */
    dst->atts[att->id = dst->cnt++] = att;
  }                             /* store attribute in attribute array */
  while (off < src->cnt) {      /* traverse the remaining attributes */
    src->atts[n] = att = src->atts[off++];
    att->id = n++;              /* shift attributes left/down */
  }                             /* and set their new identifiers */
  src->cnt = n;                 /* set the new number of attributes */
  DEL_FLDS(src);                /* delete the field map and */
  as_resize(src, 0);            /* try to shrink source att. array */
  if (dst) as_resize(dst, 0);   /* try to shrink dest.  att. array */
  return 0;                     /* return 'ok' */
}  /* as_attcut() */

/*--------------------------------------------------------------------*/

int as_attcopy (ATTSET *dst, const ATTSET *src, int mode, ...)
{                               /* --- cut/copy attributes */
  ATTID   n;                    /* loop variable, buffer */
  ATTID   off, cnt;             /* range of attributes to copy */
  ATT     *att, *h, **d;        /* to traverse atts. and hash bins */
  va_list args;                 /* list of variable arguments */

  assert(src && dst);           /* check the function arguments */

  /* --- get range of attributes --- */
  if (mode & AS_RANGE) {        /* if an index range is given, */
    va_start(args, mode);       /* start variable argument evaluation */
    off = va_arg(args, ATTID);  /* get offset to first attribute */
    cnt = va_arg(args, ATTID);  /* and number of attributes */
    va_end(args);               /* end variable argument evaluation */
    n = src->cnt -off;          /* check and adapt the */
    if (cnt > n) cnt = n; }     /* number of attributes */
  else {                        /* if no index range is given, */
    off = 0;                    /* get the full index range */
    cnt = src->cnt;             /* (cover all attributes) */
  }                             /* afterward check range params. */
  assert((off >= 0) && (cnt >= 0));
  if (cnt <= 0) return 0;       /* if range is empty, abort */
  if (as_resize(dst, dst->cnt +cnt) != 0)
    return -1;                  /* resize array and hash table */
  cnt += off;                   /* get end index of attribute range */

  /* --- copy source attributes --- */
  for (d = dst->atts +(n = dst->cnt); off < cnt; off++) {
    att = src->atts[off];       /* traverse the range of attributes */
    if ((mode & AS_MARKED)      /* if in marked mode */
    &&  (att->mark < 0))        /* and attribute is not marked, */
      continue;                 /* skip this attribute */
    for (h = dst->htab[att->hash % dst->size]; h; h = h->succ)
      if (strcmp(att->name, h->name) == 0)
        break;                  /* search attribute in destination */
    if (h) continue;            /* if att. exists in dest., skip it */
    *d = att_clone(att);        /* otherwise clone */
    if (!*d) break;             /* the source attribute */
    (*d)->set  = dst;           /* set the attribute set reference */
    (*d++)->id = n++;           /* and the attribute identifier */
  }
  if (off < cnt) {              /* if an error occured */
    while (--n >= dst->cnt) att_delete(*--d);
    return -1;                  /* delete all copied attributes */
  }                             /* and abort the function */

  /* --- insert attributes into destination --- */
  while (dst->cnt < n) {        /* traverse the copied attributes */
    att = dst->atts[dst->cnt++];
    d = dst->htab +att->hash % dst->size;
    att->succ = *d; *d = att;   /* insert attributes into hash table */
  }                             /* of the containing attribute set */
  as_resize(dst, 0);            /* try to shrink the attribute array */
  return 0;                     /* return 'ok' */
}  /* as_attcopy() */

/*--------------------------------------------------------------------*/

ATTID as_attid (const ATTSET *set, const char *name)
{                               /* --- get the id of an attribute */
  ATT *att;                     /* to traverse hash bin list */

  assert(set && name);          /* check the function arguments */
  if (set->cnt <= 0) return -1; /* if set is empty, abort */
  for (att = set->htab[hash(name) % set->size]; att; att = att->succ)
    if (strncmp(name, att->name, AS_MAXLEN) == 0)
      break;                    /* if attribute found, abort loop */
  return (att) ? att->id : -1;  /* return attribute identifier */
}  /* as_attid() */

/*--------------------------------------------------------------------*/

ATTID as_target (ATTSET *set, const char *name, int dirs)
{                               /* --- get/set a target attribute */
  ATTID i, n = -1;              /* loop variables for attributes */
  ATT   *att, *trg, **p;        /* to traverse atts. and hash bins */

  assert(set);                  /* check the function arguments */
  if (name) {                   /* if a target name is given, */
    n = as_attid(set, name);    /* find the target attribute */
    if (n < 0) return -1; }     /* and get its identifier */
  else if (dirs) {              /* if to find target from directions */
    if (set->cnt <= 0) return -1;
    for (i = 0; i < set->cnt; i++) {
      if (set->atts[i]->dir != DIR_OUT) continue;
      if (n >= 0) return -1;    /* use the output attribute */
      n = i;                    /* (there must be at most one) */
    }                           /* as the target attribute */
    if ((n < 0) && (dirs > 0))  /* if there is no output attribute, */
      n = set->cnt-1;           /* but a target is required, */
  }                             /* simply use the last attribute */
  if (n < 0) trg = NULL;        /* if no target, clear variable */
  else {                        /* if to use a target attribute, */
    trg = set->atts[n];         /* get the target attribute and */
    trg->dir = DIR_OUT;         /* make it an output attribute */
  }
  for (i = n = 0; i < set->cnt; i++) {
    att = set->atts[i];         /* keep inputs and target attribute */
    if ((att == trg) || (att->dir == DIR_IN)) {
      set->atts[att->id = n++] = att; continue; }
    p = set->htab +att->hash % set->size;
    while (*p != att) p = &(*p)->succ;
    *p = att->succ;             /* for all other attributes: */
    att->id = -1;               /* remove att. from the hash table */
    set->delfn(att);            /* invalidate attribute identifier */
  }                             /* and delete the attribute */
  set->cnt = n;                 /* set the new number of attributes */
  DEL_FLDS(set);                /* delete the field map and */
  as_resize(set, 0);            /* try to shrink source att. array */
  return (trg) ? trg->id : n;   /* return the target identifier */
}  /* as_target() */

/*--------------------------------------------------------------------*/

void as_apply (ATTSET *set, ATT_APPFN appfn, void *data)
{                               /* --- apply a function to all atts. */
  ATTID k;                      /* loop variable */

  assert(set && appfn);         /* check the function arguments */
  for (k = 0; k < set->cnt; k++)/* traverse the attributes and */
    appfn(set->atts[k], data);  /* apply the function to them */
}  /* as_apply() */

/*----------------------------------------------------------------------
  Debug Function
----------------------------------------------------------------------*/
#ifndef NDEBUG

void as_stats (const ATTSET *set)
{                               /* --- compute and print statistics */
  ATTID  i;                     /* loop variable */
  size_t k;                     /* loop variable */
  size_t cnt;                   /* number of attributes/values */
  size_t size;                  /* size of hash table */
  size_t used;                  /* number of used hash bins */
  size_t len;                   /* length of current bin list */
  size_t min, max;              /* min. and max. bin list length */
  size_t lcs[10];               /* counter for bin list lengths */
  const ATT *att = NULL;        /* to traverse attributes */
  const VAL *val = NULL;        /* to traverse values */

  assert(set);                  /* check for a valid attribute set */
  if (set->size <= 0) return;   /* check hash table size */
  for (i = -1; i < set->cnt; i++) {
    min = DTINT_MAX; max = used = 0;
    memset(lcs, 0, sizeof(lcs));/* initialize the variables */
    if (i < 0) {                /* statistics for attribute set */
      printf("attribute set \"%s\"\n", set->name);
      size = (size_t)set->size; /* get hash table size */
      cnt  = (size_t)set->cnt;} /* and number of attributes */
    else {                      /* statistics for an attribute */
      att = set->atts[i];       /* get attribute and check it */
      if ((att->type != AT_NOM) || (att->size <= 0)) continue;
      printf("\nattribute \"%s\"\n", att->name);
      size = (size_t)att->size; /* print attribute name */
      cnt  = (size_t)att->cnt;  /* and get hash table size */
    }                           /* and number of values */
    for (k = 0; k < size; k++){ /* traverse the bin array */
      len = 0;                  /* determine bin list length */
      if (i < 0) for (att = set->htab[k]; att; att = att->succ) len++;
      else       for (val = att->htab[k]; val; val = val->succ) len++;
      if (len > 0)   used++;    /* count used hash bins */
      if (len < min) min = len; /* determine minimal and */
      if (len > max) max = len; /* maximal list length */
      lcs[(len >= 9) ? 9 : len]++;
    }                           /* count list length */
    printf("number of objects  : %"SIZE_FMT"\n", cnt);
    printf("number of hash bins: %"SIZE_FMT"\n", size);
    printf("used hash bins     : %"SIZE_FMT"\n", used);
    printf("minimal list length: %"SIZE_FMT"\n", min);
    printf("maximal list length: %"SIZE_FMT"\n", max);
    printf("average list length: %g\n", (double)cnt/(double)size);
    printf("ditto, of used bins: %g\n", (double)cnt/(double)used);
    printf("length distribution:\n");
    for (k = 0; k < 9; k++) printf("%3"SIZE_FMT" ", k);
    printf(" >8\n");
    for (k = 0; k < 9; k++) printf("%3"SIZE_FMT" ", lcs[k]);
    printf("%3"SIZE_FMT"\n", lcs[9]);
  }
}  /* as_stats() */

#endif  /* #ifndef NDEBUG */
