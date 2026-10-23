/**
 * The AffrescoUI extension: diagnostics from `affrescoui check --json`, the live preview, navigation and hovers. The
 * schemas and snippets are contributed by package.json alone.
 */

import { promises as fs, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import * as vscode from 'vscode';
import { Definitions, definitionsOf, describeKey } from './catalog';
import { parseReport, placeProblem } from './check';
import { member, nodeAt } from './json';
import { Doc, Place, Sym, definitions, includesOf, makeDoc, positionAt, references, symbolAt } from './resolver';
import { Preview } from './preview';
import { ToolResult, runTool } from './tool';

/** The most files a screen and its includes may be, for navigation. */
const maxFiles = 64;

/** The largest included file read, in bytes. */
const maxFileBytes = 8 * 1024 * 1024;

/** The parts seen included, each with the screen that includes it. */
const parents = new Map<string, string>();

/**
 * Tells whether a document is a screen.
 * @param document a document
 * @return true for a *.screen.json file
 */
function isScreen(document: vscode.TextDocument): boolean {
	return document.uri.fsPath.endsWith('.screen.json');
}

/**
 * Tells whether a document is a scenarios file.
 * @param document a document
 * @return true for a *.scenarios.json file
 */
function isScenarios(document: vscode.TextDocument): boolean {
	return document.uri.fsPath.endsWith('.scenarios.json');
}

/**
 * Tells whether navigation and hovers apply to a document: a screen, a part a screen includes, or a scenarios file.
 * @param document a document
 * @return true if they do
 */
function isAffresco(document: vscode.TextDocument): boolean {
	return isScreen(document) || isScenarios(document) || parents.has(document.uri.toString());
}

/**
 * Finds the screen a scenarios file tests, from its "screen" member.
 * @param document the scenarios file
 * @return the screen's URI, or undefined when it names none
 */
function screenOfScenarios(document: vscode.TextDocument): vscode.Uri | undefined {
	const screen = member(makeDoc(document.uri.toString(), document.getText()).root, 'screen')?.value;
	if (document.uri.scheme !== 'file' || screen?.kind !== 'string' || typeof screen.value !== 'string' || screen.value === '') return undefined;
	return vscode.Uri.file(resolve(dirname(document.uri.fsPath), screen.value));
}

/**
 * Reads a screen and the parts it includes, the open ones as they are in the editor.
 * @param document the screen, a part a screen includes, or a scenarios file
 * @return the screen first, then its parts, then the scenarios file when it was given
 */
async function loadDocs(document: vscode.TextDocument): Promise<Doc[]> {
	if (isScenarios(document)) {
		const scenarios = makeDoc(document.uri.toString(), document.getText());
		const screen = screenOfScenarios(document);
		if (!screen) return [scenarios];
		const docs = await loadFrom(screen);
		return [...docs.filter((d) => d.uri !== scenarios.uri), scenarios];
	}
	const start = isScreen(document) ? document.uri : vscode.Uri.parse(parents.get(document.uri.toString()) ?? document.uri.toString());
	const docs = await loadFrom(start);
	if (docs.length === 0 || docs[0].uri !== start.toString()) docs.unshift(makeDoc(document.uri.toString(), document.getText()));
	return docs;
}

/**
 * Reads a screen and the parts it includes, breadth first, the open ones as they are in the editor.
 * @param start the screen
 * @return what could be read, the screen first
 */
async function loadFrom(start: vscode.Uri): Promise<Doc[]> {
	const docs: Doc[] = [];
	const seen = new Set<string>();
	const queue: vscode.Uri[] = [start];
	while (queue.length > 0 && docs.length < maxFiles) {
		const uri = queue.shift()!;
		if (seen.has(uri.toString())) continue;
		seen.add(uri.toString());
		const open = vscode.workspace.textDocuments.find((d) => d.uri.toString() === uri.toString());
		let text: string;
		if (open) {
			text = open.getText();
		} else {
			if (uri.scheme !== 'file') continue;
			try {
				if ((await fs.stat(uri.fsPath)).size > maxFileBytes) continue;
				text = await fs.readFile(uri.fsPath, 'utf8');
			} catch {
				continue;
			}
		}
		const doc = makeDoc(uri.toString(), text);
		docs.push(doc);
		if (uri.scheme !== 'file') continue;
		for (const path of includesOf(doc)) {
			const part = vscode.Uri.file(resolve(dirname(uri.fsPath), path));
			if (part.toString() !== start.toString()) parents.set(part.toString(), start.toString());
			queue.push(part);
		}
	}
	return docs;
}

/**
 * Finds a document in a list by its URI.
 * @param docs     the documents
 * @param document the document
 * @return it, or a fresh one from the editor
 */
function docOf(docs: Doc[], document: vscode.TextDocument): Doc {
	return docs.find((d) => d.uri === document.uri.toString()) ?? makeDoc(document.uri.toString(), document.getText());
}

/**
 * Converts a range of a document to an editor location.
 * @param docs  the documents
 * @param place the range
 * @return the location
 */
function locationOf(docs: Doc[], place: Place): vscode.Location {
	const text = docs.find((d) => d.uri === place.uri)?.text ?? '';
	const start = positionAt(text, place.start);
	const end = positionAt(text, place.end);
	return new vscode.Location(vscode.Uri.parse(place.uri), new vscode.Range(start.line, start.character, end.line, end.character));
}

/** Runs `affrescoui check --json` on screens and shows what it finds. */
class Checker {
	private readonly timers = new Map<string, NodeJS.Timeout>();
	private readonly running = new Set<string>();
	private readonly pending = new Set<string>();
	private warnedMissing = false;

	/** @param collection where the problems go */
	constructor(private readonly collection: vscode.DiagnosticCollection) {}

	/**
	 * Checks a screen shortly, once, however often it is asked.
	 * @param document the screen
	 */
	schedule(document: vscode.TextDocument): void {
		if (!isScreen(document) || document.uri.scheme !== 'file') return;
		const key = document.uri.toString();
		clearTimeout(this.timers.get(key));
		this.timers.set(key, setTimeout(() => void this.check(document), 400));
	}

	/**
	 * Forgets a screen's problems.
	 * @param document the screen
	 */
	forget(document: vscode.TextDocument): void {
		clearTimeout(this.timers.get(document.uri.toString()));
		this.collection.delete(document.uri);
	}

	/**
	 * Checks a screen as it is on disk, once at a time: a check asked for while one runs follows it.
	 * @param document the screen
	 */
	private async check(document: vscode.TextDocument): Promise<void> {
		const key = document.uri.toString();
		const config = vscode.workspace.getConfiguration('affrescoui');
		if (!vscode.workspace.isTrusted || !config.get<boolean>('check.enabled', true) || document.isClosed) {
			this.collection.delete(document.uri);
			return;
		}
		if (this.running.has(key)) {
			this.pending.add(key);
			return;
		}
		this.running.add(key);
		try {
			const timeout = config.get<number>('check.timeoutMs', 15000);
			const file = document.uri.fsPath;
			const result = await runTool(config.get<string>('cliPath', 'affrescoui'), ['check', '--json', file], dirname(file), timeout);
			if (!document.isClosed) this.show(document, result, timeout);
		} finally {
			this.running.delete(key);
			if (this.pending.delete(key)) this.schedule(document);
		}
	}

	/**
	 * Shows what a check found: its problems, or a warning at the top when the check couldn't run.
	 * @param document the screen
	 * @param result   how `affrescoui check --json` ended
	 * @param timeout  how long it was allowed, in milliseconds
	 */
	private show(document: vscode.TextDocument, result: ToolResult, timeout: number): void {
		if (result.failure === 'missing') {
			this.collection.delete(document.uri);
			this.warnMissing();
			return;
		}
		const top = new vscode.Range(0, 0, 0, 0);
		if (result.failure === 'timeout') {
			this.collection.set(document.uri, [new vscode.Diagnostic(top, `affrescoui check took longer than ${timeout} ms and was stopped.`, vscode.DiagnosticSeverity.Warning)]);
			return;
		}
		const problems = result.code === 0 || result.code === 1 ? parseReport(result.stdout) : undefined;
		if (!problems) {
			const why = (result.stderr || result.stdout).trim().split('\n')[0] || `exit ${result.code}`;
			this.collection.set(document.uri, [new vscode.Diagnostic(top, `affrescoui check couldn't check this file: ${why}`, vscode.DiagnosticSeverity.Warning)]);
			return;
		}
		const doc = makeDoc(document.uri.toString(), document.getText());
		this.collection.set(
			document.uri,
			problems.map((problem) => {
				const placed = placeProblem(doc, problem);
				const range = new vscode.Range(document.positionAt(placed.start), document.positionAt(placed.end));
				const diagnostic = new vscode.Diagnostic(range, placed.message, vscode.DiagnosticSeverity.Error);
				diagnostic.source = `affrescoui ${placed.source}`.trim();
				return diagnostic;
			}),
		);
	}

	/** Says once per session that the tool wasn't found, rather than on every check. */
	private warnMissing(): void {
		if (this.warnedMissing) return;
		this.warnedMissing = true;
		void vscode.window.showWarningMessage(
			'AffrescoUI: the affrescoui tool wasn\'t found, so screens aren\'t checked. Put it on PATH or set affrescoui.cliPath.',
		);
	}
}

/**
 * Reads the definitions of the screen schema the extension ships, for hovers.
 * @param context the extension's context
 * @return the definitions, or none when the schema can't be read
 */
function loadCatalog(context: vscode.ExtensionContext): Definitions {
	try {
		return definitionsOf(JSON.parse(readFileSync(join(context.extensionPath, 'out', 'schemas', 'affrescoui-screen.schema.json'), 'utf8')));
	} catch {
		return {};
	}
}

/**
 * Opens the preview of a screen, or shows the one already open.
 * @param previews the open previews, by file
 * @param uri      the screen, else the active editor's
 */
function openPreview(previews: Map<string, Preview>, uri?: vscode.Uri): void {
	const target = uri instanceof vscode.Uri ? uri : vscode.window.activeTextEditor?.document.uri;
	if (!target || target.scheme !== 'file' || !target.fsPath.endsWith('.screen.json')) {
		return void vscode.window.showInformationMessage('AffrescoUI: open a .screen.json file to preview it.');
	}
	if (!vscode.workspace.isTrusted) {
		return void vscode.window.showInformationMessage('AffrescoUI: the preview runs affrescoui render, so it needs a trusted workspace.');
	}
	const existing = previews.get(target.fsPath);
	if (existing) return existing.reveal();
	previews.set(target.fsPath, new Preview(target.fsPath, () => previews.delete(target.fsPath)));
}

/**
 * Finds where the symbol at a position is defined: an include's file, or the places that define a name.
 * @param document the document
 * @param position where the symbol is
 * @return the definitions, or undefined when there is no symbol there
 */
async function definitionAt(document: vscode.TextDocument, position: vscode.Position): Promise<vscode.Location | vscode.Location[] | undefined> {
	if (!isAffresco(document)) return undefined;
	const docs = await loadDocs(document);
	const sym = symbolAt(docOf(docs, document), document.offsetAt(position));
	if (!sym) return undefined;
	if (sym.kind === 'include') return new vscode.Location(vscode.Uri.file(resolve(dirname(document.uri.fsPath), sym.name)), new vscode.Position(0, 0));
	return definitions(docs, sym).map((place) => locationOf(docs, place));
}

/**
 * Finds the places that use the symbol at a position.
 * @param document           the document
 * @param position           where the symbol is
 * @param includeDeclaration whether its definitions count
 * @return the places, or undefined when there is no symbol there
 */
async function referencesAt(document: vscode.TextDocument, position: vscode.Position, includeDeclaration: boolean): Promise<vscode.Location[] | undefined> {
	if (!isAffresco(document)) return undefined;
	const docs = await loadDocs(document);
	const sym = symbolAt(docOf(docs, document), document.offsetAt(position));
	if (!sym || sym.kind === 'include') return undefined;
	return references(docs, sym, includeDeclaration).map((place) => locationOf(docs, place));
}

/**
 * Describes what is at a position: a key from the catalog, else the definition of the symbol there.
 * @param document the document
 * @param position the position
 * @param catalog  the schema's definitions
 * @return the hover, or undefined when there is nothing to say
 */
async function hoverAt(document: vscode.TextDocument, position: vscode.Position, catalog: Definitions): Promise<vscode.Hover | undefined> {
	if (!isAffresco(document)) return undefined;
	const offset = document.offsetAt(position);
	const docs = await loadDocs(document);
	const doc = docOf(docs, document);
	const key = describeKey(doc, offset, catalog);
	if (key) return new vscode.Hover(new vscode.MarkdownString(key.markdown), new vscode.Range(document.positionAt(key.start), document.positionAt(key.end)));
	const sym = symbolAt(doc, offset);
	if (!sym || sym.kind === 'include') return undefined;
	const excerpt = definitionExcerpt(docs, sym);
	if (excerpt === undefined) return undefined;
	const markdown = new vscode.MarkdownString().appendMarkdown(`**${sym.kind}** \`${sym.name}\``).appendCodeblock(excerpt, 'json');
	return new vscode.Hover(markdown, new vscode.Range(document.positionAt(sym.start), document.positionAt(sym.end)));
}

/**
 * Returns the text that defines a symbol, cut to 600 characters: the value of the member it is the key of, or the
 * value around it.
 * @param docs the documents
 * @param sym  the symbol
 * @return the text, or undefined when it has no definition
 */
function definitionExcerpt(docs: Doc[], sym: Sym): string | undefined {
	const place = definitions(docs, sym)[0];
	if (!place) return undefined;
	const defined = docs.find((d) => d.uri === place.uri)!;
	const at = defined.root && nodeAt(defined.root, place.start);
	const value = at?.key ? at.key.value : at?.node.parent ?? at?.node;
	if (!value) return undefined;
	const excerpt = defined.text.slice(value.start, value.end);
	return excerpt.length > 600 ? excerpt.slice(0, 600) + '\n…' : excerpt;
}

/**
 * Starts the extension.
 * @param context the extension's context
 */
export function activate(context: vscode.ExtensionContext): void {
	const collection = vscode.languages.createDiagnosticCollection('affrescoui');
	const checker = new Checker(collection);
	const previews = new Map<string, Preview>();
	const catalog = loadCatalog(context);
	const selector: vscode.DocumentSelector = [
		{ language: 'json', scheme: 'file' },
		{ language: 'jsonc', scheme: 'file' },
	];
	const checkAll = () => vscode.workspace.textDocuments.forEach((document) => checker.schedule(document));

	context.subscriptions.push(
		collection,
		vscode.workspace.onDidOpenTextDocument((document) => checker.schedule(document)),
		vscode.workspace.onDidSaveTextDocument((document) => {
			checker.schedule(document);
			previews.get(document.uri.fsPath)?.schedule();
		}),
		vscode.workspace.onDidCloseTextDocument((document) => checker.forget(document)),
		vscode.workspace.onDidGrantWorkspaceTrust(checkAll),
		vscode.workspace.onDidChangeConfiguration((event) => {
			if (event.affectsConfiguration('affrescoui')) checkAll();
		}),

		vscode.commands.registerCommand('affrescoui.check', () => {
			const document = vscode.window.activeTextEditor?.document;
			if (document) checker.schedule(document);
		}),
		vscode.commands.registerCommand('affrescoui.preview', (uri?: vscode.Uri) => openPreview(previews, uri)),

		vscode.languages.registerDefinitionProvider(selector, { provideDefinition: definitionAt }),
		vscode.languages.registerReferenceProvider(selector, {
			provideReferences: (document, position, refContext) => referencesAt(document, position, refContext.includeDeclaration),
		}),
		vscode.languages.registerHoverProvider(selector, { provideHover: (document, position) => hoverAt(document, position, catalog) }),
	);
	checkAll();
}

/** Stops the extension: its subscriptions are disposed by VS Code. */
export function deactivate(): void {}
