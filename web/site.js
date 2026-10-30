/**
 * @file
 * @brief Starts a program of the site (web/README.md) in its page: the host with one template. What it runs comes from
 * the page's own <body> (data-program, data-screen), written by tools/web/build.sh, and never from the page's
 * address: the site trusts nothing in its URL.
 */
"use strict";

/** The programs a page may start, and the screens and folders the host may show (the site preloads them in /assets). */
const affrescoPrograms = new Set(["affrescoui_launcher"]);
const affrescoScreenName = /^[a-z][a-z0-9-]{0,63}$/;

/** A template's folder, an example's JSON folder: the same words, in the same place, with /json after them. */
const affrescoFolderName = /^[a-z][a-z0-9-]{0,63}\/json$/;

/**
 * @brief Shows a line in the page's status bar.
 * @param text The line.
 */
function affrescoStatus(text) {
	const status = document.getElementById("status");
	if (status) status.textContent = text;
}

/**
 * @brief Builds the Emscripten Module of the page's program and loads it.
 * @return Nothing: errors go to the status bar.
 */
function affrescoStart() {
	const program = document.body.dataset.program || "";
	const folder = document.body.dataset.folder || "";
	const screen = document.body.dataset.screen || "";
	const named = affrescoScreenName.test(screen) && affrescoFolderName.test(folder);
	if (!affrescoPrograms.has(program) || (screen !== "" && !named)) {
		affrescoStatus("This page names no program of the site.");
		return;
	}
	window.Module = {
		canvas: document.getElementById("canvas"),
		arguments: screen === "" ? [] : ["/assets/" + folder], // the app's folder: the launcher opens its app.json
		print: (text) => console.log(text),
		printErr: (text) => console.warn(text),
		setStatus: (text) => affrescoStatus(text || "Running."),
		onAbort: (what) => affrescoStatus("Stopped: " + what),
	};
	const script = document.createElement("script");
	script.src = program + ".js";
	script.onerror = () => affrescoStatus("Couldn't load " + program + ".js.");
	document.body.appendChild(script);
}

document.addEventListener("DOMContentLoaded", () => {
	affrescoStart();
});
