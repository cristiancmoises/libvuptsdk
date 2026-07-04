# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Cristian Cezar Moisés
#
# Build and test all language examples.
# Each example is self-contained — uses only documented public APIs.
#
# Usage:
#   make           Build all C/C++ examples and run them
#   make python    Run Python examples
#   make all-test  Build all and run all (cross-language)

CC      ?= cc
CXX     ?= c++
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

# pkg-config will pick up vuptsdk.pc if installed.
# For in-tree testing without `make install`, set:
#   LIB_PATH=/path/to/libvuptsdk-root  (containing include/ and prebuilt/)
ifneq ($(LIB_PATH),)
  CPPFLAGS += -I$(LIB_PATH)/include
  LDFLAGS  += -L$(LIB_PATH)/prebuilt -L$(LIB_PATH)/lib \
              -Wl,-rpath,$(LIB_PATH)/prebuilt -Wl,-rpath,$(LIB_PATH)/lib
endif
LIBS = -lvuptsdk

C_EXAMPLES   = 01_pq_keypair 02_password_mode 03_field_encryption 04_file_encryption
CPP_EXAMPLES = 05_cpp_raii

.PHONY: all
all: $(C_EXAMPLES) $(CPP_EXAMPLES)

$(C_EXAMPLES): %: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) $(LIBS) -o $@

$(CPP_EXAMPLES): %: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(LDFLAGS) $(LIBS) -o $@

.PHONY: test
test: all
	@echo "═══ Running C examples ═══"
	@for ex in $(C_EXAMPLES); do \
	    echo ""; echo "──── $$ex ────"; \
	    ./$$ex || exit 1; \
	done
	@echo ""
	@echo "═══ Running C++ examples ═══"
	@for ex in $(CPP_EXAMPLES); do \
	    echo ""; echo "──── $$ex ────"; \
	    ./$$ex || exit 1; \
	done

.PHONY: python
python:
	@echo "═══ Running Python examples ═══"
ifneq ($(LIB_PATH),)
	ZUPTSDK_LIBRARY=$(LIB_PATH)/prebuilt/libvuptsdk.so.2.0.0 python3 06_python.py
else
	python3 06_python.py
endif

.PHONY: all-test
all-test: test python

.PHONY: clean
clean:
	rm -f $(C_EXAMPLES) $(CPP_EXAMPLES)
	rm -f *.pub *.priv *.enc *.txt
