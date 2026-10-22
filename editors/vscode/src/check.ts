/**
 * Reads what `affrescoui check --json` prints and finds where each problem is in the screen's text. Pure, so the tests
 * run without VS Code.
 */

import { JsonNode, member } from './json';
import { Doc } from './resolver';

/** One problem as the tool reports it. */
export interface Problem {
	source: string;
	place: string;
	message: string;
}

/** A problem placed in the text (end exclusive). */
export interface PlacedProblem extends Problem {
	start: number;
	end: number;
}

/**
 * Reads the tool's output.
 * @param stdout what `affrescoui check --json` printed
 * @return the problems, or undefined when the output isn't the tool's report
 */
export function parseReport(stdout: string): Problem[] | undefined {
	let report: unknown;
	try {
		report = JSON.parse(stdout);
	} catch {
		return undefined;
	}
	const problems = (report as { problems?: unknown })?.problems;
	if (!Array.isArray(problems)) return undefined;
	return problems
		.filter((p) => p && typeof p === 'object' && typeof p.message === 'string')
		.map((p) => ({ source: String(p.source ?? ''), place: typeof p.place === 'string' ? p.place : '', message: p.message }));
}

/**
 * Finds the value a place names, such as "root.children[0].text". A key with dots ("color.text" in tokens) is found
 * by trying the longest key first.
 * @param root  the document's value
 * @param place the place
 * @return the value reached, the member whose value it is, and whether the whole place was found; undefined when not
 *         even its first step was
 */
export function locatePlace(root: JsonNode, place: string): { node: JsonNode; key?: { start: number; end: number }; whole: boolean } | undefined {
	const steps: (string | number)[] = [];
	for (const match of place.matchAll(/\[(\d+)\]|\.?([^.[\]]+)/g)) steps.push(match[1] !== undefined ? Number(match[1]) : match[2]);
	if (steps.length === 0) return undefined;
	let node = root;
	let key: { start: number; end: number } | undefined;
	let at = 0;
	while (at < steps.length) {
		const step = steps[at];
		if (typeof step === 'number') {
			if (node.kind !== 'array' || step >= node.items.length) break;
			node = node.items[step];
			key = undefined;
			at++;
			continue;
		}
		let names = 0;
		while (at + names < steps.length && typeof steps[at + names] === 'string') names++;
		let found = false;
		for (let take = names; take > 0 && !found; take--) {
			const m = member(node, steps.slice(at, at + take).join('.'));
			if (m) {
				node = m.value;
				key = { start: m.keyStart, end: m.keyEnd };
				at += take;
				found = true;
			}
		}
		if (!found) break;
	}
	if (at === 0) return undefined;
	return { node, key, whole: at === steps.length };
}

/**
 * Finds where a problem is: from its place, else from a place at the start of its message ("root.children[0].text:
 * ..."), else from a "line L, column C" in it; else the start of the file.
 * @param doc     the screen
 * @param problem the problem
 * @return the problem with its range
 */
export function placeProblem(doc: Doc, problem: Problem): PlacedProblem {
	const candidates = [problem.place, ...problem.message.split(': ')].map((text) => text.trim()).filter(Boolean);
	let best: { node: JsonNode; key?: { start: number; end: number }; whole: boolean } | undefined;
	if (doc.root) {
		for (const candidate of candidates) {
			if (!/^[A-Za-z_$][\w$.\-[\]]*$/.test(candidate)) continue;
			const found = locatePlace(doc.root, candidate);
			if (found && (!best || (found.whole && !best.whole))) best = found;
			if (best?.whole) break;
		}
	}
	if (best) {
		const scalar = best.node.kind !== 'object' && best.node.kind !== 'array';
		const range = scalar || !best.key ? { start: best.node.start, end: best.node.end } : best.key;
		if (!scalar && !best.key) range.end = range.start + 1;
		return { ...problem, ...range };
	}
	const line = /line (\d+), column (\d+)/.exec(problem.message);
	if (line) {
		const lines = doc.text.split('\n');
		const index = Math.min(Number(line[1]), lines.length) - 1;
		let start = 0;
		for (let i = 0; i < index; i++) start += lines[i].length + 1;
		start += Math.max(0, Math.min(Number(line[2]) - 1, lines[index].length));
		return { ...problem, start, end: Math.min(start + 1, doc.text.length) };
	}
	return { ...problem, start: 0, end: Math.min(1, doc.text.length) };
}
