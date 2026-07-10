#include "runtime.h"
#include "reconciliation.h"

#include <stdio.h>
#include <string.h>

static LdtlStatus expect_ok(LdtlStatus status) {
    return status == LDTL_OK ? LDTL_OK : status;
}

void ldtl_runtime_init(LdtlRuntime *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    ldtl_asset_registry_init(&runtime->registry);
    ldtl_withdrawals_init(&runtime->withdrawals);
    ldtl_matrix_init(&runtime->matrix, 5000);
    ldtl_policy_book_init_defaults(&runtime->policy);
    ldtl_policy_check_clear(&runtime->policy_check);
    runtime->current_epoch = 1;
}

static LdtlStatus register_assets(LdtlRuntime *runtime) {
    LdtlFixtureIds *ids = &runtime->ids;
    LdtlStatus status;

    status = ldtl_asset_register(&runtime->registry, 10, "usd-liquidity",
                                 "USDC", 6, LDTL_ASSET_STABLE, 1000,
                                 100000, 1, 1, &ids->usdc);
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_asset_register(&runtime->registry, 20, "eur-liquidity",
                                 "EURC", 6, LDTL_ASSET_STABLE, 1050,
                                 75000, 1, 1, &ids->eurc);
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_asset_register(&runtime->registry, 10, "usd-liquidity",
                                 "WBTC", 8, LDTL_ASSET_VOLATILE, 3400,
                                 1000, 1, 0, &ids->wbtc);
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_asset_register(&runtime->registry, 10, "usd-liquidity",
                                 "LATT", 0, LDTL_ASSET_SYNTHETIC, 9200, 0, 0,
                                 0, &ids->latt);
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_asset_register(&runtime->registry, 30, "latam-liquidity",
                                 "MXNT", 6, LDTL_ASSET_STABLE, 1300,
                                 125000, 1, 1, &ids->mxnt);
    return status;
}

static LdtlStatus create_accounts(LdtlRuntime *runtime) {
    LdtlFixtureIds *ids = &runtime->ids;
    LdtlStatus status;

    status = ldtl_ledger_create_account(&runtime->ledger, "reserve_pool", 10,
                                        1, 0, &ids->reserve_pool);
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_create_account(&runtime->ledger, "north_mm", 10, 0, 1,
                                        &ids->north_mm);
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_create_account(&runtime->ledger, "south_mm", 10, 0, 1,
                                        &ids->south_mm);
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_create_account(&runtime->ledger, "bridge_eu", 20, 0,
                                        2, &ids->bridge_eu);
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_create_account(&runtime->ledger, "client_alpha", 10,
                                        0, 3, &ids->client_alpha);
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_create_account(&runtime->ledger, "rewards_bank", 10,
                                        1, 4, &ids->rewards_bank);
    return status;
}

static LdtlStatus seed_balances(LdtlRuntime *runtime) {
    LdtlFixtureIds *id = &runtime->ids;
    LdtlLedger *ledger = &runtime->ledger;
    LdtlStatus status;

    status = ldtl_ledger_deposit(ledger, id->reserve_pool, id->usdc,
                                 10000000000LL, "genesis");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->reserve_pool, id->eurc,
                                 5000000000LL, "genesis");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->reserve_pool, id->wbtc, 200000000,
                                 "genesis");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->reserve_pool, id->latt, 250000000,
                                 "genesis");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->reserve_pool, id->mxnt,
                                 4000000000LL, "genesis");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->north_mm, id->usdc, 250000000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->north_mm, id->eurc, 40000000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->north_mm, id->latt, 20000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->south_mm, id->usdc, 160000000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->south_mm, id->eurc, 120000000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->south_mm, id->latt, 40000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->bridge_eu, id->eurc, 700000000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->bridge_eu, id->mxnt, 900000000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->client_alpha, id->usdc, 70000000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->client_alpha, id->eurc, 50000000,
                                 "participant");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_ledger_deposit(ledger, id->rewards_bank, id->latt, 500000000,
                                 "participant");
    return status;
}

