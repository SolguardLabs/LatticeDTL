#include "ledger.h"

#include <string.h>

static LdtlStatus ensure_account_asset(const LdtlLedger *ledger,
                                       LdtlAccountId account_id,
                                       LdtlAssetId asset_id) {
    if (ledger == NULL || ldtl_ledger_account(ledger, account_id) == NULL ||
        !ldtl_asset_is_enabled(ledger->registry, asset_id)) {
        return LDTL_ERR_NOT_FOUND;
    }
    return LDTL_OK;
}

static void ledger_note_event(LdtlLedger *ledger,
                              LdtlEventKind kind,
                              int epoch,
                              LdtlAccountId account_a,
                              LdtlAccountId account_b,
                              LdtlAssetId asset_id,
                              LdtlAmount amount,
                              const char *note) {
    LdtlEvent *event;

    if (ledger->event_count >= LDTL_MAX_EVENTS) {
        return;
    }
    event = &ledger->events[ledger->event_count++];
    memset(event, 0, sizeof(*event));
    event->kind = kind;
    event->sequence = ledger->next_event++;
    event->epoch = epoch;
    event->account_a = account_a;
    event->account_b = account_b;
    event->asset_id = asset_id;
    event->amount = amount;
    ldtl_copy_label(event->note, sizeof(event->note), note);
}

void ldtl_ledger_init(LdtlLedger *ledger, const LdtlAssetRegistry *registry) {
    int i;

    memset(ledger, 0, sizeof(*ledger));
    ledger->registry = registry;
    ledger->next_event = 1;
    for (i = 0; registry != NULL && i < registry->asset_count; i++) {
        ledger->vaults[i].used = 1;
        ledger->vaults[i].asset_id = registry->assets[i].id;
        ledger->vaults[i].reserve_floor = registry->assets[i].reserve_floor;
    }
}

LdtlStatus ldtl_ledger_create_account(LdtlLedger *ledger,
                                       const char *label,
                                       LdtlDomainId domain_id,
                                       int system_account,
                                       int risk_class,
                                       LdtlAccountId *out_id) {
    LdtlAccount *account;

    if (ledger == NULL || label == NULL || risk_class < 0) {
        return LDTL_ERR_INVALID;
    }
    if (ldtl_ledger_find_account(ledger, label) >= 0) {
        return LDTL_ERR_DUPLICATE;
    }
    if (ledger->account_count >= LDTL_MAX_ACCOUNTS) {
        return LDTL_ERR_CAPACITY;
    }

    account = &ledger->accounts[ledger->account_count];
    memset(account, 0, sizeof(*account));
    account->used = 1;
    account->id = ledger->account_count;
    account->domain_id = domain_id;
    account->system_account = system_account ? 1 : 0;
    account->risk_class = risk_class;
    account->nonce = 1;
    ldtl_copy_label(account->label, sizeof(account->label), label);
    ledger->account_count++;
    ledger_note_event(ledger, LDTL_EVENT_ACCOUNT, 0, account->id, -1, -1, 0,
                      "account");

    if (out_id != NULL) {
        *out_id = account->id;
    }
    return LDTL_OK;
}

const LdtlAccount *ldtl_ledger_account(const LdtlLedger *ledger,
                                       LdtlAccountId account_id) {
    if (ledger == NULL || account_id < 0 || account_id >= ledger->account_count) {
        return NULL;
    }
    if (!ledger->accounts[account_id].used) {
        return NULL;
    }
    return &ledger->accounts[account_id];
}

LdtlAccountId ldtl_ledger_find_account(const LdtlLedger *ledger,
                                        const char *label) {
    int i;

    if (ledger == NULL || label == NULL) {
        return -1;
    }
    for (i = 0; i < ledger->account_count; i++) {
        if (ledger->accounts[i].used &&
            strcmp(ledger->accounts[i].label, label) == 0) {
            return ledger->accounts[i].id;
        }
    }
    return -1;
}

