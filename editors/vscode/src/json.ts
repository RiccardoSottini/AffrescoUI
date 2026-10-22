/**
 * A JSON parser that keeps where every value is, for navigation, hovers and placing diagnostics. It reads JSON and,
 * like VS Code's JSON with comments, skips comments and accepts trailing commas.
 */

/** A JSON value and where it is in the text. */
export type JsonNode = JsonObject | JsonArray | JsonScalar;

/** What every value has: its kind, its range in the text (end exclusive) and its parent. */
interface JsonBase {
	start: number;
	end: number;
	parent?: JsonNode;
}

/** An object: its members in order. */
export interface JsonObject extends JsonBase {
	kind: 'object';
	members: JsonMember[];
}

/** One member of an object: its key, where the key is (quotes included), and its value. */
export interface JsonMember {
	key: string;
	keyStart: number;
	keyEnd: number;
	value: JsonNode;
}

/** An array: its items in order. */
export interface JsonArray extends JsonBase {
	kind: 'array';
	items: JsonNode[];
}

/** A string, number, boolean or null, with its value. */
export interface JsonScalar extends JsonBase {
	kind: 'string' | 'number' | 'boolean' | 'null';
	value: string | number | boolean | null;
}

/** A parse failure: what is wrong and where. */
export class JsonError extends Error {
	/**
	 * @param message what is wrong
	 * @param offset  where, in the text
	 */
	constructor(message: string, readonly offset: number) {
		super(message);
	}
}

/** The deepest nesting read, so a hostile file can't exhaust the stack. */
const maxDepth = 512;

/**
 * Parses JSON text.
 * @param text the text
 * @return the root value, with parents set
 * @throws JsonError when the text isn't JSON
 */
