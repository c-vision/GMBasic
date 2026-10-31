# ============================================================================
# Makefile — sample FFI drop-in (samples/Community/FFI)
#   make        -> libs/libvec_module.dylib (macOS) / libvec_module.so (Linux)
#   make clean  -> rimuove libs/
# La dylib vive in `libs/` (auto-discovery), NON accanto al programma.
# ============================================================================

CC      ?= cc
CFLAGS   = -std=c99 -Wall -Wextra -fPIC
GMB_INC  = ../../../native/gmb_runtime_c/include

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  LIB_OUT  = libs/libvec_module.dylib
  LDFLAGS  = -dynamiclib
else
  LIB_OUT  = libs/libvec_module.so
  LDFLAGS  = -shared
endif

all: $(LIB_OUT)

$(LIB_OUT): NativeVec/vec_module.c NativeVec/gmb_ffi.h
	mkdir -p libs
	$(CC) $(CFLAGS) -I NativeVec -I$(GMB_INC) $(LDFLAGS) -o $@ NativeVec/vec_module.c

clean:
	rm -rf libs

.PHONY: all clean
