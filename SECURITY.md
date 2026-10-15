# Security policy

## Supported versions

The 2.0 replacement is under development and is not a supported production release
yet. Do not interpret a package build or a historical Store listing as current
security maintenance. Supported production versions and dates must be explicitly
recorded when a release is qualified. Historical 1.x code remains quarantined
for migration comparison; it is not the modern security boundary.

## Private reporting

Use the repository's [private advisory reporting route](https://github.com/Hadden-Industries/jpg-spinner/security/advisories/new)
when GitHub offers it to your account. Maintainers must verify that this route is
enabled before release. If it is unavailable, ask the repository maintainer for
a private channel without posting exploit details, photographs or credentials
in a public issue. There is no promised response SLA.

Include affected commit/package version and architecture, expected versus actual
behavior, a minimal synthetic reproducer, and reproduction instructions. Do not
attach personal photos or unsanitized recovery journals. Use sanitized diagnostic
export only after reviewing its contents. A report need not include a weaponized
exploit to be useful.

## Threat model

Malformed or adversarial JPEG/metadata input can target native parsing, resource
use and transformation. The design uses explicit bounds, native maintained codecs,
independent output validation, AppContainer deployment and narrowly granted
Storage objects. File revision/identity checks defend the commit boundary;
backups and immutable journals protect recoverability. These controls do not
justify processing untrusted images in a privileged process or claim immunity
from compromised storage providers or kernel/hardware faults.

Build infrastructure, dependency pins and release signing are separate trust
boundaries. Never place signing credentials in PR builds, source or caches.

## Disclosure

Maintainers should acknowledge privately when able, reproduce safely, agree on
remediation/disclosure timing with the reporter and credit them with consent.
Do not mark a finding fixed solely because a patch exists: retain a regression
test and evidence for the affected release. Public disclosure and release
publication require deliberate maintainer decisions. See the
[release gates](docs/release-qualification.md).
