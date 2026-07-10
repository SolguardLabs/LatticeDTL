#ifndef LDTL_ASSET_REGISTRY_H
#define LDTL_ASSET_REGISTRY_H

#include "ldtl_common.h"

typedef enum LdtlAssetKind {
    LDTL_ASSET_STABLE = 1,
    LDTL_ASSET_VOLATILE = 2,
    LDTL_ASSET_SYNTHETIC = 3,
    LDTL_ASSET_MEMO = 4
} LdtlAssetKind;

typedef struct LdtlAsset {
    int used;
    LdtlAssetId id;
    LdtlDomainId domain_id;
    char symbol[LDTL_SYMBOL_LEN];
    char domain[LDTL_DOMAIN_LEN];
    uint8_t decimals;
    uint16_t risk_weight_bps;
    LdtlAmount reserve_floor;
    LdtlAssetKind kind;
    int withdraw_enabled;
    int lane_primary;
    int enabled;
} LdtlAsset;

typedef struct LdtlAssetRegistry {
    LdtlAsset assets[LDTL_MAX_ASSETS];
    int asset_count;
    uint64_t version;
} LdtlAssetRegistry;

void ldtl_asset_registry_init(LdtlAssetRegistry *registry);
LdtlStatus ldtl_asset_register(LdtlAssetRegistry *registry,
                               LdtlDomainId domain_id,
                               const char *domain,
                               const char *symbol,
                               uint8_t decimals,
                               LdtlAssetKind kind,
                               uint16_t risk_weight_bps,
                               LdtlAmount reserve_floor,
                               int withdraw_enabled,
                               int lane_primary,
                               LdtlAssetId *out_id);
LdtlAsset *ldtl_asset_get(LdtlAssetRegistry *registry, LdtlAssetId asset_id);
const LdtlAsset *ldtl_asset_view(const LdtlAssetRegistry *registry,
                                 LdtlAssetId asset_id);
LdtlAssetId ldtl_asset_find_symbol(const LdtlAssetRegistry *registry,
                                   const char *symbol);
LdtlAssetId ldtl_asset_primary_for_domain(const LdtlAssetRegistry *registry,
                                          LdtlDomainId domain_id);
int ldtl_asset_is_enabled(const LdtlAssetRegistry *registry,
                          LdtlAssetId asset_id);
uint64_t ldtl_asset_registry_digest(const LdtlAssetRegistry *registry);
void ldtl_asset_registry_print_json(const LdtlAssetRegistry *registry,
                                    FILE *out,
                                    int indent);

#endif
