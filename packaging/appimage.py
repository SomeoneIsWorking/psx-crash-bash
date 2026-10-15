#!/usr/bin/env python3
"""Build a reproducible x86_64 Crash Bash AppImage without game files.

The release artifact contains the built port and the redistributable framework UI assets. The port's
own first-run screen asks for and authenticates the player's North American CHD, then keeps only its
path and the extracted executable in the user-data directory. AppImage construction uses checksum-pinned linuxdeploy, appimagetool, and type-2 runtime
inputs from ``appimage-tools.json``.

Exit 0 means a complete AppDir or AppImage was built and audited. Exit 1 means an input contradicted
the release contract. Exit 2 means the requested comparison or build could not be performed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import stat
import subprocess
import sys
import urllib.error
import urllib.request
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PACKAGING = ROOT / "packaging"
LINUX = PACKAGING / "linux"
WORK = ROOT / "build" / "package" / "appimage"
APPDIR = WORK / "AppDir"
DOWNLOADS = WORK / "downloads"
EXTRACTED_TOOLS = WORK / "tools"
TOOLS_MANIFEST = PACKAGING / "appimage-tools.json"
EXECUTABLE_MANIFEST = ROOT / "titles" / "crashbash" / "executable.json"
MODULE_MANIFESTS = tuple(sorted((ROOT / "titles" / "crashbash").glob("*_module.json")))
DESKTOP = LINUX / "dev.someoneisworking.crashbash.desktop"
ICON = LINUX / "dev.someoneisworking.crashbash.svg"
METAINFO = LINUX / "dev.someoneisworking.crashbash.metainfo.xml"
DEFAULT_BINARY = ROOT / "build" / "bin" / "crashbash_port"
ASSETS = ROOT / "external" / "psxport" / "assets"
COPYING = ROOT / "COPYING"


class Mismatch(RuntimeError):
    """A readable input contradicts the package contract."""


class Refused(RuntimeError):
    """The package could not make the requested assertion."""


@dataclass(frozen=True)
class Tool:
    name: str
    url: str
    sha256: str


@dataclass(frozen=True)
class MediaIdentity:
    executable_name: str
    executable_size: int
    executable_sha256: str
    data_path: str
    data_size: int
    data_sha256: str


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for block in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _read_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise Refused(f"cannot read {path}: {error}") from error


def _required_string(values: Mapping[str, object], field: str, source: Path) -> str:
    value = values.get(field)
    if not isinstance(value, str) or not value:
        raise Refused(f"{source} field {field} must be a non-empty string")
    return value


def media_identity() -> MediaIdentity:
    executable = _read_json(EXECUTABLE_MANIFEST)
    if not isinstance(executable, dict):
        raise Refused(f"{EXECUTABLE_MANIFEST} must contain an object")
    executable_name = _required_string(executable, "boot_path", EXECUTABLE_MANIFEST)
    executable_sha256 = _required_string(
        executable, "sha256", EXECUTABLE_MANIFEST
    ).lower()
    executable_size = executable.get("file_size")
    if not isinstance(executable_size, int) or executable_size <= 0:
        raise Refused(f"{EXECUTABLE_MANIFEST} field file_size must be positive")

    if not MODULE_MANIFESTS:
        raise Refused("no Crash Bash loaded-module manifests were found")
    data_facts: set[tuple[str, int, str]] = set()
    for manifest in MODULE_MANIFESTS:
        module = _read_json(manifest)
        if not isinstance(module, dict):
            raise Refused(f"{manifest} must contain an object")
        data_path = _required_string(module, "source_path", manifest)
        data_sha256 = _required_string(module, "source_sha256", manifest).lower()
        data_size = module.get("source_size")
        if not isinstance(data_size, int) or data_size <= 0:
            raise Refused(f"{manifest} field source_size must be positive")
        data_facts.add((data_path, data_size, data_sha256))
    if len(data_facts) != 1:
        rendered = ", ".join(
            f"{path}:{size}:{digest}" for path, size, digest in sorted(data_facts)
        )
        raise Mismatch(
            f"loaded-module manifests disagree on their complete data source: {rendered}"
        )
    data_path, data_size, data_sha256 = next(iter(data_facts))
    return MediaIdentity(
        executable_name=executable_name,
        executable_size=executable_size,
        executable_sha256=executable_sha256,
        data_path=data_path,
        data_size=data_size,
        data_sha256=data_sha256,
    )


def _load_tools() -> dict[str, Tool]:
    raw = _read_json(TOOLS_MANIFEST)
    if not isinstance(raw, dict) or raw.get("schema") != 1:
        raise Refused(f"{TOOLS_MANIFEST} must use schema 1")
    architectures = raw.get("architectures")
    if not isinstance(architectures, dict):
        raise Refused(f"{TOOLS_MANIFEST} has no architectures object")
    machine = platform.machine().lower()
    aliases = {"amd64": "x86_64", "x64": "x86_64"}
    architecture = aliases.get(machine, machine)
    selected = architectures.get(architecture)
    if not isinstance(selected, dict):
        raise Refused(f"no pinned AppImage tools are declared for {architecture}")

    tools: dict[str, Tool] = {}
    for name in ("linuxdeploy", "appimagetool", "runtime"):
        entry = selected.get(name)
        if not isinstance(entry, dict):
            raise Refused(f"{TOOLS_MANIFEST} is missing {architecture}.{name}")
        url = entry.get("url")
        digest = entry.get("sha256")
        if not isinstance(url, str) or not url.startswith("https://"):
            raise Refused(f"{architecture}.{name}.url must be HTTPS")
        if not isinstance(digest, str) or len(digest) != 64:
            raise Refused(f"{architecture}.{name}.sha256 must be a SHA-256 digest")
        tools[name] = Tool(name=name, url=url, sha256=digest.lower())
    return tools


def _reset_directory(path: Path) -> None:
    resolved = path.resolve()
    work = WORK.resolve()
    if resolved == work or not resolved.is_relative_to(work):
        raise Refused(f"refusing to clear package path outside {work}: {resolved}")
    if path.exists():
        if path.is_symlink() or not path.is_dir():
            raise Refused(f"refusing to clear non-directory package path {path}")
        shutil.rmtree(path)
    path.mkdir(parents=True)


def _remove_directory(path: Path) -> None:
    resolved = path.resolve()
    work = WORK.resolve()
    if not resolved.is_relative_to(work):
        raise Refused(f"refusing to remove package path outside {work}: {resolved}")
    if not path.exists():
        return
    if path.is_symlink() or not path.is_dir():
        raise Refused(f"refusing to remove non-directory package path {path}")
    shutil.rmtree(path)


def _download(tool: Tool) -> Path:
    DOWNLOADS.mkdir(parents=True, exist_ok=True)
    destination = DOWNLOADS / Path(urllib.request.urlparse(tool.url).path).name
    if destination.is_file() and _sha256(destination) == tool.sha256:
        return destination
    if destination.exists():
        destination.unlink()
    pending = destination.with_suffix(destination.suffix + ".pending")
    pending.unlink(missing_ok=True)
    try:
        with (
            urllib.request.urlopen(tool.url, timeout=120) as response,
            pending.open("wb") as output,
        ):
            shutil.copyfileobj(response, output)
    except (OSError, urllib.error.URLError) as error:
        pending.unlink(missing_ok=True)
        raise Refused(
            f"cannot download pinned {tool.name} from {tool.url}: {error}"
        ) from error
    actual = _sha256(pending)
    if actual != tool.sha256:
        pending.unlink(missing_ok=True)
        raise Mismatch(
            f"{tool.name} SHA-256 {actual} does not match pinned {tool.sha256}"
        )
    pending.replace(destination)
    destination.chmod(destination.stat().st_mode | stat.S_IXUSR)
    return destination


def _extract_appimage(tool: Tool) -> Path:
    source = _download(tool)
    target = EXTRACTED_TOOLS / tool.name
    marker = target / ".source-sha256"
    if marker.is_file() and marker.read_text(encoding="ascii").strip() == tool.sha256:
        app_run = target / "AppRun"
        if app_run.is_file():
            return app_run

    EXTRACTED_TOOLS.mkdir(parents=True, exist_ok=True)
    staging = EXTRACTED_TOOLS / f"{tool.name}-extract"
    _reset_directory(staging)
    completed = subprocess.run(
        [str(source), "--appimage-extract"],
        cwd=staging,
        text=True,
        capture_output=True,
        check=False,
    )
    extracted = staging / "squashfs-root"
    if completed.returncode != 0 or not (extracted / "AppRun").is_file():
        raise Refused(
            f"cannot extract pinned {tool.name}: "
            f"{(completed.stderr or completed.stdout or 'no diagnostic').strip()}"
        )
    _remove_directory(target)
    extracted.replace(target)
    marker.write_text(tool.sha256 + "\n", encoding="ascii")
    _remove_directory(staging)
    return target / "AppRun"


def _existing_executable(path: Path, description: str) -> Path:
    if not path.is_file() or not os.access(path, os.X_OK):
        raise Refused(f"{description} is missing or not executable at {path}")
    return path.resolve()


def _git_value(arguments: Sequence[str]) -> str:
    completed = subprocess.run(
        ["git", *arguments], cwd=ROOT, text=True, capture_output=True, check=False
    )
    if completed.returncode != 0:
        raise Refused(
            f"git {' '.join(arguments)} failed: "
            f"{(completed.stderr or completed.stdout or 'no diagnostic').strip()}"
        )
    return completed.stdout.strip()


def _release_version(requested: str | None, allow_dirty: bool) -> tuple[str, int]:
    dirty = bool(_git_value(["status", "--porcelain"]))
    if dirty and not allow_dirty:
        raise Refused(
            "the AppImage release path refuses a dirty worktree; finish and land the milestone first"
        )
    version = requested or _git_value(["describe", "--always", "--tags", "--dirty"])
    if not version or any(
        character
        not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._+-"
        for character in version
    ):
        raise Refused(
            f"version {version!r} contains a character that is unsafe in an artifact name"
        )
    epoch_text = _git_value(["show", "-s", "--format=%ct", "HEAD"])
    try:
        epoch = int(epoch_text)
    except ValueError as error:
        raise Refused(f"git returned invalid commit time {epoch_text!r}") from error
    return version, epoch


def _copy_runtime_assets(destination: Path) -> None:
    if not ASSETS.is_dir():
        raise Refused(f"redistributable framework assets are missing at {ASSETS}")
    shutil.copytree(ASSETS, destination, dirs_exist_ok=True)


def _run_checked(
    command: Sequence[str], *, environment: Mapping[str, str] | None = None
) -> None:
    completed = subprocess.run(
        list(command), text=True, capture_output=True, check=False, env=environment
    )
    if completed.returncode != 0:
        detail = "\n".join(
            output.strip()
            for output in (completed.stdout, completed.stderr)
            if output.strip()
        )
        raise Refused(f"{' '.join(command)} failed:\n{detail or 'no diagnostic'}")


def prepare_appdir(binary: Path, tools: Mapping[str, Tool]) -> MediaIdentity:
    identity = media_identity()
    linuxdeploy = _extract_appimage(tools["linuxdeploy"])
    _reset_directory(APPDIR)
    deploy_environment = os.environ.copy()
    deploy_environment["NO_STRIP"] = "1"
    _run_checked(
        [
            str(linuxdeploy),
            "--appdir",
            str(APPDIR),
            "--executable",
            str(binary),
            "--desktop-file",
            str(DESKTOP),
            "--icon-file",
            str(ICON),
        ],
        environment=deploy_environment,
    )

    # The port reads its assets from <APPDIR>/usr/share/psxport (psx::config::applyPackagedDefaults).
    _copy_runtime_assets(APPDIR / "usr" / "share" / "psxport" / "assets")
    if not COPYING.is_file():
        raise Refused(f"project license is missing at {COPYING}")
    licenses = APPDIR / "usr" / "share" / "licenses" / "crashbash"
    licenses.mkdir(parents=True, exist_ok=True)
    shutil.copy2(COPYING, licenses / "COPYING")
    metainfo = APPDIR / "usr" / "share" / "metainfo"
    metainfo.mkdir(parents=True, exist_ok=True)
    shutil.copy2(METAINFO, metainfo / METAINFO.name)

    app_run = APPDIR / "AppRun"
    app_run.unlink(missing_ok=True)
    app_run.symlink_to("usr/bin/crashbash_port")
    directory_icon = APPDIR / ".DirIcon"
    directory_icon.unlink(missing_ok=True)
    directory_icon.symlink_to("dev.someoneisworking.crashbash.svg")
    audit_appdir(identity)

    environment = os.environ.copy()
    environment["LD_LIBRARY_PATH"] = str(APPDIR / "usr" / "lib")
    _run_checked([str(APPDIR / "AppRun"), "--help"], environment=environment)
    return identity


def audit_appdir(identity: MediaIdentity) -> None:
    if not APPDIR.is_dir():
        raise Refused(f"AppDir is missing at {APPDIR}")
    forbidden_suffixes = {".chd", ".cue", ".bin", ".iso", ".img", ".pbp"}
    forbidden_hashes = {identity.executable_sha256, identity.data_sha256}
    regular_files = 0
    for path in APPDIR.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        regular_files += 1
        suffix = path.suffix.lower()
        if suffix in forbidden_suffixes:
            raise Mismatch(
                f"AppDir contains forbidden game-media-like file {path.relative_to(APPDIR)}"
            )
        with path.open("rb") as input_file:
            if input_file.read(8) == b"PS-X EXE":
                raise Mismatch(
                    f"AppDir contains a PS-X EXE at {path.relative_to(APPDIR)}"
                )
        size = path.stat().st_size
        if (
            size in {identity.executable_size, identity.data_size}
            and _sha256(path) in forbidden_hashes
        ):
            raise Mismatch(
                f"AppDir contains verified Crash Bash game content at {path.relative_to(APPDIR)}"
            )
    if regular_files == 0:
        raise Refused("AppDir audit scanned zero regular files")

    desktop_validator = shutil.which("desktop-file-validate")
    if desktop_validator is not None:
        _run_checked([desktop_validator, str(DESKTOP)])
    try:
        import xml.etree.ElementTree as element_tree

        element_tree.parse(ICON)
        element_tree.parse(METAINFO)
    except (OSError, element_tree.ParseError) as error:
        raise Mismatch(f"invalid release XML: {error}") from error
    print(f"[appimage] content audit: {regular_files} packaged files, 0 game files")


def build_appimage(
    output: Path, version: str, epoch: int, tools: Mapping[str, Tool]
) -> Path:
    appimagetool = _extract_appimage(tools["appimagetool"])
    runtime = _download(tools["runtime"])
    if output.suffix.casefold() == ".appimage":
        output.parent.mkdir(parents=True, exist_ok=True)
        destination = output
    else:
        output.mkdir(parents=True, exist_ok=True)
        destination = output / f"Crash_Bash-{version}-x86_64.AppImage"
    destination.unlink(missing_ok=True)
    environment = os.environ.copy()
    environment.update(
        {
            "ARCH": "x86_64",
            "VERSION": version,
            "SOURCE_DATE_EPOCH": str(epoch),
        }
    )
    _run_checked(
        [
            str(appimagetool),
            "--runtime-file",
            str(runtime),
            str(APPDIR),
            str(destination),
        ],
        environment=environment,
    )
    destination.chmod(destination.stat().st_mode | stat.S_IXUSR)
    verify_artifact(destination)
    print(
        f"[appimage] artifact: {destination} ({destination.stat().st_size} bytes, sha256 {_sha256(destination)})"
    )
    return destination


def verify_artifact(artifact: Path) -> None:
    artifact = artifact.resolve()
    check = WORK / "artifact-check"
    _reset_directory(check)
    completed = subprocess.run(
        [str(artifact), "--appimage-extract"],
        cwd=check,
        text=True,
        capture_output=True,
        check=False,
    )
    extracted = check / "squashfs-root"
    if completed.returncode != 0 or not (extracted / "AppRun").exists():
        raise Refused(
            "cannot extract the completed AppImage for verification: "
            + (completed.stderr or completed.stdout or "no diagnostic").strip()
        )
    _run_checked([str(extracted / "AppRun"), "--help"])
    _remove_directory(check)
    print("[appimage] artifact check: extracted AppRun answered --help")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--binary", type=Path, default=DEFAULT_BINARY, help="built crashbash_port"
    )
    parser.add_argument("--version", help="release version; defaults to git describe")
    parser.add_argument(
        "--output",
        type=Path,
        default=WORK / "out",
        help="artifact path or output directory",
    )
    parser.add_argument(
        "--appdir-only",
        action="store_true",
        help="prepare and audit AppDir without creating AppImage",
    )
    parser.add_argument(
        "--allow-dirty",
        action="store_true",
        help="developer-only staging override; published releases must use a clean milestone commit",
    )
    args = parser.parse_args(argv)

    try:
        binary = _existing_executable(args.binary, "Crash Bash port")
        version, epoch = _release_version(args.version, args.allow_dirty)
        tools = _load_tools()
        prepare_appdir(binary, tools)
        print(f"[appimage] AppDir: {APPDIR}")
        if not args.appdir_only:
            build_appimage(args.output, version, epoch, tools)
        return 0
    except Mismatch as error:
        print(f"MISMATCH: {error}", file=sys.stderr)
        return 1
    except Refused as error:
        print(f"REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
