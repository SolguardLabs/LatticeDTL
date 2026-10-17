#include "treasury_stress.h"

#include <limits.h>
#include <string.h>

#define LDTL_BPS_DENOMINATOR 10000
#define LDTL_MAX_COVERAGE_BPS 100000

static int amount_nonnegative(LdtlAmount value) { return value >= 0; }

static int apply_bps_floor(LdtlAmount amount,
                           uint16_t bps,
                           LdtlAmount *out) {
    LdtlAmount whole;
    LdtlAmount remainder;

    if (out == NULL || amount < 0 || bps > 20000) {
        return 0;
    }
    whole = amount / LDTL_BPS_DENOMINATOR;
    remainder = amount % LDTL_BPS_DENOMINATOR;
    if (whole > INT64_MAX / (LdtlAmount)bps) {
        return 0;
    }
    *out = whole * (LdtlAmount)bps +
           (remainder * (LdtlAmount)bps) / LDTL_BPS_DENOMINATOR;
    return 1;
}

static int apply_bps_ceil(LdtlAmount amount,
                          uint16_t bps,
                          LdtlAmount *out) {
    LdtlAmount floor_value;
    LdtlAmount remainder;

    if (!apply_bps_floor(amount, bps, &floor_value)) {
        return 0;
    }
    remainder = amount % LDTL_BPS_DENOMINATOR;
    if (remainder != 0 &&
        (remainder * (LdtlAmount)bps) % LDTL_BPS_DENOMINATOR != 0) {
        if (floor_value == INT64_MAX) {
            return 0;
        }
        floor_value++;
    }
    *out = floor_value;
    return 1;
}

static int coverage_bps(LdtlAmount resources,
                        LdtlAmount uses,
                        uint32_t *out) {
    LdtlAmount quotient;
    LdtlAmount remainder;
    uint64_t value;

    if (out == NULL || resources < 0 || uses < 0) {
        return 0;
    }
    if (uses == 0) {
        *out = LDTL_MAX_COVERAGE_BPS;
        return 1;
    }
    quotient = resources / uses;
    remainder = resources % uses;
    if (quotient >= LDTL_MAX_COVERAGE_BPS / LDTL_BPS_DENOMINATOR) {
        *out = LDTL_MAX_COVERAGE_BPS;
        return 1;
    }
    value = (uint64_t)quotient * LDTL_BPS_DENOMINATOR;
    value += ((uint64_t)remainder * LDTL_BPS_DENOMINATOR) /
             (uint64_t)uses;
    if (value > LDTL_MAX_COVERAGE_BPS) {
        value = LDTL_MAX_COVERAGE_BPS;
    }
    *out = (uint32_t)value;
    return 1;
}

static LdtlStressLevel classify(const LdtlAssetStressRow *row,
                                uint16_t minimum_coverage_bps) {
    if (row->nominal_shortfall > 0) {
        return LDTL_STRESS_RED;
    }
    if (row->coverage_bps < minimum_coverage_bps) {
        return LDTL_STRESS_AMBER;
    }
    return LDTL_STRESS_GREEN;
}

static uint64_t stress_row_digest(uint64_t hash,
                                  const LdtlAssetStressRow *row) {
    hash = ldtl_hash_u64(hash, (uint64_t)row->asset_id);
    hash = ldtl_hash_u64(hash, (uint64_t)row->opening_reserve);
    hash = ldtl_hash_u64(hash, (uint64_t)row->eligible_liquidity);
    hash = ldtl_hash_u64(hash, (uint64_t)row->total_uses);
    hash = ldtl_hash_u64(hash, (uint64_t)row->nominal_shortfall);
    hash = ldtl_hash_u64(hash, (uint64_t)row->required_buffer);
    hash = ldtl_hash_u64(hash, (uint64_t)row->coverage_bps);
    hash = ldtl_hash_u64(hash, (uint64_t)row->level);
    return hash;
}

LdtlStressConfig ldtl_stress_config_operating(void) {
    LdtlStressConfig config;
    config.redemption_bps = 800;
    config.settlement_delay_bps = 1000;
    config.reserve_haircut_bps = 500;
    config.minimum_coverage_bps = 10500;
    return config;
}

LdtlStressConfig ldtl_stress_config_severe(void) {
    LdtlStressConfig config;
    config.redemption_bps = 2500;
    config.settlement_delay_bps = 3500;
    config.reserve_haircut_bps = 2000;
    config.minimum_coverage_bps = 11500;
    return config;
}

