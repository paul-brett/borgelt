#-----------------------------------------------------------------------
# File    : makefile    (directory: table)
# Contents: build table management modules and utility programs
# Author  : Christian Borgelt
# History : 1995.??.?? file created
#           1997.08.26 makefile splitted into subdirectory makefiles
#           1998.01.04 table reader management added
#           1998.02.08 scanner transferred from parse makefile
#           1998.02.24 skeleton programs transferred from skel makefile
#           1998.02.25 programs tmerge and tsplit added
#           1999.02.05 program tjoin added (join two tables)
#           1999.02.13 program tbal  added (balance class column)
#           1999.04.17 input/output utility functions added
#           2000.11.04 modules arrays, symtab, and tabread made external
#           2001.06.24 modules attest and table split into two files
#           2001.11.08 module tab4vis added (table for visualization)
#           2002.02.22 module attset3 added, as_rwp removed
#           2003.07.22 program tnorm added (normalize numeric columns)
#           2003.08.11 program t1inn added (1-in-n coding of nominals)
#           2008.08.11 adapted to name change from vecops to arrays
#           2010.10.08 changed standard from -ansi to -std=c99
#           2010.12.31 module io.c removed (replaced by table2.c)
#           2011.01.21 program tsort added (sort a data table)
#           2011.08.22 external module random added (from util/src)
#           2016.04.20 creation of dependency files added
#-----------------------------------------------------------------------
SHELL    = /bin/bash
THISDIR  = ../../table/src
UTILDIR  = ../../util/src

CC       = gcc -std=c99
# CC       = g++
CFBASE  = -Wall -Wextra -Wno-unused-parameter -Wconversion \
          -pedantic -c $(ADDFLAGS)
CFLAGS   = $(CFBASE) -DNDEBUG -O3 -funroll-loops
# CFLAGS   = $(CFBASE) -g
# CFLAGS   = $(CFBASE) -g -DSTORAGE
INCS     = -I$(UTILDIR)

LD       = gcc
LDFLAGS  = $(ADDFLAGS)
LIBS     = -lm $(ADDLIBS)

# ADDOBJS  = $(UTILDIR)/storage.o

HDRS     = $(UTILDIR)/fntypes.h $(UTILDIR)/arrays.h   \
           $(UTILDIR)/scanner.h $(UTILDIR)/error.h    \
           $(UTILDIR)/tabread.h $(UTILDIR)/tabwrite.h \
           attset.h table.h
OBJS     = $(UTILDIR)/arrays.o  $(UTILDIR)/escape.o \
           $(UTILDIR)/tabread.o attset1.o $(ADDOBJS)
OBJS1    = $(OBJS)  $(UTILDIR)/scform.o  $(UTILDIR)/tabwrite.o \
           attset2.o table1.o table2.o
OBJS2    = $(OBJS)  $(UTILDIR)/scanner.o $(UTILDIR)/tabwrite.o \
           attset2.o attset3.o table1.o table2.o
DOM_O    = $(OBJS)  $(UTILDIR)/scform.o \
           as_read.o as_desc.o dom.o
OPC_O    = $(OBJS1) table3.o opc.o
TSORT_O  = $(OBJS2) tsort.o
TMERGE_O = $(OBJS1) tmerge.o
TSPLIT_O = $(OBJS1) $(UTILDIR)/random.o tsplit.o
TJOIN_O  = $(OBJS1) tjoin.o
TBAL_O   = $(OBJS1) tbal.o
TNORM_O  = $(OBJS1) tnorm.o
T1INN_O  = $(OBJS2) attmap.o t1inn.o
INULLS_O = $(OBJS1) $(UTILDIR)/random.o inulls.o
XMAT_O   = $(UTILDIR)/symtab.o $(UTILDIR)/escape.o \
           $(UTILDIR)/tabread.o xmat.o $(ADDOBJS)
SKEL1_O  = $(OBJS1) skel1.o
SKEL2_O  = $(OBJS2) skel2.o
PRGS     = dom opc tsort tmerge tsplit tjoin tbal tnorm t1inn inulls \
           xmat

#-----------------------------------------------------------------------
# Build Programs
#-----------------------------------------------------------------------
all:          $(PRGS)

dom:          $(DOM_O) makefile
	$(LD) $(LDFLAGS) $(DOM_O) $(LIBS) -o $@

opc:          $(OPC_O) makefile
	$(LD) $(LDFLAGS) $(OPC_O) $(LIBS) -o $@

tsort:        $(TSORT_O) makefile
	$(LD) $(LDFLAGS) $(TSORT_O) $(LIBS) -o $@

tmerge:       $(TMERGE_O) makefile
	$(LD) $(LDFLAGS) $(TMERGE_O) $(LIBS) -o $@

tsplit:       $(TSPLIT_O) makefile
	$(LD) $(LDFLAGS) $(TSPLIT_O) $(LIBS) -o $@

tjoin:        $(TJOIN_O) makefile
	$(LD) $(LDFLAGS) $(TJOIN_O) $(LIBS) -o $@

tbal:         $(TBAL_O) makefile
	$(LD) $(LDFLAGS) $(TBAL_O) $(LIBS) -o $@

tnorm:        $(TNORM_O) makefile
	$(LD) $(LDFLAGS) $(TNORM_O) $(LIBS) -o $@

t1inn:        $(T1INN_O) makefile
	$(LD) $(LDFLAGS) $(T1INN_O) $(LIBS) -o $@

inulls:       $(INULLS_O) makefile
	$(LD) $(LDFLAGS) $(INULLS_O) $(LIBS) -o $@

xmat:         $(XMAT_O) makefile
	$(LD) $(LDFLAGS) $(XMAT_O) $(LIBS) -o $@

skel1:        $(SKEL1_O) makefile
	$(LD) $(LDFLAGS) $(SKEL1_O) $(LIBS) -o $@

skel2:        $(SKEL2_O) makefile
	$(LD) $(LDFLAGS) $(SKEL2_O) $(LIBS) -o $@

#-----------------------------------------------------------------------
# Main Programs
#-----------------------------------------------------------------------
dom.o:        $(HDRS)
dom.o:        dom.c makefile
	$(CC) $(CFLAGS) $(INCS) dom.c -o $@

dom.d:        dom.c
	$(CC) -MM $(CFLAGS) $(INCS) dom.c > dom.d

opc.o:        $(HDRS)
opc.o:        opc.c makefile
	$(CC) $(CFLAGS) $(INCS) opc.c -o $@

opc.d:        opc.c
	$(CC) -MM $(CFLAGS) $(INCS) opc.c > opc.d

tsort.o:      $(HDRS)
tsort.o:      tsort.c makefile
	$(CC) $(CFLAGS) $(INCS) tsort.c -o $@

tsort.d:      tsort.c
	$(CC) -MM $(CFLAGS) $(INCS) tsort.c > tsort.d

tmerge.o:     $(HDRS)
tmerge.o:     tmerge.c makefile
	$(CC) $(CFLAGS) $(INCS) tmerge.c -o $@

tmerge.d:     tmerge.c
	$(CC) -MM $(CFLAGS) $(INCS) tmerge.c > tmerge.d

tsplit.o:     $(HDRS) $(UTILDIR)/random.h table.h
tsplit.o:     tsplit.c makefile
	$(CC) $(CFLAGS) $(INCS) tsplit.c -o $@

tsplit.d:     tsplit.c
	$(CC) -MM $(CFLAGS) $(INCS) tsplit.c > tsplit.d

tjoin.o:      $(HDRS)
tjoin.o:      tjoin.c makefile
	$(CC) $(CFLAGS) $(INCS) tjoin.c -o $@

tjoin.d:      tjoin.c
	$(CC) -MM $(CFLAGS) $(INCS) tjoin.c > tjoin.d

tbal.o:       $(HDRS)
tbal.o:       tbal.c makefile
	$(CC) $(CFLAGS) $(INCS) tbal.c -o $@

tbal.d:       tbal.c
	$(CC) -MM $(CFLAGS) $(INCS) tbal.c > tbal.d

tnorm.o:      $(HDRS)
tnorm.o:      tnorm.c makefile
	$(CC) $(CFLAGS) $(INCS) tnorm.c -o $@

tnorm.d:      tnorm.c
	$(CC) -MM $(CFLAGS) $(INCS) tnorm.c > tnorm.d

t1inn.o:      $(HDRS) attmap.h
t1inn.o:      t1inn.c makefile
	$(CC) $(CFLAGS) $(INCS) t1inn.c -o $@

t1inn.d:      t1inn.c
	$(CC) -MM $(CFLAGS) $(INCS) t1inn.c > t1inn.d

