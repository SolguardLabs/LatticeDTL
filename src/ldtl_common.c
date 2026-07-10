#include "ldtl_common.h"

#include <limits.h>
#include <string.h>

const char *ldtl_status_name(LdtlStatus status) {
    switch (status) {
    case LDTL_OK:
        return "ok";
    case LDTL_ERR_CAPACITY:
        return "capacity";
    case LDTL_ERR_NOT_FOUND:
        return "not_found";
    case LDTL_ERR_DUPLICATE:
        return "duplicate";
    case LDTL_ERR_INVALID:
        return "invalid";
    case LDTL_ERR_OVERFLOW:
        return "overflow";
    case LDTL_ERR_INSUFFICIENT_BALANCE:
        return "insufficient_balance";
    case LDTL_ERR_INSUFFICIENT_RESERVE:
        return "insufficient_reserve";
    case LDTL_ERR_DISABLED:
        return "disabled";
    case LDTL_ERR_PRECHECK:
        return "precheck";
    default:
        return "unknown";
    }
}

void ldtl_copy_label(char *dst, size_t dst_len, const char *src) {
    size_t i;

    if (dst_len == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    for (i = 0; i + 1 < dst_len && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

int ldtl_amount_add(LdtlAmount a, LdtlAmount b, LdtlAmount *out) {
    if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) {
        return 0;
    }
    *out = a + b;
    return 1;
}

int ldtl_amount_sub(LdtlAmount a, LdtlAmount b, LdtlAmount *out) {
    if (b == LLONG_MIN) {
        return 0;
    }
    return ldtl_amount_add(a, -b, out);
}

int ldtl_amount_abs(LdtlAmount value, LdtlAmount *out) {
    if (value == LLONG_MIN) {
        return 0;
    }
    *out = value < 0 ? -value : value;
    return 1;
}

uint64_t ldtl_hash_bytes(uint64_t seed, const void *data, size_t len) {
    const unsigned char *bytes = (const unsigned char *)data;
    uint64_t hash = seed == 0 ? 1469598103934665603ULL : seed;
    size_t i;

    for (i = 0; i < len; i++) {
        hash ^= (uint64_t)bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t ldtl_hash_u64(uint64_t seed, uint64_t value) {
    return ldtl_hash_bytes(seed, &value, sizeof(value));
}

void ldtl_json_string(FILE *out, const char *value) {
    const unsigned char *p = (const unsigned char *)value;

    fputc('"', out);
    if (value != NULL) {
        while (*p != '\0') {
            switch (*p) {
            case '"':
                fputs("\\\"", out);
                break;
            case '\\':
                fputs("\\\\", out);
                break;
            case '\b':
                fputs("\\b", out);
                break;
            case '\f':
                fputs("\\f", out);
                break;
            case '\n':
                fputs("\\n", out);
                break;
            case '\r':
                fputs("\\r", out);
                break;
            case '\t':
                fputs("\\t", out);
                break;
            default:
                if (*p < 0x20) {
                    fprintf(out, "\\u%04x", (unsigned int)*p);
                } else {
                    fputc((int)*p, out);
                }
                break;
            }
            p++;
        }
    }
    fputc('"', out);
}
