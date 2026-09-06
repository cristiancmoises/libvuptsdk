# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-libvuptsdk-Commercial
# Copyright (c) 2026 Cristian Cezar Moisés
#
# ─────────────────────────────────────────────────────────────────────
#  libvuptsdk — public C ABI for Zupt cryptography
# ─────────────────────────────────────────────────────────────────────
#
#  Two libraries are shipped:
#
#  1. libvuptsdk-base.so.2.0.4  (built from source in this repo)
#     The compress/extract/archive/options API. ZUPTSDK_1.0 ABI subset.
#
#  2. libvuptsdk.so.2.0.3       (legacy prebuilt, in prebuilt/)
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
SDK_VERSION_PATCH = 4
SDK_SOVERSION     = $(SDK_VERSION_MAJOR)
SDK_FULLVERSION   = $(SDK_VERSION_MAJOR).$(SDK_VERSION_MINOR).$(SDK_VERSION_PATCH)
SDK_PRERELEASE    = base.1
SDK_RELEASE       = $(SDK_FULLVERSION)-$(SDK_PRERELEASE)
CODEC_VERSION     = 2.65.11

.DEFAULT_GOAL := all

PREFIX     ?= /usr/local
LIBDIR     ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
BASE_INCLUDEDIR ?= $(INCLUDEDIR)/libvuptsdk-base
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

# Source-only targets such as `make dist` must remain usable when no compiler
# is installed. An unavailable compiler yields an unknown architecture and no
# architecture-specific flags; build targets will still fail normally.
ARCH := $(shell $(CC) -dumpmachine 2>/dev/null | cut -d- -f1)
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

SOURCE_LIB       = $(BUILD_DIR)/libvuptsdk-base.so.$(SDK_FULLVERSION)
SOURCE_STATIC    = $(BUILD_DIR)/libvuptsdk-base.a
PREBUILT_VERSION = 2.0.3
PREBUILT_LIB     = prebuilt/libvuptsdk.so.$(PREBUILT_VERSION)
STAGED_LIB       = $(BUILD_DIR)/legacy/libvuptsdk.so.$(PREBUILT_VERSION)
PKGCONFIG        = $(BUILD_DIR)/vuptsdk-base.pc
LINKER_MAP    = zuptsdk.map
CODEC_TEST    = $(BUILD_DIR)/codec_integration_test
DOC_EXAMPLE   = $(BUILD_DIR)/doc-example

# ── Version query (single source of truth for packaging scripts) ────
.PHONY: printversion
printversion:
	@echo $(SDK_FULLVERSION)

.PHONY: printrelease
printrelease:
	@echo $(SDK_RELEASE)

# ── Default target ──────────────────────────────────────────────────
.PHONY: all
all: base
	@echo ""
	@echo "  ╭──────────────────────────────────────────────────────╮"
	@echo "  │  libvuptsdk-base $(SDK_FULLVERSION) — build complete              │"
	@echo "  │                                                      │"
	@echo "  │  Source-built: build/libvuptsdk-base.so              │"
	@echo "  │  Codec:        VaptVupt $(CODEC_VERSION)                       │"
	@echo "  │                                                      │"
	@echo "  │  'make test'    — source and codec tests             │"
	@echo "  │  'make install' — install source-built base library │"
	@echo "  ╰──────────────────────────────────────────────────────╯"
	@echo "  NOTE: 'make legacy-test' explicitly checks the frozen 2.0.3 binary."

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
	$(Q)mkdir -p $(dir $@)
	$(Q)cp $(PREBUILT_LIB) $@
	$(Q)cd $(dir $@) && ln -sf libvuptsdk.so.$(PREBUILT_VERSION) libvuptsdk.so.$(SDK_SOVERSION)
	$(Q)cd $(dir $@) && ln -sf libvuptsdk.so.$(SDK_SOVERSION) libvuptsdk.so

