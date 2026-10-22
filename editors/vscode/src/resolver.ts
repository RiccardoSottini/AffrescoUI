/**
 * What a place in a screen refers to, and where that is defined and used: components (`"use"`), styles, tokens and
 * colours (`"@name"`), actions, screens (`"navigate"`), nodes (`"#id"`), state and computed paths, and includes.
 * Pure: it works on texts, so the tests run without VS Code.
 */

import { JsonNode, JsonScalar, member, memberOf, nodeAt, parseJson, pathOf, walk } from './json';

/** A screen or a part it includes, parsed. */
export interface Doc {
	uri: string;
	text: string;
	/** The parsed value, or undefined when the text isn't JSON (yet). */
	root: JsonNode | undefined;
}

/** What a name refers to. */
export type SymbolKind = 'component' | 'style' | 'token' | 'action' | 'screen' | 'node' | 'state' | 'computed' | 'include';

/** A name at a place: what it refers to, the name, and where it is written (end exclusive). */
export interface Sym {
	kind: SymbolKind;
	name: string;
	start: number;
	end: number;
}

/** A range in a document. */
export interface Place {
	uri: string;
	start: number;
	end: number;
}

/** The screen sections each kind is defined in. */
const sections: Partial<Record<SymbolKind, string[]>> = {
	component: ['components'],
	style: ['styles'],
	token: ['tokens', 'colors'],
	action: ['actions'],
	screen: ['screens'],
	state: ['state'],
	computed: ['computed'],
};

/** A state or computed path in an expression or a path string, not preceded by a name or a dot (`item.state`). */
const pathPattern = /(?<![\w$.])(state|computed)\.([A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)*)/g;

/** A token or colour, written as the whole string. */
const tokenPattern = /^@([A-Za-z_][\w.-]*)$/;

/** A node id at the start of a string: "#id" or "#id/property". */
const idPattern = /^#([A-Za-z0-9_][\w-]*)/;

/** The keys whose string value names something, when it is a name, and what it names. */
const namingKeys = new Map<string, SymbolKind>([
	['use', 'component'],
	['style', 'style'],
	['navigate', 'screen'],
]);

/**
 * Parses a document, keeping it even when its text isn't JSON.
 * @param uri  where it is
 * @param text its text
 * @return the document
 */
export function makeDoc(uri: string, text: string): Doc {
	try {
		return { uri, text, root: parseJson(text) };
	} catch {
		return { uri, text, root: undefined };
	}
}

/**
 * Tells whether a key holds a handler: an action's name, a step or steps.
 * @param key a member's key
 * @return true for onClick, onChange, onLoad... and the `run` step
 */
function handlerKey(key: string): boolean {
	return /^on[A-Z]/.test(key) || key === 'run';
}

/**
 * Lists the names a string value refers to.
 * @param text the document's text
 * @param node a string value in it
 * @return the names, in order
 */
export function symbolsInString(text: string, node: JsonScalar): Sym[] {
	if (node.kind !== 'string' || typeof node.value !== 'string') return [];
	const kind = wholeKind(node, node.value);
	if (kind) return [{ kind, name: node.value, start: node.start + 1, end: node.end - 1 }];
	return symbolsWithin(text, node, node.value);
}

/**
 * Tells whether a path is an include: "include" itself, or an item of it.
 * @param path the path of a value
 * @return true if it is
 */
function isInclude(path: (string | number)[]): boolean {
	return path[0] === 'include' && (path.length === 1 || (path.length === 2 && typeof path[1] === 'number'));
}

/**
 * Tells what a string names as a whole, from where it is: an include, a component, a style, a screen, a node's id or
 * an action.
 * @param node  a string value
 * @param value its text
 * @return the kind of name, or undefined when the string isn't a name as a whole
 */
function wholeKind(node: JsonScalar, value: string): SymbolKind | undefined {
	const key = memberOf(node)?.key;
	const path = pathOf(node);
	if (isInclude(path)) return 'include';
	const kind = key === undefined ? undefined : keyKind(key, /^[A-Za-z_][\w.-]*$/.test(value), node, path);
	if (kind) return kind;
	if (path[0] === 'keys' && path.length === 2) return 'action';
	return undefined;
}

