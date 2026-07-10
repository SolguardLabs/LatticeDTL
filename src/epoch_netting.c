#include "epoch_netting.h"

#include <string.h>

void ldtl_epoch_result_init(LdtlEpochResult *result, int epoch_id) {
    memset(result, 0, sizeof(*result));
    result->epoch_id = epoch_id;
    result->status = LDTL_OK;
}

static LdtlStatus precheck_materialized(const LdtlLedger *ledger,
                                        const LdtlMatrix *matrix) {
    LdtlAmount required[LDTL_MAX_ACCOUNTS][LDTL_MAX_ASSETS];
    int i;
    int account;
    int asset;

    memset(required, 0, sizeof(required));
    for (i = 0; i < matrix->materialized_count; i++) {
        const LdtlDebtCell *cell = ldtl_matrix_materialized(matrix, i);
        LdtlAmount next;

        if (cell == NULL) {
            return LDTL_ERR_INVALID;
        }
        if (cell->debtor < 0 || cell->debtor >= ledger->account_count ||
            cell->asset_id < 0 ||
            cell->asset_id >= ledger->registry->asset_count) {
            return LDTL_ERR_NOT_FOUND;
        }
        if (!ldtl_amount_add(required[cell->debtor][cell->asset_id],
                             cell->amount, &next)) {
            return LDTL_ERR_OVERFLOW;
        }
        required[cell->debtor][cell->asset_id] = next;
    }

    for (account = 0; account < ledger->account_count; account++) {
        for (asset = 0; asset < ledger->registry->asset_count; asset++) {
            if (required[account][asset] == 0) {
                continue;
            }
            LdtlStatus status = ldtl_ledger_check_debit(
                ledger, account, asset, required[account][asset]);
            if (status != LDTL_OK) {
                return LDTL_ERR_PRECHECK;
            }
        }
    }
    return LDTL_OK;
}

LdtlStatus ldtl_epoch_settle(LdtlLedger *ledger,
                             LdtlMatrix *matrix,
                             int epoch_id,
                             LdtlEpochResult *result) {
    int i;
    LdtlStatus status;

    if (ledger == NULL || matrix == NULL || result == NULL || epoch_id <= 0) {
        return LDTL_ERR_INVALID;
    }
    ldtl_epoch_result_init(result, epoch_id);
    result->ledger_digest_before = ldtl_ledger_digest(ledger);

    status = ldtl_matrix_materialize(matrix, ledger->registry);
    if (status != LDTL_OK) {
        result->status = status;
        return status;
    }
    result->raw_cells = matrix->cell_count;
    result->exact_rows = matrix->exact_count;
    result->compact_rows = matrix->compact_count;
    result->materialized_cells = matrix->materialized_count;
    result->matrix_digest = matrix->last_digest;

    status = precheck_materialized(ledger, matrix);
    if (status != LDTL_OK) {
        result->status = status;
        return status;
    }

    for (i = 0; i < matrix->materialized_count; i++) {
        const LdtlDebtCell *cell = ldtl_matrix_materialized(matrix, i);

        status = ldtl_ledger_transfer(ledger, cell->debtor, cell->creditor,
                                      cell->asset_id, cell->amount, epoch_id,
                                      cell->memo);
        if (status != LDTL_OK) {
            result->status = status;
            return status;
        }
        result->transfers++;
        result->volume_by_asset[cell->asset_id] += cell->amount;
    }

    result->ledger_digest_after = ldtl_ledger_digest(ledger);
    result->status = LDTL_OK;
    return LDTL_OK;
}

void ldtl_epoch_result_print_json(const LdtlEpochResult *result, FILE *out) {
    int i;
    int first = 1;

    fprintf(out,
            "  \"epoch\":{\"id\":%d,\"status\":\"%s\","
            "\"rawCells\":%d,\"exactRows\":%d,\"compactRows\":%d,"
            "\"materializedCells\":%d,\"transfers\":%d,"
            "\"matrixDigest\":\"0x%016llx\","
            "\"ledgerBefore\":\"0x%016llx\","
            "\"ledgerAfter\":\"0x%016llx\",\"volumeByAsset\":[",
            result->epoch_id, ldtl_status_name(result->status),
            result->raw_cells, result->exact_rows, result->compact_rows,
            result->materialized_cells, result->transfers,
            (unsigned long long)result->matrix_digest,
            (unsigned long long)result->ledger_digest_before,
            (unsigned long long)result->ledger_digest_after);
    for (i = 0; i < LDTL_MAX_ASSETS; i++) {
        if (result->volume_by_asset[i] == 0) {
            continue;
        }
        if (!first) {
            fputc(',', out);
        }
        first = 0;
        fprintf(out, "{\"assetId\":%d,\"amount\":%lld}", i,
                (long long)result->volume_by_asset[i]);
    }
    fputs("]},\n", out);
}
