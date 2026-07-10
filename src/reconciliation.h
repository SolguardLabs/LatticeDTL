#ifndef LDTL_RECONCILIATION_H
#define LDTL_RECONCILIATION_H

#include "ledger.h"

typedef struct LdtlReconciliationRow {
    LdtlAssetId asset_id;
    LdtlAmount balance_sum;
    LdtlAmount reserve;
    LdtlAmount liability;
    LdtlAmount reserve_floor;
    LdtlAmount reserve_gap;
    LdtlAmount liability_gap;
    int active_accounts;
    int ok;
} LdtlReconciliationRow;

typedef struct LdtlReconciliation {
    LdtlReconciliationRow rows[LDTL_MAX_ASSETS];
    int row_count;
    int ok;
    int active_balance_cells;
    uint64_t digest;
} LdtlReconciliation;

void ldtl_reconciliation_build(const LdtlLedger *ledger,
                               LdtlReconciliation *report);
const LdtlReconciliationRow *ldtl_reconciliation_row(
    const LdtlReconciliation *report,
    LdtlAssetId asset_id);
void ldtl_reconciliation_print_json(const LdtlReconciliation *report,
                                    const LdtlAssetRegistry *registry,
                                    FILE *out);

#endif