LdtlStatus ldtl_stress_config_validate(const LdtlStressConfig *config) {
    if (config == NULL || config->redemption_bps > 10000 ||
        config->settlement_delay_bps > 10000 ||
        config->reserve_haircut_bps > 10000 ||
        config->minimum_coverage_bps < 10000 ||
        config->minimum_coverage_bps > 20000) {
        return LDTL_ERR_INVALID;
    }
    return LDTL_OK;
}

LdtlStatus ldtl_treasury_stress_build(const LdtlLedger *ledger,
                                      const LdtlEpochResult *epoch,
                                      const LdtlStressConfig *config,
                                      LdtlTreasuryStress *report) {
    int asset_index;

    if (ledger == NULL || ledger->registry == NULL || epoch == NULL ||
        report == NULL || ldtl_stress_config_validate(config) != LDTL_OK) {
        return LDTL_ERR_INVALID;
    }
    memset(report, 0, sizeof(*report));
    report->config = *config;
    report->ok = 1;
    report->level = LDTL_STRESS_GREEN;

    for (asset_index = 0; asset_index < ledger->registry->asset_count;
         asset_index++) {
        const LdtlVault *vault = &ledger->vaults[asset_index];
        LdtlAssetStressRow *row = &report->rows[report->row_count++];
        LdtlAmount haircut;
        LdtlAmount required_at_floor;
        LdtlAmount next;

        row->asset_id = asset_index;
        row->opening_reserve = vault->reserve;
        row->reserve_floor = vault->reserve_floor;
        row->queued_outflow = vault->queued_outflow;
        row->available_reserve =
            vault->reserve > vault->reserve_floor
                ? vault->reserve - vault->reserve_floor
                : 0;

        if (!amount_nonnegative(vault->liability) ||
            !amount_nonnegative(vault->queued_outflow) ||
            !amount_nonnegative(epoch->volume_by_asset[asset_index]) ||
            !apply_bps_ceil(row->available_reserve,
                            config->reserve_haircut_bps, &haircut) ||
            !apply_bps_ceil(vault->liability, config->redemption_bps,
                            &row->stressed_redemptions) ||
            !apply_bps_ceil(epoch->volume_by_asset[asset_index],
                            config->settlement_delay_bps,
                            &row->delayed_settlement)) {
            report->ok = 0;
            return LDTL_ERR_OVERFLOW;
        }
        row->eligible_liquidity =
            haircut >= row->available_reserve
                ? 0
                : row->available_reserve - haircut;

        if (!ldtl_amount_add(row->stressed_redemptions,
                             row->delayed_settlement, &next) ||
            !ldtl_amount_add(next, row->queued_outflow, &row->total_uses) ||
            !coverage_bps(row->eligible_liquidity, row->total_uses,
                          &row->coverage_bps) ||
            !apply_bps_ceil(row->total_uses,
                            config->minimum_coverage_bps,
                            &required_at_floor)) {
            report->ok = 0;
            return LDTL_ERR_OVERFLOW;
        }
        row->nominal_shortfall =
            row->total_uses > row->eligible_liquidity
                ? row->total_uses - row->eligible_liquidity
                : 0;
        row->required_buffer =
            required_at_floor > row->eligible_liquidity
                ? required_at_floor - row->eligible_liquidity
                : 0;
        row->level = classify(row, config->minimum_coverage_bps);

        if (row->level == LDTL_STRESS_GREEN) {
            report->green_assets++;
        } else if (row->level == LDTL_STRESS_AMBER) {
            report->amber_assets++;
        } else {
            report->red_assets++;
        }
        if (row->level > report->level) {
            report->level = row->level;
        }
        if (!ldtl_amount_add(report->eligible_liquidity,
                             row->eligible_liquidity,
                             &report->eligible_liquidity) ||
            !ldtl_amount_add(report->total_uses, row->total_uses,
                             &report->total_uses) ||
            !ldtl_amount_add(report->nominal_shortfall,
                             row->nominal_shortfall,
                             &report->nominal_shortfall) ||
            !ldtl_amount_add(report->required_buffer, row->required_buffer,
                             &report->required_buffer)) {
            report->ok = 0;
            return LDTL_ERR_OVERFLOW;
        }
        report->digest = stress_row_digest(report->digest, row);
    }

    if (!coverage_bps(report->eligible_liquidity, report->total_uses,
                      &report->coverage_bps)) {
        report->ok = 0;
        return LDTL_ERR_OVERFLOW;
    }
    report->digest = ldtl_hash_u64(report->digest,
                                   (uint64_t)config->redemption_bps);
    report->digest = ldtl_hash_u64(report->digest,
                                   (uint64_t)config->settlement_delay_bps);
    report->digest = ldtl_hash_u64(report->digest,
                                   (uint64_t)config->reserve_haircut_bps);
    report->digest = ldtl_hash_u64(report->digest,
                                   (uint64_t)config->minimum_coverage_bps);
    return LDTL_OK;
}

