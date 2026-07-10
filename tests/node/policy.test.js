import assert from "node:assert/strict";
import test from "node:test";
import { balanceOf, runScenario } from "../helpers/lattice-cli.js";

test("baseline report includes policy exposure for every account", () => {
  const report = runScenario("baseline");

  assert.equal(report.policy.ok, true);
  assert.equal(report.policy.flaggedAccounts, 0);
  assert.equal(report.policy.rules.length, 5);
  assert.equal(report.policy.accounts.length, report.accounts.length);
  assert.equal(report.policy.lastCheck, null);

  const client = report.policy.accounts.find((row) => row.account === "client_alpha");
  assert.equal(client.riskClass, 3);
  assert.equal(client.maxWithdrawal, 20000000);
  assert.equal(client.withinExposure, true);
  assert.equal(client.activeAssets, 3);
  assert.equal(
    client.withdrawable,
    balanceOf(report, "client_alpha", "USDC") +
      balanceOf(report, "client_alpha", "EURC") +
      balanceOf(report, "client_alpha", "MXNT"),
  );

  const reserve = report.policy.accounts.find((row) => row.account === "reserve_pool");
  assert.equal(reserve.riskClass, 0);
  assert.equal(reserve.withinExposure, true);
  assert.equal(reserve.maxObservedAssetRiskBps, 9200);
});

test("policy scenario records an oversized withdrawal precheck", () => {
  const report = runScenario("policy");

  assert.equal(report.ok, true);
  assert.equal(report.scenario, "policy");
  assert.equal(report.epoch.transfers, 0);
  assert.equal(report.withdrawals.length, 0);
  assert.equal(report.policy.ok, true);
  assert.deepEqual(report.policy.lastCheck, {
    action: "withdrawal",
    account: "client_alpha",
    asset: "USDC",
    amount: 35000000,
    status: "precheck",
  });

  assert.equal(balanceOf(report, "client_alpha", "USDC"), 70000000);
});
