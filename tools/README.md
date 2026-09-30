# Adversarial fuzz tools

These programs target the historical full-ABI `easy_*` interface. Their earlier
results are not verification of the current source-built base API. See
[AUDIT.md](../AUDIT.md) for the 2.1.0-base.2 release gate. `make install` installs
the base library and does not provide the legacy interface these tools need.

The tools are small and self-contained; no test framework is needed.

| Tool | Iterations | What it tests |
|---|---|---|
| `tamper_fuzz.c` | 1,000 | Single-bit flip rejection rate |
| `tamper_fuzz_multi.c` | 10,000 | Multi-byte mutation rejection rate |
| `wrong_key_fuzz.c` | 50×50 = 2,500 | Wrong-key cross-decrypt rejection |

## Historical invocation

```bash
# Requires the original 2.0.0 binary, which is not shipped in this release:
cc -O2 -Iinclude tools/tamper_fuzz.c \
   prebuilt/libvuptsdk.so.2.0.0 -o /tmp/tf -lpthread -lm
LD_LIBRARY_PATH=prebuilt /tmp/tf
```

For another historical binary, explicitly select its matching library and
runtime dependencies. Linking an available frozen binary is not a claim that
its implementation matches the original measured artifact. Each program exits
0 on PASS and non-zero on FAIL (any undetected tampering).

## Pass criteria

- `tamper_fuzz`: `undetected == 0` out of 1000
- `tamper_fuzz_multi`: `undetected == 0` out of 10000
- `wrong_key_fuzz`: `wrong_accepted == 0` out of 2450 cross-pairs

---

Copyright 2026 Cristian Cezar Moisés. [Apache-2.0](../LICENSE).
