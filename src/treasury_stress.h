#ifndef LDTL_TREASURY_STRESS_H
#define LDTL_TREASURY_STRESS_H

#include "epoch_netting.h"

typedef enum LdtlStressLevel {
    LDTL_STRESS_GREEN = 0,
    LDTL_STRESS_AMBER = 1,
    LDTL_STRESS_RED = 2
} LdtlStressLevel;

typedef struct LdtlStressConfig {
    uint16_t redemption_bps;
    uint16_t settlement_delay_bps;
    uint16_t reserve_haircut_bps;
    uint16_t minimum_coverage_bps;
} LdtlStressConfig;

typedef struct LdtlAssetStressRow {
    LdtlAssetId asset_id;
    LdtlAmount opening_reserve;
    LdtlAmount reserve_floor;
    LdtlAmount available_reserve;
    LdtlAmount eligible_liquidity;
    LdtlAmount stressed_redemptions;
    LdtlAmount delayed_settlement;
    LdtlAmount queued_outflow;
    LdtlAmount total_uses;
    LdtlAmount nominal_shortfall;
    LdtlAmount required_buffer;
    uint32_t coverage_bps;
    LdtlStressLevel level;
} LdtlAssetStressRow;

typedef struct LdtlTreasuryStress {
    LdtlStressConfig config;
    LdtlAssetStressRow rows[LDTL_MAX_ASSETS];
    int row_count;
    int green_assets;
    int amber_assets;
    int red_assets;
    LdtlAmount eligible_liquidity;
    LdtlAmount total_uses;
    LdtlAmount nominal_shortfall;
    LdtlAmount required_buffer;
    uint32_t coverage_bps;
    LdtlStressLevel level;
    uint64_t digest;
    int ok;
} LdtlTreasuryStress;

LdtlStressConfig ldtl_stress_config_operating(void);
LdtlStressConfig ldtl_stress_config_severe(void);
LdtlStatus ldtl_stress_config_validate(const LdtlStressConfig *config);
LdtlStatus ldtl_treasury_stress_build(const LdtlLedger *ledger,
                                      const LdtlEpochResult *epoch,
                                      const LdtlStressConfig *config,
                                      LdtlTreasuryStress *report);
const LdtlAssetStressRow *ldtl_treasury_stress_asset(
    const LdtlTreasuryStress *report,
    LdtlAssetId asset_id);
const char *ldtl_stress_level_name(LdtlStressLevel level);
void ldtl_treasury_stress_print_json(const LdtlTreasuryStress *report,
                                     const LdtlAssetRegistry *registry,
                                     FILE *out);

#endif