# ── pkg-config ──────────────────────────────────────────────────────
# This rule generates the .pc with whatever PREFIX is set at build time.
# Note: at `make install` we regenerate it again with the install-time PREFIX
# so that DESTDIR= or PREFIX= overrides are honored.
$(PKGCONFIG): | $(BUILD_DIR)
	$(Q)echo "  GEN $@"
	$(Q)printf 'prefix=$(PREFIX)\nexec_prefix=$${prefix}\nlibdir=$(LIBDIR)\nincludedir=$(BASE_INCLUDEDIR)\n\nName: vuptsdk-base\nDescription: source-built libvuptsdk archive API with VaptVupt codec $(CODEC_VERSION)\nVersion: $(SDK_RELEASE)\nLibs: -L$${libdir} -lvuptsdk-base\nCflags: -I$${includedir}\n' > $@

# ── Tests ───────────────────────────────────────────────────────────
$(CODEC_TEST): tests/codec_integration_test.c $(SOURCE_STATIC)
	$(Q)echo "  CC  tests/codec_integration_test"
	$(Q)$(CC) $(CFLAGS) $(VV_SIMD_FLAGS) -Iinclude -Isrc $< \
		$(SOURCE_STATIC) -o $@ $(LDFLAGS) $(LIBS)

$(BUILD_DIR)/source_smoke: tests/source_smoke.c $(SOURCE_LIB)
	$(Q)echo "  CC  tests/source_smoke"
	$(Q)$(CC) $(CFLAGS) -Iinclude $< $(SOURCE_LIB) \
		-o $@ $(LDFLAGS) $(LIBS)

$(DOC_EXAMPLE): doc/example.c $(SOURCE_STATIC)
	$(Q)echo "  CC  $@"
	$(Q)$(CC) $(CFLAGS) -Iinclude $< $(SOURCE_STATIC) -o $@ $(LDFLAGS) $(LIBS)

.PHONY: test-source
test-source: $(BUILD_DIR)/source_smoke $(CODEC_TEST) $(DOC_EXAMPLE)
	$(Q)echo "═══ libvuptsdk source-only smoke test ═══"
	$(Q)LD_LIBRARY_PATH=$(BUILD_DIR)$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} \
		$(BUILD_DIR)/source_smoke > $(BUILD_DIR)/source_smoke.stdout
	$(Q)test ! -s $(BUILD_DIR)/source_smoke.stdout || { \
		echo "unexpected SDK output on stdout:" >&2; \
		cat $(BUILD_DIR)/source_smoke.stdout >&2; exit 1; }
	$(Q)$(RM) $(BUILD_DIR)/source_smoke.stdout
	$(Q)echo ""
	$(Q)echo "═══ documented base example ═══"
	$(Q)$(DOC_EXAMPLE)
	$(Q)echo ""
	$(Q)echo "═══ embedded VaptVupt codec integration ═══"
	$(Q)$(CODEC_TEST)
	$(Q)echo ""
	$(Q)$(MAKE) audit-licenses

.PHONY: test
test: test-source

