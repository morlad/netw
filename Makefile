# INTERNAL CONFIG
# ---------------

Q = @

# macos only:
MIN_MACOS_VERSION = 10.9

# OS DETECTION
# ------------
ifeq ($(OS),Windows_NT)
	os = windows
else
	uname_s = $(shell uname -s)
	ifeq ($(uname_s),Darwin)
		os = macos
	endif
	ifeq ($(uname_s),Linux)
		os = linux
	endif
	ifeq ($(uname_s),FreeBSD)
		os = freebsd
	endif
endif

.PHONY: help

all: help

help:
	$(Q)echo Nothing to build - include files in your project directly.

examples: example
	$(Q)echo Building Examples

ifeq ($(os),windows)
srcs += netw-win.c
else ifeq ($(os),macos)
srcs += netw-macos.m
else
srcs += netw-libcurl.c
endif

ifeq ($(os),macos)
TARGET_ARCH += -arch x86_64 -mmacosx-version-min=$(MIN_MACOS_VERSION)
CFLAGS += -Weverything
example: LDFLAGS += -framework Foundation
endif

ifeq ($(os),windows)
example: LDLIBS += -lwinhttp.lib
endif

ifeq ($(os),linux)
example: LDLIBS += -lpthread -lcurl
endif

example: netw $(srcs)

format:
	$(Q)clang-format -i *.c *.m *.h
