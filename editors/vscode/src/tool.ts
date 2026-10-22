/**
 * Runs the affrescoui command-line tool: only `check --json` and `render`, never through a shell, each with a time
 * limit and a limit on what it may print.
 */

import { execFile } from 'node:child_process';

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
 * Runs the tool.
 * @param tool      the tool: a name looked up on PATH, or a path
 * @param args      its arguments
 * @param cwd       the folder it runs in
 * @param timeoutMs how long it may run
 * @return what it gave; never rejects
 */
export function runTool(tool: string, args: string[], cwd: string, timeoutMs: number): Promise<ToolResult> {
	return new Promise((resolve) => {
		execFile(tool, args, { cwd, timeout: timeoutMs, maxBuffer: maxOutput, windowsHide: true, shell: false }, (error, stdout, stderr) => {
			if (!error) return resolve({ code: 0, stdout, stderr });
			const failure = error as NodeJS.ErrnoException & { killed?: boolean; code?: number | string };
			if (failure.code === 'ENOENT') return resolve({ code: undefined, stdout, stderr, failure: 'missing' });
			if (failure.killed) return resolve({ code: undefined, stdout, stderr, failure: 'timeout' });
			if (typeof failure.code === 'number') return resolve({ code: failure.code, stdout, stderr });
			resolve({ code: undefined, stdout, stderr: stderr || String(error.message), failure: 'failed' });
		});
	});
}
