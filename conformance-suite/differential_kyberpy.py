#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Bidirectional differential: libvuptsdk ML-KEM-768 vs kyber-py (independent
FIPS 203 impl). Requires a compiled `kd` helper (see build_kd.sh). Exit 0 iff
all shared secrets agree byte-for-byte in both directions."""
import subprocess, sys
from kyber_py.ml_kem import ML_KEM_768 as M
KD = sys.argv[1] if len(sys.argv) > 1 else "./kd"
def sdk(*a): return subprocess.run([KD,*a],capture_output=True,text=True).stdout.split()
okA = okB = 0; N = 100
for _ in range(N):                              # SDK keygen -> kyber encaps -> SDK decaps
    pk,sk = sdk("keygen"); ssp,ct = M.encaps(bytes.fromhex(pk))
    okA += (sdk("decap",sk,ct.hex())[0] == ssp.hex())
for _ in range(N):                              # kyber keygen -> SDK encaps -> kyber decaps
    ek,dk = M.keygen(); ct,ss = sdk("encap",ek.hex())
    okB += (ss == M.decaps(dk, bytes.fromhex(ct)).hex())
print(f"SDK->kyber->SDK: {okA}/{N}  |  kyber->SDK->kyber: {okB}/{N}")
sys.exit(0 if okA==N and okB==N else 1)
