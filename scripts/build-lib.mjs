import { execFileSync, spawnSync } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

export const sourceFiles = [
  "src/ldtl_common.c",
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

export function projectRoot() {
  return path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
}

function ensureBuildDir(root) {
  fs.mkdirSync(path.join(root, "build"), { recursive: true });
}

function sleep(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

function newestSourceMtime(root) {
  let newest = 0;
  for (const file of [...sourceFiles, "src/ldtl_common.h", "src/asset_registry.h", "src/ledger.h", "src/matrix.h", "src/epoch_netting.h", "src/withdrawals.h", "src/reconciliation.h", "src/policy.h", "src/runtime.h"]) {
    const stat = fs.statSync(path.join(root, file));
    newest = Math.max(newest, stat.mtimeMs);
  }
  return newest;
}

function isFresh(root, outputRelative) {
  const output = path.join(root, outputRelative);
  if (!fs.existsSync(output)) {
    return false;
  }
  return fs.statSync(output).mtimeMs >= newestSourceMtime(root);
}

function buildResult(root, outputRelative, mode, compiler, commandPrefix) {
  const binary = path.join(root, outputRelative);
  return {
    mode,
    compiler,
    binary: mode === "wsl" ? commandPrefix[2] : binary,
    command: commandPrefix ?? [binary],
  };
}

function withBuildLock(root, fn) {
  ensureBuildDir(root);
  const lockPath = path.join(root, "build", ".build.lock");
  let fd = null;

  for (let attempt = 0; attempt < 400; attempt++) {
    try {
      fd = fs.openSync(lockPath, "wx");
      break;
    } catch (error) {
      if (error.code !== "EEXIST") {
        throw error;
      }
      try {
        const age = Date.now() - fs.statSync(lockPath).mtimeMs;
        if (age > 120000) {
          fs.unlinkSync(lockPath);
        }
      } catch {
        // Another process may have released the lock between stat and unlink.
      }
      sleep(50);
    }
  }
  if (fd === null) {
    throw new Error("Timed out waiting for build lock");
  }
  try {
    return fn();
  } finally {
    fs.closeSync(fd);
    try {
      fs.unlinkSync(lockPath);
    } catch {
      // Best effort cleanup; a stale lock is handled on the next build.
    }
  }
}

function commandWorks(command, args = ["--version"]) {
  const result = spawnSync(command, args, { stdio: "ignore" });
  return !result.error && result.status === 0;
}

function compileWithUnixCompiler(root, compiler) {
  const output = path.join("build", process.platform === "win32" ? "lattice-dtl.exe" : "lattice-dtl");
  if (isFresh(root, output)) {
    return buildResult(root, output, "local", compiler, [path.join(root, output)]);
  }
  return withBuildLock(root, () => {
    if (isFresh(root, output)) {
      return buildResult(root, output, "local", compiler, [path.join(root, output)]);
    }
  const args = [
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-O2",
    "-Isrc",
    ...sourceFiles,
    "-o",
    output,
  ];
  const result = spawnSync(compiler, args, {
    cwd: root,
    encoding: "utf8",
  });
  if (result.status !== 0) {
    const details = `${result.stdout ?? ""}${result.stderr ?? ""}`.trim();
    throw new Error(`C build failed with ${compiler}${details ? `:\n${details}` : ""}`);
  }
    return buildResult(root, output, "local", compiler, [path.join(root, output)]);
  });
}

function compileWithMsvc(root) {
  const output = path.join("build", "lattice-dtl.exe");
  if (isFresh(root, output)) {
    return buildResult(root, output, "local", "cl", [path.join(root, output)]);
  }
  return withBuildLock(root, () => {
    if (isFresh(root, output)) {
      return buildResult(root, output, "local", "cl", [path.join(root, output)]);
    }
  const args = [
    "/nologo",
    "/std:c11",
    "/W4",
    "/WX",
    "/O2",
    "/Isrc",
    ...sourceFiles,
    `/Fe:${output}`,
  ];
  const result = spawnSync("cl", args, {
    cwd: root,
    encoding: "utf8",
  });
  if (result.status !== 0) {
    const details = `${result.stdout ?? ""}${result.stderr ?? ""}`.trim();
    throw new Error(`C build failed with cl${details ? `:\n${details}` : ""}`);
  }
    return buildResult(root, output, "local", "cl", [path.join(root, output)]);
  });
}

function shQuote(value) {
  return `'${value.replace(/'/g, "'\\''")}'`;
}

function wslPath(winPath) {
  return execFileSync("wsl", ["--exec", "wslpath", "-a", "-u", winPath], {
    encoding: "utf8",
  }).trim();
}

function compileWithWsl(root) {
  const wRoot = wslPath(root);
  const output = path.join("build", "lattice-dtl");
  const command = ["wsl", "--exec", `${wRoot}/build/lattice-dtl`];
  if (isFresh(root, output)) {
    return buildResult(root, output, "wsl", "gcc", command);
  }
  return withBuildLock(root, () => {
    if (isFresh(root, output)) {
      return buildResult(root, output, "wsl", "gcc", command);
    }
    const script = [
      `cd ${shQuote(wRoot)}`,
      "mkdir -p build",
      `gcc -std=c11 -Wall -Wextra -Werror -O2 -Isrc ${sourceFiles.join(" ")} -o build/lattice-dtl`,
    ].join(" && ");
  const result = spawnSync("wsl", ["--exec", "bash", "-lc", script], {
    encoding: "utf8",
  });
  if (result.status !== 0) {
    const details = `${result.stdout ?? ""}${result.stderr ?? ""}`.trim();
    throw new Error(`C build failed with WSL gcc${details ? `:\n${details}` : ""}`);
  }
    return buildResult(root, output, "wsl", "gcc", command);
  });
}

export function buildBinary(root = projectRoot()) {
  const requested = process.env.CC;
  if (requested) {
    return compileWithUnixCompiler(root, requested);
  }

  for (const compiler of ["cc", "gcc", "clang"]) {
    if (commandWorks(compiler)) {
      return compileWithUnixCompiler(root, compiler);
    }
  }

  if (os.platform() === "win32" && commandWorks("cl", [])) {
    return compileWithMsvc(root);
  }

  if (os.platform() === "win32" && commandWorks("wsl", ["--status"])) {
    return compileWithWsl(root);
  }

  throw new Error("No C compiler found. Install cc/gcc/clang, MSVC cl, or WSL with gcc.");
}

export function runBuiltBinary(build, args = []) {
  const [command, ...prefix] = build.command;
  const result = spawnSync(command, [...prefix, ...args], {
    encoding: "utf8",
  });
  if (result.status !== 0) {
    const details = `${result.stdout ?? ""}${result.stderr ?? ""}`.trim();
    throw new Error(`Scenario command failed${details ? `:\n${details}` : ""}`);
  }
  return result.stdout;
}
