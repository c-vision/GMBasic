# SPDX-License-Identifier: LicenseRef-PUL-1.1
# Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
# Licenza: Personal Use License (PUL) v1.1.
#
# netlb — load balancing (ex gmbsuite gmbnet). Self-contained: solo libm.
# Layout monorepo: sorgenti in packages/netlb/, artefatti nella sottocartella <versione>/.
CC       ?= cc
CFLAGS   ?= -std=c99 -Wall -Wextra -pedantic -O2
LDLIBS   ?= -lm
INCLUDES := -Iinclude -Isrc

OUT  := out
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(OUT)/%.o,$(SRCS))
LIB  := $(OUT)/libnetlb.a
DYLIB := $(OUT)/libnetlb.dylib
TEST := $(OUT)/run_tests

# piattaforma per il layout del pacchetto + versione + destinazione cache pacchetti
PLAT ?= osx-arm64
VER  ?= 0.1.1
VERSION_DIR := $(VER)
PKGDIR := $(HOME)/.gmbasic/packages/netlb/$(VER)

.PHONY: all lib test dylib pkg packlib clean
all: $(LIB) $(TEST)
lib: $(LIB)

$(OUT):
	mkdir -p $(OUT)

$(OUT)/%.o: src/%.c | $(OUT)
	$(CC) $(CFLAGS) -fvisibility=hidden $(INCLUDES) -c $< -o $@

$(LIB): $(OBJS)
	ar rcs $@ $^

$(TEST): test/run_tests.c $(LIB) | $(OUT)
	$(CC) $(CFLAGS) $(INCLUDES) test/run_tests.c $(LIB) $(LDLIBS) -o $@

test: $(TEST)
	./$(TEST)

# dylib FFI (per il binding BASIC via package manager, come DuckDB).
# install_name relativo: la dylib vive accanto al proprio consumer nel pacchetto.
$(DYLIB): $(OBJS)
	$(CC) -dynamiclib -install_name "@loader_path/libnetlb.dylib" -headerpad_max_install_names -o $@ $^ $(LDLIBS)
dylib: $(DYLIB)

# impacchetta nella cartella versione (manifest, .inc, header, lib/<plat>/, samples/)
packlib: $(DYLIB)
	mkdir -p $(VERSION_DIR)/lib/$(PLAT) $(VERSION_DIR)/include
	cp include/netlb.h $(VERSION_DIR)/include/
	cp $(DYLIB) $(VERSION_DIR)/lib/$(PLAT)/libnetlb.dylib

# installa nella cache del package manager: ~/.gmbasic/packages/netlb/<ver>/
pkg: packlib
	mkdir -p $(PKGDIR)/include $(PKGDIR)/lib/$(PLAT) $(PKGDIR)/samples
	cp $(VERSION_DIR)/gmb-package.json $(VERSION_DIR)/netlb.inc $(PKGDIR)/
	cp $(VERSION_DIR)/include/netlb.h $(PKGDIR)/include/
	cp $(VERSION_DIR)/lib/$(PLAT)/libnetlb.dylib $(PKGDIR)/lib/$(PLAT)/
	cp $(VERSION_DIR)/samples/*.bas $(PKGDIR)/samples/
	@echo "installato: $(PKGDIR)"

clean:
	rm -rf $(OUT)
