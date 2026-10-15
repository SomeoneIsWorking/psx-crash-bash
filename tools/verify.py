#!/usr/bin/env python3
"""Verify Crash Bash's asset-free native adapter, title tests, and execution boundary."""

from __future__ import annotations

import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "migration"


def main() -> int:
    framework = Path(os.environ.get("PSXPORT_DIR", ROOT / "external" / "psxport")).resolve()
    shared = framework / "tools" / "port" / "consumer_verify.py"
    if not shared.is_file():
        print(f"[verify] REFUSED: no framework verifier at {shared}; run `uv run --no-project https://raw.githubusercontent.com/SomeoneIsWorking/psxport/main/tools/psxport_fetch.py --auto`", file=sys.stderr)
        return 2
    sys.path.insert(0, str(framework / "tools"))
    from port.consumer_verify import ConsumerVerifyConfig, run_consumer_verification

    return run_consumer_verification(
        ConsumerVerifyConfig(
            name="Crash Bash native title adapter",
            root=ROOT,
            build=BUILD,
            psxport=framework,
            product=BUILD / ("crashbash_runtime_test.exe" if os.name == "nt" else "crashbash_runtime_test"),
            cmake_module=ROOT / "CMakeLists.txt",
            test_regex="^crashbash_",
            cmake_definitions=("-DBUILD_TESTING=ON", "-DPSXPORT_BUILD_TESTS=OFF"),
        )
    )


if __name__ == "__main__":
    raise SystemExit(main())
