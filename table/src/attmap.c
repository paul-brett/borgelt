/*----------------------------------------------------------------------
  File    : attmap.c
  Contents: attribute map management (for nominal to numeric coding)
  Author  : Christian Borgelt
  History : 2003.08.11 file created
            2003.08.12 function am_cnt added
            2004.08.10 bug concerning marked operation fixed
            2004.08.11 extended to target attribute handling
            2007.01.10 function am_target and mode AM_BIN2COL added
            2007.02.13 adapted to redesigned module attset
            2007.05.16 use of 1-in-n value corrected and extended
            2007.07.10 bug in am_exec() fixed (binary attributes)
            2013.06.10 bug in am_exec() fixed (integer null value)
            2013.07.18 adapted to general types ATTID, TPLID etc.
            2015.08.02 function am_mark() added (mark mapped attributes)
            2015.11.30 mode AM_MINUS1 added (for 1-in-(n-1) encoding)
            2016.02.15 function am_clone() added (clone a mapping)
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "attmap.h"

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

ATTMAP* am_create (ATTSET *attset, int mode, double one)
{                               /* --- create an attribute map */
  ATTID  i, k, n;               /* loop variables, offset */
  ATTMAP *map;                  /* created attribute map */
  ATT    *att;                  /* to traverse the attributes */
  AMEL   *p;                    /* to traverse the map elements */

  assert(attset);               /* check the function arguments */
  for (k = 0, i = n = as_attcnt(attset); --i >= 0; )
    if (!(mode & AM_MARKED) || (att_getmark(as_att(attset, i)) >= 0))
      k++;                      /* count the attributes to map */
  map = (ATTMAP*)malloc(sizeof(ATTMAP) +(size_t)(k-1) *sizeof(AMEL));
  if (!map) return NULL;        /* create an attribute map */
  map->attset = attset;         /* note the attribute set, */
  map->attcnt = k;              /* the number of mapped attributes, */
  map->one    = one;            /* and the value for 1-in-n */
  for (p = map->amels, i = k = 0; i < n; i++) {
    att = as_att(attset, i);    /* traverse the attributes */
    if ((mode & AM_MARKED) && (att_getmark(att) < 0))
      continue;                 /* skip unmarked attributes */
    p->att  = att;              /* note the attribute */
    p->off  = k;                /* and  the current offset */
    p->type = att_type(att);    /* get the attribute type */
    if (p->type != AT_NOM)      /* integer and float attributes */
      p->cnt = 1;               /* need only one column */
    else {                      /* if nominal attribute */
      p->cnt = att_valcnt(p->att);
      if ((mode & AM_MINUS1) || ((p->cnt == 2) && !(mode & AM_BIN2COL)))
        p->cnt -= 1;            /* use number of values unless binary */
    }                           /* and not two columns requested */
    k += p->cnt; p++;           /* advance the offset by the number */
  }                             /* of dimensions mapped to */
  map->incnt  = k;              /* note the number of inputs */
  map->outcnt = 0;              /* and  the number of outputs */
  return map;                   /* return the created attribute map */
}  /* am_create() */

/*--------------------------------------------------------------------*/

void am_delete (ATTMAP *map, int delas)
{                               /* --- delete an attribute map */
  assert(map);                  /* check the function arguments */
  if (delas) as_delete(map->attset);
  free(map);                    /* delete attribute set and map */
}  /* am_delete() */

/*--------------------------------------------------------------------*/

ATTMAP* am_clone (ATTMAP *map, int cloneas)
{                               /* --- clone an attribute map */
  ATTMAP *clone;                /* create clone of attribute map */
  size_t z;                     /* size of the attribute map */
  ATTSET *attset;               /* possible clone of attribute set */

  assert(map);                  /* check the function arguments */
  attset = (cloneas) ? as_clone(map->attset) : map->attset;
  if (!attset) return NULL;     /* clone attribute set if needed */
  z = sizeof(ATTMAP) +(size_t)(map->attcnt-1) *sizeof(AMEL);
  clone = (ATTMAP*)malloc(z);   /* get the memory block size and */
  if (!clone) return NULL;      /* create an attribute map */
  memcpy(clone, map, z);        /* copy the attribute map */
  clone->attset = attset;       /* set possibly cloned attribute set */
  return clone;                 /* return the created clone */
}  /* am_clone() */

