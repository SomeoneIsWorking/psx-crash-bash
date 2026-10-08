"""Static source-boundary checks for the Crash Bash dynarec migration."""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path


# Counted override registrations and original-call sites.
EXPECTED_OVERRIDE_REGISTRATIONS = 35
EXPECTED_ORIGINAL_CALLS = 15
SOURCE_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"})
ENVIRONMENT_CONFIG_OWNER = Path("packaging/linux/user_paths.cpp")
DIRECT_STDERR_PATTERN = re.compile(
    r"(?:\bstd::(?:cerr|clog)\b|\b(?:std::)?(?:fputs|fprintf)\s*\(\s*stderr\b|"
    r"\b(?:::)?write\s*\(\s*STDERR_FILENO\b)"
)
GETENV_PATTERN = re.compile(r"\b(?:std::)?getenv\s*\(")
STATIC_PRODUCT_MARKERS = (
    "CRASHBASH_HAVE_SUBSTRATE",
    "gen_func_",
    "rec_dispatch(",
    "overrides::install(",
    "recomp_iface.h",
    "rec_decls.h",
    "overlay_table.h",
    "LegacyGameRuntimeAdapter",
    "runtime/recomp",
)
RETIRED_TRACKED_PATHS = (
    "game/core/crashbash_application.cpp",
    "game/core/crashbash_application.h",
    "game/core/crashbash_runtime.cpp",
    "game/core/crashbash_runtime.h",
    "game/core/game_config.cpp",
    "game/core/game_hooks.cpp",
    "game/core/legacy_game_interface.h",
    "game/core/main.cpp",
    "game/core/recomp_register.cpp",
    "game/recomp_seeds.json",
    "tests/test_guest_vram_policy.cpp",
    "tools/recomp_bootstrap.py",
    "tools/inventory_render_anchors.py",
    "tools/verify_boot.py",
    "tools/verify_menu_accept.py",
    "tools/verify_cdc_phase_progress.py",
    "tools/verify_render_anchor_reach.py",
)


@dataclass(frozen=True)
class SourcePolicyReport:
    source_files: int
    override_registrations: int
    original_calls: int


class SourcePolicyError(RuntimeError):
    """Tracked sources violate the native/dynarec product boundary."""


def _source_paths(root: Path) -> list[Path]:
    game = root / "game"
    if not game.is_dir():
        raise SourcePolicyError(f"missing product source directory: {game}")
    paths = []
    for relative in ("game", "packaging"):
        directory = root / relative
        if directory.is_dir():
            paths.extend(
                path
                for path in directory.rglob("*")
                if path.is_file() and path.suffix in SOURCE_SUFFIXES
            )
    paths.extend(path for path in (root / "tools").glob("*.py") if path.name != "source_policy.py")
    paths.extend(path for path in (root / "CMakeLists.txt", root / "bootstrap.py") if path.is_file())
    paths.sort()
    if not paths:
        raise SourcePolicyError(f"scanned {game} but found 0 C/C++ source files")
    return paths


def check_source_policy(root: Path) -> SourcePolicyReport:
    root = root.resolve()
    stale = [relative for relative in RETIRED_TRACKED_PATHS if (root / relative).exists()]
    if stale:
        raise SourcePolicyError("retired static path still exists: " + ", ".join(stale))
    if (root / "generated").exists():
        raise SourcePolicyError("retired generated guest-code directory still exists")

    paths = _source_paths(root)
    texts = {path: path.read_text(encoding="utf-8") for path in paths}
    violations = [
        f"{path.relative_to(root)}: {marker}"
        for path, source in texts.items()
        for marker in STATIC_PRODUCT_MARKERS
        if marker in source
    ]
    if violations:
        raise SourcePolicyError("forbidden static execution surface:\n" + "\n".join(violations))

    direct_stderr = [
        str(path.relative_to(root))
        for path, source in texts.items()
        if path.suffix in SOURCE_SUFFIXES and DIRECT_STDERR_PATTERN.search(source)
    ]
    if direct_stderr:
        raise SourcePolicyError(
            "direct C/C++ stderr bypasses the configurable logger: "
            + ", ".join(direct_stderr)
        )

    scattered_environment_reads = [
        str(path.relative_to(root))
        for path, source in texts.items()
        if path.suffix in SOURCE_SUFFIXES
        and path.relative_to(root) != ENVIRONMENT_CONFIG_OWNER
        and GETENV_PATTERN.search(source)
    ]
    if scattered_environment_reads:
        raise SourcePolicyError(
            f"C/C++ getenv is owned only by {ENVIRONMENT_CONFIG_OWNER}: "
            + ", ".join(scattered_environment_reads)
        )

    implementation_sources = [source for path, source in texts.items() if path.suffix in {".cc", ".cpp", ".cxx"}]
    override_registrations = sum(source.count("runtime::registerNativeOverride(") for source in implementation_sources)
    original_calls = sum(source.count("runtime::callOriginal(") for source in implementation_sources)
    if override_registrations != EXPECTED_OVERRIDE_REGISTRATIONS:
        raise SourcePolicyError(
            f"runtime override boundary has {override_registrations} registrations; "
            f"expected {EXPECTED_OVERRIDE_REGISTRATIONS}"
        )
    if original_calls != EXPECTED_ORIGINAL_CALLS:
        raise SourcePolicyError(
            f"runtime original-call boundary has {original_calls} calls; expected {EXPECTED_ORIGINAL_CALLS}"
        )
    return SourcePolicyReport(len(paths), override_registrations, original_calls)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    try:
        report = check_source_policy(root)
    except SourcePolicyError as failure:
        print(f"[source-policy] FAILED: {failure}", file=sys.stderr)
        return 1
    print(
        f"[source-policy] OK: scanned {report.source_files} source file(s); "
        f"{report.override_registrations} native override registration(s) and "
        f"{report.original_calls} original call(s) against the declared boundary"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
