# ==========================
# Variables
# ==========================

projectname := sendfiled
target := $(projectname)
target_so := lib$(projectname).so
target_test := tests

# 'c99' has a very limited interface on FreeBSD, so going with cc -std=c99
# instead. Need to set CC early as well as GNU Make sets it to 'g++' if not done
# early enough.
CC ?= cc
CXX ?= c++

# Standard GNU output/installation directories
prefix ?= /usr/local
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin
libdir ?= $(exec_prefix)/lib
includedir ?= $(prefix)/include

# Project directories
srcdir := src
builddir := build
# The default value for the directory in which the server's UNIX socket file
# will be created
default_server_sockdir := /tmp

# -I$(builddir) for the config header
CFLAGS += -std=c99 -I$(builddir)\
-D_FILE_OFFSET_BITS=64\
-Werror\
-Wall\
-Wextra\
-pedantic\
-Wformat-security\
-Winit-self\
-Wswitch\
-Wconversion\
-Wbad-function-cast\
-Wcast-align\
-Wwrite-strings\
-Wmissing-prototypes -Wmissing-declarations\
-Wpadded\
-Winline\
-Wdisabled-optimization\
#-Wjump-misses-init

# -I$(builddir) for the config header
CXXFLAGS +=-std=c++1y -I$(builddir)\
-Wall\
-Wextra\
-Weverything\
-Werror\
-Wno-c++98-compat\
-Wno-documentation

ifdef NDEBUG
	CFLAGS += -O2
	CXXFLAGS += -O2
else
	CFLAGS += -g -O0
	CXXFLAGS += -g -O0
endif

# Additional flags passed (to the compiler) when linking the test binary (C++)
LDFLAGS_TEST ?=
# Additional libraries to pass when linking. OS-specific; added to later.
LDLIBS_TEST ?=

# Site-specific settings (e.g., overriding or modifying variables declared
# above). Ignored by git.
-include .site_vars.mk

INSTALL ?= install
DOXYGEN ?= doxygen
NM ?= nm
VALGRIND ?= valgrind

GTEST_FILTER ?= *

# ==========================
# Source files
# ==========================

vpath %.c $(srcdir) $(srcdir)/impl
vpath %.cpp $(srcdir)/test $(srcdir)/impl

src_common :=\
log.c\
process.c \
responses.c\
unix_sockets.c\
util.c\

src_client = $(src_common)\
protocol_client.c\
sendfiled.c\
unix_socket_client.c\

src_server = $(src_common)\
file_io.c\
protocol_server.c\
server.c\
server_resources.c\
server_responses.c\
server_xfer_table.c\
unix_socket_server.c\

src_test:=\
protocol_client.c\
test_protocol.cpp\
test_sendfiled.cpp\
test_server_xfer_table.cpp\
test_syspoll.cpp\
test_utils.cpp\

osname := $(shell uname -s)

ifeq ($(osname), Linux)
src_common += unix_sockets_linux.c util_linux.c
src_client += unix_socket_client_linux.c
src_server += file_io_linux.c syspoll_linux.c\
unix_socket_server_linux.c
src_test += test_interpose_linux.c
LDLIBS_TEST += -ldl
else ifeq ($(osname), FreeBSD)
src_common += unix_sockets_freebsd.c util_posix.c
src_client += unix_socket_client_freebsd.c
src_server += file_io_freebsd.c file_io_userspace_splice.c syspoll_kqueue.c \
unix_socket_server_freebsd.c
src_test += test_interpose_freebsd.c
LDLIBS_TEST += -lpthread
else
$(error "Unsupported platform: $(osname)")
endif

# ==========================
# Objects
# ==========================

obj_c_client:=$(src_client:%=$(builddir)/%.cli.o)
obj_c_server:=$(src_server:%=$(builddir)/%.srv.o)
obj_test:=$(src_test:%=$(builddir)/%.tst.o)

# ==========================
# Target-specific settings
# ==========================

$(builddir)/test_%.cpp.tst.o: CXXFLAGS += -Wno-error

# ==========================
# Targets
# ==========================

.PHONY: all
all: config $(builddir)/$(target) $(builddir)/$(target_so) build_tests

.PHONY: config
config: $(builddir)/sfd_config.h

.PHONY: build_tests
build_tests: config $(builddir)/$(target_test)

-include .site_rules.mk

