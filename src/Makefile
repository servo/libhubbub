# Makefile for libhubbub
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

# Manipulate include paths
CFLAGS += -I$(CURDIR)

# Release output
RELEASE = ${TOP}/${COMPONENT}.a

# Debug output
DEBUG = ${TOP}/${COMPONENT}-debug.a

# Objects
OBJS = hubbub parser

.PHONY: clean debug distclean export release setup test

# Targets
release: $(addprefix Release/, $(addsuffix .o, $(OBJS)))
	@${MAKE} -C charset release
	@${MAKE} -C input release
	@${MAKE} -C tokeniser release
	@${MAKE} -C treebuilder release
	@${MAKE} -C utils release
	@${AR} ${ARFLAGS} $(RELEASE) Release/*

debug: $(addprefix Debug/, $(addsuffix .o, $(OBJS)))
	@${MAKE} -C charset debug
	@${MAKE} -C input debug
	@${MAKE} -C tokeniser debug
	@${MAKE} -C treebuilder debug
	@${MAKE} -C utils debug
	@${AR} ${ARFLAGS} $(DEBUG) Debug/*

clean:
	@${MAKE} -C charset clean
	@${MAKE} -C input clean
	@${MAKE} -C tokeniser clean
	@${MAKE} -C treebuilder clean
	@${MAKE} -C utils clean
	-@${RM} ${RMFLAGS} $(addprefix Release/, $(addsuffix .o, ${OBJS}))
	-@${RM} ${RMFLAGS} $(addprefix Debug/, $(addsuffix .o, ${OBJS}))
	-@${RM} ${RMFLAGS} $(RELEASE) $(DEBUG)

distclean:
	-@${RM} ${RMFLAGS} -r Release
	-@${RM} ${RMFLAGS} -r Debug

setup:
	@${MKDIR} ${MKDIRFLAGS} Release
	@${MKDIR} ${MKDIRFLAGS} Debug

export:
	@${CP} ${CPFLAGS} $(RELEASE) ${EXPORT}/lib/

test:

# Pattern rules
Release/%.o: %.c
	@${ECHO} ${ECHOFLAGS} "==> $<"
	@${CC} -c ${CFLAGS} -DNDEBUG -o $@ $<

Debug/%.o: %.c
	@${ECHO} ${ECHOFLAGS} "==> $<"
	@${CC} -c -g ${CFLAGS} -o $@ $<
