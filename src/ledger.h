#ifndef LDTL_LEDGER_H
#define LDTL_LEDGER_H

#include "asset_registry.h"
#include "ldtl_common.h"

typedef enum LdtlEventKind {
    LDTL_EVENT_DEPOSIT = 1,
    LDTL_EVENT_TRANSFER = 2,
    LDTL_EVENT_WITHDRAWAL = 3,
    LDTL_EVENT_EPOCH = 4,
    LDTL_EVENT_ACCOUNT = 5
} LdtlEventKind;

typedef struct LdtlAccount {
    int used;
    LdtlAccountId id;
    LdtlDomainId domain_id;
    char label[LDTL_LABEL_LEN];
    int system_account;
    int risk_class;
    uint64_t nonce;
} LdtlAccount;

typedef struct LdtlVault {
    int used;
    LdtlAssetId asset_id;
    LdtlAmount reserve;
    LdtlAmount liability;
    LdtlAmount queued_outflow;
    LdtlAmount reserve_floor;
} LdtlVault;

typedef struct LdtlEvent {
    LdtlEventKind kind;
    uint64_t sequence;
    int epoch;
    LdtlAccountId account_a;
    LdtlAccountId account_b;
    LdtlAssetId asset_id;
    LdtlAmount amount;
    char note[LDTL_LABEL_LEN];
} LdtlEvent;

typedef struct LdtlLedger {
    const LdtlAssetRegistry *registry;
    LdtlAccount accounts[LDTL_MAX_ACCOUNTS];
    LdtlVault vaults[LDTL_MAX_ASSETS];
    LdtlAmount balances[LDTL_MAX_ACCOUNTS][LDTL_MAX_ASSETS];
    LdtlAmount locks[LDTL_MAX_ACCOUNTS][LDTL_MAX_ASSETS];
    int account_count;
    uint64_t next_event;
    LdtlEvent events[LDTL_MAX_EVENTS];
    int event_count;
} LdtlLedger;

void ldtl_ledger_init(LdtlLedger *ledger, const LdtlAssetRegistry *registry);
LdtlStatus ldtl_ledger_create_account(LdtlLedger *ledger,
                                       const char *label,
                                       LdtlDomainId domain_id,
                                       int system_account,
                                       int risk_class,
                                       LdtlAccountId *out_id);
const LdtlAccount *ldtl_ledger_account(const LdtlLedger *ledger,
                                       LdtlAccountId account_id);
LdtlAccountId ldtl_ledger_find_account(const LdtlLedger *ledger,
                                        const char *label);
LdtlStatus ldtl_ledger_deposit(LdtlLedger *ledger,
                               LdtlAccountId account_id,
                               LdtlAssetId asset_id,
                               LdtlAmount amount,
                               const char *note);
LdtlStatus ldtl_ledger_transfer(LdtlLedger *ledger,
                                LdtlAccountId debtor,
                                LdtlAccountId creditor,
                                LdtlAssetId asset_id,
                                LdtlAmount amount,
                                int epoch,
                                const char *note);
LdtlStatus ldtl_ledger_burn_for_withdrawal(LdtlLedger *ledger,
                                           LdtlAccountId account_id,
                                           LdtlAssetId asset_id,
                                           LdtlAmount amount,
                                           int epoch,
                                           const char *note);
LdtlAmount ldtl_ledger_balance(const LdtlLedger *ledger,
                               LdtlAccountId account_id,
                               LdtlAssetId asset_id);
LdtlAmount ldtl_ledger_vault_reserve(const LdtlLedger *ledger,
                                     LdtlAssetId asset_id);
LdtlAmount ldtl_ledger_vault_liability(const LdtlLedger *ledger,
                                       LdtlAssetId asset_id);
LdtlStatus ldtl_ledger_check_debit(const LdtlLedger *ledger,
                                   LdtlAccountId account_id,
                                   LdtlAssetId asset_id,
                                   LdtlAmount amount);
int ldtl_ledger_conserves(const LdtlLedger *ledger);
uint64_t ldtl_ledger_digest(const LdtlLedger *ledger);
void ldtl_ledger_print_accounts_json(const LdtlLedger *ledger, FILE *out);
void ldtl_ledger_print_balances_json(const LdtlLedger *ledger, FILE *out);
void ldtl_ledger_print_vaults_json(const LdtlLedger *ledger, FILE *out);

#endif
