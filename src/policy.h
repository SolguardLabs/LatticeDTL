#ifndef LDTL_POLICY_H
#define LDTL_POLICY_H

#include "ledger.h"

#define LDTL_MAX_POLICY_RULES 8

typedef struct LdtlPolicyRule {
    int used;
    int risk_class;
    LdtlAmount max_exposure_units;
    LdtlAmount max_withdrawal_units;
    uint16_t max_asset_risk_bps;
    int allow_system_bypass;
} LdtlPolicyRule;

typedef struct LdtlPolicyBook {
    LdtlPolicyRule rules[LDTL_MAX_POLICY_RULES];
    int rule_count;
    uint64_t version;
} LdtlPolicyBook;

typedef struct LdtlPolicyAccountRow {
    LdtlAccountId account_id;
    int risk_class;
    LdtlAmount exposure_units;
    LdtlAmount withdrawable_units;
    LdtlAmount max_exposure_units;
    LdtlAmount max_withdrawal_units;
    uint16_t max_observed_asset_risk_bps;
    int active_assets;
    int within_exposure;
} LdtlPolicyAccountRow;

typedef struct LdtlPolicyReport {
    LdtlPolicyAccountRow rows[LDTL_MAX_ACCOUNTS];
    int row_count;
    int flagged_accounts;
    int ok;
    uint64_t digest;
} LdtlPolicyReport;

typedef struct LdtlPolicyCheck {
    int used;
    LdtlStatus status;
    LdtlAccountId account_id;
    LdtlAssetId asset_id;
    LdtlAmount amount;
    char action[LDTL_LABEL_LEN];
} LdtlPolicyCheck;

void ldtl_policy_book_init(LdtlPolicyBook *book);
LdtlStatus ldtl_policy_book_add_rule(LdtlPolicyBook *book,
                                     int risk_class,
                                     LdtlAmount max_exposure_units,
                                     LdtlAmount max_withdrawal_units,
                                     uint16_t max_asset_risk_bps,
                                     int allow_system_bypass);
void ldtl_policy_book_init_defaults(LdtlPolicyBook *book);
const LdtlPolicyRule *ldtl_policy_rule_for_class(const LdtlPolicyBook *book,
                                                 int risk_class);
void ldtl_policy_assess_ledger(const LdtlLedger *ledger,
                               const LdtlPolicyBook *book,
                               LdtlPolicyReport *report);
LdtlStatus ldtl_policy_validate_withdrawal(const LdtlLedger *ledger,
                                           const LdtlPolicyBook *book,
                                           LdtlAccountId account_id,
                                           LdtlAssetId asset_id,
                                           LdtlAmount amount,
                                           LdtlPolicyCheck *check);
void ldtl_policy_check_clear(LdtlPolicyCheck *check);
void ldtl_policy_print_json(const LdtlPolicyReport *report,
                            const LdtlPolicyBook *book,
                            const LdtlPolicyCheck *check,
                            const LdtlLedger *ledger,
                            const LdtlAssetRegistry *registry,
                            FILE *out);

#endif
