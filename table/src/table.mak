#-----------------------------------------------------------------------
# File    : table.mak
# Contents: build table management modules and utility programs
# Author  : Christian Borgelt
# History : 2003.01.26 file created
#           2003.07.22 program tnorm added
#           2006.07.20 adapted to Visual Studio 8
#           2011.01.28 program tsort added (sort a data table)
#           2011.08.22 external module random added (from util/src)
#-----------------------------------------------------------------------
THISDIR  = ..\..\table\src
UTILDIR  = ..\..\util\src

CC       = cl.exe
LD       = link.exe
DEFS     = /D WIN32 /D NDEBUG /D _CONSOLE /D _CRT_SECURE_NO_WARNINGS
CFLAGS   = /nologo /W3 /O2 /GS- $(DEFS) /c
LDFLAGS  = /nologo /subsystem:console /incremental:no
INC      = /I $(UTILDIR)

HDRS     = $(UTILDIR)\fntypes.h  $(UTILDIR)\arrays.h    \
           $(UTILDIR)\scanner.h  $(UTILDIR)\error.h     \
           $(UTILDIR)\tabread.h  $(UTILDIR)\tabwrite.h  \
           attset.h table.h
OBJS     = $(UTILDIR)\arrays.obj  $(UTILDIR)\escape.obj \
           $(UTILDIR)\tabread.obj attset1.obj
OBJS1    = $(OBJS)  $(UTILDIR)\scform.obj  $(UTILDIR)\tabwrite.obj \
           attset2.obj table1.obj table2.obj
OBJS2    = $(OBJS)  $(UTILDIR)\scanner.obj $(UTILDIR)\tabwrite.obj \
           attset2.obj attset3.obj table1.obj table2.obj
DOM_O    = $(OBJS)  $(UTILDIR)\scform.obj \
           as_read.obj as_desc.obj dom.obj
OPC_O    = $(OBJS1) table3.obj opc.obj
TSORT_O  = $(OBJS2) tsort.obj
TMERGE_O = $(OBJS1) tmerge.obj
TSPLIT_O = $(OBJS1) $(UTILDIR)\random.obj tsplit.obj
TJOIN_O  = $(OBJS1) tjoin.obj
TBAL_O   = $(OBJS1) tbal.obj
TNORM_O  = $(OBJS1) tnorm.obj
T1INN_O  = $(OBJS2) attmap.obj t1inn.obj
INULLS_O = $(OBJS1) $(UTILDIR)\random.obj inulls.obj
XMAT_O   = $(UTILDIR)\symtab.obj  $(UTILDIR)\escape.obj \
           $(UTILDIR)\tabread.obj xmat.obj
SKEL1_O  = $(OBJS1) skel1.obj
SKEL2_O  = $(OBJS2) skel2.obj
PRGS     = dom.exe opc.exe tsort.exe tmerge.exe tsplit.exe tjoin.exe \
           tbal.exe tnorm.exe t1inn.exe inulls.exe xmat.exe

#-----------------------------------------------------------------------
# Build Programs
#-----------------------------------------------------------------------
all:          $(PRGS)

dom.exe:      $(DOM_O) table.mak
	$(LD) $(LDFLAGS) $(DOM_O) $(LIBS) /out:$@

opc.exe:      $(OPC_O) table.mak
	$(LD) $(LDFLAGS) $(OPC_O) $(LIBS) /out:$@

tsort.exe:    $(TSORT_O) table.mak
	$(LD) $(LDFLAGS) $(TSORT_O) $(LIBS) /out:$@

tmerge.exe:   $(TMERGE_O) table.mak
	$(LD) $(LDFLAGS) $(TMERGE_O) $(LIBS) /out:$@

tsplit.exe:   $(TSPLIT_O) table.mak
	$(LD) $(LDFLAGS) $(TSPLIT_O) $(LIBS) /out:$@

