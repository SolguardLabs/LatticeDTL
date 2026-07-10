#include "reconciliation.h"

#include <string.h>

static uint64_t row_digest(uint64_t hash, const LdtlReconciliationRow *row) {
    hash = ldtl_hash_u64(hash, (uint64_t)row->asset_id);
    hash = ldtl_hash_u64(hash, (uint64_t)row->balance_sum);
    hash = ldtl_hash_u64(hash, (uint64_t)row->reserve);
    hash = ldtl_hash_u64(hash, (uint64_t)row->liability);
    hash = ldtl_hash_u64(hash, (uint64_t)row->reserve_floor);
    hash = ldtl_hash_u64(hash, (uint64_t)row->reserve_gap);
    hash = ldtl_hash_u64(hash, (uint64_t)row->liability_gap);
    hash = ldtl_hash_u64(hash, (uint64_t)row->active_accounts);
    hash = ldtl_hash_u64(hash, (uint64_t)row->ok);
    return hash;
}

static int row_is_consistent(const LdtlReconciliationRow *row) {
    if (row->balance_sum != row->liability) {
        return 0;
    }
    if (row->reserve < row->reserve_floor) {
        return 0;
    }
    if (row->reserve < 0 || row->liability < 0 || row->balance_sum < 0) {
        return 0;
    }
    return 1;
}

void ldtl_reconciliation_build(const LdtlLedger *ledger,
                               LdtlReconciliation *report) {
    int asset;
    int account;

    memset(report, 0, sizeof(*report));
    report->ok = 1;
    if (ledger == NULL || ledger->registry == NULL) {
        report->ok = 0;
        return;
    }

    for (asset = 0; asset < ledger->registry->asset_count; asset++) {
        LdtlReconciliationRow *row = &report->rows[report->row_count++];
        row->asset_id = asset;
        row->reserve = ledger->vaults[asset].reserve;
        row->liability = ledger->vaults[asset].liability;
        row->reserve_floor = ledger->vaults[asset].reserve_floor;

        for (account = 0; account < ledger->account_count; account++) {
            LdtlAmount balance = ledger->balances[account][asset];
            LdtlAmount next;

            if (balance != 0) {
                row->active_accounts++;
                report->active_balance_cells++;
            }
            if (!ldtl_amount_add(row->balance_sum, balance, &next)) {
                row->ok = 0;
                report->ok = 0;
                continue;
            }
            row->balance_sum = next;
        }

        if (!ldtl_amount_sub(row->reserve, row->liability, &row->reserve_gap)) {
            row->ok = 0;
            report->ok = 0;
        }
        if (!ldtl_amount_sub(row->balance_sum, row->liability,
                             &row->liability_gap)) {
            row->ok = 0;
            report->ok = 0;
        }
        row->ok = row_is_consistent(row);
        if (!row->ok) {
            report->ok = 0;
        }
        report->digest = row_digest(report->digest, row);
    }
    report->digest = ldtl_hash_u64(report->digest,
                                   (uint64_t)report->active_balance_cells);
    report->digest = ldtl_hash_u64(report->digest, (uint64_t)report->ok);
}

const LdtlReconciliationRow *ldtl_reconciliation_row(
    const LdtlReconciliation *report,
    LdtlAssetId asset_id) {
    int i;

    if (report == NULL) {
        return NULL;
    }
    for (i = 0; i < report->row_count; i++) {
        if (report->rows[i].asset_id == asset_id) {
            return &report->rows[i];
        }
    }
    return NULL;
}

void ldtl_reconciliation_print_json(const LdtlReconciliation *report,
                                    const LdtlAssetRegistry *registry,
                                    FILE *out) {
    int i;

    fprintf(out,
            "  \"reconciliation\":{\"ok\":%s,\"activeBalanceCells\":%d,"
            "\"digest\":\"0x%016llx\",\"assets\":[",
            report->ok ? "true" : "false", report->active_balance_cells,
            (unsigned long long)report->digest);
    for (i = 0; i < report->row_count; i++) {
        const LdtlReconciliationRow *row = &report->rows[i];
        const LdtlAsset *asset = ldtl_asset_view(registry, row->asset_id);

        if (i > 0) {
            fputc(',', out);
        }
        fputs("{\"asset\":", out);
        ldtl_json_string(out, asset != NULL ? asset->symbol : "unknown");
        fprintf(out,
                ",\"assetId\":%d,\"balanceSum\":%lld,\"reserve\":%lld,"
                "\"liability\":%lld,\"reserveFloor\":%lld,"
                "\"reserveGap\":%lld,\"liabilityGap\":%lld,"
                "\"activeAccounts\":%d,\"ok\":%s}",
                row->asset_id, (long long)row->balance_sum,
                (long long)row->reserve, (long long)row->liability,
                (long long)row->reserve_floor, (long long)row->reserve_gap,
                (long long)row->liability_gap, row->active_accounts,
                row->ok ? "true" : "false");
    }
    fputs("]},\n", out);
}
