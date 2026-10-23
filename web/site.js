/**
 * @file
 * @brief Starts a program of the site (web/README.md) in its page: the host with one template, or the Studio. What it
 * runs comes from the page's own <body> (data-program, data-screen), written by tools/web/build.sh, and never from
 * the page's address: the site trusts nothing in its URL.
 */
"use strict";

/** The programs a page may start, and the screens and folders the host may show (the site preloads them in /assets). */
const affrescoPrograms = new Set(["affrescoui_host", "affrescoui_studio"]);
const affrescoScreenName = /^[a-z][a-z0-9-]{0,63}$/;

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
	const named = affrescoScreenName.test(screen) && affrescoScreenName.test(folder);
	if (!affrescoPrograms.has(program) || (screen !== "" && !named)) {
		affrescoStatus("This page names no program of the site.");
		return;
	}
	window.Module = {
		canvas: document.getElementById("canvas"),
		arguments: screen === "" ? [] : ["/assets/" + folder + "/index.screen.json"],
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

/**
 * @brief Sends the Studio page's connect form to the Studio: the address, and the token, which leaves the page only
 * as the connection's first message (never in a URL, the page's storage or a log), and is cleared from the form.
 * @param event The form's submit event.
 */
function affrescoConnect(event) {
	event.preventDefault();
	const address = document.getElementById("address");
	const token = document.getElementById("token");
	const connected =
		window.Module && typeof window.Module.ccall === "function"
			? window.Module.ccall("affresco_studio_connect", "number", ["string", "string"], [address.value, token.value])
			: 0;
	token.value = "";
	affrescoStatus(
		connected
			? "Connecting to " + address.value + " (the Studio shows how it goes)."
			: "The Studio isn't running yet: try again in a moment."
	);
}

document.addEventListener("DOMContentLoaded", () => {
	const form = document.getElementById("connect");
	if (form) form.addEventListener("submit", affrescoConnect);
	affrescoStart();
});
