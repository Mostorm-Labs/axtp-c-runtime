# AXTP C Runtime

This repository is reserved for the AXTP C runtime.

No C runtime implementation was present in the AXTP source repository during
this extraction. The repository now carries the AXTP Spec dependency contract so
future C runtime work can bind to the same spec source of truth as the other
runtime repositories.

## AXTP Spec Compatibility

This runtime repository implements AXTP Spec from the AXTP main specification
repository when C runtime sources are added.

See `AXTP_SPEC.lock.yaml` for:

- AXTP Spec repository
- Spec tag
- Spec version
- Source commit
- Compatibility range

Runtime code must not redefine AXTP protocol semantics. Protocol documents,
registries, schemas, business domains, business flows, and conformance cases are
maintained in the AXTP spec repository.

## AXTP Spec Dependency

Use `AXTP_SPEC_PATH` to point local tooling to a checked out AXTP spec
repository:

```bash
export AXTP_SPEC_PATH=/path/to/axtp
```

The checkout should match the tag and commit recorded in
`AXTP_SPEC.lock.yaml`. Do not depend on the `main` branch for reproducible
runtime builds.

Optional local checkout layout:

```text
third_party/axtp-spec
```

If `third_party/axtp-spec` is used, check it out to the locked tag or commit.

## Spec Lock Checks

```bash
scripts/check-axtp-spec-lock.sh
```

## AXTP Spec Upgrade

This runtime follows AXTP Spec via `AXTP_SPEC.lock.yaml`.

To upgrade:

```bash
scripts/upgrade-axtp-spec.sh spec/v0.3.0
scripts/check-axtp-spec-lock.sh
```

After upgrading, run generator checks and any runtime build, unit, and
conformance tests before merging. TODO: no dedicated C runtime conformance test
script exists yet.

## Local Generator

This repository maintains its own generator under `generators/`.

```bash
export AXTP_SPEC_PATH=/path/to/axtp
pnpm --dir generators install
pnpm --dir generators build
pnpm --dir generators test
pnpm --dir generators generate:runtime
```

Generated C artifacts are written to `include/generated/`.

To move to a later released spec tag:

```bash
scripts/upgrade-axtp-spec.sh spec/v0.1.0
```
