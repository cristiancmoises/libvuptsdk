// SPDX-License-Identifier: AGPL-3.0-or-later
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

class ZuptError : public std::runtime_error {
public:
    int code;
    ZuptError(int rc, const std::string& op)
      : std::runtime_error(op + ": " + zuptsdk_strerror(rc)),
        code(rc) {}
};

inline void check(int rc, const std::string& op) {
    if (rc != 0) throw ZuptError(rc, op);
}

std::vector<uint8_t> encrypt_for(const std::string& pubkey_path,
                                  const std::vector<uint8_t>& plaintext) {
    uint8_t* blob = nullptr;
    size_t blob_sz = 0;
    int rc = zuptsdk_easy_encrypt(pubkey_path.c_str(),
                                   plaintext.data(), plaintext.size(),
                                   &blob, &blob_sz);
    check(rc, "encrypt");
    std::vector<uint8_t> out(blob, blob + blob_sz);
    zuptsdk_free(blob);
    return out;
}

std::vector<uint8_t> decrypt_with(const std::string& privkey_path,
                                   const std::vector<uint8_t>& blob) {
    uint8_t* pt = nullptr;
    size_t pt_sz = 0;
    int rc = zuptsdk_easy_decrypt(privkey_path.c_str(),
                                   blob.data(), blob.size(),
                                   &pt, &pt_sz);
    check(rc, "decrypt");
    std::vector<uint8_t> out(pt, pt + pt_sz);
    zuptsdk_free(pt);
    return out;
}

int main() {
    std::cout << "libvuptsdk " << zuptsdk_version_string() << std::endl;

    check(zuptsdk_easy_keygen("alice.pub", "alice.priv"), "keygen");

    std::string msg = "Hello from C++!";
    std::vector<uint8_t> pt(msg.begin(), msg.end());

    auto blob = encrypt_for("alice.pub", pt);
    auto recovered = decrypt_with("alice.priv", blob);

    std::string back(recovered.begin(), recovered.end());
    std::cout << "Recovered: " << back << std::endl;

    std::remove("alice.pub");
    std::remove("alice.priv");
    return 0;
}
