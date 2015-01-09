projectname := sendfiled
target := $(projectname)
target_so := lib$(projectname).so
test_target := tests

-include .site_vars.mk

prefix ?= /usr/local
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin
libdir ?= $(exec_prefix)/lib
includedir ?= $(prefix)/include

builddir ?= build
srcdir ?= src

# The directory in which the server's UNIX socket will be located (default
# value)
default_server_sockdir := /tmp

# ----------------
# CLANG
# ----------------
#CC := clang
#warnflags :=\
#-Werror\
#-Wall\
#-Wextra\
#-Weverything\
#-Wno-documentation\

# -------------------------
# Default C compiler (gcc?)
# -------------------------
CC := c99

warnflags ?= \
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
-Wlogical-op\
-Wmissing-prototypes -Wmissing-declarations\
-Wpadded \
-Winline \
-Wdisabled-optimization\
#-Wjump-misses-init\

CXX ?= c++

warnflags_cxx ?= \
-Wall\
-Wextra\
-Weverything\
-Werror\
-Wno-c++98-compat\
-Wno-documentation\

LIB_SEARCH_DIRS_CXX ?=

header_search_dirs :=\
	-I$(builddir)\

HEADER_SEARCH_DIRS_CXX := \
	$(header_search_dirs) \
	$(HEADER_SEARCH_DIRS_CXX)

ifdef NDEBUG
	dbgflags := -O2
else
	dbgflags := -g -O0
endif

CFLAGS += $(dbgflags) $(warnflags) $(header_search_dirs)

CXXFLAGS += -std=c++11 \
	$(dbgflags) $(warnflags_cxx) $(HEADER_SEARCH_DIRS_CXX)

binflags := -fpie
soflags := -fpic -fvisibility=hidden

INSTALL ?= install
DOXYGEN ?= doxygen
NM ?= nm
VALGRIND ?= valgrind

GTEST_FILTER ?= *

vpath %.c $(srcdir) $(srcdir)/impl
vpath %.cpp $(srcdir)/test $(srcdir)/impl

src_common :=\
process.c \
responses.c\
unix_sockets.c\
unix_sockets_linux.c\
util.c\

src_client := $(src_common)\
protocol_client.c\
sendfiled.c\
sendfiled_linux.c\
unix_socket_client.c\
unix_socket_client_linux.c\

src_server := $(src_common)\
file_io.c\
file_io_linux.c\
process_linux.c\
protocol_server.c\
server.c\
server_xfer_table.c\
syspoll_linux.c \
unix_socket_server.c\
unix_socket_server_linux.c\

src_test:=\
protocol_client.c\
test_interpose_linux.c \
test_protocol.cpp\
test_sendfiled.cpp\
test_server_xfer_table.cpp\
test_utils.cpp\

obj_c_client:=$(src_client:%=$(builddir)/%.cli.o)
obj_c_server:=$(src_server:%=$(builddir)/%.srv.o)
obj_test:=$(src_test:%=$(builddir)/%.tst.o)

$(builddir)/test_%.cpp.tst.o: CXXFLAGS += -Wno-error

.PHONY: all
all: config $(builddir)/$(target) $(builddir)/$(target_so) build_tests

.PHONY: config
config: $(builddir)/sfd_config.h

.PHONY: build_tests
build_tests: config $(builddir)/$(test_target)

-include .site_rules.mk

$(builddir)/sfd_config.h: $(lastword $(MAKEFILE_LIST))
	@echo "#define SFD_PROGNAME \"$(projectname)\"" > $@
	@echo "#define SFD_SRV_SOCKDIR \"$(default_server_sockdir)\"" >> $@

ifneq ($(MAKECMDGOALS), clean)
-include $(builddir)/main.c.srv.d
-include $(src_server:%=$(builddir)/%.srv.d)
-include $(src_client:%=$(builddir)/%.cli.d)
-include $(src_test:%=$(builddir)/%.tst.d)
endif

define DEPEND_C
@echo "DEP $<"
@$(CC) $(header_search_dirs) -MM -MT "$(basename $@).o $@" $< > $@
endef

define DEPEND_CXX
@echo "DEP $<"
@$(CXX) -std=c++11 -stdlib=libc++ $(HEADER_SEARCH_DIRS_CXX) \
-MM -MT "$(basename $@).o $@" $< > $@
endef

$(builddir)/%.c.srv.d: %.c $(builddir)/sfd_config.h
	$(DEPEND_C)

$(builddir)/%.c.cli.d: %.c $(builddir)/sfd_config.h
	$(DEPEND_C)

$(builddir)/%.cpp.tst.d: %.cpp $(builddir)/sfd_config.h
	$(DEPEND_CXX)

$(builddir)/%.c.tst.d: %.c $(builddir)/sfd_config.h
	$(DEPEND_C)

$(builddir)/%.c.srv.o: %.c
	@echo "CC  $<"
	@$(CC) $(CFLAGS) $(binflags) -c -o $@ $<

$(builddir)/%.c.cli.o: %.c
	@echo "CC  $<"
	@$(CC) $(CFLAGS) $(soflags) -c -o $@ $<

$(builddir)/%.cpp.tst.o: %.cpp
	@echo "CXX $<"
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

$(builddir)/%.c.tst.o: %.c
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(builddir)/$(target): $(obj_c_server) $(builddir)/main.c.srv.o
	@echo "LNK $(notdir $@)"
	@$(CC) $(CFLAGS) $(lib_search_dirs) -o $@ $^ $(linkflags)

$(builddir)/$(target_so): $(obj_c_client)
	@echo "LNK $(notdir $@)"
	@$(CC) $(CFLAGS) $(soflags) $(lib_search_dirs) -shared -o $@ $^ $(linkflags)

$(builddir)/$(test_target): $(obj_c_server) $(obj_test) $(builddir)/$(target_so)
	@echo "LNK $(notdir $@)"
	@$(CXX) $(CXXFLAGS) $(LIB_SEARCH_DIRS_CXX) -o $@ $^ \
	$(linkflags) -ldl -lgtest -lgtest_main

.PHONY: install
install: $(builddir)/$(target) $(builddir)/$(target_so)
	$(INSTALL) -d $(bindir)
	$(INSTALL) -d $(libdir)
	$(INSTALL) -o root -g root -m 4555 $(builddir)/$(target) $(bindir)
	$(INSTALL) -m 555 $(builddir)/$(target_so) $(libdir)
	$(INSTALL) -d $(includedir)/$(projectname)
	$(INSTALL) -m 444 $(srcdir)/*.h $(includedir)/$(projectname)

.PHONY: clean
clean:
ifdef builddir
	$(RM) $(builddir)/*
else
	$(warning 'builddir' undefined)
endif

.PHONY: test
test: config $(builddir)/$(test_target) $(builddir)/$(target)
	$(info --------------)
	$(info T E S T S)
	$(info --------------)
# Set PATH to the builddir (only) in order to ensure the just-built application
# binary is found instead of an installed version.
	@env PATH=$(builddir) $(builddir)/$(test_target) \
$(GTEST_FLAGS) --gtest_filter=$(GTEST_FILTER)

.PHONY: doc
doc:
	$(DOXYGEN) doxyfile

.PHONY: memcheck
memcheck: $(builddir)/$(test_target) $(builddir)/$(target)
	env PATH=$(builddir) $(VALGRIND) \
	--track-origins=yes \
	--tool=memcheck \
	--leak-check=full \
	--show-reachable=yes \
	--read-var-info=yes \
	--track-fds=yes $< --gtest_filter=$(GTEST_FILTER)
