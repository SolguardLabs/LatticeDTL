#include "policy.h"

#include <string.h>

static LdtlAmount weighted_amount(LdtlAmount amount, uint16_t risk_bps) {
    LdtlAmount whole;
    LdtlAmount rem;

    if (amount <= 0 || risk_bps == 0) {
        return 0;
    }
    whole = amount / 10000;
    rem = amount % 10000;
    return whole * (LdtlAmount)risk_bps +
           (rem * (LdtlAmount)risk_bps) / 10000;
}

static uint64_t policy_rule_digest(uint64_t hash, const LdtlPolicyRule *rule) {
    hash = ldtl_hash_u64(hash, (uint64_t)rule->risk_class);
    hash = ldtl_hash_u64(hash, (uint64_t)rule->max_exposure_units);
    hash = ldtl_hash_u64(hash, (uint64_t)rule->max_withdrawal_units);
    hash = ldtl_hash_u64(hash, (uint64_t)rule->max_asset_risk_bps);
    hash = ldtl_hash_u64(hash, (uint64_t)rule->allow_system_bypass);
    return hash;
}

static uint64_t policy_account_digest(uint64_t hash,
                                      const LdtlPolicyAccountRow *row) {
    hash = ldtl_hash_u64(hash, (uint64_t)row->account_id);
    hash = ldtl_hash_u64(hash, (uint64_t)row->risk_class);
    hash = ldtl_hash_u64(hash, (uint64_t)row->exposure_units);
    hash = ldtl_hash_u64(hash, (uint64_t)row->withdrawable_units);
    hash = ldtl_hash_u64(hash, (uint64_t)row->max_exposure_units);
    hash = ldtl_hash_u64(hash, (uint64_t)row->max_withdrawal_units);
    hash = ldtl_hash_u64(hash, (uint64_t)row->max_observed_asset_risk_bps);
    hash = ldtl_hash_u64(hash, (uint64_t)row->active_assets);
    hash = ldtl_hash_u64(hash, (uint64_t)row->within_exposure);
    return hash;
}

void ldtl_policy_book_init(LdtlPolicyBook *book) {
    memset(book, 0, sizeof(*book));
    book->version = 1;
}

LdtlStatus ldtl_policy_book_add_rule(LdtlPolicyBook *book,
                                     int risk_class,
                                     LdtlAmount max_exposure_units,
                                     LdtlAmount max_withdrawal_units,
                                     uint16_t max_asset_risk_bps,
                                     int allow_system_bypass) {
    LdtlPolicyRule *rule;

    if (book == NULL || risk_class < 0 || max_exposure_units < 0 ||
        max_withdrawal_units < 0 || max_asset_risk_bps > 10000) {
        return LDTL_ERR_INVALID;
    }
    if (book->rule_count >= LDTL_MAX_POLICY_RULES) {
        return LDTL_ERR_CAPACITY;
    }
    if (ldtl_policy_rule_for_class(book, risk_class) != NULL) {
        return LDTL_ERR_DUPLICATE;
    }
    rule = &book->rules[book->rule_count++];
    memset(rule, 0, sizeof(*rule));
    rule->used = 1;
    rule->risk_class = risk_class;
    rule->max_exposure_units = max_exposure_units;
    rule->max_withdrawal_units = max_withdrawal_units;
    rule->max_asset_risk_bps = max_asset_risk_bps;
    rule->allow_system_bypass = allow_system_bypass ? 1 : 0;
    book->version++;
    return LDTL_OK;
}

void ldtl_policy_book_init_defaults(LdtlPolicyBook *book) {
    ldtl_policy_book_init(book);
    (void)ldtl_policy_book_add_rule(book, 0, 20000000000LL, 2000000000LL,
                                    10000, 1);
    (void)ldtl_policy_book_add_rule(book, 1, 2000000000LL, 80000000LL, 5000,
                                    0);
    (void)ldtl_policy_book_add_rule(book, 2, 1500000000LL, 60000000LL, 4000,
                                    0);
    (void)ldtl_policy_book_add_rule(book, 3, 600000000LL, 20000000LL, 2000, 0);
    (void)ldtl_policy_book_add_rule(book, 4, 1000000000LL, 0, 10000, 1);
}

