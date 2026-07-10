#include "withdrawals.h"

#include <string.h>

void ldtl_withdrawals_init(LdtlWithdrawalQueue *queue) {
    memset(queue, 0, sizeof(*queue));
    queue->next_id = 1;
}

LdtlStatus ldtl_withdrawals_execute(LdtlWithdrawalQueue *queue,
                                    LdtlLedger *ledger,
                                    LdtlAccountId account_id,
                                    LdtlAssetId asset_id,
                                    LdtlAmount amount,
                                    int epoch,
                                    const char *external_ref,
                                    uint64_t *out_id) {
    LdtlWithdrawal *item;
    LdtlStatus status;

    if (queue == NULL || ledger == NULL || amount <= 0 || epoch < 0) {
        return LDTL_ERR_INVALID;
    }
    if (queue->count >= LDTL_MAX_WITHDRAWALS) {
        return LDTL_ERR_CAPACITY;
    }

    item = &queue->items[queue->count++];
    memset(item, 0, sizeof(*item));
    item->used = 1;
    item->id = queue->next_id++;
    item->account_id = account_id;
    item->asset_id = asset_id;
    item->amount = amount;
    item->requested_epoch = epoch;
    ldtl_copy_label(item->external_ref, sizeof(item->external_ref),
                    external_ref);

    status = ldtl_ledger_burn_for_withdrawal(ledger, account_id, asset_id,
                                             amount, epoch, "withdrawal");
    item->status = status;
    if (status == LDTL_OK) {
        item->state = LDTL_WITHDRAWAL_EXECUTED;
        item->executed_epoch = epoch;
    } else {
        item->state = LDTL_WITHDRAWAL_REJECTED;
        item->executed_epoch = 0;
    }
    if (out_id != NULL) {
        *out_id = item->id;
    }
    return status;
}

const LdtlWithdrawal *ldtl_withdrawals_get(const LdtlWithdrawalQueue *queue,
                                           uint64_t id) {
    int i;

    if (queue == NULL) {
        return NULL;
    }
    for (i = 0; i < queue->count; i++) {
        if (queue->items[i].used && queue->items[i].id == id) {
            return &queue->items[i];
        }
    }
    return NULL;
}

static const char *withdrawal_state_name(LdtlWithdrawalState state) {
    switch (state) {
    case LDTL_WITHDRAWAL_EXECUTED:
        return "executed";
    case LDTL_WITHDRAWAL_REJECTED:
        return "rejected";
    case LDTL_WITHDRAWAL_EMPTY:
    default:
        return "empty";
    }
}

void ldtl_withdrawals_print_json(const LdtlWithdrawalQueue *queue, FILE *out) {
    int i;

    fputs("  \"withdrawals\":[", out);
    for (i = 0; i < queue->count; i++) {
        const LdtlWithdrawal *item = &queue->items[i];
        if (i > 0) {
            fputc(',', out);
        }
        fprintf(out,
                "{\"id\":%llu,\"state\":\"%s\",\"status\":\"%s\","
                "\"accountId\":%d,\"assetId\":%d,\"amount\":%lld,"
                "\"requestedEpoch\":%d,\"executedEpoch\":%d,"
                "\"externalRef\":",
                (unsigned long long)item->id,
                withdrawal_state_name(item->state),
                ldtl_status_name(item->status), item->account_id,
                item->asset_id, (long long)item->amount,
                item->requested_epoch, item->executed_epoch);
        ldtl_json_string(out, item->external_ref);
        fputc('}', out);
    }
    fputs("],\n", out);
}
