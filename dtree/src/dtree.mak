#-----------------------------------------------------------------------
# File    : dtree.mak
# Contents: commands to build decision and regression tree programs
# Author  : Christian Borgelt
# History : 2003.01.27 file created
#           2006.07.20 adapted to Visual Studio 8
#           2016.04.20 completed dependencies on header files
#-----------------------------------------------------------------------
THISDIR  = ..\..\dtree\src
UTILDIR  = ..\..\util\src
MATHDIR  = ..\..\math\src
TABLEDIR = ..\..\table\src

CC       = cl.exe
DEFS     = /D WIN32 /D NDEBUG /D _CONSOLE /D _CRT_SECURE_NO_WARNINGS
CFLAGS   = /nologo /W3 /O2 /GS- $(DEFS) /c
INCS     = /I $(UTILDIR) /I $(MATHDIR) /I $(TABLEDIR)

LD       = link.exe
LDFLAGS  = /nologo /subsystem:console /incremental:no
LIBS     = 

HDRS_0   = $(UTILDIR)\fntypes.h  $(UTILDIR)\arrays.h   \
           $(MATHDIR)\gamma.h
HDRS_1   = $(UTILDIR)\fntypes.h  $(UTILDIR)\scanner.h  \
           $(TABLEDIR)\attset.h  $(TABLEDIR)\table.h
HDRS_2   = $(HDRS_1)             $(UTILDIR)\arrays.h   \
           $(MATHDIR)\normal.h
HDRS     = $(HDRS_1)             $(UTILDIR)\tabread.h  \
           $(UTILDIR)\error.h
OBJS     = $(UTILDIR)\arrays.obj   $(UTILDIR)\escape.obj   \
           $(UTILDIR)\tabread.obj  $(UTILDIR)\scanner.obj  \
           $(TABLEDIR)\attset1.obj $(TABLEDIR)\attset2.obj \
           $(TABLEDIR)\attset3.obj
TABOBJS  = $(TABLEDIR)\table1.obj  $(TABLEDIR)\tab2ro.obj
DTI_O    = $(MATHDIR)\gamma.obj    $(OBJS) $(TABOBJS) \
           ft_eval.obj vt_eval.obj dtree1.obj dt_grow.obj dti.obj
DTP_O    = $(MATHDIR)\normal.obj   $(OBJS) $(TABOBJS) \
           frqtab.obj vartab.obj dt_exec.obj dt_prune.obj dtp.obj
DTX_O    = $(UTILDIR)\tabwrite.obj $(OBJS) $(TABOBJS) \
           dt_exec.obj dtx.obj
DTR_O    = $(OBJS) rules.obj dt_rule.obj dtr.obj
RSX_O    = $(UTILDIR)\tabwrite.obj $(OBJS) $(TABOBJS) \
           rs_pars.obj rsx.obj
PRGS     = dti.exe dtp.exe dtx.exe dtr.exe rsx.exe

#-----------------------------------------------------------------------
# Build Programs
#-----------------------------------------------------------------------
all:          $(PRGS)

dti.exe:      $(DTI_O) dtree.mak
	$(LD) $(LDFLAGS) $(DTI_O) $(LIBS) /out:$@

dtp.exe:      $(DTP_O) dtree.mak
	$(LD) $(LDFLAGS) $(DTP_O) $(LIBS) /out:$@

dtx.exe:      $(DTX_O) dtree.mak
	$(LD) $(LDFLAGS) $(DTX_O) $(LIBS) /out:$@

dtr.exe:      $(DTR_O) dtree.mak
	$(LD) $(LDFLAGS) $(DTR_O) $(LIBS) /out:$@

rsx.exe:      $(RSX_O) dtree.mak
	$(LD) $(LDFLAGS) $(RSX_O) $(LIBS) /out:$@

#-----------------------------------------------------------------------
# Main Programs
#-----------------------------------------------------------------------
dti.obj:      $(HDRS) $(UTILDIR)\arrays.h frqtab.h vartab.h dtree.h
dti.obj:      dti.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) dti.c /Fo$@

dtp.obj:      $(HDRS) frqtab.h vartab.h dtree.h
dtp.obj:      dtp.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) dtp.c /Fo$@

dtx.obj:      $(HDRS) $(UTILDIR)\tabwrite.h frqtab.h vartab.h dtree.h
dtx.obj:      dtx.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) dtx.c /Fo$@

dtr.obj:      $(HDRS) frqtab.h vartab.h dtree.h rules.h
dtr.obj:      dtr.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) dtr.c /Fo$@

rsx.obj:      $(HDRS) $(UTILDIR)\tabwrite.h rules.h
rsx.obj:      rules.h rsx.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) rsx.c /Fo$@

#-----------------------------------------------------------------------
# Frequency Table Management
#-----------------------------------------------------------------------
frqtab.obj:   $(HDRS_0)
frqtab.obj:   frqtab.h frqtab.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) frqtab.c /Fo$@

