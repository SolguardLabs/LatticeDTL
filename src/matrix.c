#include "matrix.h"

#include <string.h>

static void account_pair(LdtlAccountId a,
                         LdtlAccountId b,
                         LdtlAccountId *low,
                         LdtlAccountId *high) {
    if (a < b) {
        *low = a;
        *high = b;
    } else {
        *low = b;
        *high = a;
    }
}

static LdtlAmount signed_for_pair(LdtlAccountId debtor,
                                  LdtlAccountId creditor,
                                  LdtlAccountId low,
                                  LdtlAmount amount) {
    if (debtor == low) {
        (void)creditor;
        return amount;
    }
    return -amount;
}

static LdtlExactRow *find_exact_row(LdtlMatrix *matrix,
                                    LdtlAccountId low,
                                    LdtlAccountId high,
                                    LdtlAssetId asset_id,
                                    LdtlDomainId domain_id) {
    int i;

    for (i = 0; i < matrix->exact_count; i++) {
        LdtlExactRow *row = &matrix->exact_rows[i];
        if (row->used && row->low_account == low && row->high_account == high &&
            row->asset_id == asset_id && row->domain_id == domain_id) {
            return row;
        }
    }
    return NULL;
}

static LdtlCompactRow *find_compact_row(LdtlMatrix *matrix,
                                        LdtlAccountId low,
                                        LdtlAccountId high,
                                        LdtlDomainId domain_id) {
    int i;

    for (i = 0; i < matrix->compact_count; i++) {
        LdtlCompactRow *row = &matrix->compact_rows[i];
        if (row->used && row->low_account == low && row->high_account == high &&
            row->domain_id == domain_id) {
            return row;
        }
    }
    return NULL;
}

static LdtlStatus push_materialized(LdtlMatrix *matrix,
                                    LdtlAccountId debtor,
                                    LdtlAccountId creditor,
                                    LdtlAssetId asset_id,
                                    LdtlDomainId domain_id,
                                    LdtlAmount amount,
                                    const char *memo) {
    LdtlDebtCell *cell;

    if (amount == 0 || debtor == creditor) {
        return LDTL_OK;
    }
    if (amount < 0 || matrix->materialized_count >= LDTL_MAX_DEBTS) {
        return LDTL_ERR_CAPACITY;
    }
    cell = &matrix->materialized[matrix->materialized_count++];
    memset(cell, 0, sizeof(*cell));
    cell->used = 1;
    cell->id = matrix->next_cell_id++;
    cell->debtor = debtor;
    cell->creditor = creditor;
    cell->asset_id = asset_id;
    cell->domain_id = domain_id;
    cell->amount = amount;
    cell->priority = 1;
    ldtl_copy_label(cell->memo, sizeof(cell->memo), memo);
    return LDTL_OK;
}

