/** Tests of the JSON parser, the resolver and the catalog hovers, run with `node --test` after `npm run compile`. */

import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { definitionsOf, describeKey } from '../catalog';
import { JsonError, nodeAt, parseJson, pathOf } from '../json';
import { Doc, definitions, includesOf, makeDoc, positionAt, references, symbolAt } from '../resolver';

/** A screen using every kind of name the resolver knows. */
const screen = `{
  // comments and trailing commas are accepted
  "format": "affrescoui/screen", "version": 2,
  "include": ["parts/styles.json"],
  "tokens": { "gap": 8 },
  "colors": { "accent": "#3366ff" },
  "state": { "count": 0, "user": { "name": "Ada" } },
  "computed": { "double": "{{state.count * 2}}" },
  "actions": { "add": [{ "set": "state.count", "to": "{{state.count + 1}}" }] },
  "keys": { "Ctrl+A": "add" },
  "components": { "Chip": { "params": {}, "node": { "type": "label", "text": "chip" } } },
  "root": { "type": "column", "spacing": "@gap", "children": [
    { "type": "label", "id": "total", "text": "{{state.count}} / {{computed.double}} / {{state.user.name}}" },
    { "type": "button", "id": "add", "text": "Add", "style": "primary", "onClick": "add" },
    { "use": "Chip" },
    { "type": "label", "style": "{{state.count > 1 ? 'big' : 'small'}}", "text": "#total" },
  ] },
}`;

/** The part the screen includes. */
const part = `{ "styles": { "primary": { "color": "@accent" } } }`;

const docs: Doc[] = [makeDoc('file:///s.screen.json', screen), makeDoc('file:///parts/styles.json', part)];

/**
 * Finds the offset of the n-th occurrence of a text in the screen, plus a shift.
 * @param needle the text
 * @param shift  added to the offset
 * @param nth    which occurrence, from 0
 * @param text   the text searched
 * @return the offset
 */
function at(needle: string, shift = 1, nth = 0, text = screen): number {
	let index = -1;
	for (let i = 0; i <= nth; i++) index = text.indexOf(needle, index + 1);
	assert.ok(index >= 0, `${needle} not found`);
	return index + shift;
}

/**
 * Returns the text of each place.
 * @param places ranges in the documents
 * @return the texts, with their document's last path part
 */
function texts(places: { uri: string; start: number; end: number }[]): string[] {
	return places.map((p) => `${p.uri.split('/').pop()}:${docs.find((d) => d.uri === p.uri)!.text.slice(p.start, p.end)}`);
}

test('the parser keeps offsets, skips comments and accepts trailing commas', () => {
	const root = parseJson(screen);
	assert.equal(root.kind, 'object');
	const found = nodeAt(root, at('"Ada"', 2));
	assert.ok(found && !found.key);
	assert.deepEqual(pathOf(found.node), ['state', 'user', 'name']);
	assert.equal(screen.slice(found.node.start, found.node.end), '"Ada"');
	const key = nodeAt(root, at('"spacing"', 2));
	assert.equal(key?.key?.key, 'spacing');
});

test('the parser reports where bad JSON is', () => {
	assert.throws(() => parseJson('{ "a": }'), JsonError);
	assert.throws(() => parseJson('{ "a": 1 } x'), JsonError);
	assert.throws(() => parseJson('['.repeat(600) + ']'.repeat(600)), JsonError);
	assert.equal(makeDoc('file:///bad.json', '{').root, undefined);
});

test('names are found at the cursor', () => {
	const doc = docs[0];
	assert.deepEqual(symbolAt(doc, at('"Chip" }', 2)), { kind: 'component', name: 'Chip', start: at('"Chip" }'), end: at('"Chip" }', 5) });
	assert.equal(symbolAt(doc, at('"@gap"', 3))?.name, 'gap');
	assert.equal(symbolAt(doc, at('"@gap"', 3))?.kind, 'token');
	assert.equal(symbolAt(doc, at('"#total"', 3))?.kind, 'node');
	assert.equal(symbolAt(doc, at('"onClick": "add"', 13))?.kind, 'action');
	assert.equal(symbolAt(doc, at('"Ctrl+A": "add"', 12))?.kind, 'action');
	assert.equal(symbolAt(doc, at('"primary"', 2))?.kind, 'style');
	assert.equal(symbolAt(doc, at('"parts/styles.json"', 2))?.kind, 'include');
	const path = symbolAt(doc, at('state.user.name', 8));
	assert.equal(path?.kind, 'state');
	assert.equal(path?.name, 'user.name');
	assert.equal(symbolAt(doc, at('computed.double}}', 10))?.name, 'double');
	assert.equal(symbolAt(doc, at("'big'", 1)), undefined, 'an expression in "style" is no style name');
	assert.deepEqual(includesOf(doc), ['parts/styles.json']);
});

