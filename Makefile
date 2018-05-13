#
# Makefile 
# Paul Brett <paul@neotextus.org>
# Copyright 2017 - All rights reserved.
#

SUBDIRS = table dtree

all clean: $(SUBDIRS)
	for dir in $(SUBDIRS); \
	do \
	  $(MAKE) -C $$dir/src $@; \
	done

.PHONY: all clean

# vim: set syn=make:
