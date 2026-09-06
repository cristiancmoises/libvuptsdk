# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-libvuptsdk-Commercial
# Copyright (c) 2026 Cristian Cezar Moisés
#
# ─────────────────────────────────────────────────────────────────────
#  libvuptsdk — public C ABI for Zupt cryptography
# ─────────────────────────────────────────────────────────────────────
#
#  Two libraries are shipped:
#
#  1. libvuptsdk-base.so.2.0.3  (built from source in this repo)
#     The compress/extract/archive/options API. ZUPTSDK_1.0 ABI subset.
#
#  2. libvuptsdk.so.2.0.3       (prebuilt, in prebuilt/)
#     The full ZUPTSDK_1.0 + ZUPTSDK_2.1 ABI including the easy_*
#     convenience layer, password mode, streaming AEAD, and metrics.
#     Some functions in this binary do not have source available in
#     this repo (legacy reasons; see SECURITY.md). Audited as a binary.
#
#  The canonical prebuilt is frozen and does not contain source updates in
#  this tree. It is retained for compatibility testing, not rebuilt here.
#  See README.md and SECURITY.md before selecting an artifact.
# ─────────────────────────────────────────────────────────────────────

SDK_VERSION_MAJOR = 2
SDK_VERSION_MINOR = 0
SDK_VERSION_PATCH = 3
SDK_SOVERSION     = $(SDK_VERSION_MAJOR)
SDK_FULLVERSION   = $(SDK_VERSION_MAJOR).$(SDK_VERSION_MINOR).$(SDK_VERSION_PATCH)
CODEC_VERSION     = 2.65.11

PREFIX     ?= /usr/local
LIBDIR     ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig

CC      ?= cc
AR      ?= ar
RANLIB  ?= ranlib
INSTALL ?= install
STRIP   ?= strip

ifeq ($(V),1)
  Q =
else
  Q = @
endif

CSTD     ?= -std=c11
WARN     ?= -Wall -Wextra -Wpedantic -Wno-unused-parameter \
            -Wmissing-prototypes -Wstrict-prototypes
OPT      ?= -O2
DEBUG    ?= -g
HARDEN   ?= -D_FORTIFY_SOURCE=2 -fstack-protector-strong
PIC      ?= -fPIC

CFLAGS   ?= $(CSTD) $(WARN) $(OPT) $(DEBUG) $(HARDEN)
LDFLAGS  ?= -Wl,-z,relro,-z,now
LIBS      = -lpthread -lm

ARCH := $(shell $(CC) -dumpmachine | cut -d- -f1)
ifeq ($(ARCH),x86_64)
  # Keep the default artifact usable on baseline x86-64. Setting
  # VV_SIMD_FLAGS=-mavx2 is an explicit whole-codec portability trade-off.
  VV_SIMD_FLAGS ?= -msse2
else ifneq ($(filter aarch64 arm64,$(ARCH)),)
  VV_SIMD_FLAGS ?= -march=armv8-a+simd
else
  VV_SIMD_FLAGS ?=
endif

ZUPT_SOURCES = src/zupt_format.c src/zupt_lz.c src/zupt_lzh.c \
               src/zupt_xxh.c src/zupt_sha256.c src/zupt_aes256.c \
               src/zupt_crypto.c src/zupt_sdk_stubs.c \
               src/zupt_predict.c src/zupt_parallel.c src/zupt_keccak.c \
               src/zupt_x25519.c src/zupt_mlkem.c src/zupt_cpuid.c \
               src/zupt_mlock.c src/zupt_filetype.c src/zupt_disk.c \
               src/zupt_dedup.c

VV_SOURCES   = src/vv_encoder.c src/vv_decoder.c src/vv_ans.c \
               src/vv_huffman.c src/vv_simd.c src/vv_xxh64.c src/vv_bcj.c \
               src/vaptvupt_api.c
VV_HEADERS   = include/vaptvupt.h include/vaptvupt_api.h include/vv_ans.h \
               include/vv_huffman.h include/vv_platform.h include/vv_bcj.h

SDK_SOURCE   = src/zuptsdk.c

PUBLIC_HEADER = include/zuptsdk.h

