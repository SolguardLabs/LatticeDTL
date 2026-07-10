#ifndef LDTL_WITHDRAWALS_H
#define LDTL_WITHDRAWALS_H

#include "ledger.h"

typedef enum LdtlWithdrawalState {
    LDTL_WITHDRAWAL_EMPTY = 0,
    LDTL_WITHDRAWAL_EXECUTED = 1,
    LDTL_WITHDRAWAL_REJECTED = 2
} LdtlWithdrawalState;

typedef struct LdtlWithdrawal {
    int used;
    uint64_t id;
    LdtlWithdrawalState state;
    LdtlAccountId account_id;
    LdtlAssetId asset_id;
    LdtlAmount amount;
    int requested_epoch;
    int executed_epoch;
    char external_ref[LDTL_EXTERNAL_REF_LEN];
    LdtlStatus status;
} LdtlWithdrawal;

typedef struct LdtlWithdrawalQueue {
    LdtlWithdrawal items[LDTL_MAX_WITHDRAWALS];
    int count;
    uint64_t next_id;
} LdtlWithdrawalQueue;

void ldtl_withdrawals_init(LdtlWithdrawalQueue *queue);
LdtlStatus ldtl_withdrawals_execute(LdtlWithdrawalQueue *queue,
                                    LdtlLedger *ledger,
                                    LdtlAccountId account_id,
                                    LdtlAssetId asset_id,
                                    LdtlAmount amount,
                                    int epoch,
                                    const char *external_ref,
                                    uint64_t *out_id);
const LdtlWithdrawal *ldtl_withdrawals_get(const LdtlWithdrawalQueue *queue,
                                           uint64_t id);
void ldtl_withdrawals_print_json(const LdtlWithdrawalQueue *queue, FILE *out);

#endif