tjoin.exe:    $(TJOIN_O) table.mak
	$(LD) $(LDFLAGS) $(TJOIN_O) $(LIBS) /out:$@

tbal.exe:     $(TBAL_O) table.mak
	$(LD) $(LDFLAGS) $(TBAL_O) $(LIBS) /out:$@

tnorm.exe:    $(TNORM_O) table.mak
	$(LD) $(LDFLAGS) $(TNORM_O) $(LIBS) /out:$@

t1inn.exe:    $(T1INN_O) table.mak
	$(LD) $(LDFLAGS) $(T1INN_O) $(LIBS) /out:$@

xmat.exe:     $(XMAT_O) table.mak
	$(LD) $(LDFLAGS) $(XMAT_O) $(LIBS) /out:$@

inulls.exe:   $(INULLS_O) table.mak
	$(LD) $(LDFLAGS) $(INULLS_O) $(LIBS) /out:$@

skel1.exe:    $(SKEL1_O) table.mak
	$(LD) $(LDFLAGS) $(SKEL1_O) $(LIBS) /out:$@

skel2.exe:    $(SKEL2_O) table.mak
	$(LD) $(LDFLAGS) $(SKEL2_O) $(LIBS) /out:$@

#-----------------------------------------------------------------------
# Main Programs
#-----------------------------------------------------------------------
dom.obj:      $(HDRS) dom.c table.mak
	$(CC) $(CFLAGS) $(INC) dom.c /Fo$@

opc.obj:      $(HDRS) opc.c table.mak
	$(CC) $(CFLAGS) $(INC) opc.c /Fo$@

tsort.obj:    $(HDRS) tsort.c table.mak
	$(CC) $(CFLAGS) $(INC) tsort.c /Fo$@

tmerge.obj:   $(HDRS) tmerge.c table.mak
	$(CC) $(CFLAGS) $(INC) tmerge.c /Fo$@

tsplit.obj:   $(HDRS) tsplit.c table.mak
	$(CC) $(CFLAGS) $(INC) tsplit.c /Fo$@

tjoin.obj:    $(HDRS) tjoin.c table.mak
	$(CC) $(CFLAGS) $(INC) tjoin.c /Fo$@

tbal.obj:     $(HDRS) tbal.c table.mak
	$(CC) $(CFLAGS) $(INC) tbal.c /Fo$@

tnorm.obj:    $(HDRS) tnorm.c table.mak
	$(CC) $(CFLAGS) $(INC) tnorm.c /Fo$@

t1inn.obj:    $(HDRS) attmap.h t1inn.c table.mak
	$(CC) $(CFLAGS) $(INC) t1inn.c /Fo$@

inulls.obj:   $(HDRS) $(UTILDIR)\arrays.h $(UTILDIR)\random.h
inulls.obj:   inulls.c table.mak
	$(CC) $(CFLAGS) $(INC) inulls.c /Fo$@

xmat.obj:     $(UTILDIR)\fntypes.h $(UTILDIR)\arrays.h \
              $(UTILDIR)\symtab.h  $(UTILDIR)\tabread.h \
              $(UTILDIR)\error.h
xmat.obj:     xmat.c table.mak
	$(CC) $(CFLAGS) $(INC) xmat.c /Fo$@

skel1.obj:    $(HDRS) table.h io.h skel1.c table.mak
	$(CC) $(CFLAGS) $(INC) skel1.c /Fo$@

skel2.obj:    $(HDRS) table.h io.h skel2.c table.mak
	$(CC) $(CFLAGS) $(INC) skel2.c /Fo$@

#-----------------------------------------------------------------------
# Attribute Set Management
#-----------------------------------------------------------------------
attset1.obj:  $(UTILDIR)\fntypes.h  $(UTILDIR)\arrays.h \
              $(UTILDIR)\scanner.h
attset1.obj:  attset.h attset1.c table.mak
	$(CC) $(CFLAGS) $(INC) attset1.c /Fo$@