$(builddir)/sfd_config.h: $(lastword $(MAKEFILE_LIST))
	@echo "#define SFD_PROGNAME \"$(projectname)\"" > $@
	@echo "#define SFD_SRV_SOCKDIR \"$(default_server_sockdir)\"" >> $@

# ----------------------
# Dependencies
# ----------------------

ifneq ($(MAKECMDGOALS), clean)
-include $(builddir)/main.c.srv.d
-include $(src_server:%=$(builddir)/%.srv.d)
-include $(src_client:%=$(builddir)/%.cli.d)
-include $(src_test:%=$(builddir)/%.tst.d)
endif

define DEPEND_C
@echo "DEP $<"
@$(CC) -MM $(CPPFLAGS) $(CFLAGS) -MT "$(basename $@).o $@" $< > $@
endef

define DEPEND_CXX
@echo "DEP $<"
@$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) -MT "$(basename $@).o $@" $< > $@
endef

$(builddir)/%.c.srv.d: %.c $(builddir)/sfd_config.h
	$(DEPEND_C)

$(builddir)/%.c.cli.d: %.c $(builddir)/sfd_config.h
	$(DEPEND_C)

$(builddir)/%.cpp.tst.d: %.cpp $(builddir)/sfd_config.h
	$(DEPEND_CXX)

$(builddir)/%.c.tst.d: %.c $(builddir)/sfd_config.h
	$(DEPEND_C)

# ----------------------
# Server executable
# ----------------------

$(builddir)/%.c.srv.o: %.c
	@echo "CC  $<"
	@$(CC) -c $(CPPFLAGS) $(CFLAGS) -fpie -o $@ $<

$(builddir)/$(target): $(obj_c_server) $(builddir)/main.c.srv.o
	@echo "LNK $(notdir $@)"
	@$(CC) $(LDFLAGS) -pie -o $@ $^

# ----------------------
# Client shared library
# ----------------------

$(builddir)/%.c.cli.o: %.c
	@echo "CC  $<"
	@$(CC) -c $(CPPFLAGS) $(CFLAGS) -fpic -fvisibility=hidden -o $@ $<

$(builddir)/$(target_so): $(obj_c_client)
	@echo "LNK $(notdir $@)"
	@$(CC) -shared $(LDFLAGS) -fpic -fvisibility=hidden -o $@ $^

# ----------------------
# Test executable
# ----------------------

$(builddir)/%.cpp.tst.o: %.cpp
	@echo "CXX $<"
	@$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

$(builddir)/%.c.tst.o: %.c
	@echo "CC  $<"
	@$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(builddir)/$(target_test): $(obj_c_server) $(obj_test) $(builddir)/$(target_so)
	@echo "LNK $(notdir $@)"
	@$(CXX) $(LDFLAGS_TEST) -o $@ $^ $(LDLIBS_TEST) -lgtest -lgtest_main

# ----------------------
# Misc. targets
# ----------------------

.PHONY: clean
clean:
ifdef builddir
	$(RM) $(builddir)/*
else
	$(warning 'builddir' undefined)
endif

.PHONY: test
test: config $(builddir)/$(target_test) $(builddir)/$(target)
	$(info --------------)
	$(info T E S T S)
	$(info --------------)
# Set PATH to the builddir (only) in order to ensure the just-built application
# binary is found instead of an installed version.
	@env PATH=$(builddir) $(builddir)/$(target_test) \
$(GTEST_FLAGS) --gtest_filter=$(GTEST_FILTER)

.PHONY: install
install: $(builddir)/$(target) $(builddir)/$(target_so)
	$(INSTALL) -d $(bindir)
	$(INSTALL) -d $(libdir)
	$(INSTALL) -o root -g root -m 4555 $(builddir)/$(target) $(bindir)
	$(INSTALL) -m 555 $(builddir)/$(target_so) $(libdir)
	$(INSTALL) -d $(includedir)/$(projectname)
	$(INSTALL) -m 444 $(srcdir)/*.h $(includedir)/$(projectname)

.PHONY: doc
doc:
	$(DOXYGEN) doxyfile

.PHONY: memcheck
memcheck: $(builddir)/$(target_test) $(builddir)/$(target)
	env PATH=$(builddir) $(VALGRIND) \
	--track-origins=yes \
	--tool=memcheck \
	--leak-check=full \
	--show-reachable=yes \
	--read-var-info=yes \
	--track-fds=yes $< --gtest_filter=$(GTEST_FILTER)
