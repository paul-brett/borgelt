#-----------------------------------------------------------------------
# File    : makefile    (directory: dtree)
# Contents: commands to build decision and regression tree programs
# Author  : Christian Borgelt
# History : 1995.??.?? file created
#           1997.08.26 makefile splitted into subdirectory makefiles
#           1998.01.04 table reader management added
#           1998.02.08 adapted to changed parse functions
#           1998.05.18 rule module added
#           1998.05.26 rule extraction program 'dtr' added
#           1999.04.17 input/output utility functions added
#           2000.09.12 variance table module added
#           2000.12.12 frqtab.o added to objects DTP_O
#           2002.01.22 adapted to changed table makefile
#           2002.07.04 module gamma added
#           2002.07.06 rule set execution added (program rsx)
#           2004.08.12 adapted to new module parse
#           2008.08.11 adapted to name change from vecops to arrays
#           2011.07.28 external mathematical module normal   added
#           2011.07.29 external utility      module tabwrite added
#           2013.08.23 modified CFBASE to higher warning level
#           2016.04.20 creation of dependency files added
#-----------------------------------------------------------------------
SHELL    = /bin/bash
THISDIR  = ../../dtree/src
UTILDIR  = ../../util/src
MATHDIR  = ../../math/src
TABLEDIR = ../../table/src

CC       = gcc -std=c99
# CC       = g++
CFBASE   = -Wall -Wextra -Wno-unused-parameter -Wconversion \
           -pedantic -c $(ADDFLAGS)
CFLAGS   = $(CFBASE) -DNDEBUG -O3 -funroll-loops
# CFLAGS   = $(CFBASE) -g
# CFLAGS   = $(CFBASE) -g -DSTORAGE
INCS     = -I$(UTILDIR) -I$(TABLEDIR) -I$(MATHDIR)

LD       = gcc
LDFLAGS  = $(ADDFLAGS)
LIBS     = -lm $(ADDLIBS)

# ADDOBJS  = $(UTILDIR)/storage.o

HDRS_0   = $(UTILDIR)/fntypes.h  $(UTILDIR)/arrays.h   \
           $(MATHDIR)/gamma.h
HDRS_1   = $(UTILDIR)/fntypes.h  $(UTILDIR)/scanner.h  \
           $(TABLEDIR)/attset.h  $(TABLEDIR)/table.h
HDRS_2   = $(HDRS_1)             $(UTILDIR)/arrays.h   \
           $(MATHDIR)/normal.h
HDRS     = $(HDRS_1)             $(UTILDIR)/tabread.h  \
           $(UTILDIR)/error.h
OBJS     = $(UTILDIR)/arrays.o   $(UTILDIR)/escape.o   \
           $(UTILDIR)/tabread.o  $(UTILDIR)/scanner.o  \
           $(TABLEDIR)/attset1.o $(TABLEDIR)/attset2.o \
           $(TABLEDIR)/attset3.o $(ADDOBJS)
TABOBJS  = $(TABLEDIR)/table1.o  $(TABLEDIR)/tab2ro.o
DTI_O    = $(MATHDIR)/gamma.o    $(OBJS) $(TABOBJS) \
           ft_eval.o vt_eval.o   dtree1.o dt_grow.o dti.o
DTP_O    = $(MATHDIR)/normal.o   $(OBJS) $(TABOBJS) \
           frqtab.o vartab.o dt_exec.o dt_prune.o dtp.o
DTX_O    = $(UTILDIR)/tabwrite.o $(OBJS) $(TABOBJS) \
           dt_exec.o dtx.o
DTR_O    = $(OBJS) rules.o dt_rule.o dtr.o
RSX_O    = $(UTILDIR)/tabwrite.o $(OBJS) $(TABOBJS) \
           rs_pars.o rsx.o
PRGS     = dti dtp dtx dtr rsx

#-----------------------------------------------------------------------
# Build Programs
#-----------------------------------------------------------------------
all:          $(PRGS)

dti:          $(DTI_O) makefile
	$(LD) $(LDFLAGS) $(DTI_O) $(LIBS) -o $@

dtp:          $(DTP_O) makefile
	$(LD) $(LDFLAGS) $(DTP_O) $(LIBS) -o $@

dtx:          $(DTX_O) makefile
	$(LD) $(LDFLAGS) $(DTX_O) $(LIBS) -o $@

dtr:          $(DTR_O) makefile
	$(LD) $(LDFLAGS) $(DTR_O) $(LIBS) -o $@

rsx:          $(RSX_O) makefile
	$(LD) $(LDFLAGS) $(RSX_O) $(LIBS) -o $@

