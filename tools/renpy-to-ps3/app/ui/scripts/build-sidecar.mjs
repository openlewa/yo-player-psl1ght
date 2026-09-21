import { execSync } from "node:child_process";
import { mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const scriptsDir = dirname(fileURLToPath(import.meta.url));
const uiDir = join(scriptsDir, "..");
const goDir = join(uiDir, "..", "go");
const host = execSync("rustc -vV", { encoding: "utf8" }).match(/host: (\S+)/)[1];
const ext = process.platform === "win32" ? ".exe" : "";
const outDir = join(uiDir, "src-tauri", "binaries");
mkdirSync(outDir, { recursive: true });
const out = join(outDir, `renpy-to-ps3-${host}${ext}`);
execSync(`go build -o "${out}" ./cmd/renpy-to-ps3`, {
  cwd: goDir,
  stdio: "inherit",
});
console.log("sidecar", out);
