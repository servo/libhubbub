# Component settings
COMPONENT := hubbub
COMPONENT_TYPE := lib-static

# Build settings
TARGET := nix
LIBEXT := .a

# Toolchain flags
WARNFLAGS := -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs -Werror -pedantic
CFLAGS := $(CFLAGS) -std=c99 -D_BSD_SOURCE -I$(CURDIR)/include/ \
	-I$(CURDIR)/src $(WARNFLAGS) 

include build/makefiles/Makefile.top

# Further toolchain settings which rely on Makefile.top
CFLAGS := $(CFLAGS) $(shell $(PKGCONFIG) libparserutils --cflags)
LDFLAGS := $(LDFLAGS) $(shell $(PKGCONFIG) libparserutils --libs)

ifeq ($(BUILD),release)
  CFLAGS := $(CFLAGS) -DNDEBUG -O2
else
  CFLAGS := $(CFLAGS) -g -O0
endif

# Extra installation rules
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/errors.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/functypes.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/hubbub.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/parser.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/tree.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /include/hubbub:include/hubbub/types.h
INSTALL_ITEMS := $(INSTALL_ITEMS) /lib/pkgconfig:lib$(COMPONENT).pc.in
INSTALL_ITEMS := $(INSTALL_ITEMS) /lib:$(BUILDDIR)/lib$(COMPONENT)$(LIBEXT)