as_read.obj:  $(UTILDIR)\tabread.h $(UTILDIR)\scanner.h
as_read.obj:  attset.h attset2.c table.mak
	$(CC) $(CFLAGS) $(INC) /D AS_READ attset2.c /Fo$@

attset2.obj:  $(UTILDIR)\tabread.h $(UTILDIR)\tabwrite.h \
              $(UTILDIR)\scanner.h
attset2.obj:  attset.h attset2.c table.mak
	$(CC) $(CFLAGS) $(INC) /D AS_READ /D AS_WRITE attset2.c /Fo$@

as_desc.obj:  $(UTILDIR)\scanner.h
as_desc.obj:  attset.h attset3.c table.mak
	$(CC) $(CFLAGS) $(INC) /D AS_DESC attset3.c /Fo$@

attset3.obj:  $(UTILDIR)\scanner.h
attset3.obj:  attset.h attset3.c table.mak
	$(CC) $(CFLAGS) $(INC) /D AS_DESC /D AS_PARSE attset3.c /Fo$@

#-----------------------------------------------------------------------
# Attribute Map Management
#-----------------------------------------------------------------------
attmap.obj:   $(UTILDIR)\fntypes.h $(UTILDIR)\scanner.h attset.h
attmap.obj:   attmap.h attmap.c table.mak
	$(CC) $(CFLAGS) $(INC) attmap.c /Fo$@

#-----------------------------------------------------------------------
# Table Management
#-----------------------------------------------------------------------
table1.obj:   $(UTILDIR)\fntypes.h  $(UTILDIR)\arrays.h \
              $(UTILDIR)\scanner.h  attset.h
table1.obj:   table.h table1.c table.mak
	$(CC) $(CFLAGS) $(INC) table1.c /Fo$@

tab2ro.obj:   $(UTILDIR)\fntypes.h  $(UTILDIR)\scanner.h \
              $(UTILDIR)\tabread.h  attset.h
tab2ro.obj:   table.h table2.c table.mak
	$(CC) $(CFLAGS) $(INC) /D TAB_READ table2.c /Fo$@

table2.obj:   $(UTILDIR)\fntypes.h  $(UTILDIR)\scanner.h  \
              $(UTILDIR)\tabread.h  $(UTILDIR)\tabwrite.h \
              attset.h
table2.obj:   table.h table2.c table.mak
	$(CC) $(CFLAGS) $(INC) /D TAB_READ /D TAB_WRITE table2.c /Fo$@

table3.obj:   $(UTILDIR)\fntypes.h  $(UTILDIR)\scanner.h attset.h
table3.obj:   table.h table3.c table.mak
	$(CC) $(CFLAGS) $(INC) table3.c /Fo$@

#-----------------------------------------------------------------------
# Utility Functions for Visualization Programs
#-----------------------------------------------------------------------
tab4vis.obj:  $(UTILDIR)\fntypes.h  $(UTILDIR)\scanner.h \
              $(UTILDIR)\tabread.h  attset.h table.h
tab4vis.obj:  tab4vis.h tab4vis.c table.mak
	$(CC) $(CFLAGS) $(INC) tab4vis.c /Fo$@

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
$(UTILDIR)\symtab.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak symtab.obj
	cd $(THISDIR)
$(UTILDIR)\random.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak random.obj
	cd $(THISDIR)
$(UTILDIR)\tabread.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak tabread.obj
	cd $(THISDIR)
$(UTILDIR)\tabwrite.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak tabwrite.obj
	cd $(THISDIR)
$(UTILDIR)\scform.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak scform.obj
	cd $(THISDIR)
$(UTILDIR)\scanner.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak scanner.obj
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
	-@erase /Q *~ *.obj *.idb *.pch $(PRGS) skel1 skel2

clean:
	$(MAKE) /f table.mak localclean
	cd $(UTILDIR)
	$(MAKE) /f util.mak clean
	cd $(THISDIR)
