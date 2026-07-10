#include "asset_registry.h"

#include <string.h>

void ldtl_asset_registry_init(LdtlAssetRegistry *registry) {
    memset(registry, 0, sizeof(*registry));
    registry->version = 1;
}

static const char *asset_kind_name(LdtlAssetKind kind) {
    switch (kind) {
    case LDTL_ASSET_STABLE:
        return "stable";
    case LDTL_ASSET_VOLATILE:
        return "volatile";
    case LDTL_ASSET_SYNTHETIC:
        return "synthetic";
    case LDTL_ASSET_MEMO:
        return "memo";
    default:
        return "unknown";
    }
}

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
                               LdtlAssetId *out_id) {
    LdtlAsset *asset;

    if (registry == NULL || symbol == NULL || domain == NULL || decimals > 18 ||
        risk_weight_bps > 10000 || reserve_floor < 0) {
        return LDTL_ERR_INVALID;
    }
    if (ldtl_asset_find_symbol(registry, symbol) >= 0) {
        return LDTL_ERR_DUPLICATE;
    }
    if (registry->asset_count >= LDTL_MAX_ASSETS) {
        return LDTL_ERR_CAPACITY;
    }

    asset = &registry->assets[registry->asset_count];
    memset(asset, 0, sizeof(*asset));
    asset->used = 1;
    asset->id = registry->asset_count;
    asset->domain_id = domain_id;
    asset->decimals = decimals;
    asset->kind = kind;
    asset->risk_weight_bps = risk_weight_bps;
    asset->reserve_floor = reserve_floor;
    asset->withdraw_enabled = withdraw_enabled ? 1 : 0;
    asset->lane_primary = lane_primary ? 1 : 0;
    asset->enabled = 1;
    ldtl_copy_label(asset->symbol, sizeof(asset->symbol), symbol);
    ldtl_copy_label(asset->domain, sizeof(asset->domain), domain);
    registry->asset_count++;
    registry->version++;

    if (out_id != NULL) {
        *out_id = asset->id;
    }
    return LDTL_OK;
}

LdtlAsset *ldtl_asset_get(LdtlAssetRegistry *registry, LdtlAssetId asset_id) {
    if (registry == NULL || asset_id < 0 || asset_id >= registry->asset_count) {
        return NULL;
    }
    if (!registry->assets[asset_id].used) {
        return NULL;
    }
    return &registry->assets[asset_id];
}

const LdtlAsset *ldtl_asset_view(const LdtlAssetRegistry *registry,
                                 LdtlAssetId asset_id) {
    if (registry == NULL || asset_id < 0 || asset_id >= registry->asset_count) {
        return NULL;
    }
    if (!registry->assets[asset_id].used) {
        return NULL;
    }
    return &registry->assets[asset_id];
}

LdtlAssetId ldtl_asset_find_symbol(const LdtlAssetRegistry *registry,
                                   const char *symbol) {
    int i;

    if (registry == NULL || symbol == NULL) {
        return -1;
    }
    for (i = 0; i < registry->asset_count; i++) {
        if (registry->assets[i].used &&
            strcmp(registry->assets[i].symbol, symbol) == 0) {
            return registry->assets[i].id;
        }
    }
    return -1;
}

LdtlAssetId ldtl_asset_primary_for_domain(const LdtlAssetRegistry *registry,
                                          LdtlDomainId domain_id) {
    int i;

    if (registry == NULL) {
        return -1;
    }
    for (i = 0; i < registry->asset_count; i++) {
        if (registry->assets[i].used &&
            registry->assets[i].domain_id == domain_id &&
            registry->assets[i].lane_primary) {
            return registry->assets[i].id;
        }
    }
    for (i = 0; i < registry->asset_count; i++) {
        if (registry->assets[i].used &&
            registry->assets[i].domain_id == domain_id) {
            return registry->assets[i].id;
        }
    }
    return -1;
}

int ldtl_asset_is_enabled(const LdtlAssetRegistry *registry,
                          LdtlAssetId asset_id) {
    const LdtlAsset *asset = ldtl_asset_view(registry, asset_id);
    return asset != NULL && asset->enabled;
}

uint64_t ldtl_asset_registry_digest(const LdtlAssetRegistry *registry) {
    uint64_t hash = 0;
    int i;

    if (registry == NULL) {
        return 0;
    }
    hash = ldtl_hash_u64(hash, (uint64_t)registry->asset_count);
    hash = ldtl_hash_u64(hash, registry->version);
    for (i = 0; i < registry->asset_count; i++) {
        const LdtlAsset *asset = &registry->assets[i];
        hash = ldtl_hash_u64(hash, (uint64_t)asset->id);
        hash = ldtl_hash_u64(hash, (uint64_t)asset->domain_id);
        hash = ldtl_hash_u64(hash, (uint64_t)asset->decimals);
        hash = ldtl_hash_u64(hash, (uint64_t)asset->risk_weight_bps);
        hash = ldtl_hash_u64(hash, (uint64_t)asset->reserve_floor);
        hash = ldtl_hash_u64(hash, (uint64_t)asset->kind);
        hash = ldtl_hash_u64(hash, (uint64_t)asset->withdraw_enabled);
        hash = ldtl_hash_bytes(hash, asset->symbol, strlen(asset->symbol));
        hash = ldtl_hash_bytes(hash, asset->domain, strlen(asset->domain));
    }
    return hash;
}

void ldtl_asset_registry_print_json(const LdtlAssetRegistry *registry,
                                    FILE *out,
                                    int indent) {
    int i;
    const char *pad = indent == 2 ? "  " : "    ";

    fprintf(out, "%s\"assets\":[", pad);
    for (i = 0; i < registry->asset_count; i++) {
        const LdtlAsset *asset = &registry->assets[i];
        if (i > 0) {
            fputc(',', out);
        }
        fputc('{', out);
        fprintf(out, "\"id\":%d,\"symbol\":", asset->id);
        ldtl_json_string(out, asset->symbol);
        fprintf(out, ",\"domainId\":%d,\"domain\":", asset->domain_id);
        ldtl_json_string(out, asset->domain);
        fprintf(out,
                ",\"decimals\":%u,\"kind\":\"%s\",\"riskWeightBps\":%u,"
                "\"reserveFloor\":%lld,\"withdrawEnabled\":%s,"
                "\"lanePrimary\":%s",
                (unsigned int)asset->decimals, asset_kind_name(asset->kind),
                (unsigned int)asset->risk_weight_bps,
                (long long)asset->reserve_floor,
                asset->withdraw_enabled ? "true" : "false",
                asset->lane_primary ? "true" : "false");
        fputc('}', out);
    }
    fputs("],\n", out);
}
