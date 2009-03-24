ifeq ($(MAKECMDGOALS),test)
# Extend toolchain settings
# We require the presence of libjson -- http://oss.metaparadigm.com/json-c/
CFLAGS := $(CFLAGS) $(shell $(PKGCONFIG) $(PKGCONFIGFLAGS) --cflags json) \
		-Wno-unused-parameter
LDFLAGS := $(LDFLAGS) $(shell $(PKGCONFIG) $(PKGCONFIGFLAGS) --libs json)
endif

# Tests
DIR_TEST_SOURCES := csdetect.c dict.c entities.c hubbub.c parser.c \
	tokeniser.c tokeniser2.c tokeniser3.c tree.c tree2.c tree-buf.c

include build/makefiles/Makefile.subdir
