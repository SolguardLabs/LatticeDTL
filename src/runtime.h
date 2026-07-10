#ifndef LDTL_RUNTIME_H
#define LDTL_RUNTIME_H

#include "epoch_netting.h"
#include "policy.h"
#include "withdrawals.h"

typedef struct LdtlFixtureIds {
    LdtlAssetId usdc;
    LdtlAssetId eurc;
    LdtlAssetId wbtc;
    LdtlAssetId latt;
    LdtlAssetId mxnt;
    LdtlAccountId reserve_pool;
    LdtlAccountId north_mm;
    LdtlAccountId south_mm;
    LdtlAccountId bridge_eu;
    LdtlAccountId client_alpha;
    LdtlAccountId rewards_bank;
} LdtlFixtureIds;

typedef struct LdtlRuntime {
    LdtlAssetRegistry registry;
    LdtlLedger ledger;
    LdtlMatrix matrix;
    LdtlPolicyBook policy;
    LdtlPolicyCheck policy_check;
    LdtlWithdrawalQueue withdrawals;
    LdtlFixtureIds ids;
    int current_epoch;
} LdtlRuntime;

void ldtl_runtime_init(LdtlRuntime *runtime);
LdtlStatus ldtl_runtime_fixture(LdtlRuntime *runtime);
int ldtl_runtime_run_scenario(const char *scenario, FILE *out);
void ldtl_runtime_print_help(FILE *out);

#endif
