(function () {
  var tasks = [
    { name: "Convert a game to a PS3 bundle (.rpk)", verb: "pack", inputLabel: "Game folder", inputHint: "ren'py 'game' folder", folder: true, hasOutput: true, outputRequired: true, outputHint: ".rpk file to output as", outputFile: true, isPack: true },
    { name: "Check if a game is convertible", verb: "info", inputLabel: "Game folder", inputHint: "ren'py 'game' folder", folder: true },
    { name: "Compile scripts to bytecode", verb: "compile", inputLabel: "Scripts", inputHint: "'game' folder or a single .rpyc file", folder: true, hasOutput: true, outputHint: ".rbc file to output as (optional)", outputFile: true },
    { name: "List an archive's contents", verb: "list", inputLabel: "Archive", inputHint: ".rpa archive file" },
    { name: "Extract an archive to a folder", verb: "extract", inputLabel: "Archive", inputHint: ".rpa archive file", hasOutput: true, outputHint: "folder to extract into", outputRequired: true, folderOut: true },
    { name: "Inspect a bundle (.rpk)", verb: "rpk", inputLabel: "Bundle", inputHint: ".rpk bundle file" },
    { name: "Show a script as text", verb: "script", inputLabel: "Script", inputHint: ".rpyc script file" },
    { name: "Dump a script's raw tree", verb: "ast", inputLabel: "Script", inputHint: ".rpyc script file", isAst: true },
    { name: "Dump a script's animation nodes", verb: "atldump", inputLabel: "Script", inputHint: ".rpyc script file" }
  ];

  function $(id) { return document.getElementById(id); }

  var taskEl = $("task");
  var inputLabel = $("input-label");
  var inputPath = $("input-path");
  var outputRow = $("output-row");
  var outputPath = $("output-path");
  var packRow = $("pack-row");
  var astRow = $("ast-row");
  var astLabel = $("ast-label");
  var maxEdge = $("max-edge");
  var asciiText = $("ascii-text");
  var noCache = $("no-cache");
  var clearCache = $("clear-cache");
  var logView = $("log");
  var status = $("status");
  var runBtn = $("run");
  var picker = $("picker");
  var pickerPath = $("picker-path");
  var pickerList = $("picker-list");
  var pickerUse = $("picker-use");
  var pickerName = $("picker-name");
  var pickerMode = "input";
  var pickerCurrent = "";
  var pickerParent = "";

  function currentTask() {
    return tasks[taskEl.selectedIndex] || tasks[0];
  }

  function onTaskChosen() {
    var task = currentTask();
    inputLabel.innerHTML = task.inputLabel + ":";
    inputPath.placeholder = task.inputHint;
    outputRow.className = task.hasOutput ? "row" : "row hidden";
    outputPath.placeholder = task.outputHint || "";
    packRow.className = task.isPack ? "row" : "row hidden";
    astRow.className = task.isAst ? "row" : "row hidden";
    status.innerHTML = "";
  }

  function appendLog(chunk) {
    logView.value += chunk;
    logView.scrollTop = logView.scrollHeight;
  }

  function quoteArg(arg) {
    return arg.indexOf(" ") >= 0 ? '"' + arg + '"' : arg;
  }

  var xhr = null;

  function wantsFolder() {
    var task = currentTask();
    return (pickerMode === "input" && task.folder) || (pickerMode === "output" && task.folderOut);
  }

  function openPicker(mode) {
    pickerMode = mode;
    picker.className = "picker";
    var folder = wantsFolder();
    pickerUse.style.display = "inline";
    pickerUse.innerHTML = folder ? "Select this folder" : "Use this name";
    pickerName.style.display = folder ? "none" : "inline";
    if (!folder) {
      pickerName.value = pickerMode === "output" && currentTask().isPack ? "out.rpk" : "out.rbc";
    }
    var start = pickerMode === "input" ? inputPath.value : outputPath.value;
    loadFs(start || "");
  }

  function closePicker() {
    picker.className = "picker hidden";
  }

  function loadFs(path) {
    var req = new XMLHttpRequest();
    req.open("GET", "/api/fs?path=" + encodeURIComponent(path || ""), true);
    req.onreadystatechange = function () {
      if (req.readyState !== 4) return;
      var data;
      try { data = JSON.parse(req.responseText); } catch (e) { return; }
      pickerCurrent = data.path || "";
      pickerParent = data.parent || "";
      pickerPath.innerHTML = "";
      pickerPath.appendChild(document.createTextNode(pickerCurrent || "(drives)"));
      pickerList.innerHTML = "";
      if (data.error) {
        var err = document.createElement("div");
        err.innerHTML = data.error;
        pickerList.appendChild(err);
        return;
      }
      var entries = data.entries || [];
      var i;
      for (i = 0; i < entries.length; i++) {
        (function (ent) {
          var row = document.createElement("div");
          row.appendChild(document.createTextNode((ent.dir ? "[dir] " : "") + ent.name));
          row.onclick = function () {
            if (ent.dir) {
              loadFs(ent.path);
            } else if (!wantsFolder()) {
              choosePath(ent.path);
            }
          };
          pickerList.appendChild(row);
        })(entries[i]);
      }
    };
    req.send(null);
  }

  function choosePath(path) {
    if (pickerMode === "input") inputPath.value = path;
    else outputPath.value = path;
    closePicker();
  }

  function run() {
    var task = currentTask();
    var input = inputPath.value.replace(/^\s+|\s+$/g, "");
    var output = outputPath.value.replace(/^\s+|\s+$/g, "");
    if (!input) {
      status.innerHTML = "pick the " + task.inputLabel.toLowerCase() + " first";
      openPicker("input");
      return;
    }
    if (task.outputRequired && !output) {
      status.innerHTML = "pick the output first";
      openPicker("output");
      return;
    }
    var args = [task.verb, input];
    if (task.hasOutput && output) args.push(output);
    if (task.isPack) {
      var n = parseInt(maxEdge.value, 10);
      if (!isFinite(n) || n < 16) {
        status.innerHTML = "max image size must be a number (16 or more)";
        return;
      }
      args.push("--max", String(n));
      if (asciiText.checked) args.push("--ascii-text");
      if (noCache.checked) args.push("--no-cache");
      if (clearCache.checked) args.push("--clear-cache");
    }
    if (task.isAst) {
      var label = astLabel.value.replace(/^\s+|\s+$/g, "");
      if (label) args.push(label);
    }

    appendLog("> renpy-to-ps3 " + args.map(quoteArg).join(" ") + "\n");
    runBtn.disabled = true;
    status.innerHTML = "running ...";

    var seen = 0;
    xhr = new XMLHttpRequest();
    xhr.open("POST", "/api/run", true);
    xhr.setRequestHeader("Content-Type", "application/json");
    xhr.onprogress = function () {
      if (xhr.responseText.length > seen) {
        appendLog(xhr.responseText.substring(seen));
        seen = xhr.responseText.length;
      }
    };
    xhr.onreadystatechange = function () {
      if (xhr.readyState !== 4) return;
      if (xhr.responseText.length > seen) {
        appendLog(xhr.responseText.substring(seen));
      }
      var text = xhr.responseText;
      var code = 1;
      var m = text.match(/__EXIT__ (-?\d+)\s*$/);
      if (m) code = parseInt(m[1], 10);
      else if (xhr.status === 200) code = 0;
      status.innerHTML = code === 0 ? "done" : "failed (see log)";
      runBtn.disabled = false;
    };
    xhr.onerror = function () {
      appendLog("request failed\n");
      status.innerHTML = "failed (see log)";
      runBtn.disabled = false;
    };
    xhr.send(JSON.stringify({ args: args }));
  }

  var i;
  for (i = 0; i < tasks.length; i++) {
    var opt = document.createElement("option");
    opt.appendChild(document.createTextNode(tasks[i].name));
    taskEl.appendChild(opt);
  }
  taskEl.onchange = onTaskChosen;
  $("browse-input").onclick = function () { openPicker("input"); };
  $("browse-output").onclick = function () { openPicker("output"); };
  $("picker-cancel").onclick = closePicker;
  $("picker-up").onclick = function () { loadFs(pickerParent); };
  function withExt(name, ext) {
    if (name.length >= ext.length && name.slice(name.length - ext.length).toLowerCase() === ext) return name;
    return name + ext;
  }

  function joinPath(dir, name) {
    if (!dir) return name;
    var slash = dir.indexOf("\\") >= 0 ? "\\" : "/";
    return dir.replace(/[\\\/]+$/, "") + slash + name;
  }

  $("picker-use").onclick = function () {
    var path = pickerCurrent;
    if (!wantsFolder()) {
      var name = pickerName.value.replace(/^\s+|\s+$/g, "");
      if (!name) return;
      if (currentTask().isPack) name = withExt(name, ".rpk");
      else if (currentTask().verb === "compile") name = withExt(name, ".rbc");
      path = joinPath(pickerCurrent, name);
    }
    if (!path) return;
    choosePath(path);
  };
  runBtn.onclick = run;
  $("clear").onclick = function () { logView.value = ""; };
  onTaskChosen();
})();