static LdtlStatus note_exact(LdtlMatrix *matrix, const LdtlDebtCell *cell) {
    LdtlAccountId low;
    LdtlAccountId high;
    LdtlExactRow *row;
    LdtlAmount signed_amount;
    LdtlAmount next;

    account_pair(cell->debtor, cell->creditor, &low, &high);
    signed_amount = signed_for_pair(cell->debtor, cell->creditor, low,
                                    cell->amount);
    row = find_exact_row(matrix, low, high, cell->asset_id, cell->domain_id);
    if (row == NULL) {
        if (matrix->exact_count >= LDTL_MAX_ROWS) {
            return LDTL_ERR_CAPACITY;
        }
        row = &matrix->exact_rows[matrix->exact_count++];
        memset(row, 0, sizeof(*row));
        row->used = 1;
        row->low_account = low;
        row->high_account = high;
        row->asset_id = cell->asset_id;
        row->domain_id = cell->domain_id;
    }
    if (!ldtl_amount_add(row->signed_amount, signed_amount, &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    row->signed_amount = next;
    row->source_count++;
    row->fingerprint = ldtl_hash_u64(row->fingerprint, cell->id);
    row->fingerprint = ldtl_hash_u64(row->fingerprint,
                                     (uint64_t)cell->amount);
    return LDTL_OK;
}

static LdtlStatus note_compact(LdtlMatrix *matrix, const LdtlDebtCell *cell) {
    LdtlAccountId low;
    LdtlAccountId high;
    LdtlCompactRow *row;
    LdtlAmount signed_amount;
    LdtlAmount next;

    account_pair(cell->debtor, cell->creditor, &low, &high);
    signed_amount = signed_for_pair(cell->debtor, cell->creditor, low,
                                    cell->amount);
    row = find_compact_row(matrix, low, high, cell->domain_id);
    if (row == NULL) {
        if (matrix->compact_count >= LDTL_MAX_ROWS) {
            return LDTL_ERR_CAPACITY;
        }
        row = &matrix->compact_rows[matrix->compact_count++];
        memset(row, 0, sizeof(*row));
        row->used = 1;
        row->low_account = low;
        row->high_account = high;
        row->domain_id = cell->domain_id;
        row->settlement_asset = cell->asset_id;
    }
    if (!ldtl_amount_add(row->signed_amount, signed_amount, &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    row->signed_amount = next;
    row->source_count++;
    row->fingerprint = ldtl_hash_u64(row->fingerprint, cell->id);
    row->fingerprint = ldtl_hash_u64(row->fingerprint,
                                     (uint64_t)cell->amount);
    return LDTL_OK;
}

static LdtlStatus emit_exact_rows(LdtlMatrix *matrix) {
    int i;

    for (i = 0; i < matrix->exact_count; i++) {
        LdtlExactRow *row = &matrix->exact_rows[i];
        LdtlAmount amount;
        LdtlAccountId debtor;
        LdtlAccountId creditor;

        if (!row->used || row->signed_amount == 0) {
            continue;
        }
        if (!ldtl_amount_abs(row->signed_amount, &amount)) {
            return LDTL_ERR_OVERFLOW;
        }
        if (row->signed_amount > 0) {
            debtor = row->low_account;
            creditor = row->high_account;
        } else {
            debtor = row->high_account;
            creditor = row->low_account;
        }
        LdtlStatus status = push_materialized(matrix, debtor, creditor,
                                              row->asset_id, row->domain_id,
                                              amount, "exact-row");
        if (status != LDTL_OK) {
            return status;
        }
    }
    return LDTL_OK;
}

static LdtlStatus emit_compact_rows(LdtlMatrix *matrix) {
    int i;

    for (i = 0; i < matrix->compact_count; i++) {
        LdtlCompactRow *row = &matrix->compact_rows[i];
        LdtlAmount amount;
        LdtlAccountId debtor;
        LdtlAccountId creditor;

        if (!row->used || row->signed_amount == 0) {
            continue;
        }
        if (!ldtl_amount_abs(row->signed_amount, &amount)) {
            return LDTL_ERR_OVERFLOW;
        }
        if (row->signed_amount > 0) {
            debtor = row->low_account;
            creditor = row->high_account;
        } else {
            debtor = row->high_account;
            creditor = row->low_account;
        }
        LdtlStatus status = push_materialized(matrix, debtor, creditor,
                                              row->settlement_asset,
                                              row->domain_id, amount,
                                              "compact-row");
        if (status != LDTL_OK) {
            return status;
        }
    }
    return LDTL_OK;
}

void ldtl_matrix_init(LdtlMatrix *matrix, LdtlAmount compact_threshold) {
    memset(matrix, 0, sizeof(*matrix));
    matrix->compact_threshold = compact_threshold < 0 ? 0 : compact_threshold;
    matrix->next_cell_id = 1;
}

void ldtl_matrix_clear(LdtlMatrix *matrix) {
    LdtlAmount threshold = matrix->compact_threshold;
    uint64_t next_id = matrix->next_cell_id;

    memset(matrix, 0, sizeof(*matrix));
    matrix->compact_threshold = threshold;
    matrix->next_cell_id = next_id;
}

LdtlStatus ldtl_matrix_add_debt(LdtlMatrix *matrix,
                                LdtlAccountId debtor,
                                LdtlAccountId creditor,
                                LdtlAssetId asset_id,
                                LdtlDomainId domain_id,
                                LdtlAmount amount,
                                uint8_t priority,
                                const char *memo) {
    LdtlDebtCell *cell;

    if (matrix == NULL || debtor < 0 || creditor < 0 || debtor == creditor ||
        asset_id < 0 || amount <= 0) {
        return LDTL_ERR_INVALID;
    }
    if (matrix->cell_count >= LDTL_MAX_DEBTS) {
        return LDTL_ERR_CAPACITY;
    }
    cell = &matrix->cells[matrix->cell_count++];
    memset(cell, 0, sizeof(*cell));
    cell->used = 1;
    cell->id = matrix->next_cell_id++;
    cell->debtor = debtor;
    cell->creditor = creditor;
    cell->asset_id = asset_id;
    cell->domain_id = domain_id;
    cell->amount = amount;
    cell->priority = priority;
    ldtl_copy_label(cell->memo, sizeof(cell->memo), memo);
    return LDTL_OK;
}

LdtlStatus ldtl_matrix_materialize(LdtlMatrix *matrix,
                                   const LdtlAssetRegistry *registry) {
    int i;
    LdtlStatus status;

    if (matrix == NULL || registry == NULL) {
        return LDTL_ERR_INVALID;
    }
    memset(matrix->exact_rows, 0, sizeof(matrix->exact_rows));
    memset(matrix->compact_rows, 0, sizeof(matrix->compact_rows));
    memset(matrix->materialized, 0, sizeof(matrix->materialized));
    matrix->exact_count = 0;
    matrix->compact_count = 0;
    matrix->materialized_count = 0;

    for (i = 0; i < matrix->cell_count; i++) {
        LdtlDebtCell *cell = &matrix->cells[i];
        if (!cell->used) {
            continue;
        }
        if (!ldtl_asset_is_enabled(registry, cell->asset_id)) {
            return LDTL_ERR_NOT_FOUND;
        }
        if (cell->amount <= matrix->compact_threshold) {
            status = note_compact(matrix, cell);
        } else {
            status = note_exact(matrix, cell);
        }
        if (status != LDTL_OK) {
            return status;
        }
    }

    status = emit_exact_rows(matrix);
    if (status != LDTL_OK) {
        return status;
    }
    status = emit_compact_rows(matrix);
    if (status != LDTL_OK) {
        return status;
    }
    matrix->last_digest = ldtl_matrix_digest(matrix);
    return LDTL_OK;
}

const LdtlDebtCell *ldtl_matrix_materialized(const LdtlMatrix *matrix,
                                             int index) {
    if (matrix == NULL || index < 0 || index >= matrix->materialized_count) {
        return NULL;
    }
    return &matrix->materialized[index];
}

uint64_t ldtl_matrix_digest(const LdtlMatrix *matrix) {
    uint64_t hash = 0;
    int i;

    if (matrix == NULL) {
        return 0;
    }
    hash = ldtl_hash_u64(hash, (uint64_t)matrix->cell_count);
    hash = ldtl_hash_u64(hash, (uint64_t)matrix->exact_count);
    hash = ldtl_hash_u64(hash, (uint64_t)matrix->compact_count);
    hash = ldtl_hash_u64(hash, (uint64_t)matrix->materialized_count);
    hash = ldtl_hash_u64(hash, (uint64_t)matrix->compact_threshold);
    for (i = 0; i < matrix->materialized_count; i++) {
        const LdtlDebtCell *cell = &matrix->materialized[i];
        hash = ldtl_hash_u64(hash, (uint64_t)cell->debtor);
        hash = ldtl_hash_u64(hash, (uint64_t)cell->creditor);
        hash = ldtl_hash_u64(hash, (uint64_t)cell->asset_id);
        hash = ldtl_hash_u64(hash, (uint64_t)cell->domain_id);
        hash = ldtl_hash_u64(hash, (uint64_t)cell->amount);
    }
    return hash;
}

void ldtl_matrix_print_json(const LdtlMatrix *matrix, FILE *out) {
    int i;

    fprintf(out,
            "  \"matrix\":{\"rawCells\":%d,\"exactRows\":%d,"
            "\"compactRows\":%d,\"materializedCells\":%d,"
            "\"compactThreshold\":%lld,\"digest\":\"0x%016llx\",",
            matrix->cell_count, matrix->exact_count, matrix->compact_count,
            matrix->materialized_count, (long long)matrix->compact_threshold,
            (unsigned long long)matrix->last_digest);
    fputs("\"materialized\":[", out);
    for (i = 0; i < matrix->materialized_count; i++) {
        const LdtlDebtCell *cell = &matrix->materialized[i];
        if (i > 0) {
            fputc(',', out);
        }
        fprintf(out,
                "{\"debtor\":%d,\"creditor\":%d,\"assetId\":%d,"
                "\"domainId\":%d,\"amount\":%lld}",
                cell->debtor, cell->creditor, cell->asset_id, cell->domain_id,
                (long long)cell->amount);
    }
    fputs("]},\n", out);
}
