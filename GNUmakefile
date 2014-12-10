projectname := fiod
target := $(projectname)
target_so := lib$(projectname).so
test_target := tests

prefix ?= /usr/local
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin
libdir ?= $(exec_prefix)/lib
includedir ?= $(prefix)/include
datarootdir ?= $(prefix)/share
datadir ?= $(datarootdir)
docdir ?= $(datarootdir)/doc/$(projectname)
htmldir ?= $(docdir)

builddir := build
srcdir := src
docdir := doc
htmldir := $(docdir)/html

install := install

# The directory in which the server's UNIX socket will be located (default value)
default_server_sockdir := /tmp

# ----------------
# CLANG
# ----------------
#CC := /usr/local/llvm342/bin/clang
#warnflags :=\
#-Werror\
#-Wall\
#-Wextra\
#-Weverything\
#-Wno-documentation\

# ----------------------
# GCC/default C compiler
# ----------------------
CC := c99
warnflags :=\
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

CXX := /usr/local/llvm342/bin/clang++
warnflags_cxx := \
-Wall\
-Wextra\
-Weverything\
-Werror\
-Wno-c++98-compat\
-Wno-documentation\

lib_search_dirs :=
lib_search_dirs_cxx := -L/usr/local/llvm342/lib

header_search_dirs :=\
-I$(builddir)\

header_search_dirs_cxx :=\
	$(header_search_dirs) \
-isystem/usr/local/llvm342/include\
-isystem/usr/local/llvm342/include/c++/v1\

ifdef NDEBUG
	dbgflags := -O2
else
	dbgflags := -g -O0
endif

soflags := -fPIC -fvisibility=hidden

CFLAGS += $(dbgflags) $(warnflags) $(header_search_dirs)
CXXFLAGS += -std=c++11 -stdlib=libc++\
$(dbgflags) $(warnflags_cxx) $(header_search_dirs_cxx)

DOXYGEN ?= doxygen
NM ?= nm
VALGRIND ?= valgrind

GTEST_FILTER ?= *

vpath %.c $(srcdir) $(srcdir)/impl
vpath %.cpp $(srcdir) $(srcdir)/impl
vpath %.odg $(docdir)/img
vpath %.png $(htmldir)/img

src_common :=\
process.c \
responses.c\
unix_sockets.c\
unix_sockets_linux.c\
util.c\

src_client := $(src_common)\
fiod.c\
fiod_linux.c\
protocol_client.c\
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
test_fiod.cpp\
test_interpose_linux.c \
test_protocol.cpp\
test_server_xfer_table.cpp\
test_utils.cpp\

obj_c_client:=$(src_client:%=$(builddir)/%.cli.o)
obj_c_server:=$(src_server:%=$(builddir)/%.srv.o)
obj_test:=$(src_test:%=$(builddir)/%.tst.o)

$(builddir)/test_%.cpp.tst.o: CXXFLAGS += -Wno-error

.PHONY: all
all: config $(builddir)/$(target) $(builddir)/$(target_so) build_tests

.PHONY: config
config: $(builddir)/fiod_config.h

$(builddir)/fiod_config.h: $(lastword $(MAKEFILE_LIST))
	@echo "#define FIOD_PROGNAME \"$(projectname)\"" > $@
	@echo "#define FIOD_SRV_SOCKDIR \"$(default_server_sockdir)\"" >> $@

.PHONY: build_tests
build_tests: $(builddir)/$(test_target)

ifneq ($(MAKECMDGOALS), clean)
-include $(builddir)/main.c.srv.d
-include $(src_server:%=$(builddir)/%.srv.d)
-include $(src_client:%=$(builddir)/%.cli.d)
-include $(src_test:%=$(builddir)/%.tst.d)
endif

$(builddir)/%.c.srv.d: %.c $(builddir)/$(projectname)_config.h
	$(CC) $(CFLAGS) -MM -MT "$(builddir)/$*.c.srv.o $(builddir)/$*.c.srv.d" $< > $@

$(builddir)/%.c.cli.d: %.c $(builddir)/$(projectname)_config.h
	$(CC) $(CFLAGS) -MM -MT "$(builddir)/$*.c.cli.o $(builddir)/$*.c.cli.d" $< > $@

$(builddir)/%.cpp.tst.d: %.cpp $(builddir)/$(projectname)_config.h
	$(CXX) $(CXXFLAGS) -MM -MT "$(builddir)/$*.cpp.tst.o $(builddir)/$*.cpp.tst.d" $< > $@

$(builddir)/%.c.tst.d: %.c $(builddir)/$(projectname)_config.h
	$(CC) $(CFLAGS) -MM -MT "$(builddir)/$*.c.tst.o $(builddir)/$*.c.tst.d" $< > $@

$(builddir)/%.c.srv.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(builddir)/%.c.cli.o: %.c
	$(CC) $(CFLAGS) $(soflags) -c -o $@ $<

$(builddir)/%.cpp.tst.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(builddir)/%.c.tst.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(builddir)/$(target): $(obj_c_server) $(builddir)/main.c.srv.o
	$(CC) $(CFLAGS) $(lib_search_dirs) -o $@ $^ $(linkflags)

$(builddir)/$(target_so): $(obj_c_client)
	$(CC) $(CFLAGS) $(soflags) $(lib_search_dirs) -shared -o $@ $^ $(linkflags)

$(builddir)/$(test_target): $(obj_c_server) $(obj_test) $(builddir)/$(target_so)
	$(CXX) $(CXXFLAGS) $(lib_search_dirs_cxx) -o $@ $^ \
	$(linkflags) -ldl -lgtest -lgtest_main

.PHONY: install
install: $(builddir)/$(target) $(builddir)/$(target_so)
	$(install) -d $(bindir)
	$(install) -d $(libdir)
	$(install) $(builddir)/$(target) $(bindir)
	$(install) $(builddir)/$(target_so) $(libdir)
	$(install) -d $(includedir)/$(projectname)
	$(install) -m 444 $(srcdir)/*.h $(includedir)/$(projectname)

.PHONY: clean
clean:
ifdef builddir
	$(RM) $(builddir)/*
else
	$(warning 'builddir' undefined)
endif

.PHONY: test
test: $(builddir)/$(test_target) $(builddir)/$(target)
	$(info --------------)
	$(info T E S T S)
	$(info --------------)
	@$< $(GTEST_FLAGS) --gtest_filter=$(GTEST_FILTER)

archive_name := $(projectname)_`date +%Y-%m-%d_%H%M%S`
.PHONY: bu
bu:
	git gc --quiet
	tarsnap --exclude $(builddir) --exclude $(htmldir) -cf $(archive_name) .

.PHONY: doc
doc:
	$(DOXYGEN) doxyfile

.PHONY: exports
exports: $(builddir)/$(target_so)
	$(NM) -gC --defined-only $<

.PHONY: memcheck
memcheck: $(builddir)/$(test_target) $(builddir)/$(target)
	$(VALGRIND) \
	--track-origins=yes \
	--tool=memcheck \
	--leak-check=full \
	--show-reachable=yes \
	--read-var-info=yes \
	--track-fds=yes $< --gtest_filter=$(GTEST_FILTER)
