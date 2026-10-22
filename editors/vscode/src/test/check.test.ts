/** Tests of reading `affrescoui check --json` and placing its problems, and of reading PNG sizes. */

import { strict as assert } from 'node:assert';
import { test } from 'node:test';
import { locatePlace, parseReport, placeProblem } from '../check';
import { pngSize } from '../png';
import { makeDoc } from '../resolver';

const text = `{
  "format": "affrescoui/screen", "version": 1,
  "fonts": { "default": "Missing.ttf" },
  "state": { "count": 0 },
  "root": { "type": "column", "children": [
    { "type": "label", "text": "{{state.cuont}}" },
    { "type": "button", "radiuss": 4 }
  ] }
}`;
const doc = makeDoc('file:///s.screen.json', text);

/**
 * Returns the text a problem is placed on.
 * @param problem what the tool reported
 * @return the text of its range
 */
function placed(problem: { source: string; place: string; message: string }): string {
	const result = placeProblem(doc, problem);
	return text.slice(result.start, result.end);
}

test('the report is read, and anything else is refused', () => {
	const report = { file: 's.screen.json', ok: false, problems: [{ source: 'schema', place: 'root', message: 'unknown property' }] };
	assert.deepEqual(parseReport(JSON.stringify(report)), report.problems);
	assert.deepEqual(parseReport(JSON.stringify({ file: 'a', ok: true, problems: [] })), []);
	assert.equal(parseReport('not json'), undefined);
	assert.equal(parseReport('{"problems": 3}'), undefined);
});

test('places are found in the text', () => {
	const found = locatePlace(doc.root!, 'root.children[1].radiuss');
	assert.ok(found?.whole);
	assert.equal(locatePlace(doc.root!, 'root.children[5]')?.whole, false);
});

test('problems land on what they are about', () => {
	assert.equal(placed({ source: 'schema', place: 'root.children[1].radiuss', message: 'unknown property' }), '4');
	assert.equal(
		placed({ source: 'checker', place: '', message: "root.children[0].text: 'state.cuont' doesn't exist (did you mean 'state.count'?)" }),
		'"{{state.cuont}}"',
	);
	assert.equal(placed({ source: 'format', place: '', message: 'AffrescoUI screen: fonts.default: no such file' }), '"Missing.ttf"');
	const parse = placeProblem(doc, { source: 'json', place: '', message: 'parse error at line 6, column 7: x' });
	assert.equal(parse.start, text.indexOf('{ "type": "label"') + 2);
	const nowhere = placeProblem(doc, { source: 'checker', place: '', message: 'something' });
	assert.equal(nowhere.start, 0);
	assert.ok(nowhere.end > nowhere.start);
});

test('PNG sizes are read from the header', () => {
	const png = new Uint8Array(24);
	png.set([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52]);
	new DataView(png.buffer).setUint32(16, 640);
	new DataView(png.buffer).setUint32(20, 480);
	assert.deepEqual(pngSize(png), { width: 640, height: 480 });
	assert.equal(pngSize(png.subarray(0, 20)), undefined);
	assert.equal(pngSize(new Uint8Array(24)), undefined);
});
