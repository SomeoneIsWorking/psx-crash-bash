#!/usr/bin/env python3
"""Verify Crash Bash's asset-free native adapter, title tests, and execution boundary."""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "migration"


def main() -> int:
    bootstrap = subprocess.run(
        [sys.executable, ROOT / "tools" / "psxport_fetch.py", "--auto"],
        cwd=ROOT,
        check=False,
    )
    if bootstrap.returncode:
        return bootstrap.returncode
    framework = Path(os.environ.get("PSXPORT_DIR", ROOT / "external" / "psxport")).resolve()
    shared = framework / "tools" / "port" / "consumer_verify.py"
    if not shared.is_file():
        print(f"[verify] required shared consumer verifier is missing: {shared}", file=sys.stderr)
        return 2
    sys.path.insert(0, str(framework / "tools"))
    from port.consumer_verify import ConsumerVerifyConfig, run_consumer_verification

    return run_consumer_verification(
        ConsumerVerifyConfig(
            name="Crash Bash native title adapter",
            root=ROOT,
            build=BUILD,
            psxport=framework,
            product=BUILD / ("crashbash_title_adapter_test.exe" if os.name == "nt" else "crashbash_title_adapter_test"),
            cmake_module=ROOT / "CMakeLists.txt",
            test_regex="^crashbash_",
            cmake_definitions=("-DBUILD_TESTING=ON", "-DPSXPORT_BUILD_TESTS=OFF"),
        )
    )


if __name__ == "__main__":
    raise SystemExit(main())