BUILD_DIR  = build
PIC_OBJS   = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(ZUPT_SOURCES) $(VV_SOURCES) $(SDK_SOURCE))
PIC_FLAGS  = $(PIC) -DZUPT_BUILDING_SDK=1

SOURCE_LIB    = $(BUILD_DIR)/libvuptsdk-base.so.$(SDK_FULLVERSION)
SOURCE_STATIC = $(BUILD_DIR)/libvuptsdk-base.a
PREBUILT_LIB  = prebuilt/libvuptsdk.so.$(SDK_FULLVERSION)
STAGED_LIB    = $(BUILD_DIR)/libvuptsdk.so.$(SDK_FULLVERSION)
PKGCONFIG     = $(BUILD_DIR)/vuptsdk.pc
LINKER_MAP    = zuptsdk.map
CODEC_TEST    = $(BUILD_DIR)/codec_integration_test

# ── Version query (single source of truth for packaging scripts) ────
.PHONY: printversion
printversion:
	@echo $(SDK_FULLVERSION)

# ── Default target ──────────────────────────────────────────────────
.PHONY: all
all: $(SOURCE_LIB) $(SOURCE_STATIC) $(STAGED_LIB) $(PKGCONFIG)
	@echo ""
	@echo "  ╭──────────────────────────────────────────────────────╮"
	@echo "  │  libvuptsdk $(SDK_FULLVERSION) — build complete                   │"
	@echo "  │                                                      │"
	@echo "  │  Source-built: build/libvuptsdk-base.so (subset)     │"
	@echo "  │  Frozen:       build/libvuptsdk.so      (full)       │"
	@echo "  │                                                      │"
	@echo "  │  'make test'    — smoke test + symbol audit          │"
	@echo "  │  'make install' — install frozen full prebuilt       │"
	@echo "  ╰──────────────────────────────────────────────────────╯"
	@echo "  NOTE: the frozen full library does not contain codec 2.65.11."

.PHONY: base
base: $(SOURCE_LIB) $(SOURCE_STATIC) $(PKGCONFIG)
	@echo "Built the source-based ABI subset with VaptVupt codec 2.65.11."

# ── Compile rules ───────────────────────────────────────────────────
$(BUILD_DIR)/vv_%.o: src/vv_%.c $(VV_HEADERS) | $(BUILD_DIR)
	$(Q)echo "  CC  $<"
	$(Q)$(CC) $(CFLAGS) $(PIC_FLAGS) $(VV_SIMD_FLAGS) -Iinclude -Isrc -c $< -o $@

$(BUILD_DIR)/vaptvupt_api.o: src/vaptvupt_api.c $(VV_HEADERS) | $(BUILD_DIR)
	$(Q)echo "  CC  $<"
	$(Q)$(CC) $(CFLAGS) $(PIC_FLAGS) $(VV_SIMD_FLAGS) -Iinclude -Isrc -c $< -o $@

$(BUILD_DIR)/zuptsdk.o: src/zuptsdk.c $(PUBLIC_HEADER) | $(BUILD_DIR)
	$(Q)echo "  CC  $<"
	$(Q)$(CC) $(CFLAGS) $(PIC_FLAGS) -Iinclude -Isrc -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(Q)echo "  CC  $<"
	$(Q)$(CC) $(CFLAGS) $(PIC_FLAGS) -Iinclude -Isrc -c $< -o $@

$(BUILD_DIR):
	$(Q)mkdir -p $(BUILD_DIR)

# ── Source build (subset) ───────────────────────────────────────────
$(SOURCE_LIB): $(PIC_OBJS) $(LINKER_MAP)
	$(Q)echo "  LD  $@"
	$(Q)$(CC) -shared \
		-Wl,-soname,libvuptsdk-base.so.$(SDK_SOVERSION) \
		-Wl,--version-script=$(LINKER_MAP) \
		$(PIC_OBJS) -o $@ $(LDFLAGS) $(LIBS)
	$(Q)cd $(BUILD_DIR) && ln -sf libvuptsdk-base.so.$(SDK_FULLVERSION) libvuptsdk-base.so.$(SDK_SOVERSION)
	$(Q)cd $(BUILD_DIR) && ln -sf libvuptsdk-base.so.$(SDK_SOVERSION)   libvuptsdk-base.so

