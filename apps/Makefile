#
# Asterisk -- A telephony toolkit for Linux.
# 
# Makefile for PBX applications
#
# Copyright (C) 1999-2006, Digium, Inc.
#
# This program is free software, distributed under the terms of
# the GNU General Public License
#

-include ../menuselect.makeopts ../menuselect.makedeps

MENUSELECT_CATEGORY=APPS
MENUSELECT_DESCRIPTION=Applications

ALL_C_MODS:=$(patsubst %.c,%,$(wildcard app_*.c))
ALL_CC_MODS:=$(patsubst %.cc,%,$(wildcard app_*.cc))

C_MODS:=$(filter-out $(MENUSELECT_APPS),$(ALL_C_MODS))
CC_MODS:=$(filter-out $(MENUSELECT_APPS),$(ALL_CC_MODS))

LOADABLE_MODS:=$(C_MODS) $(CC_MODS)

ifneq ($(findstring apps,$(MENUSELECT_EMBED)),)
  EMBEDDED_MODS:=$(LOADABLE_MODS)
  LOADABLE_MODS:=
endif

all: _all

include $(ASTTOPDIR)/Makefile.moddir_rules