#-----------------------------------------------------------------------
# Main Programs
#-----------------------------------------------------------------------
dti.o:        $(HDRS) $(UTILDIR)/arrays.h frqtab.h vartab.h dtree.h
dti.o:        dti.c makefile
	$(CC) $(CFLAGS) $(INCS) dti.c -o $@

dti.d:        dti.c
	$(CC) -MM $(CFLAGS) $(INCS) dti.c > dti.d

dtp.o:        $(HDRS) frqtab.h vartab.h dtree.h
dtp.o:        dtp.c makefile
	$(CC) $(CFLAGS) $(INCS) dtp.c -o $@

dtp.d:        dtp.c
	$(CC) -MM $(CFLAGS) $(INCS) dtp.c > dtp.d

dtx.o:        $(HDRS) $(UTILDIR)/tabwrite.h frqtab.h vartab.h dtree.h
dtx.o:        dtx.c makefile
	$(CC) $(CFLAGS) $(INCS) dtx.c -o $@

dtx.d:        dtx.c
	$(CC) -MM $(CFLAGS) $(INCS) dtx.c > dtx.d

dtr.o:        $(HDRS) frqtab.h vartab.h dtree.h rules.h
dtr.o:        dtr.c makefile
	$(CC) $(CFLAGS) $(INCS) dtr.c -o $@

dtr.d:        dtr.c
	$(CC) -MM $(CFLAGS) $(INCS) dtr.c > dtr.d

rsx.o:        $(HDRS) $(UTILDIR)/tabwrite.h rules.h
rsx.o:        rsx.c makefile
	$(CC) $(CFLAGS) $(INCS) rsx.c -o $@

rsx.d:        rsx.c
	$(CC) -MM $(CFLAGS) $(INCS) rsx.c > rsx.d

#-----------------------------------------------------------------------
# Frequency Table Management
#-----------------------------------------------------------------------
frqtab.o:     $(HDRS_0)
frqtab.o:     frqtab.h frqtab.c makefile
	$(CC) $(CFLAGS) $(INCS) frqtab.c -o $@

frqtab.d:     frqtab.c
	$(CC) -MM $(CFLAGS) $(INCS) frqtab.c > frqtab.d

ft_eval.o:    $(HDRS_0)
ft_eval.o:    frqtab.h frqtab.c makefile
	$(CC) $(CFLAGS) $(INCS) -DFT_EVAL frqtab.c -o $@

ft_eval.d:    frqtab.c
	$(CC) -MM $(CFLAGS) $(INCS) -DFT_EVAL frqtab.c > ft_eval.d

#-----------------------------------------------------------------------
# Variation Table Management
#-----------------------------------------------------------------------
vartab.o:     vartab.h vartab.c makefile
	$(CC) $(CFLAGS) vartab.c -o $@

vartab.d:     vartab.c
	$(CC) -MM $(CFLAGS) vartab.c > vartab.d

vt_eval.o:    vartab.h vartab.c makefile
	$(CC) $(CFLAGS) -DVT_EVAL vartab.c -o $@

vt_eval.d:    vartab.c
	$(CC) -MM $(CFLAGS) -DVT_EVAL vartab.c > vt_eval.d

#-----------------------------------------------------------------------
# Decision and Regression Tree Management
#-----------------------------------------------------------------------
dtree1.o:     $(HDRS_1) frqtab.h vartab.h
dtree1.o:     dtree.h dtree1.c makefile
	$(CC) $(CFLAGS) $(INCS) dtree1.c -o $@

dtree1.d:     dtree1.c
	$(CC) -MM $(CFLAGS) $(INCS) dtree1.c > dtree1.d

dt_grow.o:    $(HDRS_2) frqtab.h vartab.h
dt_grow.o:    dtree.h dtree2.c makefile
	$(CC) $(CFLAGS) $(INCS) -DDT_GROW dtree2.c -o $@

dt_grow.d:    dtree2.c
	$(CC) -MM $(CFLAGS) $(INCS) -DDT_GROW dtree2.c > dt_grow.d

dt_prune.o:   $(HDRS_2) frqtab.h vartab.h
dt_prune.o:   dtree.h dtree2.c makefile
	$(CC) $(CFLAGS) $(INCS) -DDT_PRUNE dtree2.c -o $@

dt_prune.d:   dtree2.c
	$(CC) -MM $(CFLAGS) $(INCS) -DDT_PRUNE dtree2.c > dt_prune.d

dt_exec.o:    $(HDRS_1) frqtab.h vartab.h
dt_exec.o:    dtree.h dtree1.c makefile
	$(CC) $(CFLAGS) $(INCS) -DDT_PARSE dtree1.c -o $@

dt_exec.d:    dtree1.c
	$(CC) -MM $(CFLAGS) $(INCS) -DDT_PARSE dtree1.c > dt_exec.d