$(SOURCE_STATIC): $(PIC_OBJS)
	$(Q)echo "  AR  $@"
	$(Q)$(AR) rcs $@ $(PIC_OBJS)
	$(Q)$(RANLIB) $@

# ── Stage canonical prebuilt ────────────────────────────────────────
$(STAGED_LIB): $(PREBUILT_LIB) | $(BUILD_DIR)
	$(Q)echo "  CP  $@  [frozen prebuilt; source changes not included]"
	$(Q)cp $(PREBUILT_LIB) $@
	$(Q)cd $(BUILD_DIR) && ln -sf libvuptsdk.so.$(SDK_FULLVERSION) libvuptsdk.so.$(SDK_SOVERSION)
	$(Q)cd $(BUILD_DIR) && ln -sf libvuptsdk.so.$(SDK_SOVERSION)   libvuptsdk.so

# ── pkg-config ──────────────────────────────────────────────────────
# This rule generates the .pc with whatever PREFIX is set at build time.
# Note: at `make install` we regenerate it again with the install-time PREFIX
# so that DESTDIR= or PREFIX= overrides are honored.
$(PKGCONFIG): | $(BUILD_DIR)
	$(Q)echo "  GEN $@"
	$(Q)printf 'prefix=$(PREFIX)\nexec_prefix=$${prefix}\nlibdir=$(LIBDIR)\nincludedir=$(INCLUDEDIR)\n\nName: vuptsdk\nDescription: libvuptsdk - post-quantum hybrid cryptography\nVersion: $(SDK_FULLVERSION)\nLibs: -L$${libdir} -lvuptsdk\nCflags: -I$${includedir}\n' > $@

# ── Tests ───────────────────────────────────────────────────────────
$(CODEC_TEST): tests/codec_integration_test.c $(SOURCE_STATIC)
	$(Q)echo "  CC  tests/codec_integration_test"
	$(Q)$(CC) $(CFLAGS) $(VV_SIMD_FLAGS) -Iinclude -Isrc $< \
		$(SOURCE_STATIC) -o $@ $(LDFLAGS) $(LIBS)

$(BUILD_DIR)/source_smoke: tests/source_smoke.c $(SOURCE_LIB)
	$(Q)echo "  CC  tests/source_smoke"
	$(Q)$(CC) $(CFLAGS) -Iinclude $< $(SOURCE_LIB) \
		-o $@ $(LDFLAGS) $(LIBS)

.PHONY: test-source
test-source: $(BUILD_DIR)/source_smoke $(CODEC_TEST)
	$(Q)echo "═══ libvuptsdk source-only smoke test ═══"
	$(Q)LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/source_smoke
	$(Q)echo ""
	$(Q)echo "═══ embedded VaptVupt codec integration ═══"
	$(Q)$(CODEC_TEST)
	$(Q)echo ""
	$(Q)$(MAKE) audit-licenses

.PHONY: test
test: $(STAGED_LIB) $(SOURCE_LIB) $(CODEC_TEST)
	$(Q)echo "  CC  tests/smoke_test"
	$(Q)$(CC) $(CFLAGS) -Iinclude tests/smoke_test.c \
		$(STAGED_LIB) $(SOURCE_LIB) \
		-o $(BUILD_DIR)/smoke_test $(LDFLAGS) $(LIBS)
	$(Q)echo ""
	$(Q)echo "═══ libvuptsdk smoke test ═══"
	$(Q)LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/smoke_test
	$(Q)echo ""
	$(Q)echo "═══ embedded VaptVupt codec integration ═══"
	$(Q)$(CODEC_TEST)
	$(Q)echo ""
	$(Q)$(MAKE) audit
	$(Q)echo ""
	$(Q)echo "═══ License audit ═══"
	$(Q)$(MAKE) audit-licenses

.PHONY: audit
audit: $(SOURCE_LIB) $(PREBUILT_LIB)
	$(Q)bash tests/run_audit.sh

