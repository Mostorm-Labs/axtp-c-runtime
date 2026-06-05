# AXTP C Runtime

C runtime and SDK primitives for AXTP.

This repository owns the C implementation and its runtime generator. The AXTP
specification, registry, schemas, protocol documents, and test vectors are owned
by the main AXTP spec repository.

## Runtime Surface

The P0 runtime follows the same architecture as the C++ and Node runtimes:

```text
Transport -> axtp_endpoint_t -> axtp_core_t -> axtp_broker_t -> handler
```

It includes:

- FramedBinary standard frame encode/decode with CRC16-CCITT-FALSE
- binary RPC payload encode/decode with JSON, TLV, raw, and binary body markers
- `axtp_mock_transport_t` for in-process client/server tests
- `axtp_client_t` and `axtp_server_t` helpers for JSON handlers
- generated registry lookup helpers in `include/generated/`

P0 intentionally does not implement the full schema-aware TLV object codec.

## Local Development

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## AXTP Spec Compatibility

This runtime repository implements AXTP Spec from the AXTP main specification
repository.

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

## Versioning

This repository keeps AXTP Spec, runtime, and generated artifact versions
separate:

- AXTP Spec tags use `spec/vX.Y.Z` and are recorded in `AXTP_SPEC.lock.yaml`.
- Runtime releases use `vX.Y.Z`.
- Generated artifact metadata is recorded in `generated/axtp_generated_manifest.json`.

Use `scripts/check-generated-version.sh` to verify that the lock file,
generated manifest, runtime version, and generated constants are aligned.

See `docs/generator/GENERATED_VERSIONING.md` for generator versioning details.

## Release

Runtime releases are created from runtime tags:

- Runtime tags: `vX.Y.Z`
- AXTP Spec tags: `spec/vX.Y.Z`

AXTP Spec updates create upgrade PRs. They do not automatically create runtime
releases. A runtime release is created only after maintainers tag this runtime
repository with `vX.Y.Z`.

Each release records runtime version, AXTP Spec tag, AXTP Spec commit, generator
version, and the generated manifest.