.PHONY: legacy-test
legacy-test: $(STAGED_LIB) $(SOURCE_LIB) $(CODEC_TEST)
	$(Q)echo "  CC  tests/smoke_test"
	$(Q)$(CC) $(CFLAGS) -Iinclude tests/smoke_test.c \
		$(STAGED_LIB) $(SOURCE_LIB) \
		-o $(BUILD_DIR)/smoke_test $(LDFLAGS) $(LIBS)
	$(Q)echo ""
	$(Q)echo "═══ libvuptsdk smoke test ═══"
	$(Q)LD_LIBRARY_PATH=$(dir $(STAGED_LIB)):$(BUILD_DIR)$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} $(BUILD_DIR)/smoke_test
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
install: base
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0755 $(SOURCE_LIB) $(DESTDIR)$(LIBDIR)/
	# Strip the installed library to remove debug info (information disclosure
	# hardening). Override with `make install STRIP_INSTALL=0` to keep symbols.
	@if [ "$(STRIP_INSTALL)" != "0" ]; then \
	    echo "  STRIP $(DESTDIR)$(LIBDIR)/libvuptsdk-base.so.$(SDK_FULLVERSION)"; \
	    $(STRIP) --strip-unneeded --remove-section=.note.GNU-gold-version \
	             --remove-section=.note.gnu.gold-version \
	             $(DESTDIR)$(LIBDIR)/libvuptsdk-base.so.$(SDK_FULLVERSION); \
	fi
	cd $(DESTDIR)$(LIBDIR) && ln -sf libvuptsdk-base.so.$(SDK_FULLVERSION) libvuptsdk-base.so.$(SDK_SOVERSION)
	cd $(DESTDIR)$(LIBDIR) && ln -sf libvuptsdk-base.so.$(SDK_SOVERSION) libvuptsdk-base.so
	$(INSTALL) -m 0644 $(SOURCE_STATIC) $(DESTDIR)$(LIBDIR)/libvuptsdk-base.a
	$(INSTALL) -d $(DESTDIR)$(BASE_INCLUDEDIR)
	$(INSTALL) -m 0644 include/zuptsdk.h $(DESTDIR)$(BASE_INCLUDEDIR)/
	$(INSTALL) -d $(DESTDIR)$(PKGCONFIGDIR)
	$(Q)printf 'prefix=$(PREFIX)\nexec_prefix=$${prefix}\nlibdir=$(LIBDIR)\nincludedir=$(BASE_INCLUDEDIR)\n\nName: vuptsdk-base\nDescription: source-built libvuptsdk archive API with VaptVupt codec $(CODEC_VERSION)\nVersion: $(SDK_RELEASE)\nLibs: -L$${libdir} -lvuptsdk-base\nCflags: -I$${includedir}\n' > $(DESTDIR)$(PKGCONFIGDIR)/vuptsdk-base.pc
	$(Q)chmod 0644 $(DESTDIR)$(PKGCONFIGDIR)/vuptsdk-base.pc

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/libvuptsdk-base.so*
	rm -f $(DESTDIR)$(LIBDIR)/libvuptsdk-base.a
	rm -f $(DESTDIR)$(BASE_INCLUDEDIR)/zuptsdk.h
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/vuptsdk-base.pc

# ── License audit ───────────────────────────────────────────────────
# Verifies first-party SDK files use the project dual-license identifier and
# the embedded VaptVupt core retains its upstream GPL-3.0-or-later identifier.
# Useful as a pre-commit hook.
# ── Extended local audit (current source plus frozen artifact) ──────
# Runs source, codec, license, sanitizer, legacy-symbol and historical
# frozen-binary adversarial checks. Pass criteria are inside each subtarget.
.PHONY: formal-audit
formal-audit:
	$(Q)echo "═══════════════════════════════════════════════════════════"
	$(Q)echo "      libvuptsdk extended local verification"
	$(Q)echo "═══════════════════════════════════════════════════════════"
	$(Q)echo ""
	$(Q)echo "── Phase 1/4: source, codec and license gates ──"
	$(Q)$(MAKE) test-source
	$(Q)echo ""
	$(Q)echo "── Phase 2/4: ASAN/UBSAN ──"
	$(Q)$(MAKE) test-asan
	$(Q)echo ""
	$(Q)echo "── Phase 3/4: frozen prebuilt compatibility gate ──"
	$(Q)$(MAKE) legacy-test
	$(Q)echo ""
	$(Q)echo "── Phase 4/4: frozen prebuilt adversarial checks ──"
	$(Q)$(MAKE) audit-fuzz
	$(Q)echo ""
	$(Q)echo "═══════════════════════════════════════════════════════════"
	$(Q)echo "  local verification complete — see AUDIT.md for scope"
	$(Q)echo "═══════════════════════════════════════════════════════════"

# ── Adversarial fuzz audit ──────────────────────────────────────────
# Builds and runs the historical frozen-binary audit tools from tools/.
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
	@LD_LIBRARY_PATH=$(dir $(STAGED_LIB))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} $(BUILD_DIR)/tamper_fuzz
	@echo ""
	@echo "═══ Multi-byte tamper fuzz (10000 iters) ═══"
	@LD_LIBRARY_PATH=$(dir $(STAGED_LIB))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} $(BUILD_DIR)/tamper_fuzz_multi
	@echo ""
	@echo "═══ Wrong-key cross-decrypt (50x50=2500) ═══"
	@LD_LIBRARY_PATH=$(dir $(STAGED_LIB))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} $(BUILD_DIR)/wrong_key_fuzz
	@echo ""
	@echo "═══ Format fuzz: random bytes (50000 iters) ═══"
	@LD_LIBRARY_PATH=$(dir $(STAGED_LIB))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} $(BUILD_DIR)/format_fuzz
	@echo ""
	@echo "═══ Key isolation (1293 secrets x 100 ciphertexts) ═══"
	@LD_LIBRARY_PATH=$(dir $(STAGED_LIB))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} $(BUILD_DIR)/key_isolation
	@echo ""
	@echo "═══ Timing variance (500 iters per failure mode) ═══"
	@LD_LIBRARY_PATH=$(dir $(STAGED_LIB))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} $(BUILD_DIR)/timing_variance

