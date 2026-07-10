import assert from "node:assert/strict";
import test from "node:test";
import { balanceOf, runScenario, vaultOf } from "../helpers/lattice-cli.js";

test("baseline scenario emits a conserved multi-asset settlement report", () => {
  const report = runScenario("baseline");

  assert.equal(report.lab, "LatticeDTL");
  assert.equal(report.scenario, "baseline");
  assert.equal(report.ok, true);
  assert.equal(report.conservation, true);
  assert.equal(report.reconciliation.ok, true);
  assert.equal(report.reconciliation.assets.length, 5);
  assert.equal(report.epoch.status, "ok");
  assert.equal(report.epoch.rawCells, 4);
  assert.equal(report.epoch.exactRows, 3);
  assert.equal(report.epoch.compactRows, 0);
  assert.equal(report.epoch.materializedCells, 3);
  assert.equal(report.epoch.transfers, 3);

  assert.equal(balanceOf(report, "north_mm", "USDC"), 232000000);
  assert.equal(balanceOf(report, "south_mm", "USDC"), 178000000);
  assert.equal(balanceOf(report, "client_alpha", "EURC"), 38000000);
  assert.equal(balanceOf(report, "bridge_eu", "EURC"), 712000000);
  assert.equal(balanceOf(report, "bridge_eu", "MXNT"), 882000000);
  assert.equal(balanceOf(report, "client_alpha", "MXNT"), 18000000);

  assert.equal(vaultOf(report, "USDC").reserve, 10480000000);
  assert.equal(vaultOf(report, "USDC").liability, 10480000000);
  assert.equal(report.reconciliation.assets.find((row) => row.asset === "USDC").liabilityGap, 0);
  assert.match(report.ledgerDigest, /^0x[0-9a-f]{16}$/);
});

test("snapshot scenario exposes deterministic fixture metadata", () => {
  const report = runScenario("snapshot");

  assert.equal(report.ok, true);
  assert.equal(report.scenario, "snapshot");
  assert.equal(report.assets.length, 5);
  assert.equal(report.accounts.length, 6);
  assert.equal(report.matrix.rawCells, 0);
  assert.equal(report.epoch.id, 0);
  assert.equal(report.epoch.transfers, 0);

  const symbols = report.assets.map((asset) => asset.symbol);
  assert.deepEqual(symbols, ["USDC", "EURC", "WBTC", "LATT", "MXNT"]);
});