test('keys of the sections define names', () => {
	const doc = docs[0];
	assert.deepEqual(symbolAt(doc, at('"Chip": {', 2)), { kind: 'component', name: 'Chip', start: at('"Chip": {'), end: at('"Chip": {', 5) });
	assert.equal(symbolAt(doc, at('"name": "Ada"', 2))?.name, 'user.name');
	assert.equal(symbolAt(doc, at('"type": "column"', 2)), undefined);
});

test('definitions are found across the included parts', () => {
	assert.deepEqual(texts(definitions(docs, { kind: 'component', name: 'Chip' })), ['s.screen.json:Chip']);
	assert.deepEqual(texts(definitions(docs, { kind: 'style', name: 'primary' })), ['styles.json:primary']);
	assert.deepEqual(texts(definitions(docs, { kind: 'token', name: 'accent' })), ['s.screen.json:accent']);
	assert.deepEqual(texts(definitions(docs, { kind: 'node', name: 'total' })), ['s.screen.json:total']);
	assert.deepEqual(texts(definitions(docs, { kind: 'state', name: 'user.name' })), ['s.screen.json:name']);
	assert.deepEqual(texts(definitions(docs, { kind: 'state', name: 'user.missing' })), ['s.screen.json:user']);
	assert.deepEqual(definitions(docs, { kind: 'action', name: 'nothing' }), []);
});

test('references are found across the included parts', () => {
	assert.deepEqual(texts(references(docs, { kind: 'action', name: 'add' }, false)), ['s.screen.json:add', 's.screen.json:add']);
	assert.deepEqual(texts(references(docs, { kind: 'token', name: 'accent' }, false)), ['styles.json:accent']);
	assert.deepEqual(texts(references(docs, { kind: 'node', name: 'total' }, false)), ['s.screen.json:total']);
	const count = texts(references(docs, { kind: 'state', name: 'count' }, true));
	assert.equal(count.length, 6);
	assert.ok(count.includes('s.screen.json:count'));
	assert.deepEqual(texts(references(docs, { kind: 'state', name: 'user' }, false)), ['s.screen.json:user.name']);
});

test('offsets convert to lines and characters', () => {
	assert.deepEqual(positionAt('ab\ncd', 4), { line: 1, character: 1 });
	assert.deepEqual(positionAt('ab\r\ncd', 5), { line: 1, character: 1 });
});

test('hovers describe sections, node properties and steps from the catalog', () => {
	const defs = definitionsOf({
		$defs: {
			v2: {
				screen: { properties: { tokens: { description: 'Named values.' } } },
				node: { properties: { spacing: { $ref: '#/$defs/v2/length' }, text: { anyOf: [{ description: 'Text shown.' }] } } },
				length: { description: 'A length in points.' },
				step: { description: 'One of: set(+to), push(+value).', properties: { set: {}, to: {}, push: {}, value: {} } },
			},
		},
	});
	const doc = docs[0];
	assert.match(describeKey(doc, at('"tokens"', 2), defs)!.markdown, /Screen section `tokens`.*Named values/);
	assert.match(describeKey(doc, at('"spacing"', 2), defs)!.markdown, /Node property `spacing`.*points/);
	assert.match(describeKey(doc, at('"text": "chip"', 2), defs)!.markdown, /Text shown/, 'a component\'s node is a node');
	assert.match(describeKey(doc, at('"set"', 2), defs)!.markdown, /Step.*set\(\+to\)/);
	assert.match(describeKey(doc, at('"to"', 2), defs)!.markdown, /Argument of the `set` step/);
	assert.equal(describeKey(doc, at('"Ada"', 2), defs), undefined);
});
