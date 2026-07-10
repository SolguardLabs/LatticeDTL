import assert from "node:assert/strict";
import test from "node:test";
import { balanceOf, runScenario, vaultOf } from "../helpers/lattice-cli.js";

test("withdrawal scenario debits participant balance and vault liability", () => {
  const report = runScenario("withdrawal");

  assert.equal(report.ok, true);
  assert.equal(report.epoch.status, "ok");
  assert.equal(report.epoch.rawCells, 0);
  assert.equal(report.withdrawals.length, 1);
  assert.equal(report.withdrawals[0].state, "executed");
  assert.equal(report.withdrawals[0].status, "ok");
  assert.equal(report.withdrawals[0].amount, 15000000);

  assert.equal(balanceOf(report, "client_alpha", "USDC"), 55000000);
  assert.equal(vaultOf(report, "USDC").reserve, 10465000000);
  assert.equal(vaultOf(report, "USDC").liability, 10465000000);
});

test("batch scenario combines netting and post-epoch withdrawal", () => {
  const report = runScenario("batch");

  assert.equal(report.ok, true);
  assert.equal(report.matrix.rawCells, 6);
  assert.equal(report.matrix.exactRows, 3);
  assert.equal(report.matrix.compactRows, 1);
  assert.equal(report.matrix.materializedCells, 4);
  assert.equal(report.epoch.transfers, 4);
  assert.equal(report.withdrawals.length, 1);
  assert.equal(report.withdrawals[0].state, "executed");

  assert.equal(balanceOf(report, "north_mm", "USDC"), 243999400);
  assert.equal(balanceOf(report, "south_mm", "USDC"), 165000000);
  assert.equal(balanceOf(report, "client_alpha", "USDC"), 70000600);
  assert.equal(balanceOf(report, "north_mm", "EURC"), 43000000);
  assert.equal(balanceOf(report, "south_mm", "EURC"), 124000000);
});
