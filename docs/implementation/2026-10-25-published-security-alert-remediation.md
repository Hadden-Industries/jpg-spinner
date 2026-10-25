# Published security alert remediation

Status: the affected build-tool dependency is repaired locally and focused
regressions pass. The five historical CodeQL paths were already removed by the
signed source-retirement checkpoint. GitHub alert-state reconciliation, source
publication and complete release qualification remain separate.

## Intake and scope

On 2026-10-25 the owner requested remediation of the repository's existing
[Dependabot](https://github.com/Hadden-Industries/jpg-spinner/security/dependabot)
and [code-scanning](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning)
queues after the WinRT probe repair. The authenticated, fully paginated REST
intake returned four general Dependabot alerts, no malware alerts and five
CodeQL alerts. All nine inputs and all five CodeQL instance collections are
retained individually; similar findings were not dropped or merged.

The source under assessment is signed checkpoint
`65e0cd151433301acac44e0f1e454db324db26a7`. The owner permits targeted known-finding
remediation, while prohibiting new broad security reviews, external reviews and
implementation delegation. No scan, remote workflow or alert dismissal was
started for this repair.

## Compatible native dependency repair

Ajv 8.20.0 uses `fast-uri` through its existing `^3.0.1` dependency range. The
SPDX validation lock selected affected version 3.1.8. These four upstream
advisories identify 3.1.9 as the patched 3.x release:

| Dependabot alert                                                            | Upstream advisory                                                                                  | Broken URI property                                           |
| --------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| [4](https://github.com/Hadden-Industries/jpg-spinner/security/dependabot/4) | [GHSA-w74x-v6f9-j3xg](https://github.com/fastify/fast-uri/security/advisories/GHSA-w74x-v6f9-j3xg) | Encoded schemes become live schemes.                          |
| [3](https://github.com/Hadden-Industries/jpg-spinner/security/dependabot/3) | [GHSA-xhq7-xmqj-769h](https://github.com/fastify/fast-uri/security/advisories/GHSA-xhq7-xmqj-769h) | Normalizing a relative reference changes its resolved target. |
| [2](https://github.com/Hadden-Industries/jpg-spinner/security/dependabot/2) | [GHSA-wjr9-j795-jxcr](https://github.com/fastify/fast-uri/security/advisories/GHSA-wjr9-j795-jxcr) | A second parser silently changes malformed host data.         |
| [1](https://github.com/Hadden-Industries/jpg-spinner/security/dependabot/1) | [GHSA-333x-w4r6-6crp](https://github.com/fastify/fast-uri/security/advisories/GHSA-333x-w4r6-6crp) | Suffix serialization introduces a scheme.                     |

The observed npm registry 3.x release list ends at 3.1.9. Native
[`npm update`](https://docs.npmjs.com/cli/v11/commands/npm-update/), constrained by
Ajv's existing range, changes only the lockfile's `fast-uri` version, download URL
and integrity. Its BSD-3-Clause license is unchanged. Ajv remains exactly 8.20.0;
there is no direct dependency addition, override, fork or parsing workaround.
Native `npm ci --ignore-scripts --no-audit --no-fund` restores each lock in a
separate caller-owned private tree and verifies package integrity.

The actual consumer is `eng/spdx-validation/validate-spdx-schema.cjs`, invoked
through `scripts/Test-SpdxJsonSchema.ps1`. It compiles an authenticated official
SPDX schema synchronously, with no network loader, URI allowlist, renderer or
suffix option. The npm tree is build tooling and is not shipped as application
runtime code. The affected dependency is established; an exploitable product
path matching the advisories' complete preconditions is not established. The
static triage preserves that distinction rather than asserting an application
XSS or SSRF vulnerability.

## Retained RED/GREEN and compatibility checks

`scripts/tests/Test-SpdxSchemaUriResolution.cjs` loads the URI package that Ajv
actually resolves from the explicitly selected private tree. It uses Node's
built-in test runner and assertions, without a stub parser or new test package.
The cases follow the upstream advisories and
[native patch tests](https://github.com/fastify/fast-uri/compare/v3.1.8...v3.1.9).

- With the original 3.1.8 lock, all four advisory groups fail their intended
  semantic assertions. Both ordinary URI normalization and real Ajv reference
  validation controls pass: six tests, two passing and four failing.
- With the native 3.1.9 lock, all six tests pass. Alternate inputs include legacy
  Unicode scheme escapes, multiple authority delimiters, TAB/CR/LF host data,
  leading parent references, protected colon segments and three suffix schemes.
- The existing SPDX schema control entry point runs these cases before its real
  document checks. The unchanged
  [official SPDX 3.0.1 example](https://github.com/spdx/spdx-examples/blob/master/software/example1/spdx3.0/example1.spdx3.json)
  passes the digest-qualified official schema. Wrong context, unknown class,
  wrong JSON type and wrong private-tool lock still fail through their intended
  native boundaries. The input document's hash remains unchanged.

This is focused dependency qualification. It is not a complete SBOM producer,
clean-package, hosted QA or release qualification run.

## Historical CodeQL results

All five alerts are `cpp/integer-multiplication-cast-to-long` from managed
build-mode `none` analysis of commit
`e6c17b62658bb7197a220deee3e3982a0dd2e04c`, before source retirement:

| CodeQL alert                                                                   | Historical location             |
| ------------------------------------------------------------------------------ | ------------------------------- |
| [1](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/1) | `JPG Spinner/jdmainct.c:186`    |
| [2](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/2) | `JPG Spinner/jmemmgr.c:601–602` |
| [3](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/3) | `JPG Spinner/jmemmgr.c:603–604` |
| [4](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/4) | `JPG Spinner/jmemmgr.c:609–610` |
| [5](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/5) | `JPG Spinner/jmemmgr.c:611–612` |

Signed retirement `e1eb2361f432f459ffd5f99a47faddf8a0cd9523` deletes the entire
legacy application and copied codec. The maintained JPEG backend uses pinned
libjpeg-turbo 3.2.0#1. Native Git inventory and the actual retirement policy
confirm the affected files and legacy solution are absent. The policy's valid
source control and seven independent refusal fixtures also pass.

These findings require no new patch to the maintained implementation. That does
not assert that historical installed 1.x packages are fixed. The retained static
verdict is specific to current source; GitHub still needs a current analysis to
reconcile its historical records. No alert is dismissed or relabeled as a false
positive to conceal that gap.

## HISEW evidence and remaining work

This work continues accepted R2 execution
`1f8e9d2c-38ef-4f91-a9d3-d114332d547e`. The targeted route addendum is retained;
native rerouting refused because it requires the exact paused owner. The
execution remains active under the owner's explicit additional instruction.
No artificial pause or verification rerun was introduced to alter bookkeeping.

Raw API pages, original native logs, authoritative release metadata and the
upstream patch are retained through the managed security artifact collection
and HISEW resource inventory `b91f43cc50354253a444131923d2e70a`. Generated npm
trees and cache bytes are reconstructible from the retained locks and registry
integrities; native cleanup must preserve required evidence before disposal.

The owner authorized publication of signed fix
`0d78e54bc27018856742647a9e3c4098fea8a1da`. GitHub's subsequent native alert
records report all four Dependabot alerts **fixed**, with `fixed_at` values on
2026-10-25 at 08:20:21–08:20:22 UTC. No alert was dismissed. The fully paginated
open-alert readback is empty. The associated
[ordinary manual QA run passed](2026-10-25-hosted-qa-completion.md).

The five historical CodeQL records require a separately authorized current
analysis; this document does not authorize it. Full final
verification, existing review restrictions, trusted Store-upgrade evidence,
device/accessibility checks and legal/publication approvals remain as recorded
in the [release register](../release-qualification.md).