const LdtlPolicyRule *ldtl_policy_rule_for_class(const LdtlPolicyBook *book,
                                                 int risk_class) {
    int i;

    if (book == NULL) {
        return NULL;
    }
    for (i = 0; i < book->rule_count; i++) {
        if (book->rules[i].used && book->rules[i].risk_class == risk_class) {
            return &book->rules[i];
        }
    }
    return NULL;
}

void ldtl_policy_assess_ledger(const LdtlLedger *ledger,
                               const LdtlPolicyBook *book,
                               LdtlPolicyReport *report) {
    int account;
    int asset;

    memset(report, 0, sizeof(*report));
    report->ok = 1;
    if (ledger == NULL || ledger->registry == NULL || book == NULL) {
        report->ok = 0;
        return;
    }

    for (account = 0; account < ledger->account_count; account++) {
        const LdtlAccount *acct = &ledger->accounts[account];
        const LdtlPolicyRule *rule =
            ldtl_policy_rule_for_class(book, acct->risk_class);
        LdtlPolicyAccountRow *row = &report->rows[report->row_count++];

        row->account_id = acct->id;
        row->risk_class = acct->risk_class;
        if (rule != NULL) {
            row->max_exposure_units = rule->max_exposure_units;
            row->max_withdrawal_units = rule->max_withdrawal_units;
        }

        for (asset = 0; asset < ledger->registry->asset_count; asset++) {
            const LdtlAsset *info = &ledger->registry->assets[asset];
            LdtlAmount balance = ledger->balances[account][asset];
            LdtlAmount risk_units = weighted_amount(balance,
                                                    info->risk_weight_bps);
            LdtlAmount next;

            if (balance <= 0) {
                continue;
            }
            row->active_assets++;
            if (info->risk_weight_bps > row->max_observed_asset_risk_bps) {
                row->max_observed_asset_risk_bps = info->risk_weight_bps;
            }
            if (!ldtl_amount_add(row->exposure_units, risk_units, &next)) {
                report->ok = 0;
                continue;
            }
            row->exposure_units = next;
            if (info->withdraw_enabled) {
                if (!ldtl_amount_add(row->withdrawable_units, balance, &next)) {
                    report->ok = 0;
                    continue;
                }
                row->withdrawable_units = next;
            }
        }

        row->within_exposure =
            rule != NULL && row->exposure_units <= rule->max_exposure_units;
        if (!row->within_exposure) {
            report->flagged_accounts++;
            report->ok = 0;
        }
        report->digest = policy_account_digest(report->digest, row);
    }
    report->digest = ldtl_hash_u64(report->digest,
                                   (uint64_t)report->flagged_accounts);
    report->digest = ldtl_hash_u64(report->digest, (uint64_t)book->version);
}

void ldtl_policy_check_clear(LdtlPolicyCheck *check) {
    if (check != NULL) {
        memset(check, 0, sizeof(*check));
        check->status = LDTL_OK;
    }
}

LdtlStatus ldtl_policy_validate_withdrawal(const LdtlLedger *ledger,
                                           const LdtlPolicyBook *book,
                                           LdtlAccountId account_id,
                                           LdtlAssetId asset_id,
                                           LdtlAmount amount,
                                           LdtlPolicyCheck *check) {
    const LdtlAccount *account;
    const LdtlAsset *asset;
    const LdtlPolicyRule *rule;
    LdtlStatus status = LDTL_OK;

    if (check != NULL) {
        memset(check, 0, sizeof(*check));
        check->used = 1;
        check->account_id = account_id;
        check->asset_id = asset_id;
        check->amount = amount;
        ldtl_copy_label(check->action, sizeof(check->action), "withdrawal");
    }
    if (ledger == NULL || book == NULL || amount <= 0) {
        status = LDTL_ERR_INVALID;
        goto done;
    }

    account = ldtl_ledger_account(ledger, account_id);
    asset = ldtl_asset_view(ledger->registry, asset_id);
    if (account == NULL || asset == NULL) {
        status = LDTL_ERR_NOT_FOUND;
        goto done;
    }
    rule = ldtl_policy_rule_for_class(book, account->risk_class);
    if (rule == NULL) {
        status = LDTL_ERR_PRECHECK;
        goto done;
    }
    if (!asset->withdraw_enabled) {
        status = LDTL_ERR_DISABLED;
        goto done;
    }
    if (!rule->allow_system_bypass &&
        asset->risk_weight_bps > rule->max_asset_risk_bps) {
        status = LDTL_ERR_PRECHECK;
        goto done;
    }
    if (!rule->allow_system_bypass && amount > rule->max_withdrawal_units) {
        status = LDTL_ERR_PRECHECK;
        goto done;
    }
    status = ldtl_ledger_check_debit(ledger, account_id, asset_id, amount);

done:
    if (check != NULL) {
        check->status = status;
    }
    return status;
}

