############################################
# A test makefile for incremental building.#
############################################
############################################################################
# By default, building non-OFFICIAL, non-DEBUG version.                    #
# If you want to get a debug version, use                                  #
# `make DEBUG=true`                                                        #
# If you want to get a official version, which does always autoupdate, use #
# `make release`                                                     #
############################################################################

##############################
# A name of server executable.


GAME ?= sh1x
LOCAL_TEST ?= 0
ifeq ($(GAME),sh1x)
TARGETNAME=sh1x_dedrun
else ifeq ($(GAME),cod4x)
TARGETNAME=cod4x18_dedrun
else
$(error GAME must be sh1x or cod4x)
endif

###################################################################
# Build system specific information.
# In git not exist there will be some errors, but nothing critical.
BUILD_NUMBER:=$(shell git rev-list --count HEAD)
BUILD_BRANCH:=$(shell git rev-parse --abbrev-ref HEAD)
BUILD_REVISION:=$(shell git rev-parse HEAD)

ifneq ($(OS),Windows_NT)
VERSION=$(shell grep '\#define SYS_COMMONVERSION' src/version/version.c | cut -d' ' -f3)
$(info You build CoD4 version: $(VERSION))
endif

ifeq ($(BUILD_NUMBER), )
BUILD_NUMBER:=0
endif

ifeq ($(BUILD_BRANCH), )
BUILD_BRANCH:=no-branch
endif

ifeq ($(BUILD_REVISION), )
BUILD_REVISION:=no-revision
endif

###################
# Compiler options.
CC=gcc
CPP=g++
WIN_DEFINES=WINVER=0x501
LINUX_DEFINES=_GNU_SOURCE
COMMON_FLAGS=-m32 -msse2 -mfpmath=sse -Wall -fno-omit-frame-pointer -fmax-errors=15 -MMD -MP $(OL_INC)
CFLAGS=$(COMMON_FLAGS) -std=gnu11
CXXFLAGS=$(COMMON_FLAGS) -std=gnu++11

ifeq ($(DEBUG), true)
DCFLAGS=-fno-pie -O0 -g
else
DCFLAGS=-fno-pie -O1 -DNDEBUG
endif

WIN_LFLAGS=-m32 -g -Wl,--nxcompat,--large-address-aware,--stack,0x800000 -mwindows -static-libgcc -static -lm
WIN_LLIBS=tomcrypt mbedtls mbedcrypto mbedx509 ws2_32 wsock32 iphlpapi gdi32 winmm crypt32 advapi32 stdc++
LINUX_LFLAGS=-m32 -g -static-libgcc -rdynamic -Wl,-rpath=./
LINUX_LLIBS=tomcrypt mbedtls mbedcrypto mbedx509 dl pthread m stdc++
BSD_LLIBS=tomcrypt mbedtls mbedcrypto mbedx509 pthread m execinfo stdc++
COD4X_DEFINES=COD4X18UPDATE BUILD_NUMBER=$(BUILD_NUMBER) BUILD_BRANCH=$(BUILD_BRANCH) BUILD_REVISION=$(BUILD_REVISION)
ifeq ($(GAME),sh1x)
COD4X_DEFINES+=OL_SERVER
ifeq ($(LOCAL_TEST),1)
COD4X_DEFINES+=LOCAL_TEST_BUILD
endif
endif

########################
# Setup directory names.
SRC_DIR=src
PLUGIN_DIR=plugins
BIN_DIR=bin
LIB_DIR=lib
OBJ_DIR=obj/$(GAME)
PLUGINS_DIR=plugins
ZLIB_DIR=$(SRC_DIR)/zlib
WIN_DIR=$(SRC_DIR)/win32
LINUX_DIR=$(SRC_DIR)/unix
ASSETS_DIR=$(SRC_DIR)/xassets
#####################################
# CoDOL sources live under src/online/<area>/ (see docs/SOURCE_LAYOUT.md).
# Every area directory is an include path so `#include "ol_x.h"` keeps
# working from anywhere; object files stay flat in $(OBJ_DIR).
OL_DIR=$(SRC_DIR)/online
OL_AREAS=net snapshot player weapon vehicle script assets collision stats
DIRS=$(addprefix $(OL_DIR)/,$(OL_AREAS))
OL_INC=-I$(SRC_DIR) $(addprefix -I,$(DIRS))
EXTERNAL=mbedtls tomcrypt

# The native test suite cannot use lib/: a cross-build leaves PE archives
# there, and the mbedtls and tomcrypt makefiles compile their objects next to
# their sources, so rebuilding them for the host would fight the cross-build
# over the same object files. test-external copies both trees into the object
# directory and builds host copies into TEST_LIB_DIR, leaving lib/ and the
# in-tree objects exactly as the cross-build left them.
TEST_LIB_DIR=lib-native
TEST_EXT_DIR=$(OBJ_DIR)/native-ext
# Build the copied libraries into the root without passing the absolute
# workspace path (which may contain spaces) through their prerequisite lists.
TEST_LIB_TARGET_REL=$(shell realpath -m --relative-to=$(TEST_EXT_DIR)/mbedtls $(TEST_LIB_DIR))

##############################
# Setup external applications.
NASM=nasm

