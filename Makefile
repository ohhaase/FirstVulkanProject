# Based on https://github.com/gszypko/spruce/blob/master/Makefile
# Which is based on http:\\make.mad-scientist.net\papers\advanced-auto-dependency-generation

# Notes (copied):
#	This file generates the dependencies and compiles each file within the CPP project.
# 	Files are only recompiled if they or their dependencies have changed.
#	The command <$(wildcard $(SRCDIR)/*.cpp)> must go as deep into source as there are files
#		(i.e., $(wildcard $(SRCDIR)/*/*.cpp)) if necessary and so on
#	Any additional source files not contained within source/ must be manually appended
#	Any additional header folders not associated with .cpp files must also be manually appended

# input arguments with default values
flags ?= -fopenmp -O3
version ?= -std=c++17

# define project directories
SRCDIR := src
BLDDIR := build
OBJDIR := $(BLDDIR)/objects
DEPDIR := $(BLDDIR)/deps

# obtain source files, include folders, and object and dependency names from source folder
SRCS := $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/*/*.cpp)
HEADERS := $(sort $(dir $(SRCS))) include
OBJS := $(SRCS:%.cpp=$(OBJDIR)/%.o)
DEPFILES = $(SRCS:%.cpp=$(DEPDIR)/%.d)
EXEC := run$(extension)

# define flags for compilation
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
CXXFLAGS = $(version) $(flags)
CPPFLAGS = $(patsubst %,-I %,$(HEADERS))
LNKFLAGS = -lm -lstdc++fs

# detect operating system and define shell commands accordingly
# each of path, makedir, removedir, removefile, and extension must defined for your operatings sytem 
# and compiler
ifeq ($(OS),Windows_NT)
define path
$(subst /,\,$1)
endef
define makedir
if not exist $1 mkdir $(call path,$1)
endef
define removedir
if exist $1 rmdir /s /q $(call path,$1)
endef
define removefile
if exist $1 del /f $(call path,$1)
endef
CXX = g++
extension=.exe
else # Unix
define path
$(subst /,/,$1)
endef
define makedir
mkdir -p $(call path,$1)
endef
define removedir
rm -r $(call path,$1)
endef
define removefile
rm -f $(call path,$1)
endef
CXX = g++-15
extension=
endif

# define targets to be completed in order
all : init run

# print compilation data to command line
init :
	@$(call removefile,$(EXEC))
	@echo -------
	@echo Flags: $(CXXFLAGS)
	@echo Linker: $(LNKFLAGS)
	@echo Headers: $(CPPFLAGS)
	@echo Sources: $(SRCS)

# define compilation goal
run : $(OBJS)
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(EXEC) $(LNKFLAGS)
	@echo Complete

# rule for making object files
$(OBJDIR)/%.o : %.cpp $(DEPDIR)/%.d | $(DEPDIR)
	@echo Compiling Object: $@ -- Dependencies: $<
	@$(call makedir,$(dir $@))
	@$(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

# rule for making dependency directories
$(DEPDIR) : ; @$(call makedir,$(dir $@))

# rule for making dependency files
$(DEPFILES):
	@$(call makedir,$(dir $@))
include $(wildcard $(DEPFILES))

# option to clean directories at the end
clean :
	@$(call removedir, $(BLDDIR))
	@$(call removefile,$(EXEC))