LdtlStatus ldtl_runtime_fixture(LdtlRuntime *runtime) {
    LdtlStatus status;

    ldtl_runtime_init(runtime);
    status = register_assets(runtime);
    if (status != LDTL_OK) {
        return status;
    }
    ldtl_ledger_init(&runtime->ledger, &runtime->registry);
    status = create_accounts(runtime);
    if (status != LDTL_OK) {
        return status;
    }
    return seed_balances(runtime);
}

static void print_report(const LdtlRuntime *runtime,
                         const char *scenario,
                         const LdtlEpochResult *epoch,
                         FILE *out) {
    int ok = epoch->status == LDTL_OK && ldtl_ledger_conserves(&runtime->ledger);
    LdtlReconciliation reconciliation;
    LdtlPolicyReport policy_report;

    ldtl_reconciliation_build(&runtime->ledger, &reconciliation);
    ldtl_policy_assess_ledger(&runtime->ledger, &runtime->policy,
                              &policy_report);

    fputs("{\n", out);
    fputs("  \"lab\":\"LatticeDTL\",\n", out);
    fprintf(out, "  \"scenario\":");
    ldtl_json_string(out, scenario);
    fputs(",\n", out);
    fprintf(out, "  \"ok\":%s,\n", ok ? "true" : "false");
    ldtl_asset_registry_print_json(&runtime->registry, out, 2);
    ldtl_ledger_print_accounts_json(&runtime->ledger, out);
    ldtl_ledger_print_balances_json(&runtime->ledger, out);
    ldtl_ledger_print_vaults_json(&runtime->ledger, out);
    ldtl_reconciliation_print_json(&reconciliation, &runtime->registry, out);
    ldtl_policy_print_json(&policy_report, &runtime->policy,
                           &runtime->policy_check, &runtime->ledger,
                           &runtime->registry, out);
    ldtl_matrix_print_json(&runtime->matrix, out);
    ldtl_epoch_result_print_json(epoch, out);
    ldtl_withdrawals_print_json(&runtime->withdrawals, out);
    fprintf(out, "  \"conservation\":%s,\n",
            ldtl_ledger_conserves(&runtime->ledger) ? "true" : "false");
    fprintf(out, "  \"registryDigest\":\"0x%016llx\",\n",
            (unsigned long long)ldtl_asset_registry_digest(&runtime->registry));
    fprintf(out, "  \"ledgerDigest\":\"0x%016llx\"\n",
            (unsigned long long)ldtl_ledger_digest(&runtime->ledger));
    fputs("}\n", out);
}

static LdtlStatus add_baseline_debts(LdtlRuntime *runtime) {
    LdtlFixtureIds *id = &runtime->ids;
    LdtlMatrix *m = &runtime->matrix;
    LdtlStatus status;

    status = ldtl_matrix_add_debt(m, id->north_mm, id->south_mm, id->usdc, 10,
                                  25000000, 7, "rfq-us");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_matrix_add_debt(m, id->south_mm, id->north_mm, id->usdc, 10,
                                  7000000, 5, "rfq-us");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_matrix_add_debt(m, id->client_alpha, id->bridge_eu, id->eurc,
                                  20, 12000000, 4, "eur-route");
    if (status != LDTL_OK) {
        return status;
    }
    return ldtl_matrix_add_debt(m, id->bridge_eu, id->client_alpha, id->mxnt,
                                30, 18000000, 4, "mx-route");
}

static LdtlStatus add_compact_debts(LdtlRuntime *runtime) {
    LdtlFixtureIds *id = &runtime->ids;
    LdtlMatrix *m = &runtime->matrix;
    LdtlStatus status;

    status = ldtl_matrix_add_debt(m, id->north_mm, id->south_mm, id->usdc, 10,
                                  3200, 1, "micro-a");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_matrix_add_debt(m, id->south_mm, id->north_mm, id->usdc, 10,
                                  700, 1, "micro-b");
    if (status != LDTL_OK) {
        return status;
    }
    return ldtl_matrix_add_debt(m, id->south_mm, id->north_mm, id->usdc, 10,
                                200, 1, "micro-c");
}

