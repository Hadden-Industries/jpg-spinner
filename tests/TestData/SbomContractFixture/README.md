# Native SBOM contract fixture

`payload/fixture.txt` is synthetic authored text, not a photograph or application
binary. `source/packages.lock.json` retains the real reviewed C++/WinRT package
identity/content hash as a contract input; the accompanying unmodified NuGet
`.nuspec` is the resolved metadata consumed by the native detector. This fixture
never restores or executes that package. The libjpeg-turbo, Exiv2 and Adobe XMP
`vcpkg.spdx.json` inputs retain native vcpkg metadata from the qualified x64
static-MD installation (3.2.0#1, 0.28.9#1 and 2025.03#1). Line endings may follow
Git checkout normalization; the installed snapshot retains the original producer
bytes. These contain metadata,
not third-party source or binaries. Its document data license is CC0-1.0.

The independent expected result is SPDX 3.0.1, the precise payload SHA-256, and
the NuGet and vcpkg package identities. A native generation exit zero alone is
not evidence of dependency completeness. In particular, vcpkg detection consumes
installed native SBOM files, not the top-level `vcpkg.json` manifest alone.
The vcpkg expectation includes port revision `#1`, independently fixed by the
authored override and retained native input. The initial draft's `3.2.0` omitted
that packaging revision; accepting either version would weaken identity.
Microsoft documents the combined `version#port-version` identity in its
[override specification](https://learn.microsoft.com/en-us/vcpkg/reference/vcpkg-json#overrides).

The native port documents deliberately reuse document-local IDs such as
`SPDXRef-port` and `SPDXRef-binary`. All expected component identities must survive
composition; a valid document that silently collapses ports fails this fixture.
The SDK uses its own content-addressed package identity before serialization,
not a custom SPDX parser or JSON repair pass.

Sources: [Syft's pinned public SDK](https://github.com/anchore/syft/tree/v1.54.1/syft),
[vcpkg native SBOM contract](https://learn.microsoft.com/en-us/vcpkg/reference/software-bill-of-materials),
[Component Detection vcpkg inputs](https://github.com/microsoft/component-detection/blob/main/docs/detectors/vcpkg.md).
