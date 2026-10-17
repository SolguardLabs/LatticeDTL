import assert from "node:assert/strict";
import test from "node:test";

import { runScenario } from "../helpers/lattice-cli.js";

test("baseline publishes an operating liquidity waterfall per asset", () => {
    const report = runScenario("baseline");
    const stress = report.treasuryStress;

    assert.equal(stress.ok, true);
    assert.equal(stress.config.redemptionBps, 800);
    assert.equal(stress.config.settlementDelayBps, 1000);
    assert.equal(stress.config.reserveHaircutBps, 500);
    assert.equal(stress.config.minimumCoverageBps, 10500);
    assert.equal(stress.assets.length, report.assets.length);
    assert.ok(stress.eligibleLiquidity > 0);
    assert.match(stress.digest, /^0x[0-9a-f]{16}$/);
});

test("severe stress applies the institutional scenario", () => {
    const report = runScenario("stress");
    const stress = report.treasuryStress;

    assert.equal(report.scenario, "stress");
    assert.equal(stress.config.redemptionBps, 2500);
    assert.equal(stress.config.settlementDelayBps, 3500);
    assert.equal(stress.config.reserveHaircutBps, 2000);
    assert.equal(stress.config.minimumCoverageBps, 11500);
    assert.ok(["green", "amber", "red"].includes(stress.level));
    assert.equal(
        stress.greenAssets + stress.amberAssets + stress.redAssets,
        stress.assets.length,
    );
});

test("asset rows reconcile uses, shortfall, and required buffer", () => {
    const report = runScenario("stress");

    for (const row of report.treasuryStress.assets) {
        assert.equal(
            row.totalUses,
            row.stressedRedemptions + row.delayedSettlement + row.queuedOutflow,
        );
        assert.equal(row.nominalShortfall, Math.max(0, row.totalUses - row.eligibleLiquidity));
        assert.ok(row.requiredBuffer >= row.nominalShortfall);
        assert.ok(row.coverageBps >= 0 && row.coverageBps <= 100000);
    }
});
