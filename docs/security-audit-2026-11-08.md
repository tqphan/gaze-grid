# Security audit — 2026-11-08

Scope: static review of application input and text handling, keyboard XML loading, Windows secure-desktop integration, dynamic library loading, local persistence, installer, and CI/release scripts. No application source or release settings were changed. Severity reflects prerequisites as well as impact; this is not a penetration test or a complete dependency vulnerability scan.

## Findings

### 1. Medium — mutable actions execute in the signing job

Evidence: `.github/workflows/windows-release.yml:26`, `:35`, `:60`, `:90`.

The packaging job runs actions referenced by mutable `@v4` tags and later imports the persistent signing key into the same runner's certificate store. A compromised upstream action could persist within that job, intercept signing secrets, or sign altered output. This requires an upstream supply-chain compromise; no compromise was observed. The separate release job's limited permissions do not isolate the private key from earlier packaging steps.

Remediation: pin every action to a reviewed full commit SHA and automate reviewed updates. Prefer a separate signing boundary backed by a non-exportable key or managed signing service. Environment approvals and tag protection should also be checked in GitHub; their live configuration was not accessible in this review.

Reference: https://docs.github.com/en/actions/reference/security/secure-use

### 2. Medium — default, persistent machine-wide certificate trust

Status: persistent trust remediated in source after this audit. At the user's
request, trust remains enabled by default for UIAccess keyboard functionality,
including silent installs; declining remains available. Installation uses a removable certificate component
only when the certificate is absent or already installer-managed. Repairs retain
the chosen mode and ownership; uninstall preserves independent and legacy trust.
The helper compiled, WiX compiled and linked a validation package, and compiled
MSI properties, component flags, and action sequencing were checked. Privileged
install/repair/uninstall regression cases were updated but could not run here
because the Windows Installer service is unavailable in this sandbox. Existing
legacy trust still requires explicit administrator removal as documented in
`docs/windows.md`.

Evidence: `platform/windows/Installer.wxs.in:6`, `:93` and its `iis:Certificate` declaration; `platform/windows/New-gaze-gridCertificate.ps1:29`.

The installer defaults certificate acceptance to 1, including silent installs, installs the self-signed code-signing identity in the machine root store, and marks that component permanent. Uninstall therefore leaves publisher trust behind. If the reusable private key is later compromised, affected machines retain trust in code signed with that identity. This is a documented design choice with an interactive opt-out, rather than an authentication bypass. The certificate is code-signing scoped; this finding does not assert arbitrary TLS certificate issuance.

Remediation: use a publicly trusted code-signing identity where feasible. Otherwise require explicit opt-in for machine trust, including silent installation, and provide an intentional revocation/removal path that accounts for other installed releases. Protect the persistent signing key separately from the build runner.

### 3. Medium — keyboard XML recursion has no depth limit

Evidence: `src/KeyboardPack.cpp:17`, `:25`, `:64`; `src/Controller.cpp:192`.

The loader caps file size at 4 MiB but recursively parses every nested element without bounding nesting depth or node count. A small, deeply nested XML document can exhaust the native stack or memory before layout validation rejects it. Startup inspects the XML files in the keyboard directory, and the editor can open supplied XML. An attacker must get a malicious keyboard opened or placed in that directory. The installed Program Files directory normally requires elevated write access; no remote delivery or privilege escalation was established.

Remediation: enforce depth and node-count limits during parsing, before recursion/allocation, and add a regression test with deeply nested XML. Consider iterative parsing. This is a code-supported denial-of-service risk; a crashing payload was not executed during this audit.

### 4. Low — personal text persistence relies on inherited permissions

Evidence: `src/Controller.cpp:47`, `:286`, `:1323`; `src/ProfileStore.cpp:18`, `:168`.

Learned words and their frequency counts are saved as plaintext JSON. Preferences can also contain personal conversation phrases. These writers create directories and files without enforcing owner-only permissions, unlike `PortalPermissionStore`. On Linux, a permissive umask and traversable configuration directories could expose personal data to other local users. Windows exposure depends on inherited ACLs. No cross-user disclosure was demonstrated, and default private profile directories may prevent it.

Remediation: create a private configuration directory and enforce owner-only permissions for personal files on Unix; preserve or explicitly validate user-scoped ACLs on Windows. Apply the policy to backup files as well as primary files. Encryption is not a replacement for access control.

## Defenses observed

- Keyboard XML rejects DTD and entity declarations and limits input size.
- Layout navigation checks destinations against the keyboard inventory.
- Windows Tobii loading restricts discovery to vendor installation trees, checks canonical containment, and uses restricted DLL dependency search flags.
- Secure-desktop startup restricts command-line modes, clears several Qt/configuration overrides, restricts QML import paths, and disables prediction and learning.
- Native and UI Automation text readers check password-field indicators before accepting text context.
- Portal permission tokens use atomic writes, owner-only file permissions, locking, size limits, and single-use consumption.
- Screenshot portal responses require a local file URI and bounded image dimensions.

These observations are not proof that every platform-specific boundary is secure.

## Verification and limits

Ran the existing `build/local-test/core-tests.exe` tests `desktopPermissionTokensArePrivateAndSingleUse`, `corruptPermissionTokenCannotBeReused`, and `rejectsMalformedAndEntities`: three tests passed, plus initialization and cleanup; zero failures. Output: `build/security-audit-tests.txt`. This was an existing binary, not a fresh build of the reviewed source. The Unix permission-bit assertion is conditional and was not exercised on Windows.

No privileged installer, secure-desktop attack, or destructive payload was run. No live GitHub account settings, installed driver versions, binary signatures, or comprehensive dependency/CVE inventory was verified. Git status was unavailable due to Git's repository ownership check; no global safe-directory exception was added. Further validation should include a fresh build, XML depth regression, Linux cross-user permission checks, and review of release signing and environment protections.