static LdtlStatus add_domain_debts(LdtlRuntime *runtime) {
    LdtlFixtureIds *id = &runtime->ids;
    LdtlMatrix *m = &runtime->matrix;
    LdtlStatus status;

    status = ldtl_matrix_add_debt(m, id->north_mm, id->south_mm, id->usdc, 10,
                                  9000000, 3, "usd-lane");
    if (status != LDTL_OK) {
        return status;
    }
    return ldtl_matrix_add_debt(m, id->south_mm, id->north_mm, id->latt, 10,
                                9000, 3, "memo-lane");
}

static LdtlStatus add_batch_debts(LdtlRuntime *runtime) {
    LdtlFixtureIds *id = &runtime->ids;
    LdtlMatrix *m = &runtime->matrix;
    LdtlStatus status;

    status = ldtl_matrix_add_debt(m, id->north_mm, id->south_mm, id->usdc, 10,
                                  8000000, 5, "batch-us-a");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_matrix_add_debt(m, id->south_mm, id->north_mm, id->usdc, 10,
                                  2000000, 5, "batch-us-b");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_matrix_add_debt(m, id->client_alpha, id->north_mm, id->eurc,
                                  20, 3000000, 4, "batch-eu-a");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_matrix_add_debt(m, id->bridge_eu, id->south_mm, id->eurc, 20,
                                  4000000, 4, "batch-eu-b");
    if (status != LDTL_OK) {
        return status;
    }
    status = ldtl_matrix_add_debt(m, id->north_mm, id->client_alpha, id->usdc,
                                  10, 1000, 1, "batch-micro-a");
    if (status != LDTL_OK) {
        return status;
    }
    return ldtl_matrix_add_debt(m, id->client_alpha, id->north_mm, id->usdc, 10,
                                400, 1, "batch-micro-b");
}

static int run_settlement_scenario(const char *name,
                                   LdtlStatus (*load)(LdtlRuntime *),
                                   FILE *out) {
    LdtlRuntime runtime;
    LdtlEpochResult result;
    LdtlStatus status;

    status = ldtl_runtime_fixture(&runtime);
    if (status != LDTL_OK) {
        return (int)status;
    }
    status = load(&runtime);
    if (status != LDTL_OK) {
        return (int)status;
    }
    status = ldtl_epoch_settle(&runtime.ledger, &runtime.matrix,
                               runtime.current_epoch, &result);
    if (status != LDTL_OK) {
        result.status = status;
    }
    print_report(&runtime, name, &result, out);
    return status == LDTL_OK ? 0 : (int)status;
}

static int run_withdrawal(FILE *out) {
    LdtlRuntime runtime;
    LdtlEpochResult result;
    LdtlStatus status;
    uint64_t withdrawal_id = 0;

    status = ldtl_runtime_fixture(&runtime);
    if (status != LDTL_OK) {
        return (int)status;
    }
    status = ldtl_epoch_settle(&runtime.ledger, &runtime.matrix,
                               runtime.current_epoch, &result);
    if (status != LDTL_OK) {
        result.status = status;
    }
    status = ldtl_withdrawals_execute(
        &runtime.withdrawals, &runtime.ledger, runtime.ids.client_alpha,
        runtime.ids.usdc, 15000000, runtime.current_epoch, "wire-1042",
        &withdrawal_id);
    (void)withdrawal_id;
    if (status != LDTL_OK) {
        result.status = status;
    }
    print_report(&runtime, "withdrawal", &result, out);
    return status == LDTL_OK ? 0 : (int)status;
}

