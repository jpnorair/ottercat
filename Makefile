CC=gcc

THISMACHINE := $(shell uname -srm | sed -e 's/ /-/g')
THISSYSTEM	:= $(shell uname -s)

APP         ?= ottercat
PKGDIR      := ../_hbpkg/$(THISMACHINE)
SYSDIR      := ../_hbsys/$(THISMACHINE)
EXT_DEF     ?= 
EXT_INC     ?= 
EXT_LIBFLAGS ?= 
EXT_LIBS    ?= 
VERSION     ?= 1.0.a

# Try to get git HEAD commit value
ifneq ($(INSTALLER_HEAD),)
    GITHEAD := $(INSTALLER_HEAD)
else
    GITHEAD := $(shell git rev-parse --short HEAD)
endif

ifeq ($(MAKECMDGOALS),debug)
	APPDIR      := bin/$(THISMACHINE)
	BUILDDIR    := build/$(THISMACHINE)_debug
	DEBUG_MODE  := 1
else
	APPDIR      := bin/$(THISMACHINE)
	BUILDDIR    := build/$(THISMACHINE)
	DEBUG_MODE  := 0
endif


# Make sure the LD_LIBRARY_PATH includes the _hbsys directory
ifneq ($(findstring $(SYSDIR)/lib,$(LD_LIBRARY_PATH)),)
	error "$(SYSDIR)/lib not in LD_LIBRARY_PATH.  Please update your settings to include this."
endif


DEFAULT_DEF := -DOTTERCAT_PARAM_GITHEAD=\"$(GITHEAD)\"
LIBMODULES  := argtable cJSON bintex $(EXT_LIBS)
SUBMODULES  := main

SRCEXT      := c
DEPEXT      := d
OBJEXT      := o

CFLAGS_DEBUG:= -std=gnu99 -Og -g -Wall -pthread
CFLAGS      := -std=gnu99 -O3 -pthread
INC         := -I. -I./include -I./$(SYSDIR)/include
INCDEP      := -I.
LIBINC      := -L./$(SYSDIR)/lib
LIB         := -largtable -lbintex -lcJSON -ltalloc -lm -lc

OTTERCAT_PKG   := $(PKGDIR)
OTTERCAT_DEF   := $(DEFAULT_DEF) $(EXT_DEF)
OTTERCAT_INC   := $(INC) $(EXT_INC)
OTTERCAT_LIBINC:= $(LIBINC)
OTTERCAT_LIB   := $(EXT_LIBFLAGS) $(LIB)
OTTERCAT_BLD   := $(BUILDDIR)
OTTERCAT_APP   := $(APPDIR)


# Export the following variables to the shell: will affect submodules
export OTTERCAT_PKG
export OTTERCAT_DEF
export OTTERCAT_LIBINC
export OTTERCAT_INC
export OTTERCAT_LIB
export OTTERCAT_BLD
export OTTERCAT_APP

deps: $(LIBMODULES)
all: release
release: directories $(APP)
debug: directories $(APP).debug
obj: $(SUBMODULES)
pkg: deps all install
remake: cleaner all


install: 
	@rm -rf $(PKGDIR)/$(APP).$(VERSION)
	@mkdir -p $(PKGDIR)/$(APP).$(VERSION)
	@cp $(APPDIR)/$(APP) $(PKGDIR)/$(APP).$(VERSION)/
	@rm -f $(PKGDIR)/$(APP)
	@ln -s $(APP).$(VERSION) ./$(PKGDIR)/$(APP)
	cd ../_hbsys && $(MAKE) sys_install INS_MACHINE=$(THISMACHINE) INS_PKGNAME=ottercat

directories:
	@mkdir -p $(APPDIR)
	@mkdir -p $(BUILDDIR)

# Clean only this machine
clean:
	@$(RM) -rf $(BUILDDIR)
	@$(RM) -rf $(APPDIR)

# Clean all builds
cleaner: 
	@$(RM) -rf ./build
	@$(RM) -rf ./bin

#Linker
$(APP): $(SUBMODULES) 
	$(eval OBJECTS := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)"))
	$(CC) $(CFLAGS) $(OTTERCAT_DEF) $(OTTERCAT_INC) $(OTTERCAT_LIBINC) -o $(APPDIR)/$(APP) $(OBJECTS) $(OTTERCAT_LIB)

$(APP).debug: $(SUBMODULES)
	$(eval OBJECTS_D := $(shell find $(BUILDDIR) -type f -name "*.$(OBJEXT)"))
	$(CC) $(CFLAGS_DEBUG) $(OTTERCAT_DEF) -D__DEBUG__ $(OTTERCAT_INC) $(OTTERCAT_LIBINC) -o $(APPDIR)/$(APP).debug $(OBJECTS_D) $(OTTERCAT_LIB)

#Library dependencies (not in ottercat sources)
$(LIBMODULES): %: 
	cd ./../$@ && $(MAKE) pkg

#ottercat submodules
$(SUBMODULES): %: directories
	cd ./$@ && $(MAKE) -f $@.mk obj EXT_DEBUG=$(DEBUG_MODE)

#Non-File Targets
.PHONY: deps all release debug obj pkg remake install directories clean cleaner

