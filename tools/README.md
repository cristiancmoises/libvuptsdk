# Adversarial fuzz tools

These programs reproduce the audit results in [`AUDIT.md`](../AUDIT.md).
They are deliberately small and self-contained; no test framework needed.

| Tool | Iterations | What it tests |
|---|---|---|
| `tamper_fuzz.c` | 1,000 | Single-bit flip rejection rate |
| `tamper_fuzz_multi.c` | 10,000 | Multi-byte mutation rejection rate |
| `wrong_key_fuzz.c` | 50×50 = 2,500 | Wrong-key cross-decrypt rejection |

## Run locally

```bash
# After `make install` or with prebuilt:
cc -O2 -Iinclude tools/tamper_fuzz.c \
   prebuilt/libzuptsdk.so.2.0.0 -o /tmp/tf -lpthread -lm
LD_LIBRARY_PATH=prebuilt /tmp/tf
```

Each program exits 0 on PASS, non-zero on FAIL (any undetected tampering).

## Pass criteria

- `tamper_fuzz`: `undetected == 0` out of 1000
- `tamper_fuzz_multi`: `undetected == 0` out of 10000
- `wrong_key_fuzz`: `wrong_accepted == 0` out of 2450 cross-pairs

---

**License**: AGPL-3.0-or-later. See [LICENSE](../LICENSE).
