/** Tests of finding the affrescoui tool: by absolute path only, never from the screen's folder or a relative one. */

import { strict as assert } from 'node:assert';
import { join } from 'node:path';
import { test } from 'node:test';
import { findTool } from '../tool';

/** Files that exist, for the tests. */
const files = new Set([join('/opt/aui', 'affrescoui'), join('/opt/aui', 'affrescoui.exe'), join('screens', 'affrescoui.exe')]);

/**
 * Tells whether a test file exists.
 * @param file the path
 * @return whether it does
 */
function exists(file: string): boolean {
	return files.has(file);
}

test('a bare name is found in PATH\'s absolute folders only', () => {
	assert.equal(findTool('affrescoui', '/usr/bin:/opt/aui', false, exists), join('/opt/aui', 'affrescoui'));
	assert.equal(findTool('affrescoui', '.:screens:/opt/aui', false, exists), join('/opt/aui', 'affrescoui'));
	assert.equal(findTool('affrescoui', 'screens;;.;/opt/aui', true, exists), join('/opt/aui', 'affrescoui.exe'));
	assert.equal(findTool('affrescoui', 'screens;.', true, exists), undefined); // the screen's folder is never searched
	assert.equal(findTool('affrescoui', '', false, exists), undefined);
});

test('a path is run only when it is absolute', () => {
	assert.equal(findTool('/opt/aui/affrescoui', '', false, exists), '/opt/aui/affrescoui');
	assert.equal(findTool('screens/affrescoui.exe', '/opt/aui', true, exists), undefined);
	assert.equal(findTool('.\\affrescoui.exe', '/opt/aui', true, exists), undefined);
	assert.equal(findTool('', '/opt/aui', false, exists), undefined);
});