void ldtl_policy_print_json(const LdtlPolicyReport *report,
                            const LdtlPolicyBook *book,
                            const LdtlPolicyCheck *check,
                            const LdtlLedger *ledger,
                            const LdtlAssetRegistry *registry,
                            FILE *out) {
    int i;

    fprintf(out,
            "  \"policy\":{\"ok\":%s,\"flaggedAccounts\":%d,"
            "\"digest\":\"0x%016llx\",\"rules\":[",
            report->ok ? "true" : "false", report->flagged_accounts,
            (unsigned long long)report->digest);
    for (i = 0; i < book->rule_count; i++) {
        const LdtlPolicyRule *rule = &book->rules[i];
        uint64_t digest = policy_rule_digest(0, rule);
        if (i > 0) {
            fputc(',', out);
        }
        fprintf(out,
                "{\"riskClass\":%d,\"maxExposure\":%lld,"
                "\"maxWithdrawal\":%lld,\"maxAssetRiskBps\":%u,"
                "\"systemBypass\":%s,\"digest\":\"0x%016llx\"}",
                rule->risk_class, (long long)rule->max_exposure_units,
                (long long)rule->max_withdrawal_units,
                (unsigned int)rule->max_asset_risk_bps,
                rule->allow_system_bypass ? "true" : "false",
                (unsigned long long)digest);
    }
    fputs("],\"accounts\":[", out);
    for (i = 0; i < report->row_count; i++) {
        const LdtlPolicyAccountRow *row = &report->rows[i];
        const LdtlAccount *account = ldtl_ledger_account(ledger,
                                                         row->account_id);
        if (i > 0) {
            fputc(',', out);
        }
        fputs("{\"account\":", out);
        ldtl_json_string(out, account != NULL ? account->label : "unknown");
        fprintf(out,
                ",\"accountId\":%d,\"riskClass\":%d,"
                "\"exposure\":%lld,\"withdrawable\":%lld,"
                "\"maxExposure\":%lld,\"maxWithdrawal\":%lld,"
                "\"maxObservedAssetRiskBps\":%u,\"activeAssets\":%d,"
                "\"withinExposure\":%s}",
                row->account_id, row->risk_class,
                (long long)row->exposure_units,
                (long long)row->withdrawable_units,
                (long long)row->max_exposure_units,
                (long long)row->max_withdrawal_units,
                (unsigned int)row->max_observed_asset_risk_bps,
                row->active_assets,
                row->within_exposure ? "true" : "false");
    }
    fputs("],\"lastCheck\":", out);
    if (check == NULL || !check->used) {
        fputs("null", out);
    } else {
        const LdtlAccount *account = ldtl_ledger_account(ledger,
                                                         check->account_id);
        const LdtlAsset *asset = ldtl_asset_view(registry, check->asset_id);
        fputs("{\"action\":", out);
        ldtl_json_string(out, check->action);
        fputs(",\"account\":", out);
        ldtl_json_string(out, account != NULL ? account->label : "unknown");
        fputs(",\"asset\":", out);
        ldtl_json_string(out, asset != NULL ? asset->symbol : "unknown");
        fprintf(out, ",\"amount\":%lld,\"status\":\"%s\"}",
                (long long)check->amount, ldtl_status_name(check->status));
    }
    fputs("},\n", out);
}