inulls.o:     $(HDRS) $(UTILDIR)/arrays.h $(UTILDIR)/random.h
inulls.o:     inulls.c makefile
	$(CC) $(CFLAGS) $(INCS) inulls.c -o $@

inulls.d:     inulls.c
	$(CC) -MM $(CFLAGS) $(INCS) inulls.c > inulls.d

xmat.o:       $(UTILDIR)/fntypes.h $(UTILDIR)/arrays.h \
              $(UTILDIR)/symtab.h  $(UTILDIR)/tabread.h \
              $(UTILDIR)/error.h
xmat.o:       xmat.c makefile
	$(CC) $(CFLAGS) $(INCS) xmat.c -o $@

xmat.d:       xmat.c
	$(CC) -MM $(CFLAGS) $(INCS) xmat.c > xmat.d

skel1.o:      $(HDRS)
skel1.o:      skel1.c makefile
	$(CC) $(CFLAGS) $(INCS) skel1.c -o $@

skel1.d:      skel1.c
	$(CC) -MM $(CFLAGS) $(INCS) skel1.c > skel1.d

skel2.o:      $(HDRS)
skel2.o:      skel2.c makefile
	$(CC) $(CFLAGS) $(INCS) skel2.c -o $@

skel2.d:      skel2.c
	$(CC) -MM $(CFLAGS) $(INCS) skel2.c > skel2.d

#-----------------------------------------------------------------------
# Attribute Set Management
#-----------------------------------------------------------------------
attset1.o:    $(UTILDIR)/fntypes.h  $(UTILDIR)/arrays.h \
              $(UTILDIR)/scanner.h
attset1.o:    attset.h attset1.c makefile
	$(CC) $(CFLAGS) $(INCS) attset1.c -o $@

attset1.d:    attset1.c
	$(CC) -MM $(CFLAGS) $(INCS) attset1.c > attset1.d

as_read.o:    $(UTILDIR)/tabread.h $(UTILDIR)/scanner.h
as_read.o:    attset.h attset2.c makefile
	$(CC) $(CFLAGS) $(INCS) -DAS_READ attset2.c -o $@

as_read.d:    attset2.c
	$(CC) -MM $(CFLAGS) $(INCS) -DAS_READ attset2.c > as_read.d

attset2.o:    $(UTILDIR)/tabread.h $(UTILDIR)/tabwrite.h \
              $(UTILDIR)/scanner.h
attset2.o:    attset.h attset2.c makefile
	$(CC) $(CFLAGS) $(INCS) -DAS_READ -DAS_WRITE attset2.c -o $@

attset2.d:    attset2.c
	$(CC) -MM $(CFLAGS) $(INCS) -DAS_READ -DAS_WRITE \
              attset2.c > attset2.d

as_desc.o:    $(UTILDIR)/scanner.h
as_desc.o:    attset.h attset3.c makefile
	$(CC) $(CFLAGS) $(INCS) -DAS_DESC attset3.c -o $@

as_desc.d:    attset3.c
	$(CC) -MM $(CFLAGS) $(INCS) -DAS_DESC attset3.c > as_desc.d

attset3.o:    $(UTILDIR)/scanner.h
attset3.o:    attset.h attset3.c makefile
	$(CC) $(CFLAGS) $(INCS) -DAS_DESC -DAS_PARSE attset3.c -o $@

attset3.d:    attset3.c
	$(CC) -MM $(CFLAGS) $(INCS) -DAS_DESC -DAS_PARSE \
              attset3.c > attset.d

#-----------------------------------------------------------------------
# Attribute Map Management
#-----------------------------------------------------------------------
attmap.o:     $(UTILDIR)/fntypes.h $(UTILDIR)/scanner.h attset.h
attmap.o:     attmap.h attmap.c makefile
	$(CC) $(CFLAGS) $(INCS) attmap.c -o $@

attmap.d:     attmap.c
	$(CC) -MM $(CFLAGS) $(INCS) attmap.c > attmap.d

#-----------------------------------------------------------------------
# Table Management
#-----------------------------------------------------------------------
table1.o:     $(UTILDIR)/fntypes.h  $(UTILDIR)/arrays.h \
              $(UTILDIR)/scanner.h  attset.h
table1.o:     table.h table1.c makefile
	$(CC) $(CFLAGS) $(INCS) table1.c -o $@

table1.d:     table1.c
	$(CC) -MM $(CFLAGS) $(INCS) table1.c > table1.d

