CC := /usr/local/llvm342/bin/clang
CXX := /usr/local/llvm342/bin/clang++
CFLAGS := -std=c99 -g -O0
CXXFLAGS := -std=c++11 -g -O0

projectname := fiod
target := $(projectname)
target_so := lib$(projectname).so
test_target := tests

builddir := build
srcdir := src

vpath %.c $(srcdir)
vpath %.cpp $(srcdir)

src_c:=\
fiod.c

src_test:=\

src_all := $(src_c) $(src_test)

obj_c:=$(src_c:%.c=$(builddir)/%.o)

# Add some flags and libraries to the test-related targets
$(builddir)/test_%.cpp.o: CXXFLAGS += -Wno-error

.PHONY: all
all: $(builddir)/$(target) $(builddir)/$(target_so)

$(builddir)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

ifneq ($(MAKECMDGOALS), clean)
-include $(src_all:%.c=$(builddir)/%.d)
endif

$(builddir)/%.d: %.c
	$(CC) $(CFLAGS) -MM -MT "$(builddir)/$*.o $(builddir)/$*.d" $< > $@

$(builddir)/$(target): $(obj_c) $(builddir)/main.o
	$(CC) $(CFLAGS) $(lib_search_dirs) -o $@ $^ $(linkflags)

$(builddir)/$(target_so): $(obj_c)
	$(CC) $(CFLAGS) $(lib_search_dirs) -shared -o $@ $^ $(linkflags)

.PHONY: clean
clean:
	rm -f $(builddir)/*
