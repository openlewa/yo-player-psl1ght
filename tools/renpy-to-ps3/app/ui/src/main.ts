import { open, save } from "@tauri-apps/plugin-dialog";
import { Command } from "@tauri-apps/plugin-shell";

type Task = {
  name: string;
  verb: string;
  inputLabel: string;
  inputHint: string;
  inputFileFilter?: { name: string; extensions: string[] };
  inputAllowsFolder?: boolean;
  hasOutput?: boolean;
  outputHint?: string;
  outputFileFilter?: { name: string; extensions: string[] };
  outputRequired?: boolean;
  isPack?: boolean;
  isAst?: boolean;
};

const tasks: Task[] = [
  {
    name: "Convert a game to a PS3 bundle (.rpk)",
    verb: "pack",
    inputLabel: "Game folder",
    inputHint: "ren'py 'game' folder",
    inputAllowsFolder: true,
    hasOutput: true,
    outputRequired: true,
    outputHint: ".rpk file to output as",
    outputFileFilter: { name: "PS3 bundle", extensions: ["rpk"] },
    isPack: true,
  },
  {
    name: "Check if a game is convertible",
    verb: "info",
    inputLabel: "Game folder",
    inputHint: "ren'py 'game' folder",
    inputAllowsFolder: true,
  },
  {
    name: "Compile scripts to bytecode",
    verb: "compile",
    inputLabel: "Scripts",
    inputHint: "'game' folder or a single .rpyc file",
    inputAllowsFolder: true,
    hasOutput: true,
    outputHint: ".rbc file to output as (optional)",
    outputFileFilter: { name: "bytecode", extensions: ["rbc"] },
  },
  {
    name: "List an archive's contents",
    verb: "list",
    inputLabel: "Archive",
    inputHint: ".rpa archive file",
    inputFileFilter: { name: "Ren'Py archive", extensions: ["rpa"] },
  },
  {
    name: "Extract an archive to a folder",
    verb: "extract",
    inputLabel: "Archive",
    inputHint: ".rpa archive file",
    inputFileFilter: { name: "Ren'Py archive", extensions: ["rpa"] },
    hasOutput: true,
    outputHint: "folder to extract into",
    outputRequired: true,
  },
  {
    name: "Inspect a bundle (.rpk)",
    verb: "rpk",
    inputLabel: "Bundle",
    inputHint: ".rpk bundle file",
    inputFileFilter: { name: "PS3 bundle", extensions: ["rpk"] },
  },
  {
    name: "Show a script as text",
    verb: "script",
    inputLabel: "Script",
    inputHint: ".rpyc script file",
    inputFileFilter: { name: "Ren'Py script", extensions: ["rpyc"] },
  },
  {
    name: "Dump a script's raw tree",
    verb: "ast",
    inputLabel: "Script",
    inputHint: ".rpyc script file",
    inputFileFilter: { name: "Ren'Py script", extensions: ["rpyc"] },
    isAst: true,
  },
  {
    name: "Dump a script's animation nodes",
    verb: "atldump",
    inputLabel: "Script",
    inputHint: ".rpyc script file",
    inputFileFilter: { name: "Ren'Py script", extensions: ["rpyc"] },
  },
];

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T;

const taskEl = $<HTMLSelectElement>("task");
const inputLabel = $<HTMLLabelElement>("input-label");
const inputPath = $<HTMLInputElement>("input-path");
const inputHint = $<HTMLSpanElement>("input-hint");
const outputRow = $<HTMLDivElement>("output-row");
const outputPath = $<HTMLInputElement>("output-path");
const outputHint = $<HTMLSpanElement>("output-hint");
const packRow = $<HTMLDivElement>("pack-row");
const astRow = $<HTMLDivElement>("ast-row");
const astLabel = $<HTMLInputElement>("ast-label");
const maxEdge = $<HTMLInputElement>("max-edge");
const asciiText = $<HTMLInputElement>("ascii-text");
const noCache = $<HTMLInputElement>("no-cache");
const clearCache = $<HTMLInputElement>("clear-cache");
const logView = $<HTMLTextAreaElement>("log");
const status = $<HTMLSpanElement>("status");
const runBtn = $<HTMLButtonElement>("run");

