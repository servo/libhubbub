# Makefile for Hubbub testcases
#
# Toolchain is exported by top-level makefile
#
# Top-level makefile also exports the following variables:
#
# COMPONENT  Name of component
# EXPORT     Absolute path of export directory
# TOP        Absolute path of source tree root
#
# The top-level makefile requires the following targets to exist:
#
# clean      Clean source tree
# debug      Create a debug binary
# distclean  Fully clean source tree, back to pristine condition
# export     Export distributable components to ${EXPORT}
# release    Create a release binary
# setup      Perform any setup required prior to compilation
# test       Execute any test cases

# Extend toolchain settings
# We require the presence of libjson -- http://oss.metaparadigm.com/json-c/
CFLAGS += -I${TOP}/src/ -I$(CURDIR) \
		`${PKGCONFIG} ${PKGCONFIGFLAGS} --cflags json`
LDFLAGS += `${PKGCONFIG} ${PKGCONFIGFLAGS} --libs json`

# Release output
RELEASE =

# Debug output
DEBUG =

# Objects
OBJS = aliases cscodec csdetect dict entities filter hubbub \
	inputstream parser parser-utf16 tokeniser tokeniser2 \
	tree
OBJS += regression/cscodec-segv regression/filter-segv regression/stream-nomem

.PHONY: clean debug export release setup test

# Targets
release:

debug:

clean:
	-@${RM} ${RMFLAGS} $(addsuffix ${EXEEXT},  $(OBJS))

distclean:
	-@${RM} ${RMFLAGS} log

setup:

export:

test: $(OBJS)
	@${PERL} testrunner.pl ${EXEEXT}

# Pattern rules
%: %.c
	@${ECHO} ${ECHOFLAGS} "==> $<"
	@${CC} -c -g ${CFLAGS} -o $@.o $<
	@${LD} -g -o $@ $@.o ${LDFLAGS} -lhubbub-debug
	@${RM} ${RMFLAGS} $@.o