const LdtlAssetStressRow *ldtl_treasury_stress_asset(
    const LdtlTreasuryStress *report,
    LdtlAssetId asset_id) {
    int index;
    if (report == NULL) {
        return NULL;
    }
    for (index = 0; index < report->row_count; index++) {
        if (report->rows[index].asset_id == asset_id) {
            return &report->rows[index];
        }
    }
    return NULL;
}

const char *ldtl_stress_level_name(LdtlStressLevel level) {
    switch (level) {
    case LDTL_STRESS_GREEN:
        return "green";
    case LDTL_STRESS_AMBER:
        return "amber";
    case LDTL_STRESS_RED:
        return "red";
    default:
        return "unknown";
    }
}

void ldtl_treasury_stress_print_json(const LdtlTreasuryStress *report,
                                     const LdtlAssetRegistry *registry,
                                     FILE *out) {
    int index;

    fprintf(out,
            "  \"treasuryStress\":{\"ok\":%s,\"level\":\"%s\"," 
            "\"coverageBps\":%u,\"eligibleLiquidity\":%lld,"
            "\"totalUses\":%lld,\"nominalShortfall\":%lld,"
            "\"requiredBuffer\":%lld,\"greenAssets\":%d,"
            "\"amberAssets\":%d,\"redAssets\":%d,"
            "\"digest\":\"0x%016llx\",\"config\":{"
            "\"redemptionBps\":%u,\"settlementDelayBps\":%u,"
            "\"reserveHaircutBps\":%u,\"minimumCoverageBps\":%u},"
            "\"assets\":[",
            report->ok ? "true" : "false",
            ldtl_stress_level_name(report->level), report->coverage_bps,
            (long long)report->eligible_liquidity,
            (long long)report->total_uses,
            (long long)report->nominal_shortfall,
            (long long)report->required_buffer, report->green_assets,
            report->amber_assets, report->red_assets,
            (unsigned long long)report->digest,
            (unsigned int)report->config.redemption_bps,
            (unsigned int)report->config.settlement_delay_bps,
            (unsigned int)report->config.reserve_haircut_bps,
            (unsigned int)report->config.minimum_coverage_bps);
    for (index = 0; index < report->row_count; index++) {
        const LdtlAssetStressRow *row = &report->rows[index];
        const LdtlAsset *asset = ldtl_asset_view(registry, row->asset_id);
        if (index > 0) {
            fputc(',', out);
        }
        fputs("{\"asset\":", out);
        ldtl_json_string(out, asset != NULL ? asset->symbol : "unknown");
        fprintf(out,
                ",\"assetId\":%d,\"level\":\"%s\","
                "\"openingReserve\":%lld,\"reserveFloor\":%lld,"
                "\"availableReserve\":%lld,\"eligibleLiquidity\":%lld,"
                "\"stressedRedemptions\":%lld,"
                "\"delayedSettlement\":%lld,\"queuedOutflow\":%lld,"
                "\"totalUses\":%lld,\"coverageBps\":%u,"
                "\"nominalShortfall\":%lld,\"requiredBuffer\":%lld}",
                row->asset_id, ldtl_stress_level_name(row->level),
                (long long)row->opening_reserve,
                (long long)row->reserve_floor,
                (long long)row->available_reserve,
                (long long)row->eligible_liquidity,
                (long long)row->stressed_redemptions,
                (long long)row->delayed_settlement,
                (long long)row->queued_outflow,
                (long long)row->total_uses, row->coverage_bps,
                (long long)row->nominal_shortfall,
                (long long)row->required_buffer);
    }
    fputs("]},\n", out);
}
