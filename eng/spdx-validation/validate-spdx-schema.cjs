// Validate the unchanged official schema through its recommended native engine.
// Caller has already rejected duplicate JSON members before any lossy parse.
// No schema rewriting, data coercion, defaults, custom keywords or network load.
const fs = require('node:fs');
const path = require('node:path');
const { createRequire } = require('node:module');
const { parseArgs } = require('node:util');

try {
  const { values } = parseArgs({ options: {
    document: { type: 'string' }, schema: { type: 'string' }, tools: { type: 'string' },
  }, strict: true, allowPositionals: false });
  if (!values.document || !values.schema || !values.tools) throw new Error('Explicit document, schema and tools paths are required.');
  const load = createRequire(path.join(path.resolve(values.tools), 'package.json'));
  const version = load('ajv/package.json').version;
  const requiredVersion = require('./package.json').dependencies.ajv;
  if (version !== requiredVersion) throw new Error('Ajv differs from the reviewed package pin.');
  // Refuse parent/global fallback when the explicit private tool tree is absent.
  const resolvedPackage = fs.realpathSync(load.resolve('ajv/package.json'));
  const privatePackage = fs.realpathSync(path.join(values.tools, 'node_modules/ajv/package.json'));
  if (resolvedPackage !== privatePackage) throw new Error('Ajv resolved outside the explicit private tool tree.');
  const Ajv2020 = load('ajv/dist/2020');
  const schema = JSON.parse(fs.readFileSync(values.schema, 'utf8'));
  const document = JSON.parse(fs.readFileSync(values.document, 'utf8'));
  const validate = new Ajv2020({ coerceTypes: false, useDefaults: false, removeAdditional: false }).compile(schema);
  if (!validate(document)) {
    console.error(JSON.stringify(validate.errors));
    process.exitCode = 1;
  } else {
    console.log(`PASS: unchanged official SPDX schema; Ajv ${version}; Node ${process.version}.`);
  }
} catch (error) {
  console.error(error.message);
  process.exitCode = 2;
}
