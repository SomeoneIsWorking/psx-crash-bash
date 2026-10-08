#!/usr/bin/env python3
"""Build and verify the pinned Android package. Exit 1: package contradicts the contract; 2: input missing."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
import zipfile
from collections.abc import Mapping, Sequence

ROOT = pathlib.Path(__file__).resolve().parent.parent
ANDROID = ROOT / "platform" / "android"
CONFIG = ANDROID / "package.json"
WRAPPER = ANDROID / "gradle" / "wrapper" / "gradle-wrapper.properties"
ANDROID_BUILD = ROOT / "build" / "android"
GRADLE_CACHE = ANDROID_BUILD / "gradle"
GRADLE_PROJECT_CACHE = ANDROID_BUILD / "project-cache"
APK_OUTPUT = ANDROID_BUILD / "project" / "app" / "outputs" / "apk"
NATIVE_LIBRARY = ANDROID_BUILD / "native" / "arm64-v8a" / "libmain.so"
FORBIDDEN_SUFFIXES = {
    ".bin",
    ".chd",
    ".cue",
    ".exe",
    ".img",
    ".iso",
    ".pbp",
}
FORBIDDEN_NAMES = {"scus_945.70", "system.cnf", "crashbsh.dat"}
FORBIDDEN_DIGESTS = {
    "fd5727a18feb2a2d5a6359a55966f0266284d1e50f64ee9b8a127a97091bd516"
}
RELEASE_SIGNING_KEYS = (
    "CRASHBASH_RELEASE_STORE_FILE",
    "CRASHBASH_RELEASE_STORE_PASSWORD",
    "CRASHBASH_RELEASE_KEY_ALIAS",
    "CRASHBASH_RELEASE_KEY_PASSWORD",
)
REQUIRED_JNI_SYMBOLS = {
    "Java_io_github_someoneisworking_crashbash_CrashBashActivity_nativeStartGame",
    "Java_io_github_someoneisworking_crashbash_CrashBashActivity_nativeValidateAndInstall",
}


class PackageFailure(RuntimeError):
    """A checked project or APK contradicted the package contract."""


class PackageRefused(RuntimeError):
    """The environment cannot support the requested package assertion."""


def load_config(path: pathlib.Path = CONFIG) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise PackageRefused(f"cannot read Android package config {path}: {error}") from error
    if not isinstance(value, dict):
        raise PackageRefused(f"Android package config {path} must be an object")
    required = {
        "applicationId",
        "versionCode",
        "versionName",
        "compileSdk",
        "targetSdk",
        "minSdk",
        "buildToolsVersion",
        "ndkVersion",
        "agpVersion",
        "gradleVersion",
        "gradleDistributionSha256",
        "abis",
    }
    missing = sorted(required - value.keys())
    if missing:
        raise PackageRefused(f"Android package config is missing: {', '.join(missing)}")
    return value


def parse_properties(path: pathlib.Path = WRAPPER) -> dict[str, str]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise PackageRefused(f"cannot read Gradle wrapper properties {path}: {error}") from error
    properties: dict[str, str] = {}
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or "=" not in stripped:
            continue
        key, value = stripped.split("=", 1)
        properties[key] = value.replace("\\:", ":")
    return properties


def check_project() -> dict[str, object]:
    config = load_config()
    properties = parse_properties()
    version = str(config["gradleVersion"])
    expected_url = f"https://services.gradle.org/distributions/gradle-{version}-bin.zip"
    checks = {
        "distributionUrl": expected_url,
        "distributionSha256Sum": str(config["gradleDistributionSha256"]),
    }
    disagreements = [
        f"{key}: {properties.get(key)!r}, expected {expected!r}"
        for key, expected in checks.items()
        if properties.get(key) != expected
    ]
    abis = config["abis"]
    if abis != ["arm64-v8a"]:
        disagreements.append(f"abis: {abis!r}, expected ['arm64-v8a']")

    required_files = (
        ANDROID / "settings.gradle",
        ANDROID / "app" / "build.gradle",
        ANDROID / "app" / "src" / "main" / "AndroidManifest.xml",
        ANDROID / "app" / "src" / "main" / "res" / "drawable" / "ic_launcher_foreground.xml",
        ANDROID / "art" / "crashbash-icon.svg",
    )
    disagreements.extend(f"missing package source {path}" for path in required_files if not path.is_file())

    packaged_sources = ANDROID / "app" / "src" / "main"
    for path in packaged_sources.rglob("*"):
        if not path.is_file():
            continue
        if forbidden_name(path.name):
            disagreements.append(f"forbidden game-file name in package sources: {path}")
    if disagreements:
        raise PackageFailure("Android package project check failed:\n  " + "\n  ".join(disagreements))
    return config


def forbidden_name(name: str) -> bool:
    lower = pathlib.PurePosixPath(name).name.casefold()
    return lower in FORBIDDEN_NAMES or pathlib.PurePosixPath(lower).suffix in FORBIDDEN_SUFFIXES


def _java_major(executable: pathlib.Path) -> int:
    try:
        completed = subprocess.run(
            [str(executable), "-version"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
    except OSError as error:
        raise PackageRefused(f"cannot execute {executable}: {error}") from error
    match = re.search(r'(?:version\s+")?(\d+)(?:\.|\s)', completed.stdout)
    if completed.returncode != 0 or match is None:
        raise PackageRefused(f"cannot identify Java version from {executable}: {completed.stdout.strip()}")
    return int(match.group(1))


def coherent_java_home(environ: Mapping[str, str] = os.environ) -> pathlib.Path:
    raw = environ.get("JAVA_HOME")
    if not raw:
        raise PackageRefused(
            "JAVA_HOME is not set; select one coherent JDK whose bin/java and bin/javac match"
        )
    home = pathlib.Path(raw).expanduser().resolve()
    java = home / "bin" / ("java.exe" if os.name == "nt" else "java")
    javac = home / "bin" / ("javac.exe" if os.name == "nt" else "javac")
    if not java.is_file() or not javac.is_file():
        raise PackageRefused(f"JAVA_HOME={home} does not contain both bin/java and bin/javac")
    java_major = _java_major(java)
    javac_major = _java_major(javac)
    if java_major != javac_major:
        raise PackageRefused(
            f"JAVA_HOME={home} is incoherent: java is {java_major}, javac is {javac_major}"
        )
    if not 17 <= java_major <= 26:
        raise PackageRefused(
            f"Gradle 9.7.1 requires a supported JDK 17..26; JAVA_HOME={home} is {java_major}"
        )
    return home


def android_sdk(environ: Mapping[str, str] = os.environ) -> pathlib.Path:
    raw = environ.get("ANDROID_HOME") or environ.get("ANDROID_SDK_ROOT")
    if not raw:
        raise PackageRefused("set ANDROID_HOME to an Android SDK containing the pinned packages")
    sdk = pathlib.Path(raw).expanduser().resolve()
    config = load_config()
    required = (
        sdk / "platforms" / f"android-{config['compileSdk']}",
        sdk / "build-tools" / str(config["buildToolsVersion"]),
        sdk / "ndk" / str(config["ndkVersion"]),
    )
    missing = [str(path) for path in required if not path.is_dir()]
    if missing:
        raise PackageRefused("Android SDK is missing pinned package path(s): " + ", ".join(missing))
    return sdk


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_native_runtime(library: pathlib.Path, sdk: pathlib.Path) -> None:
    """Require the setup Activity's JNI contract in the exact library that will ship."""
    if not library.is_file():
        raise PackageRefused(
            f"release requires native game runtime {library}; setup-only APKs are not releases"
        )
    config = load_config()
    prebuilt_root = sdk / "ndk" / str(config["ndkVersion"]) / "toolchains" / "llvm" / "prebuilt"
    candidates = sorted(prebuilt_root.glob("*/bin/llvm-nm"))
    if len(candidates) != 1:
        raise PackageRefused(
            f"expected one NDK llvm-nm below {prebuilt_root}, found {len(candidates)}"
        )
    completed = subprocess.run(
        [str(candidates[0]), "-D", "--defined-only", str(library)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.returncode != 0:
        raise PackageFailure(f"cannot inspect native runtime {library}:\n{completed.stdout}")
    exported = {line.split()[-1] for line in completed.stdout.splitlines() if line.split()}
    missing = sorted(REQUIRED_JNI_SYMBOLS - exported)
    if missing:
        raise PackageFailure(
            f"native runtime {library} is missing setup JNI symbol(s): {', '.join(missing)}"
        )


def gradle_executable(config: Mapping[str, object]) -> pathlib.Path:
    version = str(config["gradleVersion"])
    archive = GRADLE_CACHE / f"gradle-{version}-bin.zip"
    unpacked = GRADLE_CACHE / f"gradle-{version}"
    executable = unpacked / "bin" / ("gradle.bat" if os.name == "nt" else "gradle")
    if executable.is_file():
        if os.name != "nt":
            executable.chmod(executable.stat().st_mode | 0o111)
        prune_superseded_gradle(version)
        return executable

    GRADLE_CACHE.mkdir(parents=True, exist_ok=True)
    url = f"https://services.gradle.org/distributions/{archive.name}"
    if not archive.is_file():
        try:
            with urllib.request.urlopen(url, timeout=60) as response, archive.open("wb") as output:
                shutil.copyfileobj(response, output)
        except (OSError, urllib.error.URLError) as error:
            raise PackageRefused(f"cannot download pinned Gradle distribution {url}: {error}") from error
    digest = sha256_file(archive)
    expected = str(config["gradleDistributionSha256"])
    if digest != expected:
        raise PackageFailure(
            f"Gradle distribution digest {digest} disagrees with pinned {expected}: {archive}"
        )
    with zipfile.ZipFile(archive) as source:
        root = GRADLE_CACHE.resolve()
        for info in source.infolist():
            target = (GRADLE_CACHE / info.filename).resolve()
            if not target.is_relative_to(root):
                raise PackageFailure(f"Gradle distribution contains unsafe path {info.filename!r}")
        source.extractall(GRADLE_CACHE)
    if not executable.is_file():
        raise PackageFailure(f"Gradle distribution did not create {executable}")
    if os.name != "nt":
        # zipfile drops Unix executable bits.
        executable.chmod(executable.stat().st_mode | 0o111)
    prune_superseded_gradle(version)
    return executable


def prune_superseded_gradle(current_version: str) -> None:
    """Remove only obsolete pinned distributions from this tool's bounded cache."""
    if not GRADLE_CACHE.is_dir():
        return
    keep = {f"gradle-{current_version}", "user-home"}
    root = GRADLE_CACHE.resolve()
    for candidate in GRADLE_CACHE.iterdir():
        if candidate.name in keep or not re.fullmatch(r"gradle-[0-9][0-9A-Za-z.\-]*(?:-bin\.zip)?", candidate.name):
            continue
        resolved = candidate.resolve()
        if not resolved.is_relative_to(root):
            raise PackageFailure(f"refusing to clean Gradle cache path outside {root}: {resolved}")
        if candidate.is_dir():
            shutil.rmtree(candidate)
        else:
            candidate.unlink()

    for parent_name in ("caches", "daemon", "notifications"):
        parent = GRADLE_CACHE / "user-home" / parent_name
        if not parent.is_dir():
            continue
        for candidate in parent.iterdir():
            if candidate.name == current_version or not re.fullmatch(r"[0-9]+(?:\.[0-9]+)+", candidate.name):
                continue
            resolved = candidate.resolve()
            if not resolved.is_relative_to(root):
                raise PackageFailure(f"refusing to clean Gradle cache path outside {root}: {resolved}")
            if candidate.is_dir():
                shutil.rmtree(candidate)
            else:
                candidate.unlink()


def run_gradle(task: str, *, release: bool = False) -> pathlib.Path:
    config = check_project()
    java_home = coherent_java_home()
    sdk = android_sdk()
    if release:
        verify_native_runtime(NATIVE_LIBRARY, sdk)
        missing_signing = [key for key in RELEASE_SIGNING_KEYS if not os.environ.get(key)]
        if missing_signing:
            raise PackageRefused("release signing variables are missing: " + ", ".join(missing_signing))

    environment = dict(os.environ)
    environment.update(
        JAVA_HOME=str(java_home),
        ANDROID_HOME=str(sdk),
        ANDROID_SDK_ROOT=str(sdk),
        GRADLE_USER_HOME=str(GRADLE_CACHE / "user-home"),
    )
    command = [
        str(gradle_executable(config)),
        "--no-daemon",
        "--project-cache-dir",
        str(GRADLE_PROJECT_CACHE),
        "--stacktrace",
        f":app:{task}",
    ]
    completed = subprocess.run(command, cwd=ANDROID, env=environment, check=False)
    if completed.returncode != 0:
        raise PackageFailure(f"Gradle task :app:{task} failed with exit {completed.returncode}")

    variant = "release" if release else "debug"
    candidates = sorted((APK_OUTPUT / variant).glob("*.apk"))
    if len(candidates) != 1:
        raise PackageFailure(
            f"Gradle task created {len(candidates)} {variant} APK(s), expected exactly one under {APK_OUTPUT / variant}"
        )
    return candidates[0]


def inspect_apk(source: pathlib.Path | io.BytesIO, *, require_native: bool = True) -> tuple[int, int]:
    with zipfile.ZipFile(source) as apk:
        files = [info for info in apk.infolist() if not info.is_dir()]
        forbidden: list[str] = []
        native_count = 0
        for info in files:
            if forbidden_name(info.filename):
                forbidden.append(info.filename)
            digest = hashlib.sha256()
            with apk.open(info) as entry:
                for chunk in iter(lambda: entry.read(1024 * 1024), b""):
                    digest.update(chunk)
            if digest.hexdigest() in FORBIDDEN_DIGESTS:
                forbidden.append(f"{info.filename} (known game-content digest)")
            if info.filename == "lib/arm64-v8a/libmain.so":
                native_count += 1
        if forbidden:
            raise PackageFailure("APK contains forbidden game content: " + ", ".join(forbidden))
        if require_native and native_count != 1:
            raise PackageFailure(f"APK contains {native_count} arm64 libmain.so entries, expected exactly one")
        return len(files), native_count


def verify_signature(apk: pathlib.Path, sdk: pathlib.Path) -> None:
    config = load_config()
    apksigner = sdk / "build-tools" / str(config["buildToolsVersion"]) / (
        "apksigner.bat" if os.name == "nt" else "apksigner"
    )
    if not apksigner.is_file():
        raise PackageRefused(f"signature verifier is missing at {apksigner}")
    completed = subprocess.run(
        [str(apksigner), "verify", "--verbose", "--print-certs", str(apk)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.returncode != 0:
        raise PackageFailure(f"APK signature verification failed:\n{completed.stdout}")
    print(completed.stdout, end="")


def verify_apk(apk: pathlib.Path) -> None:
    if not apk.is_file():
        raise PackageRefused(f"APK does not exist: {apk}")
    files, native_count = inspect_apk(apk)
    verify_signature(apk, android_sdk())
    print(f"[android] APK content: {files} files, {native_count} native game runtime")


def _zip(entries: Mapping[str, bytes]) -> io.BytesIO:
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w") as archive:
        for name, data in entries.items():
            archive.writestr(name, data)
    output.seek(0)
    return output


def selftest() -> None:
    good = _zip(
        {
            "AndroidManifest.xml": b"manifest",
            "classes.dex": b"dex",
            "lib/arm64-v8a/libmain.so": b"native",
        }
    )
    files, native_count = inspect_apk(good)
    if (files, native_count) != (3, 1):
        raise PackageFailure("selftest valid package did not report its denominator")

    forbidden = _zip(
        {
            "AndroidManifest.xml": b"manifest",
            "lib/arm64-v8a/libmain.so": b"native",
            "assets/SCUS_945.70": b"game",
        }
    )
    try:
        inspect_apk(forbidden)
    except PackageFailure as error:
        if "SCUS_945.70" not in str(error):
            raise PackageFailure("selftest game-content refusal did not name the file") from error
    else:
        raise PackageFailure("selftest accepted APK-embedded game content")

    missing_native = _zip({"AndroidManifest.xml": b"manifest", "classes.dex": b"dex"})
    try:
        inspect_apk(missing_native)
    except PackageFailure as error:
        if "0 arm64 libmain.so" not in str(error):
            raise PackageFailure("selftest missing-runtime refusal lacks its denominator") from error
    else:
        raise PackageFailure("selftest accepted an APK without the native runtime")
    print("[android] selftest PASS: 3 package classes, 2 required refusals fired")


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check", help="validate pinned project metadata and package sources")
    commands.add_parser("assemble-debug", help="build the explicitly labelled setup-shell APK")
    commands.add_parser("assemble-release", help="build and verify a signed release APK")
    verify = commands.add_parser("verify-apk", help="scan game content, native runtime, and signature")
    verify.add_argument("apk", type=pathlib.Path)
    commands.add_parser("selftest", help="prove the APK verifier accepts and refuses opposite cases")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.command == "check":
            config = check_project()
            print(
                "[android] project PASS: "
                f"AGP {config['agpVersion']}, Gradle {config['gradleVersion']}, "
                f"SDK {config['compileSdk']}, NDK {config['ndkVersion']}, "
                f"{len(config['abis'])} ABI"
            )
        elif args.command == "assemble-debug":
            apk = run_gradle("assembleDebug")
            files, native_count = inspect_apk(apk, require_native=False)
            print(f"[android] debug shell: {apk} ({files} files, {native_count} native game runtime)")
        elif args.command == "assemble-release":
            apk = run_gradle("assembleRelease", release=True)
            verify_apk(apk)
            print(f"[android] verified release APK: {apk}")
        elif args.command == "verify-apk":
            verify_apk(args.apk.resolve())
        else:
            selftest()
    except PackageFailure as error:
        print(f"[android] FAIL: {error}", file=sys.stderr)
        return 1
    except PackageRefused as error:
        print(f"[android] REFUSED: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
