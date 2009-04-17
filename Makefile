# Component settings
COMPONENT := hubbub
COMPONENT_VERSION := 0.0.1
# Default to a static library
COMPONENT_TYPE ?= lib-static

# Setup the tooling
include build/makefiles/Makefile.tools

TESTRUNNER := $(PERL) build/testtools/testrunner.pl

# Toolchain flags
WARNFLAGS := -Wall -Wundef -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs -Werror -pedantic
ifneq ($(GCCVER),2)
  WARNFLAGS := $(WARNFLAGS) -Wextra
endif
CFLAGS := $(CFLAGS) -D_BSD_SOURCE -I$(CURDIR)/include/ \
	-I$(CURDIR)/src $(WARNFLAGS) 
ifneq ($(GCCVER),2)
  CFLAGS := $(CFLAGS) -std=c99
else
  # __inline__ is a GCCism
  CFLAGS := $(CFLAGS) -Dinline="__inline__"
endif

# Parserutils
ifneq ($(PKGCONFIG),)
  CFLAGS := $(CFLAGS) $(shell $(PKGCONFIG) libparserutils-0 --cflags)
  LDFLAGS := $(LDFLAGS) $(shell $(PKGCONFIG) libparserutils-0 --libs)
else
  CFLAGS := $(CFLAGS) -I$(PREFIX)/include/parserutils0
  LDFLAGS := $(LDFLAGS) -lparserutils0
endif

include build/makefiles/Makefile.top

ifeq ($(WANT_TEST),yes)
  # We require the presence of libjson -- http://oss.metaparadigm.com/json-c/
  ifneq ($(PKGCONFIG),)
    TESTCFLAGS := $(TESTCFLAGS) \
		$(shell $(PKGCONFIG) $(PKGCONFIGFLAGS) --cflags json)
    TESTLDFLAGS := $(TESTLDFLAGS) \
		$(shell $(PKGCONFIG) $(PKGCONFIGFLAGS) --libs json)
  else
    TESTCFLAGS := $(TESTCFLAGS) -I$(PREFIX)/include/json
    TESTLDFLAGS := $(TESTLDFLAGS) -ljson
  endif

  ifneq ($(GCCVER),2)
    TESTCFLAGS := $(TESTCFLAGS) -Wno-unused-parameter
  endif
endif

# Extra installation rules
I := /include/hubbub$(major-version)/hubbub
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/hubbub/errors.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/hubbub/functypes.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/hubbub/hubbub.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/hubbub/parser.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/hubbub/tree.h
INSTALL_ITEMS := $(INSTALL_ITEMS) $(I):include/hubbub/types.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /lib/pkgconfig:lib$(COMPONENT).pc.in
INSTALL_ITEMS := $(INSTALL_ITEMS) /lib:$(OUTPUT)