/**
 * Tells what a string names from the key it is the value of.
 * @param key  the key
 * @param name whether the string is a name
 * @param node the string value
 * @param path its path
 * @return the kind of name, or undefined
 */
function keyKind(key: string, name: boolean, node: JsonScalar, path: (string | number)[]): SymbolKind | undefined {
	const named = namingKeys.get(key);
	if (named && name) return named;
	if (key === 'id' && node.parent?.kind === 'object' && path.length > 1) return 'node';
	if (handlerKey(key) && name) return 'action';
	return undefined;
}

/**
 * Lists the names written inside a string: a token, a node id, or state and computed paths.
 * @param text  the document's text
 * @param node  a string value in it
 * @param value its decoded text
 * @return the names, in order
 */
function symbolsWithin(text: string, node: JsonScalar, value: string): Sym[] {
	const inner = node.start + 1;
	const token = tokenPattern.exec(value);
	if (token) return [{ kind: 'token', name: token[1], start: inner + 1, end: node.end - 1 }];
	const id = idPattern.exec(value);
	if (id && text.startsWith(id[0], inner)) {
		return [{ kind: 'node', name: id[1], start: inner + 1, end: inner + id[0].length }];
	}
	const found: Sym[] = [];
	const raw = text.slice(inner, node.end - 1);
	for (const match of raw.matchAll(pathPattern)) {
		const start = inner + match.index! + match[1].length + 1;
		found.push({ kind: match[1] as SymbolKind, name: match[2], start, end: start + match[2].length });
	}
	return found;
}

/**
 * Tells whether a member's key defines a name: a key of a screen section, or a key nested in "state".
 * @param path the path of the member's value
 * @return the definition's kind and name, or undefined
 */
function definedBy(path: (string | number)[]): { kind: SymbolKind; name: string } | undefined {
	if (path.length < 2 || path.some((step) => typeof step === 'number')) return undefined;
	const section = path[0] as string;
	if (section === 'state' || (section === 'computed' && path.length === 2)) {
		return { kind: section, name: path.slice(1).join('.') };
	}
	if (path.length !== 2) return undefined;
	for (const [kind, names] of Object.entries(sections)) {
		if (names!.includes(section) && kind !== 'state' && kind !== 'computed') {
			return { kind: kind as SymbolKind, name: path[1] as string };
		}
	}
	return undefined;
}

/**
 * Finds the name at an offset: one a value refers to, or one a key defines.
 * @param doc    the document
 * @param offset an offset in its text
 * @return the name, or undefined
 */
export function symbolAt(doc: Doc, offset: number): Sym | undefined {
	if (!doc.root) return undefined;
	const at = nodeAt(doc.root, offset);
	if (!at) return undefined;
	if (at.key) {
		const defined = definedBy(pathOf(at.node));
		return defined && { ...defined, start: at.key.keyStart + 1, end: at.key.keyEnd - 1 };
	}
	if (at.node.kind !== 'string') return undefined;
	return symbolsInString(doc.text, at.node).find((sym) => offset >= sym.start && offset <= sym.end);
}

/**
 * Finds where a name is defined, in a screen and the parts it includes. An include is resolved by the caller.
 * @param docs the screen and its parts
 * @param sym  the name
 * @return its definitions: the section key (for a state path, the deepest key that exists) or the node's "id"
 */
export function definitions(docs: Doc[], sym: Pick<Sym, 'kind' | 'name'>): Place[] {
	const found: Place[] = [];
	for (const doc of docs) {
		if (doc.root?.kind !== 'object') continue;
		if (sym.kind === 'node') nodeDefinitions(doc, doc.root, sym.name, found);
		else if (sym.kind === 'state') stateDefinition(doc, doc.root, sym.name, found);
		else sectionDefinitions(doc, doc.root, sym, found);
	}
	return found;
}

/**
 * Finds the nodes of a document that have an id: the "id" values, the root's own excepted.
 * @param doc   the document
 * @param root  its value
 * @param name  the id
 * @param found where the places go
 */
