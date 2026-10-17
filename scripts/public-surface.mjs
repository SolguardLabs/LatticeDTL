import { readFileSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";

const roots = ["README.md", "SECURITY.md", "docs", "src", "tests"];
const forbidden = [
    new RegExp(`\\b${["l", "ab"].join("")}\\b`, "i"),
    new RegExp(["labor", "ator"].join(""), "i"),
    new RegExp(`\\b${["c", "tf"].join("")}\\b`, "i"),
    new RegExp(["vulnerab", "ilidad"].join(""), "i"),
    new RegExp(["vulnerab", "ility"].join(""), "i"),
    new RegExp(["expl", "oit"].join(""), "i"),
];

function files(path) {
    if (statSync(path).isFile()) return [path];
    return readdirSync(path, { withFileTypes: true }).flatMap((entry) => {
        const child = join(path, entry.name);
        return entry.isDirectory() ? files(child) : [child];
    });
}

const findings = [];
for (const path of roots.flatMap(files)) {
    const content = readFileSync(path, "utf8");
    for (const pattern of forbidden) {
        if (pattern.test(content)) findings.push(`${path}: ${pattern.source}`);
    }
}

if (findings.length > 0) {
    console.error(findings.join("\n"));
    process.exit(1);
}

console.log(`public surface verified across ${roots.length} roots`);
