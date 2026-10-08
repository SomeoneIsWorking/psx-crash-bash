"""Provision, build, and launch the current Crash Bash port product."""

from __future__ import annotations

import argparse
import ctypes.util
import os
import runpy
import shlex
import shutil
import subprocess
import sys
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build" / "player"
PORT = (
    ROOT
    / "build"
    / "bin"
    / ("crashbash_port.exe" if os.name == "nt" else "crashbash_port")
)
EXECUTABLE = ROOT / "scratch" / "bin" / "crashbash" / "SCUS_945.70"

COMMAND_CAPABILITIES = {
    "cmake": "cmake",
    "git": "git",
    "pkg_config": "pkg-config",
    "glslc": "glslc",
}
PKG_CONFIG_CAPABILITIES = {
    "sdl3": "sdl3",
    "sdl3_image": "sdl3-image",
    "freetype": "freetype2",
}
LIBRARY_CAPABILITIES = {"zlib": "z", "zstd": "zstd"}
HEADER_CAPABILITIES = {"zlib": "zlib.h"}
PACKAGE_NAMES = {
    "dnf": {
        "cmake": "cmake",
        "git": "git",
        "pkg_config": "pkgconf-pkg-config",
        "glslc": "glslc",
        "c_compiler": "gcc",
        "cxx_compiler": "gcc-c++",
        "sdl3": "SDL3-devel",
        "sdl3_image": "SDL3_image-devel",
        "freetype": "freetype-devel",
        "zlib": "zlib-devel",
        "zstd": "libzstd-devel",
    },
    "apt": {
        "cmake": "cmake",
        "git": "git",
        "pkg_config": "pkg-config",
        "glslc": "glslc",
        "c_compiler": "build-essential",
        "cxx_compiler": "build-essential",
        "sdl3": "libsdl3-dev",
        "sdl3_image": "libsdl3-image-dev",
        "freetype": "libfreetype-dev",
        "zlib": "zlib1g-dev",
        "zstd": "libzstd-dev",
    },
    "brew": {
        "cmake": "cmake",
        "git": "git",
        "pkg_config": "pkg-config",
        "glslc": "shaderc",
        "sdl3": "sdl3",
        "sdl3_image": "sdl3_image",
        "freetype": "freetype",
        "zlib": "zlib",
        "zstd": "zstd",
    },
}


class LaunchError(RuntimeError):
    """The player path could not be prepared."""


@dataclass(frozen=True)
class ProductPaths:
    build: Path = BUILD
    port: Path = PORT
    executable: Path = EXECUTABLE


Runner = Callable[[Sequence[str], Mapping[str, str]], None]


def _os_release(path: Path = Path("/etc/os-release")) -> dict[str, str]:
    if not path.is_file():
        return {}
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        key, separator, raw = line.partition("=")
        if not separator:
            continue
        values[key] = raw.strip().strip("\"'")
    return values


def package_family(
    *, platform: str = sys.platform, release: Mapping[str, str] | None = None
) -> str | None:
    """Return the supported host package family without identifying a compiler."""
    if platform == "darwin":
        return "brew"
    if platform.startswith("win"):
        return "windows"
    if platform.startswith("linux"):
        release = release if release is not None else _os_release()
        identities = {release.get("ID", ""), *release.get("ID_LIKE", "").split()}
        if identities & {"fedora", "rhel", "centos"}:
            return "dnf"
        if identities & {"debian", "ubuntu"}:
            return "apt"
    return None


def _configured_parts(
    variable: str, fallback: str, environ: Mapping[str, str]
) -> list[str]:
    raw = environ.get(variable, fallback)
    try:
        parts = shlex.split(raw)
    except ValueError as error:
        raise LaunchError(f"${variable} is not a valid command: {error}") from error
    if not parts:
        raise LaunchError(f"${variable} is empty")
    return parts


def _configured_command(
    variable: str, fallback: str, environ: Mapping[str, str]
) -> str:
    return _configured_parts(variable, fallback, environ)[0]


