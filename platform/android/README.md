# Crash Bash Android package

This is the Android package owner. Lucent owns
Storage Access Framework access, bounded private staging, and atomic promotion; `CrashBashMediaImport`
owns Crash Bash's media policy and hands a staged document to the title-native validator. The Activity
only composes setup UI. Native game behavior and virtual-pad policy remain separate owners.

The debug variant is intentionally labelled as a setup shell. A release build refuses unless
`build/android/native/arm64-v8a/libmain.so` exists and all four `CRASHBASH_RELEASE_*` signing variables
are set. Gradle's distribution, user home, project cache, project outputs, and native library all
stay under the repository's top-level `build/android/`; no disc, executable, extracted module, or
other copyrighted game file belongs in the APK.

Use the repository Python package driver, not an unpinned system Gradle:

```text
uv run --frozen python tools/android_package.py check
uv run --frozen python tools/android_package.py assemble-debug
uv run --frozen python tools/android_package.py assemble-release
```

`assemble-release` is not a publication claim. A release APK still needs signature verification and
the named-device correctness, frame-time, thermal, memory, loading, rendering, and audio matrix.
