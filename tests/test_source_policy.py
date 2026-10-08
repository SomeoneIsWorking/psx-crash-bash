from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from source_policy import (  # noqa: E402
    EXPECTED_ORIGINAL_CALLS,
    EXPECTED_OVERRIDE_REGISTRATIONS,
    SourcePolicyError,
    STATIC_PRODUCT_MARKERS,
    check_source_policy,
)


class SourcePolicyTests(unittest.TestCase):
    def test_shipping_tree_passes(self) -> None:
        report = check_source_policy(ROOT)
        self.assertEqual(report.override_registrations, EXPECTED_OVERRIDE_REGISTRATIONS)
        self.assertEqual(report.original_calls, EXPECTED_ORIGINAL_CALLS)

    def test_forbidden_generated_directory_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "game").mkdir()
            (root / "game" / "owner.cpp").write_text(
                "runtime::registerNativeOverride(" * EXPECTED_OVERRIDE_REGISTRATIONS
                + "runtime::callOriginal(" * EXPECTED_ORIGINAL_CALLS,
                encoding="utf-8",
            )
            (root / "generated").mkdir()
            with self.assertRaisesRegex(SourcePolicyError, "generated guest-code"):
                check_source_policy(root)

    def test_forbidden_dispatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "game").mkdir()
            (root / "game" / "owner.cpp").write_text("rec_dispatch(core, target);", encoding="utf-8")
            with self.assertRaisesRegex(SourcePolicyError, "forbidden static execution"):
                check_source_policy(root)

    def test_legacy_runtime_adapter_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "game").mkdir()
            (root / "game" / "owner.cpp").write_text(
                "LegacyGameRuntimeAdapter adapter;", encoding="utf-8"
            )
            with self.assertRaisesRegex(SourcePolicyError, "LegacyGameRuntimeAdapter"):
                check_source_policy(root)

    def test_runtime_recomp_include_path_fails(self) -> None:
        marker = next(marker for marker in STATIC_PRODUCT_MARKERS if marker.startswith("runtime/"))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "game").mkdir()
            (root / "game" / "owner.cpp").write_text(
                f'#include "{marker}/core.h"', encoding="utf-8"
            )
            with self.assertRaisesRegex(SourcePolicyError, marker):
                check_source_policy(root)

    def test_direct_stderr_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "game").mkdir()
            (root / "game" / "owner.cpp").write_text(
                'std::fprintf(stderr, "failure\\n");', encoding="utf-8"
            )
            with self.assertRaisesRegex(SourcePolicyError, "configurable logger"):
                check_source_policy(root)

    def test_getenv_outside_config_owner_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "game").mkdir()
            (root / "game" / "owner.cpp").write_text(
                'const char *value = std::getenv("OPTION");', encoding="utf-8"
            )
            with self.assertRaisesRegex(SourcePolicyError, "getenv is owned only"):
                check_source_policy(root)


if __name__ == "__main__":
    unittest.main()
