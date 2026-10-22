/**
 * Descriptions from the catalog, as the screen's JSON Schema carries it: a screen's sections, a node's properties and
 * the steps. Pure, so the tests run without VS Code.
 */

import { JsonMember, JsonNode, member, nodeAt, pathOf } from './json';
import { Doc } from './resolver';

/** The parts of a JSON Schema the hovers read. */
interface Schema {
	description?: string;
	$ref?: string;
	anyOf?: Schema[];
	oneOf?: Schema[];
	properties?: Record<string, Schema>;
}

/** The screen schema's version 1 definitions. */
export type Definitions = Record<string, Schema>;

/**
 * Reads the definitions out of the screen's JSON Schema.
 * @param schema the parsed schema/affrescoui-screen.schema.json
 * @return its version 1 definitions, or an empty set when it has none
 */
export function definitionsOf(schema: unknown): Definitions {
	return ((schema as { $defs?: { v1?: Definitions } })?.$defs?.v1 ?? {}) as Definitions;
}

/**
 * Returns a schema's description: its own, else the definition it refers to, else its first option's.
 * @param schema a schema
 * @param defs   the definitions, for $ref
 * @param depth  how many references were followed
 * @return the description, or ""
 */
function describe(schema: Schema | undefined, defs: Definitions, depth = 0): string {
	if (!schema || depth > 8) return '';
	if (schema.description) return schema.description;
	if (schema.$ref?.startsWith('#/$defs/v1/')) return describe(defs[schema.$ref.slice('#/$defs/v1/'.length)], defs, depth + 1);
	for (const option of [...(schema.anyOf ?? []), ...(schema.oneOf ?? [])]) {
		const text = describe(option, defs, depth + 1);
		if (text) return text;
	}
	return '';
}

/** The sections and keys whose values are nodes, or name → node maps. */
const nodeMaps = new Set(['components', 'screens', 'templates']);

/**
 * Tells whether an object is a node: the root, a child, a component (or its "node"), a screen or a template, or
 * anything with a "type" or a "use".
 * @param node an object
 * @return true if it is one
 */
function isNode(node: JsonNode): boolean {
	if (member(node, 'type') || member(node, 'use')) return true;
	const path = pathOf(node);
	const last = path[path.length - 1];
	const before = path[path.length - 2];
	if (path.length === 1 && last === 'root') return true;
	if (typeof last === 'number' && before === 'children') return true;
	if (path.length === 3 && path[0] === 'components' && last === 'node') return true;
	return path.length === 2 && nodeMaps.has(String(before));
}

/**
 * Finds a step's entry in the step schema's description ("set(+to), push(+value), ...").
 * @param description the step schema's description
 * @param name        the step
 * @return e.g. "set(+to)", or the name alone
 */
function stepEntry(description: string, name: string): string {
	const entry = new RegExp(`(?:^|[,:;] )(${name}(?:\\([^)]*\\))?)(?=,|;|$)`).exec(description);
	return entry ? entry[1] : name;
}

/**
 * Describes the key at an offset, from the catalog.
 * @param doc    the screen
 * @param offset an offset in its text
 * @param defs   the schema's definitions
 * @return Markdown and the key's range, or undefined when the offset isn't on a key the catalog describes
 */
export function describeKey(doc: Doc, offset: number, defs: Definitions): { markdown: string; start: number; end: number } | undefined {
	if (!doc.root) return undefined;
	const at = nodeAt(doc.root, offset);
	if (!at?.key) return undefined;
	const key: JsonMember = at.key;
	const owner = at.node.parent!;
	const range = { start: key.keyStart, end: key.keyEnd };
	let text = '';
	if (owner === doc.root) {
		text = describe(defs.screen?.properties?.[key.key], defs);
		if (text) return { markdown: `**Screen section \`${key.key}\`**: ${text}`, ...range };
	}
	if (isNode(owner)) {
		text = describe(defs.node?.properties?.[key.key], defs);
		if (text) return { markdown: `**Node property \`${key.key}\`**: ${text}`, ...range };
	}
	const steps = defs.step;
	if (owner.kind === 'object' && steps?.properties && steps.description) {
		const first = owner.members[0]?.key;
		if (first !== undefined && first in steps.properties && key.key in steps.properties) {
			const entry = stepEntry(steps.description, first);
			if (key.key === first) return { markdown: `**Step** \`${entry}\`: the keys in brackets are its arguments.`, ...range };
			return { markdown: `**Argument of the \`${first}\` step** (\`${entry}\`).`, ...range };
		}
	}
	return undefined;
}
