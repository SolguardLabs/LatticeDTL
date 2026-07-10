#ifndef LDTL_MATRIX_H
#define LDTL_MATRIX_H

#include "asset_registry.h"
#include "ldtl_common.h"

typedef struct LdtlDebtCell {
    int used;
    uint64_t id;
    LdtlAccountId debtor;
    LdtlAccountId creditor;
    LdtlAssetId asset_id;
    LdtlDomainId domain_id;
    LdtlAmount amount;
    uint8_t priority;
    char memo[LDTL_LABEL_LEN];
} LdtlDebtCell;

typedef struct LdtlExactRow {
    int used;
    LdtlAccountId low_account;
    LdtlAccountId high_account;
    LdtlAssetId asset_id;
    LdtlDomainId domain_id;
    LdtlAmount signed_amount;
    int source_count;
    uint64_t fingerprint;
} LdtlExactRow;

typedef struct LdtlCompactRow {
    int used;
    LdtlAccountId low_account;
    LdtlAccountId high_account;
    LdtlAssetId settlement_asset;
    LdtlDomainId domain_id;
    LdtlAmount signed_amount;
    int source_count;
    uint64_t fingerprint;
} LdtlCompactRow;

typedef struct LdtlMatrix {
    LdtlDebtCell cells[LDTL_MAX_DEBTS];
    int cell_count;
    LdtlExactRow exact_rows[LDTL_MAX_ROWS];
    int exact_count;
    LdtlCompactRow compact_rows[LDTL_MAX_ROWS];
    int compact_count;
    LdtlDebtCell materialized[LDTL_MAX_DEBTS];
    int materialized_count;
    LdtlAmount compact_threshold;
    uint64_t next_cell_id;
    uint64_t last_digest;
} LdtlMatrix;

void ldtl_matrix_init(LdtlMatrix *matrix, LdtlAmount compact_threshold);
void ldtl_matrix_clear(LdtlMatrix *matrix);
LdtlStatus ldtl_matrix_add_debt(LdtlMatrix *matrix,
                                LdtlAccountId debtor,
                                LdtlAccountId creditor,
                                LdtlAssetId asset_id,
                                LdtlDomainId domain_id,
                                LdtlAmount amount,
                                uint8_t priority,
                                const char *memo);
LdtlStatus ldtl_matrix_materialize(LdtlMatrix *matrix,
                                   const LdtlAssetRegistry *registry);
const LdtlDebtCell *ldtl_matrix_materialized(const LdtlMatrix *matrix,
                                             int index);
uint64_t ldtl_matrix_digest(const LdtlMatrix *matrix);
void ldtl_matrix_print_json(const LdtlMatrix *matrix, FILE *out);

#endif
