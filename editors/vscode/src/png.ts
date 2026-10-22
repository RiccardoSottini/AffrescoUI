/** Reads the size of a PNG image, so the preview refuses one too large to show before drawing it. */

/** The eight bytes every PNG starts with. */
const signature = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];

/**
 * Reads a PNG's width and height from its header.
 * @param data the file's bytes
 * @return the size in pixels, or undefined when the bytes aren't a PNG
 */
export function pngSize(data: Uint8Array): { width: number; height: number } | undefined {
	if (data.length < 24 || signature.some((byte, index) => data[index] !== byte)) return undefined;
	if (String.fromCharCode(data[12], data[13], data[14], data[15]) !== 'IHDR') return undefined;
	const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
	return { width: view.getUint32(16), height: view.getUint32(20) };
}
