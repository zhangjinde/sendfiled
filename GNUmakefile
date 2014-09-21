projectname := fiod
target := $(projectname)
target_so := lib$(projectname).so
test_target := tests

CC := /usr/local/llvm342/bin/clang
CXX := /usr/local/llvm342/bin/clang++

lib_search_dirs := -L/usr/local/llvm342/lib

header_search_dirs := \
	-isystem/usr/local/llvm342/include \
	-isystem/usr/local/llvm342/include/c++/v1 \

warnflags := -Weverything \
-Wno-documentation \
-Wno-global-constructors \
-Wno-c++98-compat \

CFLAGS := -std=c99 -g -O0 $(warnflags) -fPIC -fvisibility=hidden $(header_search_dirs)
CXXFLAGS := -std=c++11 -stdlib=libc++ -g -O0 $(warnflags) -fPIC -fvisibility=hidden $(header_search_dirs)

DOXYGEN ?= doxygen

GTEST_FILTER ?= *

builddir := build
srcdir := src
docdir := doc
htmldir := $(docdir)/html

vpath %.c $(srcdir) $(srcdir)/impl
vpath %.cpp $(srcdir) $(srcdir)/impl
vpath %.odg $(docdir)/img
vpath %.png $(htmldir)/img

src_c:=\
fiod.c \
file_io.c\
file_io_linux.c\
fiod_linux.c \
process.c \
protocol.c \
server.c\
syspoll_linux.c \
unix_sockets.c \
unix_sockets_linux.c \

src_test:=\
test_fiod.cpp\
test_protocol.cpp\
test_utils.cpp\

src_all := $(src_c) $(src_test)

obj_c:=$(src_c:%=$(builddir)/%.o)
obj_test:=$(src_test:%=$(builddir)/%.o)

$(builddir)/test_%.cpp.o: CXXFLAGS += -Wno-error

.PHONY: all
all: $(builddir)/$(target) $(builddir)/$(target_so) build_tests

.PHONY: build_tests
build_tests: $(builddir)/$(test_target)

ifneq ($(MAKECMDGOALS), clean)
-include $(src_all:%=$(builddir)/%.d)
endif

$(builddir)/%.c.d: %.c
	$(CC) $(CFLAGS) -MM -MT "$(builddir)/$*.c.o $(builddir)/$*.c.d" $< > $@

$(builddir)/%.cpp.d: %.cpp
	$(CXX) $(CXXFLAGS) -MM -MT "$(builddir)/$*.cpp.o $(builddir)/$*.cpp.d" $< > $@

$(builddir)/%.c.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(builddir)/%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(builddir)/$(target): $(obj_c) $(builddir)/main.c.o
	$(CC) $(CFLAGS) $(lib_search_dirs) -o $@ $^ $(linkflags)

$(builddir)/$(target_so): $(obj_c)
	$(CC) $(CFLAGS) $(lib_search_dirs) -shared -o $@ $^ $(linkflags)

$(builddir)/$(test_target): $(obj_c) $(obj_test)
	$(CXX) $(CXXFLAGS) $(lib_search_dirs) -o $@ $^ $(linkflags) -lgtest -lgtest_main

.PHONY: clean
clean:
	rm -f $(builddir)/*

.PHONY: test
test: $(builddir)/$(test_target) $(builddir)/$(target)
	@echo "-----------------------------"
	@echo "T E S T S"
	@echo "-----------------------------"
	@- $< $(GTEST_FLAGS) --gtest_filter=$(GTEST_FILTER)

archive_name := $(projectname)_`date +%Y-%m-%d_%H%M%S`
.PHONY: bu
bu:
	git gc --quiet
	tarsnap --exclude $(builddir) --exclude $(htmldir) -cf $(archive_name) .

%.png: %.odg
	$(LODRAW) --headless --convert-to png  --outdir $(htmldir)/img $<
	$(CONVERT) -resize 50% -trim \
	$(htmldir)/img/$@ $(htmldir)/img/$(@:%.png=%_small.png)

.PHONY: doc
doc: $(doc_img_src:%.odg=%.png)
	$(DOXYGEN) doxyfile

.PHONY: exports
exports: $(builddir)/$(target_so)
	nm -gC --defined-only $<

.PHONY: memcheck
memcheck: $(builddir)/$(test_target) $(builddir)/$(target)
	valgrind \
	--track-origins=yes \
	--tool=memcheck \
	--leak-check=full \
	--show-reachable=yes \
	--read-var-info=yes \
	--track-fds=yes $< --gtest_filter=$(GTEST_FILTER)
