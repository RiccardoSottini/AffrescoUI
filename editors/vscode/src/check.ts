/**
 * Reads what `affrescoui check --json` prints and finds where each problem is in the screen's text. Pure, so the tests
 * run without VS Code.
 */

import { JsonMember, JsonNode, member } from './json';
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

/** A value a place names: the value, the member it is the value of, and whether the whole place was found. */
export interface FoundPlace {
	node: JsonNode;
	key?: { start: number; end: number };
	whole: boolean;
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
export function locatePlace(root: JsonNode, place: string): FoundPlace | undefined {
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
		const found = longestMember(node, steps, at);
		if (!found) break;
		node = found.member.value;
		key = { start: found.member.keyStart, end: found.member.keyEnd };
		at += found.take;
	}
	if (at === 0) return undefined;
	return { node, key, whole: at === steps.length };
}

/**
 * Finds the member that the most names of a place, joined with dots, are the key of.
 * @param node  the value the names are looked up in
 * @param steps the place's steps
 * @param at    the first name
 * @return the member and how many names its key took, or undefined when not even the first name is a key
 */
function longestMember(node: JsonNode, steps: (string | number)[], at: number): { member: JsonMember; take: number } | undefined {
	let names = 0;
	while (at + names < steps.length && typeof steps[at + names] === 'string') names++;
	for (let take = names; take > 0; take--) {
		const m = member(node, steps.slice(at, at + take).join('.'));
		if (m) return { member: m, take };
	}
	return undefined;
}

/**
 * Finds where a problem is: from its place, else from a place at the start of its message ("root.children[0].text:
 * ..."), else from a "line L, column C" in it; else the start of the file.
 * @param doc     the screen
 * @param problem the problem
 * @return the problem with its range
 */
export function placeProblem(doc: Doc, problem: Problem): PlacedProblem {
	const best = doc.root && bestPlace(doc.root, problem);
	if (best) return { ...problem, ...rangeOfPlace(best) };
	const line = /line (\d+), column (\d+)/.exec(problem.message);
	if (line) {
		const start = offsetOfLine(doc.text, Number(line[1]), Number(line[2]));
		return { ...problem, start, end: Math.min(start + 1, doc.text.length) };
	}
	return { ...problem, start: 0, end: Math.min(1, doc.text.length) };
}

/**
 * Finds the value a problem is about, from its place and the places at the start of its message: the first found
 * whole, else the first found in part.
 * @param root    the screen's value
 * @param problem the problem
 * @return the place found, or undefined
 */
function bestPlace(root: JsonNode, problem: Problem): FoundPlace | undefined {
	const candidates = [problem.place, ...problem.message.split(': ')].map((text) => text.trim()).filter(Boolean);
	let best: FoundPlace | undefined;
	for (const candidate of candidates) {
		if (!/^[A-Za-z_$][\w$.\-[\]]*$/.test(candidate)) continue;
		const found = locatePlace(root, candidate);
		if (found && (!best || (found.whole && !best.whole))) best = found;
		if (best?.whole) break;
	}
	return best;
}

/**
 * Returns the range to underline for a value found: a scalar whole, else its key, else the bracket it opens with.
 * @param found the value found
 * @return the range
 */
function rangeOfPlace(found: FoundPlace): { start: number; end: number } {
	const scalar = found.node.kind !== 'object' && found.node.kind !== 'array';
	if (scalar || !found.key) {
		return { start: found.node.start, end: scalar ? found.node.end : found.node.start + 1 };
	}
	return found.key;
}

/**
 * Converts a line and a column, both from 1, to an offset; past the end, the last line and its end.
 * @param text   the text
 * @param line   the line
 * @param column the column
 * @return the offset
 */
function offsetOfLine(text: string, line: number, column: number): number {
	const lines = text.split('\n');
	const index = Math.min(line, lines.length) - 1;
	let start = 0;
	for (let i = 0; i < index; i++) start += lines[i].length + 1;
	return start + Math.max(0, Math.min(column - 1, lines[index].length));
}
