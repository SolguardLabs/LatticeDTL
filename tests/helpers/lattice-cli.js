import { buildBinary, projectRoot, runBuiltBinary } from "../../scripts/build-lib.mjs";

let cachedBuild;

export function build() {
  if (!cachedBuild) {
    cachedBuild = buildBinary(projectRoot());
  }
  return cachedBuild;
}

export function runScenario(name = "baseline") {
  const stdout = runBuiltBinary(build(), [name]);
  return JSON.parse(stdout);
}

export function balanceOf(report, account, asset) {
  const row = report.balances.find(
    (entry) => entry.account === account && entry.asset === asset,
  );
  return row ? row.amount : 0;
}

export function vaultOf(report, asset) {
  const row = report.vaults.find((entry) => entry.asset === asset);
  if (!row) {
    throw new Error(`missing vault ${asset}`);
  }
  return row;
}

export function assetId(report, symbol) {
  const row = report.assets.find((entry) => entry.symbol === symbol);
  if (!row) {
    throw new Error(`missing asset ${symbol}`);
  }
  return row.id;
}