ft_eval.obj:  $(HDRS_0)
ft_eval.obj:  frqtab.h frqtab.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) /D FT_EVAL frqtab.c /Fo$@

#-----------------------------------------------------------------------
# Variation Table Management
#-----------------------------------------------------------------------
vartab.obj:   vartab.h vartab.c dtree.mak
	$(CC) $(CFLAGS) vartab.c /Fo$@

vt_eval.obj:  vartab.h vartab.c dtree.mak
	$(CC) $(CFLAGS) /D VT_EVAL vartab.c /Fo$@

#-----------------------------------------------------------------------
# Decision and Regression Tree Management
#-----------------------------------------------------------------------
dtree1.obj:   $(HDRS_1) frqtab.h vartab.h
dtree1.obj:   $(HDRS) dtree1.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) dtree1.c /Fo$@

dt_grow.obj:  $(HDRS_2) frqtab.h vartab.h
dt_grow.obj:  $(HDRS) dtree2.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) /D DT_GROW dtree2.c /Fo$@

dt_prune.obj: $(HDRS_2) frqtab.h vartab.h
dt_prune.obj: $(HDRS) $(MATHDIR)\normal.h dtree2.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) /D DT_PRUNE dtree2.c /Fo$@

dt_exec.obj:  $(HDRS_1) frqtab.h vartab.h
dt_exec.obj:  $(HDRS) dtree1.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) /D DT_PARSE dtree1.c /Fo$@

dt_rule.obj:  $(HDRS_1) frqtab.h vartab.h rules.h
dt_rule.obj:  $(HDRS) dtree1.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) /D DT_PARSE /D DT_RULES dtree1.c /Fo$@

#-----------------------------------------------------------------------
# Rule and Rule Set Management
#-----------------------------------------------------------------------
rules.obj:    $(HDRS_1)
rules.obj:    rules.h rules.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) /D RS_DESC rules.c /Fo$@

rs_pars.obj:  $(HDRS_1)
rs_pars.obj:  rules.h rules.c dtree.mak
	$(CC) $(CFLAGS) $(INCS) /D RS_DESC /D RS_PARSE rules.c /Fo$@

#-----------------------------------------------------------------------
# External Modules
#-----------------------------------------------------------------------
$(UTILDIR)\arrays.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak arrays.obj
	cd $(THISDIR)
$(UTILDIR)\escape.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak escape.obj
	cd $(THISDIR)
$(UTILDIR)\tabread.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak tabread.obj
	cd $(THISDIR)
$(UTILDIR)\tabwrite.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak tabwrite.obj
	cd $(THISDIR)
$(UTILDIR)\scanner.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak scanner.obj
	cd $(THISDIR)
$(UTILDIR)\parser.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak parser.obj
	cd $(THISDIR)
$(MATHDIR)\gamma.obj:
	cd $(MATHDIR)
	$(MAKE) /f math.mak gamma.obj
	cd $(THISDIR)
$(MATHDIR)\normal.obj:
	cd $(MATHDIR)
	$(MAKE) /f math.mak normal.obj
	cd $(THISDIR)
$(TABLEDIR)\attset1.obj:
	cd $(TABLEDIR)
	$(MAKE) /f table.mak attset1.obj
	cd $(THISDIR)
$(TABLEDIR)\attset2.obj:
	cd $(TABLEDIR)
	$(MAKE) /f table.mak attset2.obj
	cd $(THISDIR)
$(TABLEDIR)\attset3.obj:
	cd $(TABLEDIR)
	$(MAKE) /f table.mak attset3.obj
	cd $(THISDIR)
$(TABLEDIR)\table1.obj:
	cd $(TABLEDIR)
	$(MAKE) /f table.mak table1.obj
	cd $(THISDIR)
$(TABLEDIR)\tab2ro.obj:
	cd $(TABLEDIR)
	$(MAKE) /f table.mak tab2ro.obj
	cd $(THISDIR)
$(TABLEDIR)\table2.obj:
	cd $(TABLEDIR)
	$(MAKE) /f table.mak table2.obj
	cd $(THISDIR)

#-----------------------------------------------------------------------
# Install
#-----------------------------------------------------------------------
install:
	-@copy *.exe ..\..\..\bin

#-----------------------------------------------------------------------
# Clean up
#-----------------------------------------------------------------------
localclean:
	-@erase /Q *~ *.obj *.idb *.pch $(PRGS)

clean:
	$(MAKE) /f dtree.mak localclean
	cd $(UTILDIR)
	$(MAKE) /f util.mak clean
	cd $(MATHDIR)
	$(MAKE) /f math.mak clean
	cd $(TABLEDIR)
	$(MAKE) /f table.mak localclean
	cd $(THISDIR)
