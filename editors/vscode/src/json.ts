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

/** What the one-character escapes of a string stand for. */
const simpleEscapes: Record<string, string> = { '"': '"', '\\': '\\', '/': '/', b: '\b', f: '\f', n: '\n', r: '\r', t: '\t' };

/** A number, true, false or null at the start of a text. It is matched on the next 400 characters only, so reading
 * each value doesn't copy the rest of the text. */
const word = /^(?:true|false|null|-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)/;

/** Reads one JSON text, keeping where it is in it. */
class Parser {
	/** Where it is in the text. */
	private at = 0;

	/** @param text the text */
	constructor(private readonly text: string) {}

	/**
	 * Reads the whole text.
	 * @return the root value, with parents set
	 * @throws JsonError when the text isn't JSON
	 */
	parse(): JsonNode {
		const root = this.readValue(0);
		this.skip();
		if (this.at < this.text.length) throw new JsonError('text after the value', this.at);
		return root;
	}

	/** Skips white space and comments. */
	private skip(): void {
		const text = this.text;
		for (;;) {
			const c = text.charCodeAt(this.at);
			if (c === 0x20 || c === 0x0a || c === 0x0d || c === 0x09 || c === 0xfeff) {
				this.at++;
			} else if (c === 0x2f && text[this.at + 1] === '/') {
				const line = text.indexOf('\n', this.at);
				this.at = line < 0 ? text.length : line + 1;
			} else if (c === 0x2f && text[this.at + 1] === '*') {
				const close = text.indexOf('*/', this.at + 2);
				if (close < 0) throw new JsonError('unterminated comment', this.at);
				this.at = close + 2;
			} else {
				return;
			}
		}
	}

	/**
	 * Reads a value at the current position.
	 * @param depth how deep it is
	 * @return the value
	 */
	private readValue(depth: number): JsonNode {
		if (depth > maxDepth) throw new JsonError('nested too deep', this.at);
		this.skip();
		const start = this.at;
		switch (this.text[start]) {
			case '{':
				return this.readObject(depth);
			case '[':
				return this.readArray(depth);
			case '"': {
				const value = this.readString();
				return { kind: 'string', start, end: this.at, value };
			}
			default:
				return this.readWord();
		}
	}

	/**
	 * Reads an object at the current position, its opening brace.
	 * @param depth how deep it is
	 * @return the object
	 */
	private readObject(depth: number): JsonObject {
		const text = this.text;
		const node: JsonObject = { kind: 'object', start: this.at++, end: 0, members: [] };
		for (this.skip(); text[this.at] !== '}'; this.skip()) {
			if (text[this.at] !== '"') throw new JsonError('expected a key', this.at);
			const keyStart = this.at;
			const key = this.readString();
			const keyEnd = this.at;
			this.skip();
			if (text[this.at] !== ':') throw new JsonError("expected ':'", this.at);
			this.at++;
			const value = this.readValue(depth + 1);
			value.parent = node;
			node.members.push({ key, keyStart, keyEnd, value });
			this.readSeparator('}');
		}
		node.end = ++this.at;
		return node;
	}

	/**
	 * Reads an array at the current position, its opening bracket.
	 * @param depth how deep it is
	 * @return the array
	 */
	private readArray(depth: number): JsonArray {
		const node: JsonArray = { kind: 'array', start: this.at++, end: 0, items: [] };
		for (this.skip(); this.text[this.at] !== ']'; this.skip()) {
			if (this.at >= this.text.length) throw new JsonError("expected ']'", this.at);
			const value = this.readValue(depth + 1);
			value.parent = node;
			node.items.push(value);
			this.readSeparator(']');
		}
		node.end = ++this.at;
		return node;
	}

	/**
	 * Reads what follows a member or an item: a comma, or the closing character, which is left to be read.
	 * @param close '}' or ']'
	 */
	private readSeparator(close: '}' | ']'): void {
		this.skip();
		if (this.text[this.at] === ',') this.at++;
		else if (this.text[this.at] !== close) throw new JsonError(`expected ',' or '${close}'`, this.at);
	}

	/**
	 * Reads a string at the current position, its opening quote.
	 * @return its decoded value
	 */
	private readString(): string {
		const text = this.text;
		const start = this.at++;
		let out = '';
		const special = /["\\\n]/g;
		for (;;) {
			special.lastIndex = this.at;
			const close = special.exec(text);
			if (!close || close[0] === '\n') throw new JsonError('unterminated string', start);
			out += text.slice(this.at, close.index);
			this.at = close.index;
			if (text[this.at] === '"') {
				this.at++;
				return out;
			}
			out += this.readEscape();
		}
	}

	/**
	 * Reads an escape in a string at the current position, its backslash.
	 * @return the character it stands for
	 */
	private readEscape(): string {
		const text = this.text;
		const escape = text[this.at + 1];
		if (escape in simpleEscapes) {
			this.at += 2;
			return simpleEscapes[escape];
		}
		const hex = text.slice(this.at + 2, this.at + 6);
		if (escape !== 'u' || !/^[0-9a-fA-F]{4}$/.test(hex)) throw new JsonError('bad escape in a string', this.at);
		this.at += 6;
		return String.fromCharCode(parseInt(hex, 16));
	}

	/**
	 * Reads a number, true, false or null at the current position.
	 * @return the value
	 */
	private readWord(): JsonScalar {
		const start = this.at;
		const found = word.exec(this.text.slice(start, start + 400));
		if (!found) throw new JsonError('expected a value', start);
		this.at += found[0].length;
		const end = this.at;
		if (found[0] === 'true' || found[0] === 'false') return { kind: 'boolean', start, end, value: found[0] === 'true' };
		if (found[0] === 'null') return { kind: 'null', start, end, value: null };
		return { kind: 'number', start, end, value: Number(found[0]) };
	}
}

/**
 * Parses JSON text.
 * @param text the text
 * @return the root value, with parents set
 * @throws JsonError when the text isn't JSON
 */
export function parseJson(text: string): JsonNode {
	return new Parser(text).parse();
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