function currentTask(): Task {
  return tasks[taskEl.selectedIndex] ?? tasks[0];
}

function syncHints() {
  inputHint.style.display = inputPath.value.length === 0 ? "block" : "none";
  outputHint.style.display = outputPath.value.length === 0 ? "block" : "none";
}

function onTaskChosen() {
  const task = currentTask();
  inputLabel.textContent = task.inputLabel + ":";
  inputHint.textContent = task.inputHint;
  outputRow.classList.toggle("hidden", !task.hasOutput);
  outputHint.textContent = task.outputHint ?? "";
  packRow.classList.toggle("hidden", !task.isPack);
  astRow.classList.toggle("hidden", !task.isAst);
  status.textContent = "";
  syncHints();
}

function appendLog(chunk: string) {
  logView.value += chunk.endsWith("\n") ? chunk : chunk + "\n";
  logView.scrollTop = logView.scrollHeight;
}

function quoteArg(arg: string): string {
  return arg.includes(" ") ? `"${arg}"` : arg;
}

async function browseInput() {
  const task = currentTask();
  if (task.inputAllowsFolder) {
    const dir = await open({ directory: true, title: task.inputLabel });
    if (typeof dir === "string") inputPath.value = dir;
  } else if (task.inputFileFilter) {
    const file = await open({
      multiple: false,
      filters: [task.inputFileFilter],
      title: task.inputLabel,
    });
    if (typeof file === "string") inputPath.value = file;
  }
  syncHints();
}

async function browseOutput() {
  const task = currentTask();
  if (task.outputFileFilter) {
    const file = await save({
      filters: [task.outputFileFilter],
      title: "Output",
    });
    if (typeof file === "string") outputPath.value = file;
  } else {
    const dir = await open({ directory: true, title: "Output folder" });
    if (typeof dir === "string") outputPath.value = dir;
  }
  syncHints();
}

async function run() {
  const task = currentTask();
  const input = inputPath.value.trim();
  const output = outputPath.value.trim();
  if (!input) {
    status.textContent = "pick the " + task.inputLabel.toLowerCase() + " first";
    return;
  }
  if (task.outputRequired && !output) {
    status.textContent = "pick the output first";
    return;
  }

  const args: string[] = [task.verb, input];
  if (task.hasOutput && output) args.push(output);
  if (task.isPack) {
    const n = parseInt(maxEdge.value.trim(), 10);
    if (!Number.isFinite(n) || n < 16) {
      status.textContent = "max image size must be a number (16 or more)";
      return;
    }
    args.push("--max", String(n));
    if (asciiText.checked) args.push("--ascii-text");
    if (noCache.checked) args.push("--no-cache");
    if (clearCache.checked) args.push("--clear-cache");
  }
  if (task.isAst) {
    const label = astLabel.value.trim();
    if (label) args.push(label);
  }

  appendLog("> renpy-to-ps3 " + args.map(quoteArg).join(" ") + "\n");
  runBtn.disabled = true;
  status.textContent = "running ...";

  try {
    const cmd = Command.sidecar("binaries/renpy-to-ps3", args);
    cmd.stdout.on("data", (line) => appendLog(line));
    cmd.stderr.on("data", (line) => appendLog(line));
    const result = await cmd.execute();
    if (result.stdout && !logView.value.includes(result.stdout.trim().slice(0, 40))) {
      appendLog(result.stdout);
    }
    if (result.stderr) appendLog(result.stderr);
    status.textContent = result.code === 0 ? "done" : "failed (see log)";
  } catch (err) {
    appendLog(String(err));
    status.textContent = "failed (see log)";
  } finally {
    runBtn.disabled = false;
  }
}

for (const task of tasks) {
  const opt = document.createElement("option");
  opt.textContent = task.name;
  taskEl.appendChild(opt);
}
taskEl.addEventListener("change", onTaskChosen);
inputPath.addEventListener("input", syncHints);
outputPath.addEventListener("input", syncHints);
$("browse-input").addEventListener("click", () => void browseInput());
$("browse-output").addEventListener("click", () => void browseOutput());
runBtn.addEventListener("click", () => void run());
$("clear").addEventListener("click", () => {
  logView.value = "";
});
onTaskChosen();
