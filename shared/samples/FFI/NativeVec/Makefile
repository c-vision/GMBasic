# ============================================================================
# Makefile — modulo nativo di esempio per gmb_ffi.h (samples/FFI/NativeVec)
#
#   make          -> libvec_module.dylib (macOS) o libvec_module.so (Linux)
#   make clean    -> rimuove l'artefatto
# ============================================================================

CC      ?= cc
CFLAGS   = -std=c99 -Wall -Wextra -fPIC -I.
GMB_INC  = ../../../../native/gmb_runtime_c/include

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  LIB_EXT  = dylib
  LIB_OUT  = libvec_module.dylib
  LDFLAGS  = -dynamiclib
else
  LIB_EXT  = so
  LIB_OUT  = libvec_module.so
  LDFLAGS  = -shared
endif

all: $(LIB_OUT)

$(LIB_OUT): vec_module.c gmb_ffi.h
	$(CC) $(CFLAGS) -I$(GMB_INC) $(LDFLAGS) -o $@ vec_module.c

clean:
	rm -f $(LIB_OUT)

.PHONY: all clean
