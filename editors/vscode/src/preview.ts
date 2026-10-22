/**
 * The live preview: a webview showing the PNG `affrescoui render` draws of a screen, drawn again when the screen is
 * saved. The webview runs no script and loads nothing: its policy allows only the image, given as a data: URI.
 */

import { randomBytes } from 'node:crypto';
import { mkdtempSync, promises as fs, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { basename, dirname, join } from 'node:path';
import * as vscode from 'vscode';
import { pngSize } from './png';
import { runTool } from './tool';

/** The largest PNG read, in bytes. */
const maxFileBytes = 64 * 1024 * 1024;

/**
 * Escapes text for HTML.
 * @param text any text
 * @return the text with &, <, >, " and ' escaped
 */
function escapeHtml(text: string): string {
	return text.replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
}

/** One screen's preview panel. */
export class Preview {
	private readonly panel: vscode.WebviewPanel;
	private readonly folder: string;
	private timer: NodeJS.Timeout | undefined;
	private running = false;
	private pending = false;
	private image = '';
	private disposed = false;

	/**
	 * Opens the preview of a screen and draws it.
	 * @param file      the screen's path on disk
	 * @param onDispose called when the panel is closed
	 */
	constructor(readonly file: string, onDispose: () => void) {
		this.folder = mkdtempSync(join(tmpdir(), 'affrescoui-preview-'));
		this.panel = vscode.window.createWebviewPanel('affrescoui.preview', `Preview ${basename(file)}`, vscode.ViewColumn.Beside, {
			enableScripts: false,
			enableCommandUris: false,
			localResourceRoots: [],
		});
		this.panel.onDidDispose(() => {
			this.disposed = true;
			clearTimeout(this.timer);
			rmSync(this.folder, { recursive: true, force: true });
			onDispose();
		});
		this.show('Rendering…');
		this.schedule();
	}

	/** Brings the panel to the front. */
	reveal(): void {
		this.panel.reveal(vscode.ViewColumn.Beside, true);
	}

	/** Draws the screen again shortly, once, however often it is asked. */
	schedule(): void {
		clearTimeout(this.timer);
		this.timer = setTimeout(() => void this.render(), 300);
	}

	/** Runs `affrescoui render` and shows its image, or why there is none. */
	private async render(): Promise<void> {
		if (this.running) {
			this.pending = true;
			return;
		}
		this.running = true;
		try {
			const config = vscode.workspace.getConfiguration('affrescoui');
			const out = join(this.folder, 'frame.png');
			await fs.rm(out, { force: true });
			const args = ['render', this.file, '-o', out, '--scale', String(config.get<number>('preview.scale', 1) === 2 ? 2 : 1)];
			const size = config.get<string>('preview.size', '').trim();
			if (/^[0-9]{1,5}x[0-9]{1,5}$/.test(size)) args.push('--size', size);
			const timeout = config.get<number>('preview.timeoutMs', 20000);
			const result = await runTool(config.get<string>('cliPath', 'affrescoui'), args, dirname(this.file), timeout);
			if (this.disposed) return;
			if (result.failure === 'missing') return this.show('The affrescoui tool wasn\'t found: put it on PATH or set affrescoui.cliPath.');
			if (result.failure === 'timeout') return this.show(`affrescoui render took longer than ${timeout} ms and was stopped.`);
			if (result.code !== 0) return this.show((result.stderr || result.stdout).trim() || `affrescoui render failed (exit ${result.code}).`);
			const stat = await fs.stat(out);
			if (stat.size > maxFileBytes) return this.show(`The render is ${stat.size} bytes, more than the preview shows.`);
			const data = await fs.readFile(out);
			const size2 = pngSize(data);
			const maxPixels = config.get<number>('preview.maxPixels', 4096);
			if (!size2) return this.show('affrescoui render didn\'t write a PNG.');
			if (size2.width > maxPixels || size2.height > maxPixels) {
				return this.show(`The render is ${size2.width}×${size2.height}, larger than affrescoui.preview.maxPixels (${maxPixels}).`);
			}
			const scale = args[args.indexOf('--scale') + 1] === '2' ? 2 : 1;
			this.image = `<img alt="The screen's first frame" width="${Math.round(size2.width / scale)}" height="${Math.round(size2.height / scale)}" src="data:image/png;base64,${data.toString('base64')}">`;
			this.show(`Drawn ${new Date().toLocaleTimeString()}; saved changes draw it again.`);
		} catch (error) {
			if (!this.disposed) this.show(String(error instanceof Error ? error.message : error));
		} finally {
			this.running = false;
			if (this.pending && !this.disposed) {
				this.pending = false;
				this.schedule();
			}
		}
	}

	/**
	 * Shows a line of text above the last image.
	 * @param message the text
	 */
	private show(message: string): void {
		const nonce = randomBytes(16).toString('base64');
		this.panel.webview.html = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src data:; style-src 'nonce-${nonce}';">
<style nonce="${nonce}">
body { margin: 0; padding: 12px; font-family: var(--vscode-font-family); color: var(--vscode-foreground); }
p { margin: 0 0 12px; white-space: pre-wrap; opacity: 0.8; }
img { max-width: 100%; height: auto; image-rendering: auto; box-shadow: 0 0 0 1px var(--vscode-panel-border); }
</style>
</head>
<body>
<p>${escapeHtml(message)}</p>
${this.image}
</body>
</html>`;
	}
}