export function parseJson(text: string): JsonNode {
	let at = 0;

	/** Skips white space and comments. */
	function skip(): void {
		for (;;) {
			const c = text.charCodeAt(at);
			if (c === 0x20 || c === 0x0a || c === 0x0d || c === 0x09 || c === 0xfeff) {
				at++;
			} else if (c === 0x2f && text[at + 1] === '/') {
				const line = text.indexOf('\n', at);
				at = line < 0 ? text.length : line + 1;
			} else if (c === 0x2f && text[at + 1] === '*') {
				const close = text.indexOf('*/', at + 2);
				if (close < 0) throw new JsonError('unterminated comment', at);
				at = close + 2;
			} else {
				return;
			}
		}
	}

	/**
	 * Reads a string at the current position.
	 * @return its decoded value
	 */
	function readString(): string {
		const start = at++;
		let out = '';
		const special = /["\\\n]/g;
		for (;;) {
			special.lastIndex = at;
			const close = special.exec(text);
			if (!close || close[0] === '\n') throw new JsonError('unterminated string', start);
			out += text.slice(at, close.index);
			at = close.index;
			if (text[at] === '"') {
				at++;
				return out;
			}
			const escape = text[at + 1];
			const simple: Record<string, string> = { '"': '"', '\\': '\\', '/': '/', b: '\b', f: '\f', n: '\n', r: '\r', t: '\t' };
			if (escape in simple) {
				out += simple[escape];
				at += 2;
			} else if (escape === 'u' && /^[0-9a-fA-F]{4}$/.test(text.slice(at + 2, at + 6))) {
				out += String.fromCharCode(parseInt(text.slice(at + 2, at + 6), 16));
				at += 6;
			} else {
				throw new JsonError('bad escape in a string', at);
			}
		}
	}

	/**
	 * Reads a value at the current position.
	 * @param depth how deep it is
	 * @return the value
	 */
	function readValue(depth: number): JsonNode {
		if (depth > maxDepth) throw new JsonError('nested too deep', at);
		skip();
		const start = at;
		const c = text[at];
		if (c === '{') {
			at++;
			const node: JsonObject = { kind: 'object', start, end: start, members: [] };
			for (skip(); text[at] !== '}'; skip()) {
				if (text[at] !== '"') throw new JsonError('expected a key', at);
				const keyStart = at;
				const key = readString();
				const keyEnd = at;
				skip();
				if (text[at] !== ':') throw new JsonError("expected ':'", at);
				at++;
				const value = readValue(depth + 1);
				value.parent = node;
				node.members.push({ key, keyStart, keyEnd, value });
				skip();
				if (text[at] === ',') at++;
				else if (text[at] !== '}') throw new JsonError("expected ',' or '}'", at);
			}
			node.end = ++at;
			return node;
		}
		if (c === '[') {
			at++;
			const node: JsonArray = { kind: 'array', start, end: start, items: [] };
			for (skip(); text[at] !== ']'; skip()) {
				if (at >= text.length) throw new JsonError("expected ']'", at);
				const value = readValue(depth + 1);
				value.parent = node;
				node.items.push(value);
				skip();
				if (text[at] === ',') at++;
				else if (text[at] !== ']') throw new JsonError("expected ',' or ']'", at);
			}
			node.end = ++at;
			return node;
		}
		if (c === '"') {
			const value = readString();
			return { kind: 'string', start, end: at, value };
		}
		const word = /^(?:true|false|null|-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)/.exec(text.slice(at, at + 400));
		if (!word) throw new JsonError('expected a value', at);
		at += word[0].length;
		if (word[0] === 'true' || word[0] === 'false') return { kind: 'boolean', start, end: at, value: word[0] === 'true' };
		if (word[0] === 'null') return { kind: 'null', start, end: at, value: null };
		return { kind: 'number', start, end: at, value: Number(word[0]) };
	}

	const root = readValue(0);
	skip();
	if (at < text.length) throw new JsonError('text after the value', at);
	return root;
}

/**
 * Finds a member of an object by key.
 * @param node an object, or anything else
 * @param key  the key
 * @return the member, or undefined
 */
export function member(node: JsonNode | undefined, key: string): JsonMember | undefined {
	return node?.kind === 'object' ? node.members.find((m) => m.key === key) : undefined;
}

/**
 * Returns the member of an object whose value is a node.
 * @param node a value whose parent is an object
 * @return the member holding it, or undefined when its parent isn't an object
 */
export function memberOf(node: JsonNode): JsonMember | undefined {
	return node.parent?.kind === 'object' ? node.parent.members.find((m) => m.value === node) : undefined;
}

/**
 * Lists the keys and indexes from the root to a node.
 * @param node a value
 * @return e.g. ["root", "children", 0, "text"]
 */
export function pathOf(node: JsonNode): (string | number)[] {
	const path: (string | number)[] = [];
	for (let at: JsonNode = node; at.parent; at = at.parent) {
		const parent: JsonNode = at.parent;
		if (parent.kind === 'object') path.unshift(parent.members.find((m) => m.value === at)!.key);
		else if (parent.kind === 'array') path.unshift(parent.items.indexOf(at));
	}
	return path;
}

/**
 * Calls a function on every value, depth first.
 * @param node  the root
 * @param visit called with each value
 */
export function walk(node: JsonNode, visit: (node: JsonNode) => void): void {
	visit(node);
	if (node.kind === 'object') for (const m of node.members) walk(m.value, visit);
	else if (node.kind === 'array') for (const item of node.items) walk(item, visit);
}

/**
 * Finds the innermost value at an offset, and whether the offset is on a key.
 * @param root   the root
 * @param offset an offset in the text
 * @return the value (for a key, the value of its member) and the member when on its key, or undefined
 */
export function nodeAt(root: JsonNode, offset: number): { node: JsonNode; key?: JsonMember } | undefined {
	if (offset < root.start || offset > root.end) return undefined;
	let node: JsonNode = root;
	for (;;) {
		if (node.kind === 'object') {
			const m = node.members.find((m) => offset >= m.keyStart && offset <= m.value.end);
			if (!m) return { node };
			if (offset < m.keyEnd) return { node: m.value, key: m };
			node = m.value;
		} else if (node.kind === 'array') {
			const item = node.items.find((item) => offset >= item.start && offset <= item.end);
			if (!item) return { node };
			node = item;
		} else {
			return { node };
		}
	}
}