# ── Performance benchmark ───────────────────────────────────────────
.PHONY: bench
bench: $(STAGED_LIB)
	$(Q)$(CC) -O2 -std=c11 -Iinclude bench/bench_throughput.c \
		$(STAGED_LIB) -o $(BUILD_DIR)/bench_throughput $(LIBS)
	@LD_LIBRARY_PATH=$(dir $(STAGED_LIB))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH} $(BUILD_DIR)/bench_throughput

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
# This archive contains the reproducible source-built base library. The frozen
# full-ABI compatibility binary is intentionally not part of the archive.
DIST_NAME = libvuptsdk-base-$(SDK_RELEASE)
SOURCE_DATE_EPOCH = 1788652800

.PHONY: dist
dist:
	$(Q)set -eu; \
	  tmp=$$(mktemp -d "$${TMPDIR:-/tmp}/libvuptsdk-dist.XXXXXXXX"); \
	  trap 'rm -rf -- "$$tmp"' EXIT HUP INT TERM; \
	  root="$$tmp/$(DIST_NAME)"; \
	  mkdir -p dist "$$root"; \
	  mkdir -p "$$root/include" "$$root/tests"; \
	  cp -r src doc packaging conformance-suite jasmin \
	        Makefile zuptsdk.map \
	        README.md README.pt-BR.md CHANGELOG.md SECURITY.md AUDIT.md BENCHMARKS.md \
	        CT_VERIFICATION.md MLKEM_CONFORMANCE_FIX.md \
	        LICENSE LICENSE-AGPL-3.0 LICENSE-GPL-3.0 LICENSE-COMMERCIAL NOTICE \
	        "$$root/"; \
	  cp tests/source_smoke.c tests/codec_integration_test.c "$$root/tests/"; \
	  cp include/zuptsdk.h include/zupt.h include/zupt_acsl.h \
	        include/zupt_cpuid.h include/zupt_jasmin.h include/zupt_keccak.h \
	        include/zupt_mlkem.h include/zupt_x25519.h include/vaptvupt.h \
	        include/vaptvupt_api.h include/vv_ans.h include/vv_bcj.h \
	        include/vv_huffman.h include/vv_platform.h "$$root/include/"; \
	  if [ -d .forgejo ]; then cp -r .forgejo "$$root/"; fi; \
	  if [ -d .github ]; then cp -r .github "$$root/"; fi; \
	  find "$$root" -name '__pycache__' -type d -exec rm -rf -- {} +; \
	  find "$$root" -name target -type d -exec rm -rf -- {} +; \
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
	@echo "  make             Build the current source-based base library"
	@echo "  make base        Build only the current source-based ABI subset"
	@echo "  make test        Run source SDK and codec integration tests"
	@echo "  make legacy-test Test the frozen 2.0.3 compatibility binary"
	@echo "  make test-source Test only artifacts built from current source"
	@echo "  make audit       Verify source build is a subset of canonical"
	@echo "  make audit-licenses  Verify SDK and codec SPDX scopes"
	@echo "  make audit-hardening Verify ELF hardening (RELRO, NX, etc.)"
	@echo "  make audit-fuzz      Run adversarial fuzz suite (~1 min)"
	@echo "  make audit-all       Run everything: smoke + ASAN + license + hardening + fuzz"
	@echo "  make bench           Performance benchmark (~1 min)"
	@echo "  make formal-audit  Run current and frozen-artifact local checks"
	@echo "  make test-asan   Build from-source with ASAN/UBSAN"
	@echo "  make install     Install libvuptsdk-base + supported header/pkg-config"
	@echo "  make uninstall   Remove installed files"
	@echo "  make dist        Build the deterministic source archive"
	@echo "  make clean       Remove build artifacts"