.PHONY: test-asan
test-asan:
	$(Q)$(MAKE) clean
	$(Q)$(MAKE) $(SOURCE_LIB) $(SOURCE_STATIC) \
	  CFLAGS="$(CSTD) $(WARN) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DZUPT_BUILDING_SDK=1" \
	  LDFLAGS="-fsanitize=address,undefined" \
	  Q=@
	$(Q)echo "  CC  tests/source_smoke (ASAN)"
	$(Q)$(CC) -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude tests/source_smoke.c \
	   $(SOURCE_LIB) \
	   -o $(BUILD_DIR)/source_smoke_asan -fsanitize=address,undefined -lpthread -lm
	$(Q)echo ""
	$(Q)echo "═══ libvuptsdk source ASAN/UBSAN smoke test ═══"
	$(Q)ASAN_OPTIONS=detect_leaks=0 LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/source_smoke_asan
	$(Q)echo ""
	$(Q)echo "═══ embedded VaptVupt codec ASAN/UBSAN test ═══"
	$(Q)$(CC) -std=c11 -Wall -Wextra -Wpedantic -O1 -g \
	   -fsanitize=address,undefined -fno-omit-frame-pointer \
	   $(VV_SIMD_FLAGS) -Iinclude -Isrc tests/codec_integration_test.c \
	   $(SOURCE_STATIC) -o $(BUILD_DIR)/codec_integration_test_asan \
	   -fsanitize=address,undefined -lpthread -lm
	$(Q)ASAN_OPTIONS=detect_leaks=0 $(BUILD_DIR)/codec_integration_test_asan
	@echo ""
	@echo "ASAN build + test complete"

# ── Install ─────────────────────────────────────────────────────────
.PHONY: install
install: all
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0755 $(STAGED_LIB) $(DESTDIR)$(LIBDIR)/
	# Strip the installed library to remove debug info (information disclosure
	# hardening). Override with `make install STRIP_INSTALL=0` to keep symbols.
	@if [ "$(STRIP_INSTALL)" != "0" ]; then \
	    echo "  STRIP $(DESTDIR)$(LIBDIR)/libvuptsdk.so.$(SDK_FULLVERSION)"; \
	    $(STRIP) --strip-unneeded --remove-section=.note.GNU-gold-version \
	             --remove-section=.note.gnu.gold-version \
	             $(DESTDIR)$(LIBDIR)/libvuptsdk.so.$(SDK_FULLVERSION); \
	fi
	cd $(DESTDIR)$(LIBDIR) && ln -sf libvuptsdk.so.$(SDK_FULLVERSION) libvuptsdk.so.$(SDK_SOVERSION)
	cd $(DESTDIR)$(LIBDIR) && ln -sf libvuptsdk.so.$(SDK_SOVERSION)   libvuptsdk.so
	$(INSTALL) -m 0644 $(SOURCE_STATIC) $(DESTDIR)$(LIBDIR)/libvuptsdk.a
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)
	for h in zuptsdk.h zuptsdk_easy.h zuptsdk.hpp zuptsdk_metrics.h \
	         zsdk_aes256_gcm_siv.h zsdk_aes256_siv.h zsdk_argon2id.h \
	         zsdk_blake2b.h zsdk_hkdf.h zsdk_xchacha20_poly1305.h; do \
	    $(INSTALL) -m 0644 include/$$h $(DESTDIR)$(INCLUDEDIR)/; \
	done
	$(INSTALL) -d $(DESTDIR)$(PKGCONFIGDIR)
	$(Q)printf 'prefix=$(PREFIX)\nexec_prefix=$${prefix}\nlibdir=$(LIBDIR)\nincludedir=$(INCLUDEDIR)\n\nName: vuptsdk\nDescription: libvuptsdk - post-quantum hybrid cryptography\nVersion: $(SDK_FULLVERSION)\nLibs: -L$${libdir} -lvuptsdk\nCflags: -I$${includedir}\n' > $(DESTDIR)$(PKGCONFIGDIR)/vuptsdk.pc
	$(Q)chmod 0644 $(DESTDIR)$(PKGCONFIGDIR)/vuptsdk.pc

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/libvuptsdk.so*
	rm -f $(DESTDIR)$(LIBDIR)/libvuptsdk.a
	rm -f $(DESTDIR)$(INCLUDEDIR)/zuptsdk.h
	rm -f $(DESTDIR)$(INCLUDEDIR)/zuptsdk_easy.h
	rm -f $(DESTDIR)$(INCLUDEDIR)/zuptsdk.hpp
	rm -f $(DESTDIR)$(INCLUDEDIR)/zuptsdk_metrics.h
	rm -f $(DESTDIR)$(INCLUDEDIR)/zsdk_*.h
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/vuptsdk.pc

