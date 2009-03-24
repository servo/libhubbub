# Component settings
COMPONENT := hubbub
# Default to a static library
COMPONENT_TYPE ?= lib-static

# Setup the tooling
include build/makefiles/Makefile.tools

# Toolchain flags
WARNFLAGS := -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs -Werror -pedantic
CFLAGS := $(CFLAGS) -std=c99 -D_BSD_SOURCE -I$(CURDIR)/include/ \
	-I$(CURDIR)/src $(WARNFLAGS) 

ifneq ($(PKGCONFIG),)
  CFLAGS := $(CFLAGS) $(shell $(PKGCONFIG) libparserutils --cflags)
  LDFLAGS := $(LDFLAGS) $(shell $(PKGCONFIG) libparserutils --libs)
else
  LDFLAGS := -lparserutils
endif

include build/makefiles/Makefile.top

# Extra installation rules
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/errors.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/functypes.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/hubbub.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/parser.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/tree.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/types.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /lib/pkgconfig:lib$(COMPONENT).pc.in
INSTALL_ITEMS := $(INSTALL_ITEMS) /lib:$(BUILDDIR)/lib$(COMPONENT)$(LIBEXT)