def _compiler_has_header(header: str, environ: Mapping[str, str]) -> bool:
    command = [
        *_configured_parts("CC", "cc", environ),
        "-E",
        "-x",
        "c",
        "-",
    ]
    try:
        completed = subprocess.run(
            command,
            input=f"#include <{header}>\n",
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            text=True,
            check=False,
        )
    except OSError:
        return False
    return completed.returncode == 0


def missing_dependencies(
    *,
    environ: Mapping[str, str] = os.environ,
    which: Callable[[str], str | None] = shutil.which,
    pkg_exists: Callable[[str], bool] | None = None,
    library_exists: Callable[[str], bool] | None = None,
    header_exists: Callable[[str], bool] | None = None,
) -> list[str]:
    """Return missing capabilities while accepting any configured C/C++ compiler."""
    missing = [
        capability
        for capability, command in COMMAND_CAPABILITIES.items()
        if which(command) is None
    ]
    for variable, fallback, capability in (
        ("CC", "cc", "c_compiler"),
        ("CXX", "c++", "cxx_compiler"),
    ):
        if which(_configured_command(variable, fallback, environ)) is None:
            missing.append(capability)

    if "pkg_config" in missing:
        missing.extend(PKG_CONFIG_CAPABILITIES)
    else:
        if pkg_exists is None:
            pkg_exists = lambda module: (
                subprocess.run(
                    ["pkg-config", "--exists", module], check=False
                ).returncode
                == 0
            )
        missing.extend(
            capability
            for capability, module in PKG_CONFIG_CAPABILITIES.items()
            if not pkg_exists(module)
        )
    if library_exists is None:
        library_exists = lambda library: ctypes.util.find_library(library) is not None
    missing.extend(
        capability
        for capability, library in LIBRARY_CAPABILITIES.items()
        if not library_exists(library)
    )
    if header_exists is None:
        header_exists = lambda header: _compiler_has_header(header, environ)
    missing.extend(
        capability
        for capability, header in HEADER_CAPABILITIES.items()
        if not header_exists(header)
    )
    return list(dict.fromkeys(missing))


def install_instructions(
    capabilities: Sequence[str], *, family: str | None = None
) -> list[str]:
    family = family or package_family()
    if family in PACKAGE_NAMES:
        package_map = PACKAGE_NAMES[family]
        packages = list(
            dict.fromkeys(
                package_map[capability]
                for capability in capabilities
                if capability in package_map
            )
        )
        commands: list[str] = []
        if packages:
            prefix = {
                "dnf": "sudo dnf install",
                "apt": "sudo apt install",
                "brew": "brew install",
            }[family]
            commands.append(f"{prefix} {' '.join(packages)}")
        if family == "brew" and (
            "c_compiler" in capabilities or "cxx_compiler" in capabilities
        ):
            commands.append("xcode-select --install")
        return commands
    if family == "windows":
        commands = []
        tools = [
            package
            for capability, package in (
                ("cmake", "Kitware.CMake"),
                ("git", "Git.Git"),
            )
            if capability in capabilities
        ]
        commands.extend(f"winget install --id {package}" for package in tools)
        libraries = [
            capability.replace("_", "-")
            for capability in capabilities
            if capability in PKG_CONFIG_CAPABILITIES
        ]
        if libraries:
            commands.append("vcpkg install " + " ".join(libraries))
        return commands
    return []


def check_native_dependencies() -> None:
    missing = missing_dependencies()
    if not missing:
        return
    labels = {
        **COMMAND_CAPABILITIES,
        **PKG_CONFIG_CAPABILITIES,
        "zlib": "zlib",
        "zstd": "zstd",
        "c_compiler": "$CC or cc",
        "cxx_compiler": "$CXX or c++",
    }
    lines = ["missing native dependencies:"]
    lines.extend(f"  - {labels[capability]}" for capability in missing)
    commands = install_instructions(missing)
    if commands:
        lines.append("install them, then rerun ./run.sh:")
        lines.extend(f"  {command}" for command in commands)
    else:
        lines.append(
            "this platform has no tracked package mapping; install the named dependencies "
            "or set CC/CXX to available compilers"
        )
    raise LaunchError("\n".join(lines))


