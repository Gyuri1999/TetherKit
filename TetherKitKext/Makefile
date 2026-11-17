# Builds TetherKit.kext, the kernel-extension build of TetherKit, for
# injection by OpenCore (see README.md in this folder).
#
#   make            build build/TetherKit.kext (Release)
#   make DEBUG=1    same, with verbose logging compiled in
#   make check      build, then verify the kext's structure and that every
#                   symbol it needs is provided by the libraries it declares
#   make clean
#
# Needs only the Xcode command line toolchain; no Xcode project, no signing.

KEXT_NAME      := TetherKit
KEXT_BUNDLE_ID ?= com.prostec.tetherkit.kext
KEXT_VERSION   ?= 1.0.0
# Oldest macOS the binary will load on. IOUSBHostFamily, which the driver is
# written against, first shipped in 10.11.
MIN_MACOS      ?= 10.15
ARCHS          ?= x86_64

SDK   := $(shell xcrun --sdk macosx --show-sdk-path)
CC    := xcrun clang
CXX   := xcrun clang++
KFWK  := $(SDK)/System/Library/Frameworks/Kernel.framework

BUILD := build
OBJ   := $(BUILD)/obj
KEXT  := $(BUILD)/$(KEXT_NAME).kext
BIN   := $(KEXT)/Contents/MacOS/$(KEXT_NAME)

ARCHFLAGS := $(foreach a,$(ARCHS),-arch $(a))

ifeq ($(DEBUG),1)
OPT := -O0 -g -DDEBUG=1
else
OPT := -Os -g
endif

COMMON := $(ARCHFLAGS) -mmacosx-version-min=$(MIN_MACOS) -isysroot $(SDK) \
	-mkernel -nostdinc -fno-builtin -fno-common -fstack-protector-strong \
	-DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
	-I$(KFWK)/Headers -I$(KFWK)/PrivateHeaders \
	-Wall -Wno-inconsistent-missing-override -Wno-deprecated-declarations \
	-Wno-sign-conversion -Wno-implicit-int-conversion \
	$(OPT)

CXXFLAGS := $(COMMON) -std=gnu++17 -fapple-kext -fno-exceptions -fno-rtti \
	-fno-threadsafe-statics
CFLAGS   := $(COMMON) -std=gnu11 \
	-DKEXT_BUNDLE_ID=$(KEXT_BUNDLE_ID) -DKEXT_VERSION='"$(KEXT_VERSION)"'
LDFLAGS  := $(ARCHFLAGS) -mmacosx-version-min=$(MIN_MACOS) -isysroot $(SDK) \
	-nostdlib -Xlinker -kext -Xlinker -export_dynamic \
	-Xlinker -no_adhoc_codesign -lkmodc++ -lkmod -lcc_kext

OBJS := $(OBJ)/TetherKitRNDIS.o $(OBJ)/RNDISProtocolCore.o $(OBJ)/kmod_info.o
HDRS := TetherKitRNDIS.h ../TetherKitDriver/RNDISProtocol.h \
	../TetherKitDriver/RNDISProtocolCore.h

all: $(KEXT)

$(OBJ)/TetherKitRNDIS.o: TetherKitRNDIS.cpp $(HDRS) Makefile
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# The RNDIS framing code is the same source file the DriverKit driver uses.
$(OBJ)/RNDISProtocolCore.o: ../TetherKitDriver/RNDISProtocolCore.cpp $(HDRS) Makefile
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ)/kmod_info.o: kmod_info.c Makefile
	@mkdir -p $(OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(KEXT): $(OBJS) Info.plist Makefile
	rm -rf $(KEXT)
	mkdir -p $(KEXT)/Contents/MacOS
	$(CXX) $(LDFLAGS) $(OBJS) -o $(BIN)
	sed -e 's/__KEXT_BUNDLE_ID__/$(KEXT_BUNDLE_ID)/g' \
	    -e 's/__KEXT_VERSION__/$(KEXT_VERSION)/g' \
	    -e 's/__MIN_MACOS__/$(MIN_MACOS)/g' \
	    Info.plist > $(KEXT)/Contents/Info.plist
	plutil -lint $(KEXT)/Contents/Info.plist
	@echo "Built $(KEXT)"

check: $(KEXT)
	file $(BIN)
	kmutil libraries -p $(KEXT) --undef-symbols

clean:
	rm -rf $(BUILD)

.PHONY: all check clean
