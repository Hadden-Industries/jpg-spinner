// Exercise the URI implementation Ajv actually resolves from its private tool
// tree. These retained advisory cases qualify a dependency update; they do not
// claim that the app exposes URI allowlists, renderers or network schema loads.
const assert = require("node:assert/strict");
const path = require("node:path");
const { createRequire } = require("node:module");
const { test } = require("node:test");
const { parseArgs } = require("node:util");

const { values } = parseArgs({
    options: { tools: { type: "string" } },
    strict: true,
    allowPositionals: false,
});
if (!values.tools) throw new Error("An explicit private validator tool tree is required.");
const loadTools = createRequire(path.join(path.resolve(values.tools), "package.json"));
const loadAjvDependency = createRequire(loadTools.resolve("ajv/package.json"));
const uriResolver = loadAjvDependency("fast-uri");

test("encoded schemes cannot become executable schemes", () => {
    // GHSA-w74x-v6f9-j3xg: RFC 3986 section 3.1 excludes percent escapes in
    // schemes. Include both percent-byte and legacy Unicode escape spellings.
    for (const input of ["%6Aavascript:alert(1)", "%u006Aavascript:alert(1)"]) {
        assert.match(uriResolver.parse(input).error ?? "", /URI scheme is malformed/);
        assert.equal(uriResolver.normalize(input), input);
        assert.throws(() => uriResolver.resolve("https://example.invalid/app/", input), /URI scheme is malformed/);
    }
    assert.equal(uriResolver.equal("%68ttp://example.invalid/", "http://example.invalid/"), false);
});

test("relative normalization preserves the resolved target", () => {
    // GHSA-xhq7-xmqj-769h: normalization must preserve the meaning of a relative
    // reference, including leading parent segments and protected colon segments.
    const base = "https://example.invalid/schemas/current/root.json";
    for (const reference of ["a/../b", "../x", "./a:b", "a/..//x"]) {
        const normalized = uriResolver.normalize(reference);
        assert.equal(uriResolver.resolve(base, normalized), uriResolver.resolve(base, reference));
    }
    assert.equal(uriResolver.equal("../x", "x"), false);
    assert.equal(uriResolver.equal("a/../b", "/b"), false);
});

test("malformed authorities cannot silently change hosts", () => {
    // GHSA-wjr9-j795-jxcr: a second parser must not discard host characters that
    // the first parser treated as data. Test delimiter and whitespace variants.
    for (const host of ["a@b@evil.invalid", "ev\til.invalid", "ev\ril.invalid", "ev\nil.invalid"]) {
        const input = `https://${host}/path`;
        assert.match(uriResolver.parse(input).error ?? "", /URI host is malformed/);
        assert.equal(uriResolver.normalize(input), input);
        assert.equal(uriResolver.equal(input, input), false);
        assert.throws(() => uriResolver.resolve("https://example.invalid/", `//${host}/path`), /URI host is malformed/);
    }
});

test("suffix serialization cannot introduce a scheme", () => {
    // GHSA-333x-w4r6-6crp: Ajv does not select suffix mode, but retain its
    // regression too so all four imported dependency alerts have direct proof.
    const options = { reference: "suffix", scheme: "x" };
    for (const input of ["javascript:alert(1)", "data:text/plain,x", "vbscript:msgbox(1)"]) {
        assert.equal(uriResolver.parse(uriResolver.normalize(input, options)).scheme, undefined);
    }
    assert.equal(uriResolver.normalize("javascript:alert(1)", options), "javascript%3Aalert(1)");
    assert.equal(uriResolver.normalize("path/to/file", options), "path/to/file");
    assert.equal(uriResolver.normalize("a/b:c", options), "a/b:c");
});

test("ordinary host and reference normalization remain compatible", () => {
    assert.equal(uriResolver.normalize("https://Example.INVALID:443/"), "https://example.invalid/");
    assert.equal(uriResolver.normalize("http://%65xample.invalid/"), "http://example.invalid/");
    assert.equal(uriResolver.normalize("http://bücher.de/"), "http://xn--bcher-kva.de/");
    assert.equal(
        uriResolver.resolve("https://example.invalid/schemas/root.json", "../types/string.json"),
        "https://example.invalid/types/string.json",
    );
});

test("Ajv resolves ordinary schema references and retains type refusals", () => {
    const Ajv2020 = loadTools("ajv/dist/2020");
    const ajv = new Ajv2020({ coerceTypes: false, useDefaults: false, removeAdditional: false });
    ajv.addSchema({ $id: "https://example.invalid/types/string.json", type: "string" });
    const validate = ajv.compile({
        $id: "https://example.invalid/schemas/root.json",
        $ref: "../types/string.json",
    });
    assert.equal(validate("legitimate value"), true);
    assert.equal(validate(42), false);
    assert.equal(validate.errors[0].keyword, "type");
});
