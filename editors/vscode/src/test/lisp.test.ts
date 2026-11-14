/** Tests of Lisp screens: which files are checked, where a problem is placed, and nothing run when untrusted. */

import { strict as assert } from 'node:assert';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { test } from 'node:test';
import { checkArguments, placeLispProblem, screenNotation } from '../check';

/** A Lisp screen, as the tests read it. */
const screen = ['(defscreen main', '  (column#page :gap 4', '    (label#hello :text "x" :radiuss 4)))', ''].join('\n');

/** Its path. */
const file = '/apps/notes/index.screen.lisp';

test('a screen\'s notation comes from its name', () => {
	assert.equal(screenNotation('/a/index.screen.json'), 'json');
	assert.equal(screenNotation('/a/index.screen.lisp'), 'lisp');
	assert.equal(screenNotation('/a/state.lisp'), undefined); // a file a screen includes isn't checked on its own
	assert.equal(screenNotation('/a/notes.lisp'), undefined);
});

test('a Lisp problem is placed at its line and column', () => {
	const message = `AffrescoUI screen: ${file}: line 2, column 4: "colum" is not a node type`;
	const placed = placeLispProblem(screen, file, { source: 'lisp', place: '', message });
	assert.equal(screen.slice(placed.start, placed.end), 'column#page');
	const named = placeLispProblem(screen, file, { source: 'lisp', place: '', message: 'AffrescoUI lisp: index.screen.lisp: line 3, column 6: x' });
	assert.equal(screen.slice(named.start, named.end), 'label#hello');
});

test('a problem in another file of the app is placed at the start, not at its line here', () => {
	const message = 'AffrescoUI lisp: tally.lisp: line 3, column 5: a program\'s top level holds definitions';
	const placed = placeLispProblem(screen, file, { source: 'format', place: '', message });
	assert.deepEqual([placed.start, placed.end], [0, 1]);
});

test('a schema problem is placed at the keyword its place ends with', () => {
	const placed = placeLispProblem(screen, file, { source: 'schema', place: 'root.children[0].radiuss', message: 'unknown property' });
	assert.equal(screen.slice(placed.start, placed.end), ':radiuss');
	const nowhere = placeLispProblem(screen, file, { source: 'schema', place: 'root.children[0].width', message: 'too small' });
	assert.deepEqual([nowhere.start, nowhere.end], [0, 1]);
});

test('a check runs only on a screen, in a trusted workspace, when checks are on', () => {
	assert.deepEqual(checkArguments(file, true, true), ['check', '--json', file]);
	assert.deepEqual(checkArguments('/a/index.screen.json', true, true), ['check', '--json', '/a/index.screen.json']);
	assert.equal(checkArguments(file, false, true), undefined); // an untrusted workspace runs nothing
	assert.equal(checkArguments('/a/index.screen.json', false, true), undefined);
	assert.equal(checkArguments(file, true, false), undefined);
	assert.equal(checkArguments('/a/state.lisp', true, true), undefined);
});

test('only AffrescoUI\'s Lisp files open as Clojure, never every .lisp file', () => {
	const manifest = JSON.parse(readFileSync(join(__dirname, '..', '..', 'package.json'), 'utf8'));
	const [language] = manifest.contributes.languages;
	assert.equal(language.id, 'clojure');
	assert.equal(language.extensions, undefined);
	for (const pattern of language.filenamePatterns as string[]) assert.notEqual(pattern.replace(/^\*\*\//, ''), '*.lisp');
	assert.deepEqual(language.filenames, ['app.lisp', 'feature.lisp']);
});
