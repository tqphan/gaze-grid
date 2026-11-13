# Release security

The Windows release workflow accepts `main` or an exact version tag on a commit
reachable from `main`. Both Linux and Windows CI must have passed for that commit.
CI must come from a `main` push run of `.github/workflows/build.yml` in this
repository. Validation checks the workflow identity and the Linux/Windows jobs
in its current run attempt; same-name checks from other workflows do not count.
Publishing requires a version tag created by an administrator. A manual run on
`main` with `create_release=false` tests the full pipeline without publishing.
Checkout does not retain credentials, and tokens default to read access. Only the
draft publisher can write repository contents.

Release jobs use separate GitHub-hosted runners:

1. Build, test and stage with the public certificate and pinned Tobii headers.
2. Sign the application. Import the persistent key only around `signtool`, then
   delete the imported certificate and private key in `finally`. Cleanup attempts
   every item, reports failures, and must succeed before publishing a signed hash.
   Do not install dependencies or execute the input.
3. Package the signed application and staged files without a private key.
4. Sign the MSI on another fresh runner, with the same restrictions as step 2.
5. Verify and install the signed MSI on another runner without a private key.
6. Prepare checksums and attach verified assets to a draft release.

The build job uses `windows-build`, whose only secret is
`GAZEGRID_TOBII_HEADERS_BASE64`. Only the two signing jobs use `windows-release`,
which contains `GAZEGRID_SIGNING_PFX_BASE64` and `GAZEGRID_SIGNING_PFX_PASSWORD`.
Both environments require owner approval and allow only `main` and version tags.
Keep signing credentials out of repository and organization secrets too: those
would also be available to build jobs. Never approve a rerun of a historical
workflow that gives its build job access to the signing environment.

Secret isolation requires separate environments. Omitting a secret from a step's
`env` block or deferring PFX import does not isolate the environment's secrets
from a compromised job runner.

Signing inputs have fixed filenames and must match SHA-256 outputs from their
producing jobs in the same workflow run. Artifact uploads are immutable within
the run. Verification checks the pinned publisher identity, and upgrade testing
checks the pinned baseline hash and signature before installing it. Public
certificate and baseline pins are in `platform/windows/release-trust.json`.
The publisher independently checks the final MSI against the signing job's
digest, restricts asset filenames, checks the pinned public certificate and
checksum manifest, and regenerates release notes from source. Installer tests
cannot replace a release asset by rewriting its checksum file.
Private keys must never be committed.

The Qt installer source is pinned by commit and archive hash. Its Python runtime,
build tools and all transitive packages use exact versions and wheel hashes in
`platform/windows/qt-installer-requirements.txt`. No dependency cache is restored,
and the source package is built without dependency resolution or build isolation.
Review upstream changes and regenerate hashes when updating this lock. Qt's own
archives still depend on Qt's distribution infrastructure and aqt's verification;
hash locking Python packages does not independently authenticate Qt binaries.
Linux CI uses a container digest and a dated Arch archive with package signature
verification enabled.

Repository settings complement the workflow: require pull requests and successful
CI on `main`, prevent release tag changes/deletion, restrict the signing
environment to release refs and require administrator approval, require full SHA
action pins and an action allowlist, and enable immutable releases. Immutability
applies to releases published after it is enabled; existing releases stay intact.
For this single-administrator repository, approval is an explicit owner gate,
not independent second-person review. Administrators retain control of repository
policies and signing secrets. Compromised source, approved workflow changes,
trusted upstreams or the runner/signing tools remain risks; this is not a claim
that the build is reproducible or free of supply-chain vulnerabilities.