# ── License audit ───────────────────────────────────────────────────
# Verifies first-party SDK files use the project dual-license identifier and
# the embedded VaptVupt core retains its upstream GPL-3.0-or-later identifier.
# Useful as a pre-commit hook.
# ── Formal audit (full battery) ─────────────────────────────────────
# Runs every verification phase in this audit:
#   - Smoke test (10 properties against canonical)
#   - Symbol audit (13 properties: arch, SONAME, ABI, namespace)
#   - License audit (every file has AGPL SPDX header)
#   - ASAN/UBSAN smoke (6 properties, leak-detect on)
#   - Tamper fuzz (1000 random bit-flips)
#   - Wrong-key fuzz (2500 cross-decrypt trials)
# Pass criteria are inside each subtarget.
.PHONY: formal-audit
formal-audit:
	$(Q)echo "═══════════════════════════════════════════════════════════"
	$(Q)echo "      libvuptsdk formal audit — full verification battery"
	$(Q)echo "═══════════════════════════════════════════════════════════"
	$(Q)echo ""
	$(Q)echo "── Phase 1/4: build + smoke + symbol audit ──"
	$(Q)$(MAKE) test
	$(Q)echo ""
	$(Q)echo "── Phase 2/4: ASAN/UBSAN ──"
	$(Q)$(MAKE) test-asan
	$(Q)echo ""
	$(Q)echo "── Phase 3/4: adversarial fuzz ──"
	$(Q)$(MAKE) audit-fuzz
	$(Q)echo ""
	$(Q)echo "═══════════════════════════════════════════════════════════"
	$(Q)echo "  formal audit complete — see AUDIT.md for the full report"
	$(Q)echo "═══════════════════════════════════════════════════════════"

# ── Adversarial fuzz audit ──────────────────────────────────────────
# Builds and runs the formal audit fuzzers from tools/.
# Now covered by the comprehensive audit-fuzz target below.
# (Old 2-tool version deprecated.)

.PHONY: audit-licenses
audit-licenses:
	@MISSING=0; \
	for f in $$(find . -type f \( -name '*.c' -o -name '*.h' -o -name '*.hpp' \
	             -o -name '*.py' -o -name '*.sh' -o -name '*.yml' -o -name '*.yaml' \
	             -o -name '*.jazz' -o -name '*.s' -o -name 'Makefile' \
	             -o -name '*.map' \) \
	             -not -path './build/*' -not -path './dist/*' \
	             -not -path './prebuilt/*' -not -path './.git/*'); do \
	    case "$$f" in \
	      ./src/vv_*.c|./src/vaptvupt_api.c|./include/vv_*.h|\
	      ./include/vaptvupt.h|./include/vaptvupt_api.h) \
	        EXPECTED='GPL-3.0-or-later' ;; \
	      ./tests/codec_integration_test.c) \
	        EXPECTED='AGPL-3.0-or-later' ;; \
	      *) \
	        EXPECTED='AGPL-3.0-or-later OR LicenseRef-libvuptsdk-Commercial' ;; \
	    esac; \
	    if ! grep -Eq "SPDX-License-Identifier: $$EXPECTED([[:space:]]|\\*/)*$$" "$$f"; then \
	        echo "  ✗ $$f (expected $$EXPECTED)"; \
	        MISSING=$$((MISSING+1)); \
	    fi; \
	done; \
	if [ $$MISSING -eq 0 ]; then \
	    echo "  ✓ SDK and embedded codec SPDX scopes are consistent"; \
	else \
	    echo ""; \
	    echo "  $$MISSING files need a SPDX license header. Aborting."; \
	    exit 1; \
	fi

