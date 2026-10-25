/**
 * Runs the affrescoui command-line tool: only `check --json` and `render`, never through a shell, each with a time
 * limit and a limit on what it may print.
 */

import { execFile } from 'node:child_process';
import { statSync } from 'node:fs';
import { delimiter, isAbsolute, join } from 'node:path';

/** What a run of the tool gave. */
export interface ToolResult {
	/** The exit code, or undefined when the tool couldn't be started or was stopped. */
	code: number | undefined;
	stdout: string;
	stderr: string;
	/** Why it has no exit code: "missing" (not found), "timeout", or another start failure. */
	failure?: 'missing' | 'timeout' | 'failed';
}

/** The most the tool may print, in bytes. */
const maxOutput = 4 * 1024 * 1024;

/**
 * Finds the program to run, as an absolute path: Windows runs a bare name from the folder it is started in before
 * PATH, so a screen's folder holding an `affrescoui.exe` would otherwise run it.
 * @param tool     the setting: a bare name, looked up on PATH's absolute folders only, or an absolute path
 * @param path     the PATH variable
 * @param windows  whether names end in `.exe`
 * @param exists   whether a path is a file
 * @return the absolute path, or undefined if there is none (a relative path is never run)
 */
export function findTool(tool: string, path: string, windows: boolean, exists: (file: string) => boolean): string | undefined {
	if (isAbsolute(tool)) return tool;
	if (tool === '' || tool.includes('/') || tool.includes('\\')) return undefined;
	const name = windows && !tool.toLowerCase().endsWith('.exe') ? `${tool}.exe` : tool;
	for (const folder of path.split(windows ? ';' : delimiter)) {
		if (folder !== '' && isAbsolute(folder) && exists(join(folder, name))) return join(folder, name);
	}
	return undefined;
}

/**
 * Tells whether a path is a file.
 * @param file the path
 * @return whether it is
 */
function isFile(file: string): boolean {
	try {
		return statSync(file).isFile();
	} catch {
		return false;
	}
}

/**
 * Runs the tool.
 * @param tool      the tool: a name looked up on PATH's absolute folders, or an absolute path (findTool())
 * @param args      its arguments
 * @param cwd       the folder it runs in
 * @param timeoutMs how long it may run
 * @return what it gave; never rejects
 */
export function runTool(tool: string, args: string[], cwd: string, timeoutMs: number): Promise<ToolResult> {
	const program = findTool(tool, process.env.PATH ?? '', process.platform === 'win32', isFile);
	if (program === undefined) return Promise.resolve({ code: undefined, stdout: '', stderr: '', failure: 'missing' });
	return new Promise((resolve) => {
		execFile(program, args, { cwd, timeout: timeoutMs, maxBuffer: maxOutput, windowsHide: true, shell: false }, (error, stdout, stderr) => {
			if (!error) return resolve({ code: 0, stdout, stderr });
			const failure = error as NodeJS.ErrnoException & { killed?: boolean; code?: number | string };
			if (failure.code === 'ENOENT') return resolve({ code: undefined, stdout, stderr, failure: 'missing' });
			if (failure.killed) return resolve({ code: undefined, stdout, stderr, failure: 'timeout' });
			if (typeof failure.code === 'number') return resolve({ code: failure.code, stdout, stderr });
			resolve({ code: undefined, stdout, stderr: stderr || String(error.message), failure: 'failed' });
		});
	});
}