def run_command(arguments: Sequence[str], environment: Mapping[str, str]) -> None:
    rendered = shlex.join(arguments)
    print(f"[run] {rendered}", file=sys.stderr)
    completed = subprocess.run(
        list(arguments), cwd=ROOT, env=dict(environment), check=False
    )
    if completed.returncode:
        raise LaunchError(f"command exited {completed.returncode}: {rendered}")


def configure_arguments(build: Path) -> list[str]:
    """Configure in place so CMake can incrementally reuse the player build."""
    return [
        "cmake",
        "-S",
        ".",
        "-B",
        str(build),
        f"-DPython3_EXECUTABLE={sys.executable}",
        "-DPython3_FIND_VIRTUALENV=ONLY",
        "-DBUILD_TESTING=OFF",
        "-DPSXPORT_BUILD_TESTS=OFF",
    ]


def prepare_product(
    disc: Path | None,
    *,
    paths: ProductPaths | None = None,
    runner: Runner = run_command,
) -> dict[str, str]:
    check_native_dependencies()
    paths = paths or ProductPaths()
    environment = dict(os.environ)
    if disc is not None:
        environment["PSXPORT_CRASHBASH_DISC"] = str(disc.resolve())
    jobs = str(min(os.cpu_count() or 1, 16))

    runner([sys.executable, "-B", "tools/psxport_fetch.py", "--auto"], environment)
    runner(configure_arguments(paths.build), environment)
    runner(
        [
            "cmake",
            "--build",
            str(paths.build),
            "--target",
            "discdump",
            "-j",
            jobs,
        ],
        environment,
    )
    provision = [
        sys.executable,
        "-B",
        "tools/provision.py",
        "--discdump",
        str(paths.build / "psxport_build" / "tools" / "discdump"),
    ]
    if disc is not None:
        provision.append(str(disc))
    runner(provision, environment)
    runner(
        [
            "cmake",
            "--build",
            str(paths.build),
            "--target",
            "crashbash_port",
            "-j",
            jobs,
        ],
        environment,
    )
    missing_outputs = [
        path for path in (paths.port, paths.executable) if not path.is_file()
    ]
    if missing_outputs:
        rendered = ", ".join(str(path) for path in missing_outputs)
        raise LaunchError(
            f"build did not produce required product input(s): {rendered}"
        )
    framework = Path(environment.get("PSXPORT_DIR", ROOT / "external" / "psxport"))
    policy = runpy.run_path(str(framework / "tools/port/launch_environment.py"))
    # `product` names the run-log directory; psxport requires it.
    environment = policy["player_environment"](environment, product="crashbash")
    environment.setdefault("PSXPORT_ASSET_DIR", str(framework))
    return environment


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "disc",
        nargs="?",
        type=Path,
        help="optional Crash Bash USA CHD; otherwise use env/.env/drop-in",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--check",
        action="store_true",
        help="check native prerequisites without syncing, building, provisioning, or launching",
    )
    mode.add_argument(
        "--prepare-only",
        action="store_true",
        help="provision and build the current product without launching it",
    )
    args = parser.parse_args(argv)
    try:
        if args.check:
            check_native_dependencies()
            print("[run] native prerequisites available; no setup or launch performed")
            return 0
        environment = prepare_product(args.disc)
        if args.prepare_only:
            print(f"[run] current product prepared: {PORT}")
            return 0
        print(f"[run] launching Crash Bash port: {PORT}", file=sys.stderr)
        os.execve(PORT, [str(PORT), str(EXECUTABLE)], environment)
    except (OSError, LaunchError) as error:
        print(f"[run] error: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
