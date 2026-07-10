import assert from "node:assert/strict";
import test from "node:test";
import { assetId, balanceOf, runScenario } from "../helpers/lattice-cli.js";

test("small same-asset positions compact into a single settlement cell", () => {
  const report = runScenario("compact");
  const usdcId = assetId(report, "USDC");

  assert.equal(report.ok, true);
  assert.equal(report.matrix.rawCells, 3);
  assert.equal(report.matrix.exactRows, 0);
  assert.equal(report.matrix.compactRows, 1);
  assert.equal(report.matrix.materializedCells, 1);
  assert.equal(report.epoch.transfers, 1);
  assert.equal(report.matrix.materialized[0].assetId, usdcId);
  assert.equal(report.matrix.materialized[0].amount, 2300);

  assert.equal(balanceOf(report, "north_mm", "USDC"), 249997700);
  assert.equal(balanceOf(report, "south_mm", "USDC"), 160002300);
});

test("larger positions remain separated by asset lane", () => {
  const report = runScenario("domains");
  const materializedAssets = report.matrix.materialized.map((cell) => cell.assetId).sort();
  const expectedAssets = [assetId(report, "USDC"), assetId(report, "LATT")].sort();

  assert.equal(report.ok, true);
  assert.equal(report.matrix.rawCells, 2);
  assert.equal(report.matrix.exactRows, 2);
  assert.equal(report.matrix.compactRows, 0);
  assert.deepEqual(materializedAssets, expectedAssets);

  assert.equal(balanceOf(report, "north_mm", "USDC"), 241000000);
  assert.equal(balanceOf(report, "south_mm", "USDC"), 169000000);
  assert.equal(balanceOf(report, "north_mm", "LATT"), 29000);
  assert.equal(balanceOf(report, "south_mm", "LATT"), 31000);
});