LdtlStatus ldtl_ledger_deposit(LdtlLedger *ledger,
                               LdtlAccountId account_id,
                               LdtlAssetId asset_id,
                               LdtlAmount amount,
                               const char *note) {
    LdtlAmount next;
    LdtlStatus status = ensure_account_asset(ledger, account_id, asset_id);

    if (status != LDTL_OK) {
        return status;
    }
    if (amount <= 0) {
        return LDTL_ERR_INVALID;
    }
    if (!ldtl_amount_add(ledger->balances[account_id][asset_id], amount,
                         &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    ledger->balances[account_id][asset_id] = next;
    if (!ldtl_amount_add(ledger->vaults[asset_id].reserve, amount, &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    ledger->vaults[asset_id].reserve = next;
    if (!ldtl_amount_add(ledger->vaults[asset_id].liability, amount, &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    ledger->vaults[asset_id].liability = next;
    ledger_note_event(ledger, LDTL_EVENT_DEPOSIT, 0, account_id, -1, asset_id,
                      amount, note);
    return LDTL_OK;
}

LdtlStatus ldtl_ledger_check_debit(const LdtlLedger *ledger,
                                   LdtlAccountId account_id,
                                   LdtlAssetId asset_id,
                                   LdtlAmount amount) {
    LdtlStatus status = ensure_account_asset(ledger, account_id, asset_id);

    if (status != LDTL_OK) {
        return status;
    }
    if (amount <= 0) {
        return LDTL_ERR_INVALID;
    }
    if (ledger->balances[account_id][asset_id] -
            ledger->locks[account_id][asset_id] <
        amount) {
        return LDTL_ERR_INSUFFICIENT_BALANCE;
    }
    return LDTL_OK;
}

LdtlStatus ldtl_ledger_transfer(LdtlLedger *ledger,
                                LdtlAccountId debtor,
                                LdtlAccountId creditor,
                                LdtlAssetId asset_id,
                                LdtlAmount amount,
                                int epoch,
                                const char *note) {
    LdtlStatus status;
    LdtlAmount next;

    if (debtor == creditor) {
        return LDTL_OK;
    }
    status = ldtl_ledger_check_debit(ledger, debtor, asset_id, amount);
    if (status != LDTL_OK) {
        return status;
    }
    status = ensure_account_asset(ledger, creditor, asset_id);
    if (status != LDTL_OK) {
        return status;
    }
    if (!ldtl_amount_sub(ledger->balances[debtor][asset_id], amount, &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    ledger->balances[debtor][asset_id] = next;
    if (!ldtl_amount_add(ledger->balances[creditor][asset_id], amount, &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    ledger->balances[creditor][asset_id] = next;
    ledger->accounts[debtor].nonce++;
    ledger->accounts[creditor].nonce++;
    ledger_note_event(ledger, LDTL_EVENT_TRANSFER, epoch, debtor, creditor,
                      asset_id, amount, note);
    return LDTL_OK;
}

LdtlStatus ldtl_ledger_burn_for_withdrawal(LdtlLedger *ledger,
                                           LdtlAccountId account_id,
                                           LdtlAssetId asset_id,
                                           LdtlAmount amount,
                                           int epoch,
                                           const char *note) {
    const LdtlAsset *asset;
    LdtlAmount next;
    LdtlStatus status = ldtl_ledger_check_debit(ledger, account_id, asset_id,
                                                amount);

    if (status != LDTL_OK) {
        return status;
    }
    asset = ldtl_asset_view(ledger->registry, asset_id);
    if (asset == NULL || !asset->withdraw_enabled) {
        return LDTL_ERR_DISABLED;
    }
    if (ledger->vaults[asset_id].reserve - amount <
        ledger->vaults[asset_id].reserve_floor) {
        return LDTL_ERR_INSUFFICIENT_RESERVE;
    }
    if (!ldtl_amount_sub(ledger->balances[account_id][asset_id], amount,
                         &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    ledger->balances[account_id][asset_id] = next;
    if (!ldtl_amount_sub(ledger->vaults[asset_id].reserve, amount, &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    ledger->vaults[asset_id].reserve = next;
    if (!ldtl_amount_sub(ledger->vaults[asset_id].liability, amount, &next)) {
        return LDTL_ERR_OVERFLOW;
    }
    ledger->vaults[asset_id].liability = next;
    ledger->accounts[account_id].nonce++;
    ledger_note_event(ledger, LDTL_EVENT_WITHDRAWAL, epoch, account_id, -1,
                      asset_id, amount, note);
    return LDTL_OK;
}

LdtlAmount ldtl_ledger_balance(const LdtlLedger *ledger,
                               LdtlAccountId account_id,
                               LdtlAssetId asset_id) {
    if (ledger == NULL || account_id < 0 || account_id >= LDTL_MAX_ACCOUNTS ||
        asset_id < 0 || asset_id >= LDTL_MAX_ASSETS) {
        return 0;
    }
    return ledger->balances[account_id][asset_id];
}

LdtlAmount ldtl_ledger_vault_reserve(const LdtlLedger *ledger,
                                     LdtlAssetId asset_id) {
    if (ledger == NULL || asset_id < 0 || asset_id >= LDTL_MAX_ASSETS) {
        return 0;
    }
    return ledger->vaults[asset_id].reserve;
}

LdtlAmount ldtl_ledger_vault_liability(const LdtlLedger *ledger,
                                       LdtlAssetId asset_id) {
    if (ledger == NULL || asset_id < 0 || asset_id >= LDTL_MAX_ASSETS) {
        return 0;
    }
    return ledger->vaults[asset_id].liability;
}

int ldtl_ledger_conserves(const LdtlLedger *ledger) {
    int asset;
    int account;

    if (ledger == NULL || ledger->registry == NULL) {
        return 0;
    }
    for (asset = 0; asset < ledger->registry->asset_count; asset++) {
        LdtlAmount sum = 0;
        for (account = 0; account < ledger->account_count; account++) {
            LdtlAmount next;
            if (!ldtl_amount_add(sum, ledger->balances[account][asset],
                                 &next)) {
                return 0;
            }
            sum = next;
        }
        if (sum != ledger->vaults[asset].liability) {
            return 0;
        }
        if (ledger->vaults[asset].reserve < 0 ||
            ledger->vaults[asset].liability < 0) {
            return 0;
        }
    }
    return 1;
}

uint64_t ldtl_ledger_digest(const LdtlLedger *ledger) {
    uint64_t hash = 0;
    int account;
    int asset;

    if (ledger == NULL) {
        return 0;
    }
    hash = ldtl_hash_u64(hash, (uint64_t)ledger->account_count);
    hash = ldtl_hash_u64(hash, (uint64_t)ledger->event_count);
    for (account = 0; account < ledger->account_count; account++) {
        const LdtlAccount *acct = &ledger->accounts[account];
        hash = ldtl_hash_u64(hash, (uint64_t)acct->id);
        hash = ldtl_hash_u64(hash, (uint64_t)acct->domain_id);
        hash = ldtl_hash_u64(hash, acct->nonce);
        hash = ldtl_hash_bytes(hash, acct->label, strlen(acct->label));
        for (asset = 0; asset < ledger->registry->asset_count; asset++) {
            hash = ldtl_hash_u64(hash,
                                 (uint64_t)ledger->balances[account][asset]);
        }
    }
    for (asset = 0; asset < ledger->registry->asset_count; asset++) {
        hash = ldtl_hash_u64(hash, (uint64_t)ledger->vaults[asset].reserve);
        hash = ldtl_hash_u64(hash, (uint64_t)ledger->vaults[asset].liability);
    }
    return hash;
}

void ldtl_ledger_print_accounts_json(const LdtlLedger *ledger, FILE *out) {
    int i;

    fputs("  \"accounts\":[", out);
    for (i = 0; i < ledger->account_count; i++) {
        const LdtlAccount *acct = &ledger->accounts[i];
        if (i > 0) {
            fputc(',', out);
        }
        fprintf(out, "{\"id\":%d,\"label\":", acct->id);
        ldtl_json_string(out, acct->label);
        fprintf(out,
                ",\"domainId\":%d,\"system\":%s,\"riskClass\":%d,"
                "\"nonce\":%llu}",
                acct->domain_id, acct->system_account ? "true" : "false",
                acct->risk_class, (unsigned long long)acct->nonce);
    }
    fputs("],\n", out);
}

void ldtl_ledger_print_balances_json(const LdtlLedger *ledger, FILE *out) {
    int account;
    int asset;
    int first = 1;

    fputs("  \"balances\":[", out);
    for (account = 0; account < ledger->account_count; account++) {
        for (asset = 0; asset < ledger->registry->asset_count; asset++) {
            if (ledger->balances[account][asset] == 0) {
                continue;
            }
            if (!first) {
                fputc(',', out);
            }
            first = 0;
            fprintf(out, "{\"account\":");
            ldtl_json_string(out, ledger->accounts[account].label);
            fprintf(out, ",\"asset\":");
            ldtl_json_string(out, ledger->registry->assets[asset].symbol);
            fprintf(out, ",\"amount\":%lld}", 
                    (long long)ledger->balances[account][asset]);
        }
    }
    fputs("],\n", out);
}

void ldtl_ledger_print_vaults_json(const LdtlLedger *ledger, FILE *out) {
    int asset;

    fputs("  \"vaults\":[", out);
    for (asset = 0; asset < ledger->registry->asset_count; asset++) {
        const LdtlAsset *info = &ledger->registry->assets[asset];
        const LdtlVault *vault = &ledger->vaults[asset];
        if (asset > 0) {
            fputc(',', out);
        }
        fprintf(out, "{\"asset\":");
        ldtl_json_string(out, info->symbol);
        fprintf(out,
                ",\"reserve\":%lld,\"liability\":%lld,"
                "\"reserveFloor\":%lld,\"queuedOutflow\":%lld}",
                (long long)vault->reserve, (long long)vault->liability,
                (long long)vault->reserve_floor,
                (long long)vault->queued_outflow);
    }
    fputs("],\n", out);
}