static int run_snapshot(FILE *out) {
    LdtlRuntime runtime;
    LdtlEpochResult result;
    LdtlStatus status;

    status = ldtl_runtime_fixture(&runtime);
    if (status != LDTL_OK) {
        return (int)status;
    }
    status = ldtl_matrix_materialize(&runtime.matrix, &runtime.registry);
    if (status != LDTL_OK) {
        return (int)status;
    }
    ldtl_epoch_result_init(&result, 0);
    result.ledger_digest_before = ldtl_ledger_digest(&runtime.ledger);
    result.ledger_digest_after = result.ledger_digest_before;
    result.matrix_digest = runtime.matrix.last_digest;
    print_report(&runtime, "snapshot", &result, out);
    return 0;
}

static int run_policy(FILE *out) {
    LdtlRuntime runtime;
    LdtlEpochResult result;
    LdtlStatus status;

    status = ldtl_runtime_fixture(&runtime);
    if (status != LDTL_OK) {
        return (int)status;
    }
    status = ldtl_matrix_materialize(&runtime.matrix, &runtime.registry);
    if (status != LDTL_OK) {
        return (int)status;
    }
    ldtl_epoch_result_init(&result, 0);
    result.ledger_digest_before = ldtl_ledger_digest(&runtime.ledger);
    result.ledger_digest_after = result.ledger_digest_before;
    result.matrix_digest = runtime.matrix.last_digest;
    (void)ldtl_policy_validate_withdrawal(
        &runtime.ledger, &runtime.policy, runtime.ids.client_alpha,
        runtime.ids.usdc, 35000000, &runtime.policy_check);
    print_report(&runtime, "policy", &result, out);
    return 0;
}

static int run_batch(FILE *out) {
    LdtlRuntime runtime;
    LdtlEpochResult result;
    LdtlStatus status;

    status = ldtl_runtime_fixture(&runtime);
    if (status != LDTL_OK) {
        return (int)status;
    }
    status = add_batch_debts(&runtime);
    if (status != LDTL_OK) {
        return (int)status;
    }
    status = ldtl_epoch_settle(&runtime.ledger, &runtime.matrix,
                               runtime.current_epoch, &result);
    if (status == LDTL_OK) {
        status = ldtl_withdrawals_execute(
            &runtime.withdrawals, &runtime.ledger, runtime.ids.south_mm,
            runtime.ids.usdc, 1000000, runtime.current_epoch, "wire-2048",
            NULL);
    }
    if (status != LDTL_OK) {
        result.status = status;
    }
    print_report(&runtime, "batch", &result, out);
    return status == LDTL_OK ? 0 : (int)status;
}

int ldtl_runtime_run_scenario(const char *scenario, FILE *out) {
    if (scenario == NULL || strcmp(scenario, "baseline") == 0) {
        return run_settlement_scenario("baseline", add_baseline_debts, out);
    }
    if (strcmp(scenario, "compact") == 0) {
        return run_settlement_scenario("compact", add_compact_debts, out);
    }
    if (strcmp(scenario, "domains") == 0) {
        return run_settlement_scenario("domains", add_domain_debts, out);
    }
    if (strcmp(scenario, "withdrawal") == 0) {
        return run_withdrawal(out);
    }
    if (strcmp(scenario, "batch") == 0) {
        return run_batch(out);
    }
    if (strcmp(scenario, "snapshot") == 0) {
        return run_snapshot(out);
    }
    if (strcmp(scenario, "policy") == 0) {
        return run_policy(out);
    }
    return (int)expect_ok(LDTL_ERR_NOT_FOUND);
}

void ldtl_runtime_print_help(FILE *out) {
    fputs("LatticeDTL scenario runner\n", out);
    fputs("\n", out);
    fputs("Usage:\n", out);
    fputs("  lattice-dtl [scenario]\n", out);
    fputs("\n", out);
    fputs("Scenarios:\n", out);
    fputs("  baseline    multi-party epoch netting report\n", out);
    fputs("  compact     small-position compaction report\n", out);
    fputs("  domains     asset-domain isolation report\n", out);
    fputs("  withdrawal  direct vault withdrawal report\n", out);
    fputs("  batch       mixed epoch and withdrawal report\n", out);
    fputs("  snapshot    fixture metadata report\n", out);
    fputs("  policy      operational policy report\n", out);
}
