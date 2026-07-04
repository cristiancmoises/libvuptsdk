/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Throughput + latency benchmarks for libvuptsdk easy_* API.
 * Reports MB/s (wall-clock) and median/p99 latency in microseconds. */
#define _DEFAULT_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int cmp_d(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

static void report(const char *label, double *samples, int n, size_t bytes_each) {
    qsort(samples, n, sizeof(double), cmp_d);
    double median = samples[n / 2];
    double p99 = samples[(int)(n * 0.99)];
    double min  = samples[0];

    double mb_per_sec = (bytes_each / 1024.0 / 1024.0) / median;
    printf("  %-35s  median=%8.1fus  p99=%8.1fus  min=%8.1fus  best=%6.1f MB/s\n",
           label, median * 1e6, p99 * 1e6, min * 1e6, mb_per_sec);
}

static void bench_pq_keygen(int n) {
    double samples[n];
    char pub[] = "/tmp/bench_pub_XXXXXX"; close(mkstemp(pub));
    char priv[] = "/tmp/bench_priv_XXXXXX"; close(mkstemp(priv));

    for (int i = 0; i < n; i++) {
        double t0 = now_sec();
        zuptsdk_easy_keygen(pub, priv);
        samples[i] = now_sec() - t0;
    }
    report("PQ keygen (ML-KEM-768+X25519)", samples, n, 1);
    unlink(pub); unlink(priv);
}

static void bench_pq_encrypt(int n, size_t msg_size) {
    double samples[n];
    char pub[] = "/tmp/bench2_pub_XXXXXX"; close(mkstemp(pub));
    char priv[] = "/tmp/bench2_priv_XXXXXX"; close(mkstemp(priv));
    zuptsdk_easy_keygen(pub, priv);

    uint8_t *msg = malloc(msg_size);
    memset(msg, 0xAB, msg_size);

    for (int i = 0; i < n; i++) {
        uint8_t *blob = NULL; size_t blob_sz = 0;
        double t0 = now_sec();
        zuptsdk_easy_encrypt(pub, msg, msg_size, &blob, &blob_sz);
        samples[i] = now_sec() - t0;
        free(blob);
    }

    char label[64];
    snprintf(label, sizeof(label), "PQ encrypt (msg=%zu B)", msg_size);
    report(label, samples, n, msg_size);
    unlink(pub); unlink(priv);
    free(msg);
}

static void bench_pq_decrypt(int n, size_t msg_size) {
    double samples[n];
    char pub[] = "/tmp/bench3_pub_XXXXXX"; close(mkstemp(pub));
    char priv[] = "/tmp/bench3_priv_XXXXXX"; close(mkstemp(priv));
    zuptsdk_easy_keygen(pub, priv);

    uint8_t *msg = malloc(msg_size);
    memset(msg, 0xCD, msg_size);

    uint8_t *blob = NULL; size_t blob_sz = 0;
    zuptsdk_easy_encrypt(pub, msg, msg_size, &blob, &blob_sz);

    for (int i = 0; i < n; i++) {
        uint8_t *out = NULL; size_t out_sz = 0;
        double t0 = now_sec();
        zuptsdk_easy_decrypt(priv, blob, blob_sz, &out, &out_sz);
        samples[i] = now_sec() - t0;
        free(out);
    }

    char label[64];
    snprintf(label, sizeof(label), "PQ decrypt (msg=%zu B)", msg_size);
    report(label, samples, n, msg_size);
    free(blob); free(msg);
    unlink(pub); unlink(priv);
}

static void bench_password_kdf(int n) {
    double samples[n];
    uint8_t msg[64] = {0};
    for (int i = 0; i < n; i++) {
        uint8_t *blob = NULL; size_t blob_sz = 0;
        double t0 = now_sec();
        zuptsdk_easy_encrypt_password("benchmark password", msg, sizeof(msg),
                                       &blob, &blob_sz);
        samples[i] = now_sec() - t0;
        free(blob);
    }
    report("Password encrypt (Argon2id 64MB,t=3)", samples, n, 1);
}

static void bench_field(int n) {
    double samples[n];
    uint8_t key[32], salt[16];
    zuptsdk_easy_random_salt(salt);
    zuptsdk_easy_derive_key("master", salt, key);

    for (int i = 0; i < n; i++) {
        char *ct = NULL;
        double t0 = now_sec();
        zuptsdk_easy_encrypt_field(key, "alice@example.com", &ct);
        samples[i] = now_sec() - t0;
        free(ct);
    }
    report("Field encrypt (small string)", samples, n, 1);
}

static void bench_throughput(size_t msg_size) {
    /* Pure throughput: one keygen + N encrypts of msg_size bytes */
    char pub[] = "/tmp/bench4_pub_XXXXXX"; close(mkstemp(pub));
    char priv[] = "/tmp/bench4_priv_XXXXXX"; close(mkstemp(priv));
    zuptsdk_easy_keygen(pub, priv);

    uint8_t *msg = malloc(msg_size);
    memset(msg, 0xEF, msg_size);

    int n = msg_size > 1024*1024 ? 10 : 100;
    double t0 = now_sec();
    for (int i = 0; i < n; i++) {
        uint8_t *blob = NULL; size_t blob_sz = 0;
        zuptsdk_easy_encrypt(pub, msg, msg_size, &blob, &blob_sz);
        free(blob);
    }
    double dt = now_sec() - t0;
    double mb_per_sec = (msg_size * (double)n / 1024.0 / 1024.0) / dt;
    printf("  PQ encrypt throughput msg=%-9zu B   (%2d iters)  =  %6.1f MB/s\n",
           msg_size, n, mb_per_sec);

    free(msg);
    unlink(pub); unlink(priv);
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  libvuptsdk %s — performance benchmark\n", zuptsdk_version_string());
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");

    /* Warmup */
    bench_pq_keygen(5);
    printf("\nLatency (100 samples each):\n");
    bench_pq_keygen(100);
    bench_pq_encrypt(100, 64);
    bench_pq_encrypt(100, 4096);
    bench_pq_decrypt(100, 64);
    bench_pq_decrypt(100, 4096);
    bench_password_kdf(20);
    bench_field(500);

    printf("\nThroughput (sustained encrypt):\n");
    bench_throughput(1024);
    bench_throughput(64 * 1024);
    bench_throughput(1024 * 1024);
    bench_throughput(16 * 1024 * 1024);

    printf("\nDone.\n");
    return 0;
}
