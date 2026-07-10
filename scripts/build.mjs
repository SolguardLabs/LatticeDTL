import { buildBinary, projectRoot } from "./build-lib.mjs";

const result = buildBinary(projectRoot());

if (process.argv.includes("--json")) {
  console.log(JSON.stringify(result));
} else {
  console.log(`built ${result.binary} with ${result.mode}:${result.compiler}`);
}
