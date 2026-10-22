// Copies the repository's JSON Schemas (schema/) into out/schemas, where package.json's jsonValidation finds them,
// so the extension ships the schemas of the library it was built with.
import { copyFileSync, mkdirSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const from = join(here, '..', '..', '..', 'schema');
const to = join(here, '..', 'out', 'schemas');
mkdirSync(to, { recursive: true });
for (const name of readdirSync(from).filter((name) => name.endsWith('.schema.json'))) {
	copyFileSync(join(from, name), join(to, name));
}
