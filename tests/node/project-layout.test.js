import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import { projectRoot } from "../../scripts/build-lib.mjs";

const root = projectRoot();

test("C source tree contains the expected domain modules", () => {
  const expected = [
    "src/asset_registry.c",
    "src/ledger.c",
    "src/matrix.c",
    "src/epoch_netting.c",
    "src/withdrawals.c",
    "src/reconciliation.c",
    "src/policy.c",
    "src/runtime.c",
    "src/main.c",
  ];

  for (const file of expected) {
    assert.equal(fs.existsSync(path.join(root, file)), true, `${file} should exist`);
  }
});

test("public project metadata is self-contained", () => {
  const pkg = JSON.parse(fs.readFileSync(path.join(root, "package.json"), "utf8"));
  const ignore = fs.readFileSync(path.join(root, ".gitignore"), "utf8");
  const ci = fs.readFileSync(path.join(root, ".github/workflows/ci.yml"), "utf8");

  assert.equal(pkg.private, true);
  assert.match(pkg.scripts.build, /scripts\/build\.mjs/);
  assert.match(pkg.scripts.test, /node --test/);
  assert.match(ignore, /build\//);
  assert.match(ignore, /node_modules\//);
  assert.match(ci, /bash scripts\/ci\.sh/);
});

test("README and SECURITY are present for audit workflow", () => {
  for (const file of ["README.md", "SECURITY.md", "Makefile"]) {
    assert.equal(fs.existsSync(path.join(root, file)), true, `${file} should exist`);
  }
});