dt_rule.o:    $(HDRS_1) frqtab.h vartab.h rules.h
dt_rule.o:    dtree.h dtree1.c makefile
	$(CC) $(CFLAGS) $(INCS) -DDT_PARSE -DDT_RULES dtree1.c -o $@

dt_rule.d:    dtree1.c
	$(CC) -MM $(CFLAGS) $(INCS) -DDT_PARSE -DDT_RULES \
              dtree1.c > dt_rule.d

#-----------------------------------------------------------------------
# Rule and Rule Set Management
#-----------------------------------------------------------------------
rules.o:      $(HDRS_1)
rules.o:      rules.h rules.c makefile
	$(CC) $(CFLAGS) $(INCS) -DRS_DESC rules.c -o $@

rules.d:      rules.c
	$(CC) -MM $(CFLAGS) $(INCS) -DRS_DESC rules.c > rules.d

rs_pars.o:    $(HDRS_1)
rs_pars.o:    rules.h rules.c makefile
	$(CC) $(CFLAGS) $(INCS) -DRS_DESC -DRS_PARSE rules.c -o $@

rs_pars.d:    rules.c
	$(CC) -MM $(CFLAGS) $(INCS) -DRS_DESC -DRS_PARSE \
              rules.c > rs_pars.d

#-----------------------------------------------------------------------
# External Modules
#-----------------------------------------------------------------------
$(UTILDIR)/arrays.o:
	cd $(UTILDIR);  $(MAKE) arrays.o   ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/escape.o:
	cd $(UTILDIR);  $(MAKE) escape.o   ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/tabread.o:
	cd $(UTILDIR);  $(MAKE) tabread.o  ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/tabwrite.o:
	cd $(UTILDIR);  $(MAKE) tabwrite.o ADDFLAGS="$(ADDFLAGS)"
$(UTILDIR)/scanner.o:
	cd $(UTILDIR);  $(MAKE) scanner.o  ADDFLAGS="$(ADDFLAGS)"
$(MATHDIR)/gamma.o:
	cd $(MATHDIR);  $(MAKE) gamma.o    ADDFLAGS="$(ADDFLAGS)"
$(MATHDIR)/normal.o:
	cd $(MATHDIR);  $(MAKE) normal.o   ADDFLAGS="$(ADDFLAGS)"
$(TABLEDIR)/attset1.o:
	cd $(TABLEDIR); $(MAKE) attset1.o  ADDFLAGS="$(ADDFLAGS)"
$(TABLEDIR)/attset2.o:
	cd $(TABLEDIR); $(MAKE) attset2.o  ADDFLAGS="$(ADDFLAGS)"
$(TABLEDIR)/attset3.o:
	cd $(TABLEDIR); $(MAKE) attset3.o  ADDFLAGS="$(ADDFLAGS)"
$(TABLEDIR)/table1.o:
	cd $(TABLEDIR); $(MAKE) table1.o   ADDFLAGS="$(ADDFLAGS)"
$(TABLEDIR)/tab2ro.o:
	cd $(TABLEDIR); $(MAKE) tab2ro.o   ADDFLAGS="$(ADDFLAGS)"

#-----------------------------------------------------------------------
# Source Distribution Packages
#-----------------------------------------------------------------------
dist:
	$(MAKE) clean
	cd ../..; rm -f dtree.zip dtree.tar.gz; \
        zip -rq dtree.zip    dtree/{src,ex,doc} \
                table/src/{attset.h,attset[123].c} \
                table/src/{table.h,table[12].c} \
                table/src/{makefile,table.mak} table/doc \
                math/src/{gamma.[ch],normal.[ch]} \
                math/src/{makefile,math.mak} math/doc \
                util/src/{fntypes.h,error.h} \
                util/src/{arrays.[ch],escape.[ch]} \
                util/src/{tabread.[ch],tabwrite.[ch],scanner.[ch]} \
                util/src/{makefile,util.mak} util/doc; \
        tar cfz dtree.tar.gz dtree/{src,ex,doc} \
                table/src/{attset.h,attset[123].c} \
                table/src/{table.h,table[12].c} \
                table/src/{makefile,table.mak} table/doc \
                math/src/{gamma.[ch],normal.[ch]} \
                math/src/{makefile,math.mak} math/doc \
                util/src/{fntypes.h,error.h} \
                util/src/{arrays.[ch],escape.[ch]} \
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
	rm -f *.d *.o *~ *.flc core $(PRGS)

clean:
	$(MAKE) localclean
	cd $(UTILDIR);  $(MAKE) clean
	cd $(MATHDIR);  $(MAKE) clean
	cd $(TABLEDIR); $(MAKE) localclean
