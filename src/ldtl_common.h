#ifndef LDTL_COMMON_H
#define LDTL_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define LDTL_MAX_ASSETS 16
#define LDTL_MAX_ACCOUNTS 32
#define LDTL_MAX_DEBTS 512
#define LDTL_MAX_ROWS 512
#define LDTL_MAX_EVENTS 1024
#define LDTL_MAX_WITHDRAWALS 128
#define LDTL_LABEL_LEN 32
#define LDTL_SYMBOL_LEN 12
#define LDTL_DOMAIN_LEN 24
#define LDTL_EXTERNAL_REF_LEN 40

typedef int32_t LdtlAssetId;
typedef int32_t LdtlAccountId;
typedef int32_t LdtlDomainId;
typedef int64_t LdtlAmount;

typedef enum LdtlStatus {
    LDTL_OK = 0,
    LDTL_ERR_CAPACITY = 1,
    LDTL_ERR_NOT_FOUND = 2,
    LDTL_ERR_DUPLICATE = 3,
    LDTL_ERR_INVALID = 4,
    LDTL_ERR_OVERFLOW = 5,
    LDTL_ERR_INSUFFICIENT_BALANCE = 6,
    LDTL_ERR_INSUFFICIENT_RESERVE = 7,
    LDTL_ERR_DISABLED = 8,
    LDTL_ERR_PRECHECK = 9
} LdtlStatus;

const char *ldtl_status_name(LdtlStatus status);
void ldtl_copy_label(char *dst, size_t dst_len, const char *src);
int ldtl_amount_add(LdtlAmount a, LdtlAmount b, LdtlAmount *out);
int ldtl_amount_sub(LdtlAmount a, LdtlAmount b, LdtlAmount *out);
int ldtl_amount_abs(LdtlAmount value, LdtlAmount *out);
uint64_t ldtl_hash_bytes(uint64_t seed, const void *data, size_t len);
uint64_t ldtl_hash_u64(uint64_t seed, uint64_t value);
void ldtl_json_string(FILE *out, const char *value);

#endif