/*--------------------------------------------------------------------*/

void am_target (ATTMAP *map, ATTID trgid)
{                               /* --- set a target attribute */
  ATTID i, k;                   /* loop variables */
  AMEL  o, *p;                  /* to traverse the map elements */

  assert(map && (trgid < map->attcnt));
  p = map->amels;               /* check function arguments */
  if (map->outcnt > 0) {        /* if there is an old target */
    o = p[i = map->attcnt-1];   /* note the old target attribute */
    k = att_id(o.att);          /* and get its identifier */
    while ((--i >= 0) && (k < att_id(p[i].att)))
      p[i+1] = p[i];            /* shift up preceding attributes */
    p[i+1] = o;                 /* store old target at proper place */
    map->outcnt = 0;            /* clear the number of outputs */
  }
  if (trgid >= 0) {             /* if to set a new target attribute */
    for (i = map->attcnt; --i >= 0; )
      if (att_id(p[i].att) == trgid) break;
    if (i >= 0) {               /* find the new target attribute */
      o = p[i];                 /* note the new target attribute */
      while (++i < map->attcnt) /* traverse the successor attributes */
        p[i-1] = p[i];          /* and shift them down to close gap */
      p[i-1] = o;               /* store new target as last attribute */
      map->outcnt = o.cnt;      /* set the number of outputs */
    }
  }
  for (i = k = 0; i < map->attcnt; i++) {
    p[i].off = k; k += p[i].cnt; } /* update the mapping offsets */
  map->incnt = k -map->outcnt;  /* compute the number of inputs */
}  /* am_target() */

/*--------------------------------------------------------------------*/

ATTID am_mark (ATTMAP *map)
{                               /* --- mark all mapped attributes */
  ATTID i;                      /* loop variables for attributes */

  assert(map);                  /* check the function argument */
  i = map->attcnt;              /* mark possible target with 0 */
  att_setmark(map->amels[--i].att, (map->outcnt > 0) ? 0 : 1);
  while (--i >= 0)              /* mark the mapped attributes with 1 */
    att_setmark(map->amels[i].att, 1);
  return map->attcnt;           /* return the number of attributes */
}  /* am_mark() */

/*--------------------------------------------------------------------*/

void am_exec (ATTMAP *map, const TUPLE *tpl, int mode, double *vec)
{                               /* --- execute an attribute map */
  ATTID      k;                 /* loop variable */
  VALID      v;                 /* buffer for a nominal value */
  AMEL       *p;                /* to traverse the map elements */
  const INST *inst;             /* to traverse the instantiations */

  assert(map && vec);           /* check the function arguments */
  if (map->outcnt > 0) { k = map->attcnt -1; }
  else                 { k = map->attcnt; mode &= AM_INPUTS; }
  p = map->amels;               /* get the number of input attributes */
  if      (mode & AM_INPUTS) { if (mode & AM_TARGET) k++; }
  else if (mode & AM_TARGET) { p += k; k = 1; }
  else return;                  /* get the attribute range */
  for ( ; --k >= 0; p++) {      /* traverse the attributes */
    inst = (tpl) ? tpl_colval(tpl, att_id(p->att)) : att_inst(p->att);
    if      (p->type == AT_FLT) /* if float attribute, */
      *vec++ = (double)inst->f; /* store the value directly */
    else if (p->type == AT_INT) /* if integer attribute, check null */
      *vec++ = (inst->i <= NV_INT)  ? NAN : (double)inst->i;
    else if (p->cnt < 2) {      /* if the attribute is binary, */
      v = inst->n;              /* set the value directly */
      *vec++ = ((v < 0) || (v > 1)) ? NAN : (double)v *fabs(map->one); }
    else {                      /* if the attribute is nominal */
      memset(vec, 0, (size_t)p->cnt *sizeof(double));
      v = inst->n;              /* clear all vector elements */
      if ((v >= 0) && (v < p->cnt))
        vec[v] = (map->one < 0) ? -map->one /(double)p->cnt : map->one;
      vec += p->cnt;            /* set the corresponding element */
    }                           /* and skip the set elements */
  }                             /* of the output vector */
}  /* am_exec() */
