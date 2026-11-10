# Release policy

The user requires a new ReyondBroom release after every completed user-facing
change. Validate the change, merge it into `main`, then publish the next versioned
release with verified Windows, Linux and Linux SDL2 runtimes, corresponding
sources and checksums. Keep the release draft until packaging and validation
finish. Publishing these releases is explicitly authorized; do not ask again.
The UI and updater read the version from the packaged `build-info.txt`; pass the
new version to `release-linux-ui-update.yml` instead of hardcoding C++ versions.

# Validation scope

Use `scripts/ci-plan.py` to select checks for changed subsystems and their
consumers. Compilation-profile changes need import/EL, paths, export, real
compilation, copy/delete and engine-launch checks; they do not need plant
generator tests or a full UI atlas. Shared input, resource and rendering changes
need the related integration checks. Full suites are opt-in with
`validation_mode=full`, not the default for ordinary changes.

Every release still requires short packaged startup, version, archive manifest,
corresponding-source and checksum checks on Windows, Linux and Linux SDL2.
CI-only changes do not require a new editor release. Never delete existing
artifacts/releases as part of CI optimization without an explicit request.