###########################################################
# Setup OS-specific variables (All the garbage goes there).
ifeq ($(OS),Windows_NT)
####################
# Windows variables.
BIN_EXT=.exe
NASMFLAGS=-f win -dWin32 --prefix _
OS_SOURCES=$(wildcard $(WIN_DIR)/*.c)
OS_OBJ=$(patsubst $(WIN_DIR)/%.c,$(OBJ_DIR)/%.o,$(OS_SOURCES))
C_DEFINES=$(addprefix -D,$(COD4X_DEFINES) $(WIN_DEFINES))
LFLAGS=$(WIN_LFLAGS)
LLIBS=-L$(LIB_DIR)/ $(addprefix -l,$(WIN_LLIBS))
# Prefer the explicitly cross-prefixed windres, but fall back to the plain
# name: an msys2 mingw32 install ships only C:/msys64/mingw32/bin/windres.exe,
# and the hardcoded prefixed name failed the resource step with "Error 127"
# (command not found) -- which only surfaces on a forced rebuild, because the
# .res is otherwise never remade. Put mingw32/bin ahead of usr/bin on PATH so
# the plain name resolves to the 32-bit one (tools/BUILD-SERVER.ps1 does).
WINDRES?=$(shell command -v i686-w64-mingw32-windres >/dev/null 2>&1 && echo i686-w64-mingw32-windres || echo windres)
RESOURCE_FILE=$(OBJ_DIR)/win_sh1x.res
DEF_FILE=$(BIN_DIR)/$(TARGETNAME).def
INTERFACE_LIB=$(PLUGINS_DIR)/libcom_plugin.a
ADDITIONAL_OBJ=$(INTERFACE_LIB)
CLEAN=-del $(subst /,\\,$(OBJ_DIR)/*.o $(DEF_FILE) $(INTERFACE_LIB))
else
#################
# LINUX variables.
BIN_EXT=
NASMFLAGS=-f elf
OS_SOURCES=$(wildcard $(LINUX_DIR)/*.c)
OS_OBJ=$(patsubst $(LINUX_DIR)/%.c,$(OBJ_DIR)/%.o,$(OS_SOURCES))
C_DEFINES=$(addprefix -D,$(COD4X_DEFINES) $(LINUX_DEFINES))
LFLAGS=$(LINUX_LFLAGS)

UNAME := $(shell uname)
ifeq ($(UNAME),FreeBSD)
LLIBS=-L./$(LIB_DIR) $(addprefix -l,$(BSD_LLIBS))
else
LLIBS=-L./$(LIB_DIR) $(addprefix -l,$(LINUX_LLIBS))
endif

RESOURCE_FILE=
ADDITIONAL_OBJ=
CLEAN=-rm $(OBJ_DIR)/*.o $(DEF_FILE) $(INTERFACE_LIB)

endif

#####################
# Source files lists.
ifeq ($(GAME),sh1x)
NASMFLAGS+=-dOL_SERVER
endif
TARGET=$(addprefix $(BIN_DIR)/,$(TARGETNAME)$(BIN_EXT))
ASM_SOURCES=$(wildcard $(SRC_DIR)/asmsource/*.asm)
C_SOURCES=$(wildcard $(SRC_DIR)/*.c) $(foreach d,$(DIRS),$(wildcard $(d)/*.c))
CPP_SOURCES=$(wildcard $(SRC_DIR)/*.cpp) $(foreach d,$(DIRS),$(wildcard $(d)/*.cpp))
ZLIB_SOURCES=$(wildcard $(ZLIB_DIR)/*.c)
ASSETS_SOURCES=$(wildcard $(ASSETS_DIR)/*.c)

#################################################################
# Object files lists. (prefixes for rules may be required later).
ASM_OBJ=$(patsubst $(SRC_DIR)/asmsource/%.asm,$(OBJ_DIR)/%.o,$(ASM_SOURCES))
C_OBJ=$(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(C_SOURCES)))
CPP_OBJ=$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(notdir $(CPP_SOURCES)))
ZLIB_OBJ=$(patsubst $(ZLIB_DIR)/%.c,$(OBJ_DIR)/%.o,$(ZLIB_SOURCES))
ASSETS_OBJ=$(patsubst $(ASSETS_DIR)/%.c,$(OBJ_DIR)/%.o,$(ASSETS_SOURCES))

#############################################################
#############################################################
#############################################################
#############################################################
#############################################################


###############################
# Default rule: rebuild server.
all: notify $(EXTERNAL) $(TARGET) $(ADDITIONAL_OBJ)
	@echo Server done

notify:
	@echo Server start

###############################
# Compile every object WITHOUT linking.
#
# `all` links $(TARGET), and ld DELETES its output when a link fails -- which
# is how a bare `make` destroyed bin/sh1x_dedrun.exe on 2026-09-17. This
# target is the safe "does it still compile?" check: it can never remove or
# replace a deployed binary. tools/BUILD-SERVER.ps1 -ObjectsOnly drives it.
.PHONY: objects print-objects
objects: $(OS_OBJ) $(C_OBJ) $(CPP_OBJ) $(ZLIB_OBJ) $(ASSETS_OBJ) $(ASM_OBJ) $(OBJ_DIR)/version.o
	@echo Objects done

# The object list on one line, for a driver that wants to build a subset.
print-objects:
	@echo $(OS_OBJ) $(C_OBJ) $(CPP_OBJ) $(ZLIB_OBJ) $(ASSETS_OBJ) $(ASM_OBJ) $(OBJ_DIR)/version.o

# defines this new target for tagging and releasing
.PHONY: release
ifeq ($(GAME),sh1x)
release:
	$(error CoDOL is an incomplete port; the upstream publishing target is disabled)
else
release: C_DEFINES+=$(addprefix -D,OFFICIAL)
release: releaseprolog clean all plugins gittagging
endif

# defines warning message and buildtype official
.PHONY: releaseprolog
releaseprolog:
	@echo ""
ifneq ($(OS),Windows_NT)
	@echo "Are you sure you want to create a GitHub Release? [y/N] " && read ans && [ $${ans:-N} = y ]
endif

.PHONY: gittagging
gittagging:
ifneq ($(OS),Windows_NT)
	git tag -a v$(VERSION)
	git push origin --tags
endif	

#################################
# A rule to make mbedtls library.
mbedtls:
	@echo   $(MAKE)  $@
	@$(MAKE) -C $(SRC_DIR)/$@

##################################
# A rule to make tomcrypt library.
tomcrypt:
	@echo   $(MAKE)  $@
	@$(MAKE) -C $(SRC_DIR)/$@

##################################
# A rule to make bot library.
botlib:
	@echo   sh  $@
ifeq ($(OS),Windows_NT)
#	@cmd.exe /C "@cd $(SRC_DIR)/$@ && @comp.cmd"
else
#	cd $(SRC_DIR)/$@ && ./comp.sh
endif

###############################
# A rule to link server binary.
$(TARGET): $(OS_OBJ) $(C_OBJ) $(CPP_OBJ) $(ZLIB_OBJ) $(ASSETS_OBJ) $(ASM_OBJ) $(OBJ_DIR)/version.o $(RESOURCE_FILE) | $(EXTERNAL)
	@echo   $(CC) $(TARGET)
# CFLAGS for compiler, LFLAGS for linker.
	@$(CC) $(LFLAGS) -o $@ $^ $(LLIBS)

ifeq ($(OS),Windows_NT)
###############################
# The same server as a DLL for an in-process host (win32/sys_host.c: Sh1xHost_Init/Frame/Command/ExitCode).
# Kernel33 (the CoDOL client loader, feat/p2p-inprocess) loads it to run a player-hosted match in the client.
HOST_DLL=$(BIN_DIR)/sh1x_host.dll
host-dll: $(HOST_DLL)
$(HOST_DLL): $(OS_OBJ) $(C_OBJ) $(CPP_OBJ) $(ZLIB_OBJ) $(ASSETS_OBJ) $(ASM_OBJ) $(OBJ_DIR)/version.o $(RESOURCE_FILE) | $(EXTERNAL)
	@echo   $(CC) $(HOST_DLL)
	@$(CC) -shared $(filter-out -mwindows,$(LFLAGS)) -Wl,--dynamicbase -o $@ $^ $(LLIBS)
.PHONY: host-dll

###############################
# The same server as a static library that Kernel33.dll links (feat/server-in-kernel33: the client carries the
# match server itself, no sh1x_host.dll). The objects of host-dll (without the resource) and the external archives
# are one relocatable object in which every symbol is local but the host API (Sh1xHost_*) and the COMDAT sections
# (tools/sh1x_host_lib_symbols.py): nothing the server defines can clash with, or silently replace, a symbol of the
# client (zlib, __cxa_atexit, ...). The .drectve exports (the plugin API) are dropped: the client exports nothing
# of the server. Build it with the client's own compiler (Kernel33 scripts/build-server.sh: MSYS2 mingw32), so the
# objects, libstdc++ and libgcc agree. HOST_TOOLS is the binutils prefix (i686-w64-mingw32- for a cross build).
HOST_LIB=$(BIN_DIR)/libsh1x_host.a
HOST_TOOLS?=
PYTHON?=python3
host-lib: $(HOST_LIB)
$(HOST_LIB): $(OS_OBJ) $(C_OBJ) $(CPP_OBJ) $(ZLIB_OBJ) $(ASSETS_OBJ) $(ASM_OBJ) $(OBJ_DIR)/version.o | $(EXTERNAL)
	@echo   ld -r $(HOST_LIB)
	@$(HOST_TOOLS)ld -r -d -o $(OBJ_DIR)/sh1x_host_lib.o $^ -L$(LIB_DIR) -ltomcrypt -lmbedtls -lmbedcrypto -lmbedx509
	@$(PYTHON) tools/sh1x_host_lib_symbols.py --objdump $(HOST_TOOLS)objdump $(OBJ_DIR)/sh1x_host_lib.o > $(OBJ_DIR)/sh1x_host_lib.localize
	@$(HOST_TOOLS)objcopy --remove-section=.drectve --localize-symbols=$(OBJ_DIR)/sh1x_host_lib.localize $(OBJ_DIR)/sh1x_host_lib.o $(OBJ_DIR)/sh1x_host_lib_local.o
	@rm -f $@
	@$(HOST_TOOLS)ar rcs $@ $(OBJ_DIR)/sh1x_host_lib_local.o
.PHONY: host-lib

$(RESOURCE_FILE): src/win32/win_cod4.rc src/win32/sh1x.ico
	$(WINDRES) -I src/win32 -i $< -O coff -o $@
endif

################################
# A rule to make version module.
$(OBJ_DIR)/version.o: src/version/version.c FORCE
	@echo   $(CC)  $@ $(C_DEFINES)
	@$(CC) -c $(CFLAGS) $(DCFLAGS) $(C_DEFINES) -o $@ $<

############################################
# An empty rule to force rebuild other rule.
FORCE:

# Separate profiles cannot reuse objects compiled with different wire formats.
$(OS_OBJ) $(C_OBJ) $(CPP_OBJ) $(ZLIB_OBJ) $(ASSETS_OBJ) $(ASM_OBJ) $(OBJ_DIR)/version.o: | $(OBJ_DIR)
$(OBJ_DIR):
ifneq ($(findstring sh,$(SHELL)),)
	@mkdir -p "$@"
else
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
endif

-include $(wildcard $(OBJ_DIR)/*.d)

#####################################
# A rule to build common server code.
# -march=nocona
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo   $(CC)  $@
	@$(CC) -c $(CFLAGS) $(DCFLAGS) $(C_DEFINES) -o $@ $<

#####################################
# A rule to build common c++ server code.
# -march=nocona
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@echo   $(CPP)  $@
	@$(CPP) -c $(CXXFLAGS) $(DCFLAGS) $(C_DEFINES) -o $@ $<

#########################################
# Rules to build CoDOL code from src/online/<area>/.
define AREA_RULES
$(OBJ_DIR)/%.o: $(1)/%.c
	@echo   $(CC)  $$@
	@$(CC) -c $(CFLAGS) $(DCFLAGS) $(C_DEFINES) -o $$@ $$<
$(OBJ_DIR)/%.o: $(1)/%.cpp
	@echo   $(CPP)  $$@
	@$(CPP) -c $(CXXFLAGS) $(DCFLAGS) $(C_DEFINES) -o $$@ $$<
endef
$(foreach d,$(DIRS),$(eval $(call AREA_RULES,$(d))))

################################
# A rule to build assemler code.
$(OBJ_DIR)/_bg_animation_mp.o: src/online/player/animation_moves.inc src/online/player/animation_conditions.inc src/online/player/animation_events.inc

$(OBJ_DIR)/%.o: $(SRC_DIR)/asmsource/%.asm
	@echo   $(NASM)  $@
	@$(NASM) $(NASMFLAGS) $< -o $@

###################################
# A rule to build zlib source code.
$(OBJ_DIR)/%.o: $(ZLIB_DIR)/%.c
	@echo   $(CC)  $@
	@$(CC) -c $(CFLAGS) $(DCFLAGS) $(C_DEFINES) -o $@ $<

######################################
# A rule to build xassets source code.
$(OBJ_DIR)/%.o: $(ASSETS_DIR)/%.c
	@echo   $(CC)  $@
	@$(CC) -c $(CFLAGS) $(DCFLAGS) $(C_DEFINES) -o $@ $<

########################################
# A rule to build Windows specific code.
$(OBJ_DIR)/%.o: $(WIN_DIR)/%.c
	@echo   $(CC)  $@
	@$(CC) -c $(CFLAGS) $(DCFLAGS) $(C_DEFINES) -o $@ $<

########################################
# A rule to build Linux specific code.
$(OBJ_DIR)/%.o: $(LINUX_DIR)/%.c
	@echo   $(CC)  $@
	@$(CC) -c $(CFLAGS) $(DCFLAGS) $(C_DEFINES) -o $@ $<

########################################################
# A rule for Windows to create server interface library.
# Skip an empty .def: without pexports there is no export list to import from,
# and handing dlltool a zero-byte file only prints "Syntax error in def file".
# The plugin import library is not needed to run the server.
$(INTERFACE_LIB): $(DEF_FILE) $(TARGET)
	@echo   dlltool  $@
	@if [ -s $(DEF_FILE) ]; then \
		dlltool -D $(TARGET) -d $(DEF_FILE) -l $@; \
	else \
		echo "  $(DEF_FILE) is empty - skipping the plugin import library"; \
	fi

####################################################################
# A rule for Windows to create server module definition file (.def).
# pexports is optional and is NOT part of a default msys2 install. It only
# generates the export list the plugin interface uses, so a missing one must
# not fail a build whose executable already linked: the link runs first, and
# failing here left `make` at exit 2 with a perfectly good bin/sh1x_dedrun.exe
# sitting next to it, which reads as "the build is broken".
# Guarded before the redirect so a missing tool cannot leave a truncated .def.
$(DEF_FILE): $(TARGET)
	@echo   pexports  $@
	@if command -v pexports >/dev/null 2>&1; then \
		pexports $^ > $@; \
	else \
		echo "  pexports not installed - keeping the existing $@ (only the plugin interface needs it)"; \
		[ -f $@ ] || : > $@; \
	fi

############################
# Delete built object files. (Server only)
clean:
	@echo   clean Server
	@$(CLEAN)

############################
# Delete built object files. (Server only)
clean_all:
	@echo   clean Server
	@$(CLEAN)
	@echo   clean Mbedtls
	@$(MAKE) -C $(SRC_DIR)/mbedtls clean
	@echo   clean Tomcrypt
	@$(MAKE) -C $(SRC_DIR)/tomcrypt clean

docker: $(TARGET)
	@docker build . -t cod4x/bleeding

plugins: 
	@$(MAKE) -C $(PLUGIN_DIR)/screenshotsender
	#@$(MAKE) -C $(PLUGIN_DIR)/antispam
	@$(MAKE) -C $(PLUGIN_DIR)/censor
	@$(MAKE) -C $(PLUGIN_DIR)/cod4x_b3hide
	@$(MAKE) -C $(PLUGIN_DIR)/simplebanlist
	@$(MAKE) -C $(PLUGIN_DIR)/example_cpp_plugin
	@$(MAKE) -C $(PLUGIN_DIR)/pchat
	@$(MAKE) -C $(PLUGIN_DIR)/sourcebansplugin
	@$(MAKE) -C $(PLUGIN_DIR)/warn
	#@$(MAKE) -C $(PLUGIN_DIR)/julia
	@$(MAKE) -C $(PLUGIN_DIR)/legacybanlist


.PHONY: plugins

INFLATE_SOURCES=src/zlib/inflate.c src/zlib/inffast.c src/zlib/inftrees.c src/zlib/adler32.c src/zlib/crc32.c
.PHONY: archive
archive: $(BIN_DIR)/sh1x_archive$(BIN_EXT)
$(BIN_DIR)/sh1x_archive$(BIN_EXT): tools/sh1x_archive.c src/online/assets/assets.c src/online/assets/zone_image.c src/online/assets/fastfile.c src/online/assets/client_crypto.c $(INFLATE_SOURCES) | $(EXTERNAL)
	$(CC) -m32 -O2 $(OL_INC) $^ -L$(TEST_LIB_DIR) -lmbedcrypto -o $@

.PHONY: test
ifeq ($(OS),Windows_NT)
test:
	$(error Run the native integration test under Linux/WSL; the portable Python wire tests also support Windows)
else
.PHONY: test-external
.PHONY: test-actor-wounded
test-actor-wounded: run-test_lookat
	python3 tests/test_actor_wounded_production.py

.PHONY: test-zombies-closure
test-zombies-closure:
	bash tools/test_zombies_closure.sh

.PHONY: test-warm-standby
test-warm-standby:
	python3 tools/test_warm_standby.py --output "$$(mktemp -d /tmp/codol-warm-standby.XXXXXX)/evidence"
	python3 tools/test_resident_registration.py

test-external: | $(OBJ_DIR)
	@mkdir -p $(TEST_EXT_DIR) $(TEST_LIB_DIR)
	@# rsync keeps the host objects of earlier runs (and never copies the
	@# cross-build's in-tree objects), so the libraries rebuild incrementally.
	@if command -v rsync >/dev/null 2>&1; then \
		rsync -a --exclude='*.o' $(SRC_DIR)/mbedtls $(SRC_DIR)/tomcrypt $(TEST_EXT_DIR)/; \
	else \
		cp -r $(SRC_DIR)/mbedtls $(SRC_DIR)/tomcrypt $(TEST_EXT_DIR)/ && find $(TEST_EXT_DIR) -name '*.o' -delete; \
	fi
	@$(MAKE) -s -C $(TEST_EXT_DIR)/mbedtls TARGETDIR=$(TEST_LIB_TARGET_REL) CC=gcc
	@$(MAKE) -s -C $(TEST_EXT_DIR)/tomcrypt TARGETDIR=$(TEST_LIB_TARGET_REL) CC=gcc

# Native tests. Each TEST_<name> holds exactly the arguments its gcc
# command takes (DRV_<name> = CPP selects g++). Instead of recompiling
# every source in one command per test, each .c/.cpp is compiled once per
# distinct compile-flag set into $(TOBJ_DIR)/<flag set>/<source>.o, so
# an object is shared only by tests whose compile flags are identical. The
# binary is then linked from those objects with the original argument order,
# so -l and -Wl positions are unchanged. Every test runs from its own target
# (run-<name>) with a private TMPDIR, and its output is printed in one
# piece, so `make -jN test` works.
TOBJ_DIR=$(OBJ_DIR)/tobj
OL_EMPTY:=
OL_SPACE:=$(OL_EMPTY) $(OL_EMPTY)
LINK_ONLY=-l% -L% -Wl,% -static-libgcc -static-libstdc++
ol_drv=$(or $(DRV_$(1)),CC)
ol_cflags=$(filter-out %.c %.cpp %.o $(LINK_ONLY),$(TEST_$(1)))
incs=$(filter -I%,$(call ol_cflags,$(1)))
# Flag order is canonicalised except for the -I list, whose order matters.
ol_flags=$(sort $(filter-out -I%,$(call ol_cflags,$(1)))) $(call incs,$(1))
ol_key=$(subst =,_,$(subst /,~,$(subst $(OL_SPACE),.,$(strip $(call ol_drv,$(1)) $(patsubst -%,%,$(sort $(filter-out -I%,$(call ol_cflags,$(1))))) $(if $(subst $(strip $(OL_INC)),,$(strip $(call incs,$(1)))),$(patsubst -%,%,$(call incs,$(1))),inc)))))
ol_obj=$(TOBJ_DIR)/$(2)/$(1).o
ol_objs=$(foreach s,$(filter %.c %.cpp,$(TEST_$(1))),$(call ol_obj,$(s),$(KEY_$(1))))
link_args=$(foreach a,$(TEST_$(1)),$(if $(filter %.c %.cpp,$(a)),$(call ol_obj,$(a),$(KEY_$(1))),$(a)))
ol_run=t=$$(mktemp -d) && l=$$(mktemp) && echo '$(1)' >$$l && ( export TMPDIR=$$t; $(1) ) >>$$l 2>&1; rc=$$?; cat $$l; rm -rf $$t $$l; exit $$rc

define KEY_RULES
KEYFLAGS_$(1) := $(3)
$(TOBJ_DIR)/$(1)/%.c.o: %.c
	@mkdir -p $$(@D)
	$$($(2)) $$(KEYFLAGS_$(1)) -MMD -MP -c $$< -o $$@
$(TOBJ_DIR)/$(1)/%.cpp.o: %.cpp
	@mkdir -p $$(@D)
	$$($(2)) $$(KEYFLAGS_$(1)) -MMD -MP -c $$< -o $$@
endef

define TEST_RULES
$(if $(KEYFLAGS_$(KEY_$(1))),,$(eval $(call KEY_RULES,$(KEY_$(1)),$(call ol_drv,$(1)),$(call ol_flags,$(1)))))
TEST_DEPS += $(patsubst %.o,%.d,$(call ol_objs,$(1)))
$(OBJ_DIR)/$(1)$(BIN_EXT): $(call ol_objs,$(1)) $(filter %.o,$(TEST_$(1))) | test-external $(OBJ_DIR)
	$$($(call ol_drv,$(1))) $(call link_args,$(1)) -o $$@
.PHONY: run-$(1)
run-$(1): $(OBJ_DIR)/$(1)$(BIN_EXT)
	@$$(call ol_run,$(or $(RUN_$(1)),./$(OBJ_DIR)/$(1)$(BIN_EXT)))
endef

define PY_RULES
.PHONY: run-py-$(1)
run-py-$(1):
	@$$(call ol_run,python3 tests/$(1).py)
endef

OL_TESTS += test_melee_feedback
TEST_test_melee_feedback = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_melee_feedback.c src/online/weapon/melee_event.c -Wl,--gc-sections
test: run-test_reliable_menu run-test_melee_feedback
OL_TESTS += test_missile_stuck
TEST_test_missile_stuck = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_missile_stuck.c src/online/script/equipment_lifecycle.c -Wl,--gc-sections
OL_TESTS += test_missile_pickup
TEST_test_missile_pickup = -m32 -O1 -g -DOL_SERVER $(OL_INC) tests/test_missile_pickup.c src/online/weapon/missile_pickup.c src/online/weapon/knife_pickup.c
OL_TESTS += test_match_status
TEST_test_match_status = -m32 -O1 -g -DOL_SERVER $(OL_INC) tests/test_match_status.c src/online/net/match_status.c
OL_TESTS += test_gamestate_check
TEST_test_gamestate_check = -m32 -O1 -g $(OL_INC) tests/test_gamestate_check.c src/online/snapshot/gamestate_check.c src/online/snapshot/configstrings.c src/online/net/ol_msg.c
test: run-test_gamestate_check
OL_TESTS += test_radius_distance
TEST_test_radius_distance = -m32 -O1 -g $(OL_INC) tests/test_radius_distance.c src/online/weapon/radius_distance.c -lm
OL_TESTS += test_player_lerp
TEST_test_player_lerp = -m32 -O1 -g $(OL_INC) tests/test_player_lerp.c src/online/snapshot/player_lerp.c -lm
OL_TESTS += test_peer_pool
TEST_test_peer_pool = -m32 -msse2 -O1 -g -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_peer_pool.c -Wl,--gc-sections
OL_TESTS += test_session_reservation
TEST_test_session_reservation = -m32 -msse2 -O1 -g -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_session_reservation.c -Wl,--gc-sections
OL_TESTS += test_session_relocation
TEST_test_session_relocation = -m32 -msse2 -O1 -g -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_session_relocation.c -Wl,--gc-sections
RUN_test_session_relocation = cd $(OBJ_DIR) && ./test_session_relocation$(BIN_EXT) session-cache-test-17731.txt session-cache-test-17732.txt
test: run-test_session_relocation
OL_TESTS += test_data_fast_path
TEST_test_data_fast_path = -m32 -msse2 -O1 -g -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_data_fast_path.c -Wl,--gc-sections
OL_TESTS += test_handshake_guard
TEST_test_handshake_guard = -m32 -msse2 -O1 -g -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_handshake_guard.c -Wl,--gc-sections
test: run-test_handshake_guard
OL_TESTS += test_vote_guard
TEST_test_vote_guard = -m32 -O1 -g $(OL_INC) tests/test_vote_guard.c src/online/script/vote_guard.c
test: run-test_vote_guard
OL_TESTS += test_info_string
TEST_test_info_string = -m32 -msse2 -O1 -g -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_info_string.c src/qshared.c -Wl,--gc-sections
test: run-test_info_string
OL_TESTS += test_atomic_file
TEST_test_atomic_file = -m32 -O1 -g -D_GNU_SOURCE -Isrc/online/net tests/test_atomic_file.c src/online/net/atomic_file.c
OL_TESTS += test_bot_offhand
TEST_test_bot_offhand = -m32 -O1 -g $(OL_INC) tests/test_bot_offhand.c src/online/player/bot_offhand_logic.c -lm
OL_TESTS += test_bot_objective
TEST_test_bot_objective = -m32 -O1 -g $(OL_INC) tests/test_bot_objective.c src/online/player/bot_objective_pick.c -lm
OL_TESTS += test_owned_items
TEST_test_owned_items = -m32 -O1 -g $(OL_INC) tests/test_owned_items.c src/online/stats/owned_items.c
RUN_test_owned_items = cd $(OBJ_DIR) && ./test_owned_items$(BIN_EXT)
OL_TESTS += test_pure
TEST_test_pure = -m32 -msse2 -O1 -g -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_pure.c src/online/net/pure.c src/sv_client.c -Wl,--gc-sections
OL_TESTS += test_stats
TEST_test_stats = -m32 -O1 -g $(OL_INC) tests/test_stats.c src/online/stats/ol_stats.c src/online/stats/profile_updates.c
OL_TESTS += test_structured_data
TEST_test_structured_data = -m32 -O1 -g $(OL_INC) tests/test_structured_data.c src/online/stats/structured_data.c
OL_TESTS += test_structured_schema
TEST_test_structured_schema = -m32 -O1 -g $(OL_INC) tests/test_structured_schema.c src/online/stats/structured_schema.c
OL_TESTS += test_matchdata
TEST_test_matchdata = -m32 -O1 -g $(OL_INC) tests/test_matchdata.c src/online/stats/matchdata.c src/online/stats/structured_schema.c
OL_TESTS += test_matchdata_wire
TEST_test_matchdata_wire = -m32 -O1 -g $(OL_INC) tests/test_matchdata_wire.c src/online/stats/matchdata_wire.c src/online/net/ol_msg.c
OL_TESTS += test_profile
TEST_test_profile = -m32 -O1 -g $(OL_INC) tests/test_profile.c src/online/stats/ol_profile.c src/online/stats/ol_stats.c src/online/stats/profile_updates.c src/online/stats/structured_schema.c src/online/stats/structured_data.c
OL_TESTS += test_profile_storage
TEST_test_profile_storage = -m32 -O1 -g $(OL_INC) tests/test_profile_storage.c src/online/stats/profile_storage.c src/online/assets/assets.c src/online/assets/zone_image.c $(INFLATE_SOURCES)
OL_TESTS += test_handshake
TEST_test_handshake = -m32 -O1 -g $(OL_INC) tests/test_handshake.c src/online/net/ol_handshake.c src/online/net/ol_ecdh.c src/online/net/association.c src/online/net/ol_crypto.c src/online/net/wire.c src/online/net/connect.c -L$(TEST_LIB_DIR) -ltomcrypt -lmbedcrypto
OL_TESTS += test_connect
TEST_test_connect = -m32 -msse2 -O1 -g -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_connect.c src/online/net/connect.c src/sv_client.c -Wl,--gc-sections
OL_TESTS += test_association
TEST_test_association = -m32 -O1 -g $(OL_INC) tests/test_association.c src/online/net/association.c src/online/net/ol_crypto.c src/online/net/wire.c -L$(TEST_LIB_DIR) -ltomcrypt -lmbedcrypto
OL_TESTS += test_ecdh
TEST_test_ecdh = -m32 -O1 -g $(OL_INC) tests/test_ecdh.c src/online/net/ol_ecdh.c -L$(TEST_LIB_DIR) -lmbedcrypto
OL_TESTS += test_frame_hud
TEST_test_frame_hud = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_frame_hud.c src/online/snapshot/sv_frame.c -Wl,--gc-sections
OL_TESTS += test_player_links
TEST_test_player_links = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_player_links.c src/online/snapshot/player_links_wire.c src/online/snapshot/playerstate.c -Wl,--gc-sections
OL_TESTS += test_hud_fontscale
TEST_test_hud_fontscale = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_hud_fontscale.c src/online/script/script_hud_fontscale.c src/g_hud.c src/online/snapshot/engine_state.c src/online/weapon/weapon_inventory.c src/online/player/sway_phase.c src/online/weapon/variable_zoom.c src/online/snapshot/player_links_wire.c src/online/snapshot/player_arrays.c -Wl,--gc-sections -lm src/online/snapshot/radar_state.c
OL_TESTS += test_usability
TEST_test_usability = -m32 -O1 -g -no-pie -DOL_SERVER $(OL_INC) tests/test_usability.c src/online/script/script_weapon_controls.c $(OBJ_DIR)/test_usability_asm.o $(OBJ_DIR)/test_usability_dependencies.o
OL_TESTS += test_offhand_controls
TEST_test_offhand_controls = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_offhand_controls.c src/online/script/script_weapon_controls.c src/online/snapshot/engine_state.c src/online/weapon/weapon_inventory.c src/online/player/sway_phase.c src/online/weapon/variable_zoom.c src/online/snapshot/player_links_wire.c src/online/snapshot/playerstate.c -Wl,--gc-sections -lm src/online/snapshot/radar_state.c
OL_TESTS += test_event_parameters
TEST_test_event_parameters = -m32 -O1 -g -ffunction-sections -fdata-sections $(OL_INC) tests/test_event_parameters.c src/online/snapshot/ol_events.c src/online/player/usercmd.c src/online/net/ol_msg.c -Wl,--gc-sections
OL_TESTS += test_weapon_change_event
TEST_test_weapon_change_event = -m32 -O1 -g -DOL_SERVER $(OL_INC) tests/test_weapon_change_event.c src/online/weapon/weapon_change_event.c
OL_TESTS += test_frame_entities
TEST_test_frame_entities = -m32 -O1 -g -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_frame_entities.c src/online/snapshot/playerstate.c src/online/snapshot/sv_entities.c src/online/player/loop_sound.c src/online/snapshot/playerstate_ext.c src/online/snapshot/engine_state.c src/online/weapon/weapon_inventory.c src/online/player/sway_phase.c src/online/weapon/variable_zoom.c src/online/snapshot/player_links_wire.c src/online/snapshot/ol_events.c src/online/snapshot/entities.c src/online/snapshot/entitystate_ext.c src/online/player/usercmd.c src/online/weapon/missile_positions.c src/online/script/script_model_scale.c -Wl,--gc-sections -lm src/online/snapshot/radar_state.c
OL_TESTS += test_field_orders
TEST_test_field_orders = -m32 -msse2 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_field_orders.c src/msg.c src/online/snapshot/playerstate_ext.c src/online/snapshot/playerstate.c src/online/player/field_orders.c -Wl,--gc-sections -lm
OL_TESTS += test_viewmodels
OL_TESTS += test_archive_stack
TEST_test_archive_stack = -m32 -msse2 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_archive_stack.c src/msg.c src/online/snapshot/playerstate_ext.c -Wl,--gc-sections -lm
TEST_test_viewmodels = -m32 -msse2 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_viewmodels.c src/msg.c src/online/snapshot/playerstate_ext.c -Wl,--gc-sections -lm
OL_TESTS += test_client_appearance
TEST_test_client_appearance = -m32 -msse2 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_client_appearance.c src/msg.c src/online/snapshot/clientstate_ext.c src/online/snapshot/clients.c -Wl,--gc-sections -lm
OL_TESTS += test_appearance_override
TEST_test_appearance_override = -m32 -O1 -g -Wall -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_appearance_override.c -Wl,--gc-sections -lm
OL_TESTS += test_loadout_recipe
TEST_test_loadout_recipe = -m32 -O1 -g $(OL_INC) -ffunction-sections -fdata-sections tests/test_loadout_recipe.c src/online/script/script_profile_loadout.c src/online/weapon/weapon_compose.c src/online/assets/character_item.c -Wl,--gc-sections -lm
OL_TESTS += test_party_weapon
TEST_test_party_weapon = -m32 -O1 -g -Wall -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_party_weapon.c src/online/script/script_profile_loadout.c src/online/weapon/weapon_compose.c src/online/weapon/weapon_attachment.c src/online/weapon/weapon_native.c src/online/weapon/weapon_registry.c src/online/assets/character_item.c -Wl,--gc-sections,--wrap=LoadComposedWeapon -lm
OL_TESTS += test_path_nodes
TEST_test_path_nodes = -m32 -O1 -DOL_SERVER $(OL_INC) tests/test_path_nodes.c src/online/script/script_path_nodes.c -lm
OL_TESTS += test_physics_brush
TEST_test_physics_brush = -m32 -O1 -g -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections tests/test_physics_brush.c src/online/collision/physics_native.c -Wl,--gc-sections -lm
OL_TESTS += test_inherited
TEST_test_inherited = -m32 -O1 $(OL_INC) tests/test_inherited.c src/online/snapshot/configstrings.c src/online/net/ol_msg.c src/online/snapshot/sv_state.c src/online/snapshot/gamestate_check.c src/online/snapshot/notetrack_effects.c src/online/snapshot/const_models.c
OL_TESTS += test_native
TEST_test_native = -m32 -msse2 -O1 -g -ffunction-sections -fdata-sections -DOL_SERVER -D_GNU_SOURCE $(OL_INC) tests/test_native.c src/online/assets/zone_path.c src/netchan.c src/msg.c src/online/net/ol_netchan.c src/online/net/wire.c src/online/net/ol_crypto.c src/online/snapshot/game.c src/online/player/usercmd.c src/online/net/ol_msg.c src/online/snapshot/configstrings.c src/online/snapshot/sv_state.c src/online/snapshot/gamestate_check.c src/online/snapshot/engine_state.c src/online/weapon/weapon_inventory.c src/online/player/sway_phase.c src/online/weapon/variable_zoom.c src/online/player/recoil_state.c src/online/player/sprint_state.c src/online/player/loop_sound.c src/online/player/perks.c src/online/snapshot/player_links_wire.c src/online/script/script_events.c src/online/script/script_model_scale.c src/online/script/obituary.c src/online/snapshot/playerstate.c src/online/snapshot/clients.c src/online/snapshot/entities.c src/online/snapshot/ol_events.c src/online/snapshot/sv_frame.c src/online/snapshot/ol_sv_snapshot.c src/online/stats/matchdata.c src/online/stats/matchdata_wire.c src/online/stats/structured_schema.c src/online/snapshot/frame_builder.c src/online/snapshot/snapshot.c src/online/snapshot/player_arrays.c src/online/snapshot/delta.c src/online/snapshot/actors.c src/online/snapshot/animcmd.c src/online/snapshot/history.c src/online/weapon/weapon_bits.c src/online/weapon/weapon_lock.c src/online/weapon/weapon_lock_clients.c src/online/snapshot/playerstate_ext.c src/online/snapshot/entitystate_ext.c -Wl,--gc-sections,--wrap=NET_SendPacket -L$(TEST_LIB_DIR) -ltomcrypt -lmbedcrypto -lm src/online/snapshot/radar_state.c src/online/snapshot/radar_input.c
OL_TESTS += test_assets
TEST_test_assets = -m32 -O1 -g -ffunction-sections -fdata-sections -D_GNU_SOURCE $(OL_INC) tests/test_assets.c src/online/assets/effects_native.c src/online/assets/effects.c src/online/collision/dynents.c src/online/collision/physics_native.c src/online/collision/physics.c src/online/assets/material_native.c src/online/assets/stream_model.c src/online/assets/xsurface_native.c src/online/assets/xsurface.c src/online/assets/xmodel_native.c src/online/assets/load_blocks.c src/online/assets/xmodel_bounds.c src/online/assets/asset_stream.c src/online/assets/xmodel_collision.c src/online/assets/xmodel_skeleton.c src/online/collision/collision_static_models.c src/online/collision/zone_collision.c src/online/collision/collision_map.c src/online/collision/c1_clipmap.c src/online/collision/c1_physics.c src/online/collision/collision_brush_tree.c src/online/collision/collision_leaves.c src/online/collision/collision_materials.c src/online/collision/world_planes.c src/online/collision/collision_brushes.c src/online/collision/collision_terrain.c src/online/collision/collision_stream.c src/online/collision/map_entities.c src/online/assets/ol_stringtable.c src/online/assets/assets.c src/online/assets/fastfile.c src/online/assets/client_crypto.c src/online/assets/asset_registry.c src/online/assets/c1_materialize.c src/online/assets/c1_xmodel.c src/online/assets/c1_xanim.c src/online/assets/zone_image.c src/online/assets/c1_records.c src/online/assets/sound_native.c src/online/assets/sound_source.c src/online/script/script_source.c src/online/assets/caseless.c src/online/stats/structured_schema.c src/online/weapon/weapon_native.c src/online/weapon/weapon_attachment.c src/online/weapon/c1_weapon.c src/online/weapon/weapon_compose.c src/online/weapon/missile_stickiness.c src/online/collision/glass_groups.c src/online/assets/glass_map.c src/online/assets/glass_load.c src/online/script/script_profile_vm.c src/online/assets/xanim_native.cpp $(INFLATE_SOURCES) -L$(TEST_LIB_DIR) -lmbedcrypto -Wl,--gc-sections
OL_TESTS += test_send
TEST_test_send = -m32 -msse2 -O1 -ffunction-sections -fdata-sections -DOL_SERVER -D_GNU_SOURCE $(OL_INC) tests/test_send.c src/sv_snapshot.c src/sv_net_chan.c src/online/net/wire.c src/msg.c src/online/snapshot/sv_frame.c src/online/snapshot/ol_sv_snapshot.c src/online/stats/matchdata.c src/online/stats/matchdata_wire.c src/online/stats/structured_schema.c src/online/snapshot/frame_builder.c src/online/snapshot/engine_state.c src/online/weapon/weapon_inventory.c src/online/player/sway_phase.c src/online/weapon/variable_zoom.c src/online/snapshot/player_links_wire.c src/online/script/script_events.c src/online/player/usercmd.c src/online/snapshot/ol_events.c src/online/snapshot/sv_state.c src/online/snapshot/gamestate_check.c src/online/snapshot/entitystate_ext.c src/online/snapshot/configstrings.c src/online/snapshot/history.c src/online/snapshot/snapshot.c src/online/net/ol_msg.c src/online/snapshot/delta.c src/online/snapshot/playerstate.c src/online/snapshot/player_arrays.c src/online/snapshot/entities.c src/online/snapshot/clients.c src/online/snapshot/actors.c src/online/snapshot/animcmd.c src/online/snapshot/playerstate_ext.c src/online/weapon/weapon_bits.c src/online/player/sprint_state.c src/online/player/recoil_state.c src/online/snapshot/glass_snapshot.c src/online/script/script_player_builtins.c -lm -Wl,--gc-sections src/online/snapshot/radar_state.c
OL_TESTS += test_cached_snapshot
TEST_test_cached_snapshot = -m32 -msse2 -O1 -g -ffunction-sections -fdata-sections -DOL_SERVER -D_GNU_SOURCE $(OL_INC) tests/test_cached_snapshot.c src/sv_snapshot.c src/msg.c src/online/snapshot/entitystate_ext.c src/online/snapshot/playerstate_ext.c -lm -Wl,--gc-sections
OL_TESTS += test_asset_names
TEST_test_asset_names = -m32 -msse2 -O1 -ffunction-sections -fdata-sections -DOL_SERVER -D_GNU_SOURCE $(OL_INC) tests/test_asset_names.c src/online/snapshot/snapshot.c src/online/snapshot/delta.c src/online/net/ol_msg.c src/online/snapshot/actors.c src/online/snapshot/animcmd.c src/online/snapshot/sv_gameframe.c src/online/snapshot/frame_builder.c src/online/snapshot/sv_entities.c src/online/player/usercmd.c src/online/snapshot/playerstate.c src/online/snapshot/ol_events.c src/online/snapshot/sv_frame.c src/online/script/script_hud_fontscale.c src/online/assets/sv_assets.c src/online/snapshot/sv_state.c src/online/snapshot/gamestate_check.c src/online/snapshot/configstrings.c src/online/snapshot/engine_state.c src/online/weapon/weapon_inventory.c src/online/player/sway_phase.c src/online/weapon/variable_zoom.c src/online/snapshot/player_links_wire.c src/online/script/script_events.c src/online/snapshot/entities.c src/online/snapshot/clients.c src/online/snapshot/player_arrays.c src/online/weapon/weapon_lock.c src/online/weapon/weapon_lock_clients.c src/online/snapshot/playerstate_ext.c src/online/snapshot/entitystate_ext.c src/online/weapon/missile_positions.c src/online/weapon/weapon_bits.c src/online/snapshot/glass_snapshot.c src/online/player/perks.c src/online/player/loop_sound.c src/online/snapshot/usehint_wire.c src/online/script/equipment_support.c src/online/player/action_slots.c src/online/player/killstreak_hud.c src/online/player/field_orders.c src/online/snapshot/radar_types.c src/online/weapon/weapon_state.c src/online/snapshot/clientstate_ext.c src/online/script/script_hud_notifications.c src/online/player/recoil_state.c src/online/player/sprint_state.c src/online/script/script_model_scale.c src/online/script/obituary.c src/online/snapshot/radar_input.c src/online/script/script_objectives.c src/online/script/script_player_builtins.c src/online/script/script_client_material.c -Wl,--gc-sections -lm src/online/snapshot/radar_state.c
OL_TESTS += test_model_lookup
DRV_test_model_lookup = CPP
TEST_test_model_lookup = -m32 -O1 -ffunction-sections -fdata-sections -D_GNU_SOURCE $(OL_INC) tests/test_model_lookup.cpp src/xmodel_utils.cpp $(OBJ_DIR)/test_model_access.o -Wl,--gc-sections
OL_TESTS += test_model_trace
TEST_test_model_trace = -m32 -msse2 -O1 -ffunction-sections -fdata-sections -DOL_SERVER -D_GNU_SOURCE $(OL_INC) tests/test_model_trace.c src/online/assets/xmodel_native.c src/cm_trace.c $(OBJ_DIR)/test_model_trace_asm.o -Wl,--gc-sections -lm
OL_TESTS += test_collision_native
TEST_test_collision_native = -m32 -O1 -ffunction-sections -fdata-sections $(OL_INC) tests/test_collision_native.c src/online/collision/dynents_native.c src/online/collision/collision_native.c src/online/collision/trigger_native.c src/online/collision/map_entities.c src/cm_load.c src/cm_test.c -Wl,--gc-sections -lm
OL_TESTS += test_effects_native
TEST_test_effects_native = -m32 -O1 $(OL_INC) tests/test_effects_native.c src/online/assets/effects_native.c
OL_TESTS += test_weapon_native
TEST_test_weapon_native = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_weapon_native.c src/online/weapon/weapon_native.c src/online/weapon/missile_stickiness.c src/online/weapon/weapon_attachment.c -Wl,--gc-sections -lm
OL_TESTS += test_script_weapon_queries
TEST_test_script_weapon_queries = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_script_weapon_queries.c src/online/script/script_weapon_queries.c -Wl,--gc-sections
OL_TESTS += test_script_class
TEST_test_script_class = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_script_class.c src/online/script/script_class.c src/online/script/script_profile_vm.c src/online/script/script_profile_loadout.c src/online/stats/ol_stats.c src/online/stats/profile_updates.c src/online/stats/ol_profile.c src/online/stats/structured_schema.c src/online/stats/structured_data.c src/online/assets/character_item.c -Wl,--gc-sections
OL_TESTS += test_weapon_loader
TEST_test_weapon_loader = -m32 -O1 $(OL_INC) tests/test_weapon_loader.c src/online/weapon/weapon_loader.c src/online/weapon/weapon_native.c src/online/assets/sound_native.c src/online/weapon/weapon_registry.c src/online/weapon/weapon_attachment.c src/online/weapon/missile_stickiness.c src/online/assets/sound_source.c -lm
OL_TESTS += test_weapon_timing_metadata
TEST_test_weapon_timing_metadata = -m32 -O1 -g -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_weapon_timing_metadata.c src/online/weapon/weapon_state.c src/online/weapon/variable_zoom.c -Wl,--gc-sections -lm src/online/weapon/weapon_inventory.c
OL_TESTS += test_dualwield_fill
TEST_test_dualwield_fill = -m32 -O1 -g -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_dualwield_fill.c src/online/weapon/weapon_state.c -Wl,--gc-sections -lm src/online/weapon/weapon_inventory.c
OL_TESTS += test_variable_zoom
TEST_test_variable_zoom = -m32 -O1 -g $(OL_INC) tests/test_variable_zoom.c src/online/weapon/variable_zoom.c -lm
OL_TESTS += test_weapon_registry
TEST_test_weapon_registry = -m32 -msse2 -O1 -g -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_weapon_registry.c src/online/weapon/weapon_registry.c src/online/snapshot/engine_state.c src/online/weapon/weapon_inventory.c src/online/player/sway_phase.c src/online/weapon/variable_zoom.c src/online/snapshot/player_links_wire.c src/online/player/usercmd.c -Wl,--gc-sections src/online/snapshot/radar_state.c
OL_TESTS += test_sv_weapons
TEST_test_sv_weapons = -m32 -O1 -g -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_sv_weapons.c src/online/snapshot/game.c src/online/weapon/sv_weapons.c src/online/snapshot/engine_state.c src/online/weapon/weapon_inventory.c src/online/player/sway_phase.c src/online/weapon/variable_zoom.c src/online/snapshot/player_links_wire.c src/online/weapon/weapon_registry.c src/online/player/usercmd.c src/online/snapshot/sv_state.c src/online/snapshot/gamestate_check.c src/online/snapshot/configstrings.c src/online/weapon/weapon_lock.c src/online/weapon/weapon_lock_clients.c src/online/weapon/weapon_bits.c src/online/weapon/weapon_state.c src/online/snapshot/playerstate_ext.c -Wl,--gc-sections src/online/snapshot/radar_state.c src/online/snapshot/radar_input.c -lm
OL_TESTS += test_xanim_abi
DRV_test_xanim_abi = CPP
TEST_test_xanim_abi = -m32 -O1 -static-libgcc -static-libstdc++ $(OL_INC) tests/test_xanim_abi.cpp
OL_TESTS += test_threat_bias
TEST_test_threat_bias = -m32 -O1 $(OL_INC) tests/test_threat_bias.c src/online/player/threat_bias.c -lm
OL_TESTS += test_script_source
TEST_test_script_source = -m32 -O1 $(OL_INC) tests/test_script_source.c src/online/script/script_source.c src/online/assets/caseless.c
OL_TESTS += test_admission
TEST_test_admission = -m32 -O1 -g -D_GNU_SOURCE $(OL_INC) tests/test_admission.c src/online/net/admission.c src/online/net/ol_crypto.c src/online/net/wire.c -L$(TEST_LIB_DIR) -ltomcrypt -lmbedcrypto
OL_TESTS += test_admission_coarse
TEST_test_admission_coarse = -m32 -O1 -g -D_GNU_SOURCE -DTEST_COARSE_STAT $(OL_INC) tests/test_admission.c src/online/net/admission.c src/online/net/ol_crypto.c src/online/net/wire.c -L$(TEST_LIB_DIR) -ltomcrypt -lmbedcrypto
OL_TESTS += test_script_entity
TEST_test_script_entity = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_script_entity.c src/online/script/script_entity.c -Wl,--gc-sections
OL_TESTS += test_script_math
TEST_test_script_math = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_script_math.c src/online/script/script_math.c -Wl,--gc-sections -lm
OL_TESTS += test_ability_math
TEST_test_ability_math = -m32 -msse2 -mfpmath=sse -O1 $(OL_INC) tests/test_ability_math.c src/online/weapon/ability_math.c -lm
OL_TESTS += test_pmove_c1_movedir
TEST_test_pmove_c1_movedir = -m32 -msse2 -mfpmath=sse -O1 $(OL_INC) tests/test_pmove_c1_movedir.c src/online/player/pmove_c1_movedir.c -lm
OL_TESTS += test_vehicle_controls
DRV_test_vehicle_controls = CPP
TEST_test_vehicle_controls = -m32 -O1 -g -DOL_SERVER $(OL_INC) tests/test_vehicle_controls.cpp src/online/vehicle/vehicle_controls.cpp src/online/vehicle/vehicle_drive.cpp -lm
OL_TESTS += test_dive
TEST_test_dive = -m32 -O1 -g -DOL_SERVER $(OL_INC) tests/test_dive.c src/online/player/dive.c -lm
OL_TESTS += test_pmove_c1_tail
TEST_test_pmove_c1_tail = -m32 -msse2 -mfpmath=sse -O1 $(OL_INC) tests/test_pmove_c1_tail.c src/online/player/pmove_c1_tail.c -lm
OL_TESTS += test_angle_vectors
TEST_test_angle_vectors = -m32 -msse2 -mfpmath=sse -O1 -DOL_SERVER -ffunction-sections -fdata-sections $(OL_INC) tests/test_angle_vectors.c src/q_math.c -Wl,--gc-sections -lm
OL_TESTS += test_c1_brush_trace
TEST_test_c1_brush_trace = -m32 -msse2 -mfpmath=sse -O1 $(OL_INC) tests/test_c1_brush_trace.c src/online/collision/c1_brush_trace.c -lm
OL_TESTS += test_pmove_c1_bob
TEST_test_pmove_c1_bob = -m32 -msse2 -mfpmath=sse -O1 $(OL_INC) tests/test_pmove_c1_bob.c src/online/player/pmove_c1_bob.c -lm
OL_TESTS += test_pmove_c1_cmdscale
TEST_test_pmove_c1_cmdscale = -m32 -msse2 -mfpmath=sse -O1 $(OL_INC) tests/test_pmove_c1_cmdscale.c src/online/player/pmove_c1_cmdscale.c -lm
OL_TESTS += test_profile_push
TEST_test_profile_push = -m32 -O1 $(OL_INC) tests/test_profile_push.c src/online/stats/profile_push.c
OL_TESTS += test_collectibles
TEST_test_collectibles = -m32 -O1 $(OL_INC) tests/test_collectibles.c src/online/script/script_collectibles.c
OL_TESTS += test_script_dvar
TEST_test_script_dvar = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_script_dvar.c src/online/script/script_dvar.c -Wl,--gc-sections
OL_TESTS += test_xp_scale
TEST_test_xp_scale = -m32 -O1 -D_GNU_SOURCE -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_xp_scale.c src/cvar.c src/qshared.c src/scr_vm_functions.c -Wl,--gc-sections -lm
test: run-test_xp_scale
OL_TESTS += test_playeruse_visibility
TEST_test_playeruse_visibility = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_playeruse_visibility.c src/online/script/script_playeruse.c -Wl,--gc-sections
test: run-test_playeruse_visibility
OL_TESTS += test_unlock_rules
TEST_test_unlock_rules = -m32 -O1 $(OL_INC) tests/test_unlock_rules.c src/online/stats/unlock_rules.c
OL_TESTS += test_csv
TEST_test_csv = -m32 -O1 $(OL_INC) tests/test_csv.c src/online/assets/ol_stringtable.c
OL_TESTS += test_profile_challenge
TEST_test_profile_challenge = -m32 -O1 $(OL_INC) -ffunction-sections -fdata-sections -Wl,--gc-sections tests/test_profile_challenge.c src/online/stats/profile_challenge.c src/online/stats/ol_profile.c src/online/stats/structured_schema.c src/online/stats/structured_data.c -lm
OL_TESTS += test_profile_experience
TEST_test_profile_experience = -m32 -O1 $(OL_INC) -ffunction-sections -fdata-sections -Wl,--gc-sections tests/test_profile_experience.c src/online/stats/profile_experience.c src/online/stats/ol_profile.c src/online/stats/structured_schema.c src/online/stats/structured_data.c
OL_TESTS += test_profile_unlock
TEST_test_profile_unlock = -m32 -O1 $(OL_INC) -ffunction-sections -fdata-sections -Wl,--gc-sections tests/test_profile_unlock.c src/online/stats/profile_unlock.c src/online/stats/unlock_rules.c src/online/stats/profile_experience.c src/online/stats/profile_challenge.c src/online/stats/ol_profile.c src/online/stats/structured_schema.c src/online/stats/structured_data.c -lm
OL_TESTS += test_profile_clients
TEST_test_profile_clients = -m32 -O1 $(OL_INC) -ffunction-sections -fdata-sections -Wl,--gc-sections tests/test_profile_clients.c src/online/stats/profile_clients.c src/online/stats/ol_profile.c
OL_TESTS += test_map_custom
TEST_test_map_custom = -m32 -O1 $(OL_INC) tests/test_map_custom.c src/online/assets/map_custom.c
OL_TESTS += test_team_fx
TEST_test_team_fx = -m32 -O1 -DOL_SERVER -D_GNU_SOURCE $(OL_INC) -ffunction-sections -fdata-sections -Wl,--gc-sections tests/test_team_fx.c src/online/player/team_fx.c src/online/script/script_fx.c src/online/snapshot/configstrings.c src/online/assets/sv_assets.c
OL_TESTS += test_profile_character
TEST_test_profile_character = -m32 -O1 $(OL_INC) -ffunction-sections -fdata-sections tests/test_profile_character.c src/online/stats/profile_character.c src/online/stats/ol_profile.c src/online/stats/structured_schema.c src/online/stats/structured_data.c -Wl,--gc-sections
OL_TESTS += test_character_variant
TEST_test_character_variant = -m32 -O1 $(OL_INC) tests/test_character_variant.c src/online/assets/character_variant.c
OL_TESTS += test_character_item
TEST_test_character_item = -m32 -O1 $(OL_INC) tests/test_character_item.c src/online/assets/character_item.c src/online/assets/character_variant.c
OL_TESTS += test_character_defaults
TEST_test_character_defaults = -m32 -O1 $(OL_INC) -ffunction-sections -fdata-sections tests/test_character_defaults.c src/online/assets/character_defaults.c src/online/assets/character_item.c -Wl,--gc-sections
OL_TESTS += test_character_rig
TEST_test_character_rig = -m32 -O1 $(OL_INC) -ffunction-sections -fdata-sections tests/test_character_rig.c src/online/assets/character_defaults.c src/online/assets/character_item.c -Wl,--gc-sections
OL_TESTS += test_character_visibility
TEST_test_character_visibility = -m32 -O1 $(OL_INC) tests/test_character_visibility.c src/online/assets/character_visibility.c
OL_TESTS += test_pose_budget
TEST_test_pose_budget = -m32 -O1 $(OL_INC) tests/test_pose_budget.c src/online/player/pose_budget.c
OL_TESTS += test_character_composition
TEST_test_character_composition = -m32 -O1 -g $(OL_INC) -ffunction-sections -fdata-sections tests/test_character_composition.c src/online/player/player_appearance.c src/online/assets/character_composition.c src/online/stats/profile_character.c src/online/stats/ol_profile.c src/online/stats/structured_schema.c src/online/stats/structured_data.c src/online/assets/character_item.c src/online/assets/character_defaults.c src/online/assets/character_visibility.c src/online/assets/character_variant.c -Wl,--gc-sections
OL_TESTS += test_weapon_lock
TEST_test_weapon_lock = -m32 -O1 -g $(OL_INC) -ffunction-sections -fdata-sections tests/test_weapon_lock.c src/online/weapon/weapon_lock.c src/online/snapshot/playerstate.c -Wl,--gc-sections
OL_TESTS += test_weapon_lock_clients
TEST_test_weapon_lock_clients = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_weapon_lock_clients.c src/online/weapon/weapon_lock.c src/online/weapon/weapon_lock_clients.c src/online/snapshot/playerstate_ext.c src/online/player/recoil_state.c src/online/player/sprint_state.c src/online/snapshot/game.c src/online/snapshot/game_reset.c src/online/player/usercmd.c -Wl,--gc-sections src/online/snapshot/radar_state.c src/online/snapshot/radar_input.c
OL_TESTS += test_weapon_lock_missile
TEST_test_weapon_lock_missile = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_weapon_lock_missile.c src/online/weapon/weapon_lock_missile.c src/online/weapon/weapon_lock.c src/online/weapon/weapon_lock_clients.c src/online/snapshot/playerstate_ext.c src/online/weapon/missile_positions.c -Wl,--gc-sections
OL_TESTS += test_weapon_lock_fire
TEST_test_weapon_lock_fire = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_weapon_lock_fire.c src/online/weapon/weapon_lock_fire.c src/online/weapon/weapon_native.c src/online/weapon/weapon_lock.c src/online/weapon/weapon_lock_clients.c src/online/snapshot/playerstate_ext.c -Wl,--gc-sections
OL_TESTS += test_script_weapon_lock
TEST_test_script_weapon_lock = -m32 -O1 -g -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_script_weapon_lock.c src/online/script/script_weapon_lock.c src/online/weapon/weapon_lock.c src/online/weapon/weapon_lock_clients.c src/online/snapshot/playerstate_ext.c -Wl,--gc-sections
OL_TESTS += test_script_server
TEST_test_script_server = -m32 -O1 -DOL_SERVER $(OL_INC) tests/test_script_server.c src/online/script/script_server.c
OL_TESTS += test_vehicle_nodes
TEST_test_vehicle_nodes = -m32 -O1 -DOL_SERVER $(OL_INC) tests/test_vehicle_nodes.c src/online/script/script_vehicle_nodes.c -lm
OL_TESTS += test_animation_tables
TEST_test_animation_tables = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_animation_tables.c src/online/player/animation_tables.c -Wl,--gc-sections
OL_TESTS += test_trigger_models
TEST_test_trigger_models = -m32 -O1 $(OL_INC) tests/test_trigger_models.c src/online/collision/map_entities.c -lm
OL_TESTS += test_reliable_menu
OL_TESTS += test_dev_control
TEST_test_dev_control = -m32 -O1 -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_dev_control.c src/online/net/dev_control.c -Wl,--gc-sections
test: run-test_dev_control
OL_TESTS += test_clip_box test_dev_cheats
TEST_test_clip_box = -m32 -O1 -DOL_SERVER $(OL_INC) tests/test_clip_box.c src/online/collision/clip_box.c
TEST_test_dev_cheats = -m32 -O1 -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_dev_cheats.c src/online/player/dev_cheats.c -Wl,--gc-sections
test: run-test_clip_box run-test_dev_cheats
OL_TESTS += test_actor_drop test_lookat
OL_TESTS += test_map_entity_policy
TEST_test_map_entity_policy = -m32 -O1 $(OL_INC) tests/test_map_entity_policy.c
OL_TESTS += test_actor_team
DRV_test_actor_team = CPP
TEST_test_actor_team = -m32 -O1 -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_actor_team.cpp src/online/snapshot/actor_state.cpp -Wl,--gc-sections
test: run-test_actor_team
TEST_test_actor_drop = -m32 -O1 -DOL_SERVER $(OL_INC) -ffunction-sections -fdata-sections tests/test_actor_drop.c src/online/player/actor_drop.c src/online/player/actor_ammo.c src/online/player/actor_fire.c -Wl,--gc-sections
TEST_test_lookat = -m32 -O1 -DOL_SERVER $(OL_INC) tests/test_lookat.c src/online/snapshot/lookat.c
test: run-test_actor_drop run-test_lookat
TEST_test_reliable_menu = -m32 -O1 $(OL_INC) tests/test_reliable_menu.c src/online/net/reliable_menu.c
OL_TESTS += test_script_menu
TEST_test_script_menu = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_script_menu.c src/online/script/script_menu.c src/online/snapshot/configstrings.c src/online/snapshot/sv_state.c src/online/snapshot/gamestate_check.c -Wl,--gc-sections
OL_TESTS += test_script_lower
DRV_test_script_lower = CPP
TEST_test_script_lower = -m32 -O1 $(OL_INC) tests/test_script_lower.cpp src/online/script/script_lower.cpp
OL_TESTS += test_xanim_native
DRV_test_xanim_native = CPP
TEST_test_xanim_native = -m32 -O1 -static-libgcc -static-libstdc++ $(OL_INC) tests/test_xanim_native.cpp src/online/assets/xanim_native.cpp
OL_TESTS += test_xanim_eval
DRV_test_xanim_eval = CPP
TEST_test_xanim_eval = -m32 -msse2 -O1 -no-pie $(OL_INC) tests/test_xanim_eval.cpp src/online/assets/xanim_native.cpp $(OBJ_DIR)/test_xanim_eval_asm.o $(OBJ_DIR)/test_xanim_eval_adapter.o $(OBJ_DIR)/test_xanim_dispatch.o
RUN_test_xanim_eval = timeout 15s ./$(OBJ_DIR)/test_xanim_eval$(BIN_EXT)
OL_TESTS += test_xanim_notify
TEST_test_xanim_notify = -m32 -msse2 -O1 -no-pie -ffunction-sections -fdata-sections -DOL_SERVER $(OL_INC) tests/test_xanim_notify.c src/online/script/xanim_notify_queue.c $(OBJ_DIR)/test_xanim_notify_asm.o -Wl,--gc-sections
RUN_test_xanim_notify = timeout 15s ./$(OBJ_DIR)/test_xanim_notify$(BIN_EXT)
OL_TESTS += test_model_flags
TEST_test_model_flags = -m32 -O1 $(OL_INC) tests/test_model_flags.c $(OBJ_DIR)/test_model_flags_asm.o -Wl,--gc-sections
OL_TESTS += test_hunk_user
TEST_test_hunk_user = -m32 -O1 -fno-builtin -no-pie tests/test_hunk_user.c $(OBJ_DIR)/test_hunk_user_asm.o
OL_TESTS += test_stream_fs
TEST_test_stream_fs = -m32 -O1 -ffunction-sections -fdata-sections -DOL_SERVER -D_GNU_SOURCE $(OL_INC) tests/test_stream_fs.c src/online/assets/sv_xmodel.c src/online/assets/stream_model.c -Wl,--gc-sections

$(OBJ_DIR)/test_usability_asm.o: src/asmsource/_player_use_mp.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf -DOL_SERVER src/asmsource/_player_use_mp.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_usability_asm.o).d
$(OBJ_DIR)/test_usability_dependencies.o: tests/usability_dependencies.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf tests/usability_dependencies.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_usability_dependencies.o).d
$(OBJ_DIR)/test_model_access.o: src/asmsource/_xmodel_utils.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf src/asmsource/_xmodel_utils.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_model_access.o).d
$(OBJ_DIR)/test_model_trace_asm.o: src/asmsource/_xmodel.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf src/asmsource/_xmodel.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_model_trace_asm.o).d
$(OBJ_DIR)/test_xanim_eval_asm.o: src/asmsource/_xanim_calc.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf src/asmsource/_xanim_calc.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_xanim_eval_asm.o).d
$(OBJ_DIR)/test_xanim_notify_asm.o: src/asmsource/_xanim.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf -DOL_SERVER src/asmsource/_xanim.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_xanim_notify_asm.o).d
$(OBJ_DIR)/test_xanim_eval_adapter.o: tests/xanim_eval_adapter.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf tests/xanim_eval_adapter.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_xanim_eval_adapter.o).d
$(OBJ_DIR)/test_xanim_dispatch.o: src/asmsource/_xanim_eval.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf src/asmsource/_xanim_eval.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_xanim_dispatch.o).d
$(OBJ_DIR)/test_hunk_user_asm.o: src/asmsource/_com_memory.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf src/asmsource/_com_memory.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_hunk_user_asm.o).d
$(OBJ_DIR)/test_model_flags_asm.o: src/asmsource/_xmodel_load_obj.asm | $(OBJ_DIR)
	@mkdir -p $(TOBJ_DIR)/asm
	$(NASM) -f elf src/asmsource/_xmodel_load_obj.asm -MD $(TOBJ_DIR)/asm/$(notdir $@).d -MT $@ -o $@
TEST_DEPS += $(TOBJ_DIR)/asm/$(notdir $(OBJ_DIR)/test_model_flags_asm.o).d

PY_TESTS = test_stream_models test_iwd_crypto
# Replay-only authoritative angle regression (no engine/network fixtures).
OL_TESTS += test_theatre_angles
TEST_test_theatre_angles = -m32 -msse2 -O1 -g -DOL_SERVER $(OL_INC) tests/test_theatre_angles.c src/online/net/ol_msg.c src/online/snapshot/delta.c src/online/snapshot/playerstate.c src/online/snapshot/player_arrays.c src/online/snapshot/entities.c src/online/snapshot/clients.c src/online/snapshot/actors.c src/online/snapshot/animcmd.c src/online/snapshot/snapshot.c src/online/snapshot/history.c src/online/snapshot/glass_snapshot.c -lm

$(foreach t,$(OL_TESTS),$(eval KEY_$(t):=$(call ol_key,$(t)))$(eval $(call TEST_RULES,$(t))))
$(foreach t,$(PY_TESTS),$(eval $(call PY_RULES,$(t))))
$(TEST_DEPS):
-include $(TEST_DEPS)

.PHONY: test-missile
test-missile: run-test_missile_stuck run-test_missile_pickup

.PHONY: test-match-status
test-match-status: run-test_match_status

.PHONY: test-radius-distance
test-radius-distance: run-test_radius_distance

.PHONY: test-peer-pool
test-peer-pool: run-test_peer_pool

.PHONY: test-session-reservation
test-session-reservation: run-test_session_reservation

.PHONY: test-spectator-follow
test-spectator-follow:
	@$(call ol_run,python3 tests/test_spectator_follow.py)

.PHONY: test-atomic-file
test-atomic-file: run-test_atomic_file

.PHONY: test-bot-offhand
test-bot-offhand: run-test_bot_offhand

.PHONY: test-bot-objective
test-bot-objective: run-test_bot_objective

.PHONY: test-owned-items
test-owned-items: run-test_owned_items

.PHONY: test
# test_iwd_crypto.py compiles mbedtls itself and is the longest job,
# so it is listed first to start early under -j.
test: run-test_threat_bias test-external run-py-test_iwd_crypto run-py-test_stream_models test-missile test-match-status test-radius-distance test-peer-pool test-session-reservation run-test_data_fast_path test-atomic-file test-bot-offhand test-owned-items test-bot-objective run-test_pure run-test_stats run-test_structured_data run-test_structured_schema run-test_matchdata run-test_matchdata_wire run-test_profile run-test_profile_storage run-test_handshake run-test_connect run-test_association run-test_ecdh run-test_frame_hud run-test_player_links run-test_hud_fontscale run-test_usability run-test_offhand_controls run-test_event_parameters run-test_weapon_change_event run-test_frame_entities run-test_viewmodels run-test_client_appearance run-test_loadout_recipe run-test_path_nodes run-test_physics_brush run-test_inherited run-test_native run-test_assets run-test_send run-test_cached_snapshot run-test_asset_names run-test_model_lookup run-test_model_trace run-test_collision_native run-test_weapon_native run-test_script_weapon_queries run-test_script_class run-test_weapon_loader run-test_weapon_timing_metadata run-test_dualwield_fill run-test_variable_zoom run-test_weapon_registry run-test_sv_weapons run-test_xanim_abi run-test_script_source run-test_admission run-test_admission_coarse run-test_script_entity run-test_script_math run-test_ability_math run-test_pmove_c1_tail run-test_pmove_c1_bob run-test_pmove_c1_cmdscale run-test_c1_brush_trace run-test_angle_vectors run-test_pmove_c1_movedir run-test_profile_push run-test_collectibles run-test_script_dvar run-test_unlock_rules run-test_csv run-test_profile_challenge run-test_profile_experience run-test_profile_unlock run-test_profile_clients run-test_map_custom run-test_team_fx run-test_profile_character run-test_character_variant run-test_character_item run-test_character_defaults run-test_character_rig run-test_character_visibility run-test_pose_budget run-test_character_composition run-test_weapon_lock run-test_weapon_lock_clients run-test_weapon_lock_missile run-test_weapon_lock_fire run-test_script_weapon_lock run-test_script_server run-test_vehicle_nodes run-test_animation_tables run-test_trigger_models run-test_script_menu run-test_script_lower run-test_xanim_native run-test_xanim_eval run-test_xanim_notify run-test_effects_native run-test_model_flags run-test_stream_fs run-test_field_orders run-test_dive run-test_vehicle_controls | $(OBJ_DIR)
endif

# Windows-only regression test for freeZoneImages (read-only temporary-file image views). Cross-built with
# mingw; run the .exe on Windows (WSL interop runs it directly).
test-zone-image-win:
	i686-w64-mingw32-gcc -O1 -g $(OL_INC) tests/test_zone_image_win.c src/online/assets/zone_image.c -o bin/test_zone_image_win.exe -lpsapi
	./bin/test_zone_image_win.exe