function nodeDefinitions(doc: Doc, root: JsonNode, name: string, found: Place[]): void {
	walk(root, (node) => {
		const id = member(node, 'id');
		if (id && id.value.kind === 'string' && id.value.value === name && pathOf(node).length > 0) {
			found.push({ uri: doc.uri, start: id.value.start + 1, end: id.value.end - 1 });
		}
	});
}

/**
 * Finds a state path in a document's "state": the deepest key of it that exists.
 * @param doc   the document
 * @param root  its value
 * @param name  the path, e.g. "form.name"
 * @param found where the place goes
 */
function stateDefinition(doc: Doc, root: JsonNode, name: string, found: Place[]): void {
	let node: JsonNode | undefined = member(root, 'state')?.value;
	let deepest: Place | undefined;
	for (const step of name.split('.')) {
		const m = member(node, step);
		if (!m) break;
		deepest = { uri: doc.uri, start: m.keyStart + 1, end: m.keyEnd - 1 };
		node = m.value;
	}
	if (deepest) found.push(deepest);
}

/**
 * Finds a name among the keys of the screen sections its kind is defined in. A computed path is found by its first
 * step.
 * @param doc   the document
 * @param root  its value
 * @param sym   the name
 * @param found where the places go
 */
function sectionDefinitions(doc: Doc, root: JsonNode, sym: Pick<Sym, 'kind' | 'name'>, found: Place[]): void {
	const name = sym.kind === 'computed' ? sym.name.split('.')[0] : sym.name;
	for (const section of sections[sym.kind] ?? []) {
		const m = member(member(root, section)?.value, name);
		if (m) found.push({ uri: doc.uri, start: m.keyStart + 1, end: m.keyEnd - 1 });
	}
}

/**
 * Tells whether a name used somewhere is the name looked for.
 * @param used   the name used
 * @param wanted the name looked for
 * @return true when they are the same, or for state and computed paths when the used one goes inside the wanted one
 */
function sameName(used: Sym, wanted: Pick<Sym, 'kind' | 'name'>): boolean {
	if (used.kind !== wanted.kind) return false;
	if (used.kind === 'state' || used.kind === 'computed') {
		return used.name === wanted.name || used.name.startsWith(wanted.name + '.');
	}
	return used.name === wanted.name;
}

/**
 * Finds every place a name is used, in a screen and the parts it includes.
 * @param docs               the screen and its parts
 * @param sym                the name
 * @param includeDefinitions whether its definitions are listed too
 * @return the places, document by document in order
 */
export function references(docs: Doc[], sym: Pick<Sym, 'kind' | 'name'>, includeDefinitions: boolean): Place[] {
	const found: Place[] = [];
	for (const doc of docs) {
		if (!doc.root) continue;
		walk(doc.root, (node) => {
			if (node.kind !== 'string') return;
			for (const used of symbolsInString(doc.text, node)) {
				const definesNode = used.kind === 'node' && memberOf(node)?.key === 'id';
				if (sameName(used, sym) && !definesNode) found.push({ uri: doc.uri, start: used.start, end: used.end });
			}
		});
	}
	if (!includeDefinitions) return found;
	return [...definitions(docs, sym), ...found].sort((a, b) => (a.uri === b.uri ? a.start - b.start : 0));
}

/**
 * Lists the files a document includes, as written.
 * @param doc the document
 * @return the paths, relative to the document
 */
export function includesOf(doc: Doc): string[] {
	const include = member(doc.root, 'include')?.value;
	if (include?.kind === 'string') return [include.value as string];
	if (include?.kind === 'array') {
		return include.items.filter((item) => item.kind === 'string').map((item) => (item as JsonScalar).value as string);
	}
	return [];
}

/**
 * Converts an offset to a line and a character, both from 0, as editors count them (UTF-16 units).
 * @param text   the text
 * @param offset an offset in it
 * @return the position
 */
export function positionAt(text: string, offset: number): { line: number; character: number } {
	let line = 0;
	let lineStart = 0;
	for (let at = text.indexOf('\n'); at >= 0 && at < offset; at = text.indexOf('\n', at + 1)) {
		line++;
		lineStart = at + 1;
	}
	return { line, character: offset - lineStart };
}