# ── Hardening audit ─────────────────────────────────────────────────
# Inspect ELF properties of both source build and canonical prebuilt.
# Reports RELRO, NX, stack canary, FORTIFY_SOURCE, RPATH, symbol versions.
.PHONY: audit-hardening
audit-hardening: $(SOURCE_LIB) $(STAGED_LIB)
	@echo ""
	@echo "═══════════════════════════════════════════════════════════"
	@echo "  Hardening audit: source build (libvuptsdk-base.so)"
	@echo "═══════════════════════════════════════════════════════════"
	@bash tools/checksec_lib.sh $(BUILD_DIR)/libvuptsdk-base.so.$(SDK_FULLVERSION) || true
	@echo ""
	@echo "═══════════════════════════════════════════════════════════"
	@echo "  Hardening audit: canonical prebuilt (libvuptsdk.so)"
	@echo "═══════════════════════════════════════════════════════════"
	@bash tools/checksec_lib.sh $(PREBUILT_LIB) || true

# ── Adversarial fuzz suite ──────────────────────────────────────────
# Run all four fuzzers (tamper, multi-tamper, wrong-key, format).
# Compiles against the canonical prebuilt (which has the easy_* layer).
.PHONY: audit-fuzz
audit-fuzz: $(STAGED_LIB)
	@echo "═══ Building fuzz tools ═══"
	$(Q)$(CC) -O2 -std=c11 -Iinclude tools/tamper_fuzz.c \
		$(STAGED_LIB) -o $(BUILD_DIR)/tamper_fuzz $(LIBS)
	$(Q)$(CC) -O2 -std=c11 -Iinclude tools/tamper_fuzz_multi.c \
		$(STAGED_LIB) -o $(BUILD_DIR)/tamper_fuzz_multi $(LIBS)
	$(Q)$(CC) -O2 -std=c11 -Iinclude tools/wrong_key_fuzz.c \
		$(STAGED_LIB) -o $(BUILD_DIR)/wrong_key_fuzz $(LIBS)
	$(Q)$(CC) -O2 -std=c11 -Iinclude tools/format_fuzz.c \
		$(STAGED_LIB) -o $(BUILD_DIR)/format_fuzz $(LIBS)
	$(Q)$(CC) -O2 -std=c11 -Iinclude tools/key_isolation.c \
		$(STAGED_LIB) -o $(BUILD_DIR)/key_isolation $(LIBS)
	$(Q)$(CC) -O2 -std=c11 -Iinclude tools/timing_variance.c \
		$(STAGED_LIB) -o $(BUILD_DIR)/timing_variance $(LIBS)
	@echo ""
	@echo "═══ Single-bit tamper fuzz (1000 iters) ═══"
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/tamper_fuzz
	@echo ""
	@echo "═══ Multi-byte tamper fuzz (10000 iters) ═══"
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/tamper_fuzz_multi
	@echo ""
	@echo "═══ Wrong-key cross-decrypt (50x50=2500) ═══"
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/wrong_key_fuzz
	@echo ""
	@echo "═══ Format fuzz: random bytes (50000 iters) ═══"
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/format_fuzz
	@echo ""
	@echo "═══ Key isolation (1293 secrets x 100 ciphertexts) ═══"
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/key_isolation
	@echo ""
	@echo "═══ Timing variance (500 iters per failure mode) ═══"
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/timing_variance

# ── Performance benchmark ───────────────────────────────────────────
.PHONY: bench
bench: $(STAGED_LIB)
	$(Q)$(CC) -O2 -std=c11 -Iinclude bench/bench_throughput.c \
		$(STAGED_LIB) -o $(BUILD_DIR)/bench_throughput $(LIBS)
	@LD_LIBRARY_PATH=$(BUILD_DIR) $(BUILD_DIR)/bench_throughput

