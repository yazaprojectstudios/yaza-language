#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "yaza_runtime_v1.h"

static void put_le(unsigned char *p, uint64_t value, size_t width) {
    for (size_t i = 0; i < width; ++i) p[i] = (unsigned char)(value >> (8U * i));
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    char *end = NULL;
    const unsigned long long count = strtoull(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || count == 0 || count > 1000000ULL) return 2;
    const size_t record_size = 40;
    if (count > SIZE_MAX / record_size) return 2;
    const size_t length = (size_t)count * record_size;
    unsigned char *bytes = (unsigned char *)calloc(length, 1);
    if (bytes == NULL) return 3;
    for (unsigned long long i = 0; i < count; ++i) {
        unsigned char *record = bytes + (size_t)i * record_size;
        put_le(record, record_size, 4);
        put_le(record + 8, i * record_size, 8);
        record[32] = 15; /* SKIP */
        const uint64_t checksum = yaza_bytes_v1_ydbv1_checksum_zeroed_range(record, record_size, 4, 4);
        put_le(record + 4, checksum, 4);
    }
    char path[4096];
    const int path_length = snprintf(path, sizeof(path),
        "%s/ydb-wal-636865636b706f696e742d7465737431-seg-0.yzw", argv[1]);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) return 2;
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) { perror("open"); free(bytes); return 4; }
    size_t written = 0;
    while (written < length) {
        const ssize_t amount = write(fd, bytes + written, length - written);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) { perror("write"); close(fd); free(bytes); return 5; }
        written += (size_t)amount;
    }
    if (fsync(fd) != 0) { perror("fsync"); close(fd); free(bytes); return 6; }
    if (close(fd) != 0) { perror("close"); free(bytes); return 7; }
    printf("Generated %llu valid SKIP records (%zu bytes).\n", count, length);
    free(bytes);
    return 0;
}
