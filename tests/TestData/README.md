# Reviewed JPEG test data

Every binary fixture added under this directory must document all of the following before it is committed:

- the fixture's source or deterministic generation procedure;
- its copyright holder and license;
- the specific defect, boundary, or format feature it exercises;
- immutable dimensions, sampling factors, metadata, and coefficient-digest expectations;
- why a generated fixture cannot express the same behavior, when the fixture is externally sourced.

Task 2 creates deterministic fixtures in the test output directory rather than committing generated JPEG files here. Later tasks may add reviewed third-party regression inputs only when their provenance and expected invariant are recorded in this file.