tab2ro.o:     $(UTILDIR)/fntypes.h  $(UTILDIR)/scanner.h \
              $(UTILDIR)/tabread.h  attset.h
tab2ro.o:     table.h table2.c makefile
	$(CC) $(CFLAGS) $(INCS) -DTAB_READ table2.c -o $@

tab2ro.d:     table2.c
	$(CC) -MM $(CFLAGS) $(INCS) -DTAB_READ table2.c > tab2ro.d

table2.o:     $(UTILDIR)/fntypes.h  $(UTILDIR)/scanner.h  \
              $(UTILDIR)/tabread.h  $(UTILDIR)/tabwrite.h \
              attset.h
table2.o:     table.h table2.c makefile
	$(CC) $(CFLAGS) $(INCS) -DTAB_READ -DTAB_WRITE table2.c -o $@

table2.d:     table2.c
	$(CC) -MM $(CFLAGS) $(INCS) -DTAB_READ -DTAB_WRITE \
              table2.c > table2.d

table3.o:     $(UTILDIR)/fntypes.h  $(UTILDIR)/scanner.h attset.h
table3.o:     table.h table3.c makefile
	$(CC) $(CFLAGS) $(INCS) table3.c -o $@

table3.d:     table3.c
	$(CC) -MM $(CFLAGS) $(INCS) table3.c > table3.d

#-----------------------------------------------------------------------
# Utility Functions for Visualization Programs
#-----------------------------------------------------------------------
tab4vis.o:    $(UTILDIR)/fntypes.h  $(UTILDIR)/scanner.h \
              $(UTILDIR)/tabread.h  attset.h table.h
tab4vis.o:    tab4vis.h tab4vis.c makefile
	$(CC) $(CFLAGS) $(INCS) tab4vis.c -o $@

tab4vis.d:    tab4vis.c
	$(CC) -MM $(CFLAGS) $(INCS) tab4vis.c > tab4vis.d

#-----------------------------------------------------------------------
# External Modules
#-----------------------------------------------------------------------
$(UTILDIR)/arrays.o:
	cd $(UTILDIR); $(MAKE) arrays.o   ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/escape.o:
	cd $(UTILDIR); $(MAKE) escape.o   ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/strlist.o:
	cd $(UTILDIR); $(MAKE) strlist.o  ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/symtab.o:
	cd $(UTILDIR); $(MAKE) symtab.o   ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/random.o:
	cd $(UTILDIR); $(MAKE) random.o   ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/tabread.o:
	cd $(UTILDIR); $(MAKE) tabread.o  ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/tabwrite.o:
	cd $(UTILDIR); $(MAKE) tabwrite.o ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/scform.o:
	cd $(UTILDIR); $(MAKE) scform.o   ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/scanner.o:
	cd $(UTILDIR); $(MAKE) scanner.o  ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/storage.o:
	cd $(UTILDIR); $(MAKE) storage.o  ADDFLAGS="$(ADDFLAGS)"

#-----------------------------------------------------------------------
# Source Distribution Packages
#-----------------------------------------------------------------------
dist:
	$(MAKE) clean
	cd ../..; rm -f table.zip table.tar.gz; \
        zip -rq table.zip    table/{src,ex,doc} \
                util/src/{fntypes.h,error.h,random.[ch]} \
                util/src/{arrays.[ch],escape.[ch]} \
                util/src/{strlist.[ch],symtab.[ch]} \
                util/src/{tabread.[ch],tabwrite.[ch],scanner.[ch]} \
                util/src/{makefile,util.mak} util/doc; \
        tar cfz table.tar.gz table/{src,ex,doc} \
                util/src/{fntypes.h,error.h,random.[ch]} \
                util/src/{arrays.[ch],escape.[ch]} \
                util/src/{strlist.[ch],symtab.[ch]} \
                util/src/{tabread.[ch],tabwrite.[ch],scanner.[ch]} \
                util/src/{makefile,util.mak} util/doc

#-----------------------------------------------------------------------
# Installation
#-----------------------------------------------------------------------
install:
	cp $(PRGS) $(HOME)/bin

#-----------------------------------------------------------------------
# Clean up
#-----------------------------------------------------------------------
localclean:
	rm -f *.d *.o *~ *.flc core $(PRGS) skel1 skel2

clean:
	$(MAKE) localclean
	cd $(UTILDIR); $(MAKE) clean
