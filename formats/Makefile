#
# Asterisk -- A telephony toolkit for Linux.
# 
# Makefile for file format modules
#
# Copyright (C) 1999-2006, Digium, Inc.
#
# This program is free software, distributed under the terms of
# the GNU General Public License
#

-include ../menuselect.makeopts ../menuselect.makedeps

MENUSELECT_CATEGORY=FORMATS
MENUSELECT_DESCRIPTION=Format Interpreters

ALL_C_MODS:=$(patsubst %.c,%,$(wildcard format_*.c))
ALL_CC_MODS:=$(patsubst %.cc,%,$(wildcard format_*.cc))

C_MODS:=$(filter-out $(MENUSELECT_FORMATS),$(ALL_C_MODS))
CC_MODS:=$(filter-out $(MENUSELECT_FORMATS),$(ALL_CC_MODS))

LOADABLE_MODS:=$(C_MODS) $(CC_MODS)

ifneq ($(findstring formats,$(MENUSELECT_EMBED)),)
  EMBEDDED_MODS:=$(LOADABLE_MODS)
  LOADABLE_MODS:=
endif

all: _all

include $(ASTTOPDIR)/Makefile.moddir_rules

clean::
	$(MAKE) -C mp3 clean

$(if $(filter format_mp3,$(EMBEDDED_MODS)),modules.link,format_mp3.so): mp3/common.o mp3/dct64_i386.o mp3/decode_ntom.o mp3/layer3.o mp3/tabinit.o mp3/interface.o
