# Toolchain definitions for building on the destination platform
export CC = gcc
export AR = ar
export LD = gcc

export CP = cp
export RM = rm
export MKDIR = mkdir
export MV = mv
export ECHO = echo
export MAKE = make
export PERL = perl
export PKGCONFIG = pkg-config

# Toolchain flags
WARNFLAGS = -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wnested-externs -Werror -pedantic
export CFLAGS = -std=c99 -D_BSD_SOURCE -I$(TOP)/include/ $(WARNFLAGS) 
export ARFLAGS = -cru
export LDFLAGS = -L$(TOP)/

export CPFLAGS =
export RMFLAGS = 
export MKDIRFLAGS = -p
export MVFLAGS =
export ECHOFLAGS = 
export MAKEFLAGS =
export PKGCONFIGFLAGS =

export EXEEXT =


include build/Makefile.common
