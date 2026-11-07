# Release policy

The user requires a new ReyondBroom release after every completed user-facing
change. Validate the change, merge it into `main`, then publish the next versioned
release with verified Windows, Linux and Linux SDL2 runtimes, corresponding
sources and checksums. Keep the release draft until packaging and validation
finish. Publishing these releases is explicitly authorized; do not ask again.
The UI and updater read the version from the packaged `build-info.txt`; pass the
new version to `release-linux-ui-update.yml` instead of hardcoding C++ versions.