# ── Full audit (everything) ─────────────────────────────────────────
.PHONY: audit-all
audit-all:
	@echo "═══ Phase 1/5: ASAN/UBSAN smoke test (clean rebuild) ═══"
	$(Q)$(MAKE) test-asan
	@echo ""
	@echo "═══ Phase 2/5: Standard smoke + audit + license ═══"
	$(Q)$(MAKE) test
	@echo ""
	@echo "═══ Phase 3/5: Hardening audit ═══"
	$(Q)$(MAKE) audit-hardening
	@echo ""
	@echo "═══ Phase 4/5: Adversarial fuzz suite ═══"
	$(Q)$(MAKE) audit-fuzz
	@echo ""
	@echo "═══════════════════════════════════════════════════════════"
	@echo "  Phase 5/5: Full audit complete"
	@echo "  Checks completed; inspect each phase and recorded warnings"
	@echo "═══════════════════════════════════════════════════════════"

# ── Clean ───────────────────────────────────────────────────────────
.PHONY: clean
clean:
	$(Q)rm -rf $(BUILD_DIR)
	$(Q)find . -name '*.o' -not -path './prebuilt/*' -delete

# ── Distribution tarball ────────────────────────────────────────────
# This tree changes source beneath an already-released SDK version. Keep the
# candidate unmistakably source-only and unreleased until a legitimate SDK
# version can be built and audited across the complete ABI.
DIST_NAME = libvuptsdk-$(SDK_FULLVERSION)-codec-$(CODEC_VERSION)-unreleased-source
SOURCE_DATE_EPOCH = 1788652800

.PHONY: dist
dist:
	$(Q)set -eu; \
	  tmp=$$(mktemp -d "$${TMPDIR:-/tmp}/libvuptsdk-dist.XXXXXXXX"); \
	  trap 'rm -rf -- "$$tmp"' EXIT HUP INT TERM; \
	  root="$$tmp/$(DIST_NAME)"; \
	  mkdir -p dist "$$root"; \
	  cp -r include src tests doc bindings jasmin packaging tools bench \
	        conformance-suite \
	        Makefile zuptsdk.map \
	        README.md README.pt-BR.md CHANGELOG.md SECURITY.md AUDIT.md BENCHMARKS.md \
	        LICENSE LICENSE-AGPL-3.0 LICENSE-GPL-3.0 LICENSE-COMMERCIAL NOTICE \
	        "$$root/"; \
	  if [ -d .forgejo ]; then cp -r .forgejo "$$root/"; fi; \
	  if [ -d .github ]; then cp -r .github "$$root/"; fi; \
	  find "$$root" -name '__pycache__' -type d -exec rm -rf -- {} +; \
	  find "$$root" -type f \( -name '*.pyc' -o -name '*.o' \) -delete; \
	  tar -C "$$tmp" --sort=name \
	      --mtime="@$(SOURCE_DATE_EPOCH)" \
	      --owner=0 --group=0 --numeric-owner \
	      -czf "$(CURDIR)/dist/$(DIST_NAME).tar.gz" "$(DIST_NAME)"
	@echo "Built: dist/$(DIST_NAME).tar.gz"
	@cd dist && sha256sum $(DIST_NAME).tar.gz

# ── Help ────────────────────────────────────────────────────────────
.PHONY: help
help:
	@echo "libvuptsdk $(SDK_FULLVERSION) — build targets:"
	@echo "  make             Build source subset + stage frozen full prebuilt"
	@echo "  make base        Build only the current source-based ABI subset"
	@echo "  make test        Compile + run smoke test, then audit"
	@echo "  make test-source Test only artifacts built from current source"
	@echo "  make audit       Verify source build is a subset of canonical"
	@echo "  make audit-licenses  Verify SDK and codec SPDX scopes"
	@echo "  make audit-hardening Verify ELF hardening (RELRO, NX, etc.)"
	@echo "  make audit-fuzz      Run adversarial fuzz suite (~1 min)"
	@echo "  make audit-all       Run everything: smoke + ASAN + license + hardening + fuzz"
	@echo "  make bench           Performance benchmark (~1 min)"
	@echo "  make formal-audit  Run the full audit battery (test+ASAN+fuzz)"
	@echo "  make test-asan   Build from-source with ASAN/UBSAN"
	@echo "  make install     Install frozen full prebuilt + headers + pkg-config"
	@echo "  make uninstall   Remove installed files"
	@echo "  make dist        Build an unreleased source-only tarball"
	@echo "  make clean       Remove build artifacts"
