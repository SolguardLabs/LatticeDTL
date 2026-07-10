#ifndef LDTL_EPOCH_NETTING_H
#define LDTL_EPOCH_NETTING_H

#include "ledger.h"
#include "matrix.h"

typedef struct LdtlEpochResult {
    int epoch_id;
    LdtlStatus status;
    int raw_cells;
    int exact_rows;
    int compact_rows;
    int materialized_cells;
    int transfers;
    LdtlAmount volume_by_asset[LDTL_MAX_ASSETS];
    uint64_t matrix_digest;
    uint64_t ledger_digest_before;
    uint64_t ledger_digest_after;
} LdtlEpochResult;

void ldtl_epoch_result_init(LdtlEpochResult *result, int epoch_id);
LdtlStatus ldtl_epoch_settle(LdtlLedger *ledger,
                             LdtlMatrix *matrix,
                             int epoch_id,
                             LdtlEpochResult *result);
void ldtl_epoch_result_print_json(const LdtlEpochResult *result, FILE *out);

#endif
