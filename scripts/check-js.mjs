import { execFileSync } from "node:child_process";
import { readdirSync } from "node:fs";
import { extname, join } from "node:path";

const roots = ["scripts", "tests"];

function collect(path) {
    return readdirSync(path, { withFileTypes: true }).flatMap((entry) => {
        const child = join(path, entry.name);
        if (entry.isDirectory()) return collect(child);
        return [child];
    });
}

const files = roots.flatMap(collect).filter((path) => [".js", ".mjs"].includes(extname(path)));
for (const file of files) {
    execFileSync(process.execPath, ["--check", file], { stdio: "pipe" });
}

console.log(`checked ${files.length} JavaScript files`);
