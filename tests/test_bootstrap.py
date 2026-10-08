"""Hermetic tests for the shipping Crash Bash launcher path."""

from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import bootstrap


class DependencyTest(unittest.TestCase):
    def test_dnf_refusal_names_one_actionable_install_command(self):
        missing = ["cmake", "cxx_compiler", "sdl3", "zstd"]
        self.assertEqual(
            bootstrap.install_instructions(missing, family="dnf"),
            ["sudo dnf install cmake gcc-c++ SDL3-devel libzstd-devel"],
        )

    def test_apt_and_brew_refusals_name_platform_packages(self):
        missing = ["pkg_config", "glslc", "sdl3_image", "cxx_compiler"]
        self.assertEqual(
            bootstrap.install_instructions(missing, family="apt"),
            ["sudo apt install pkg-config glslc libsdl3-image-dev build-essential"],
        )
        self.assertEqual(
            bootstrap.install_instructions(missing, family="brew"),
            ["brew install pkg-config shaderc sdl3_image", "xcode-select --install"],
        )

    def test_compiler_environment_is_honored_without_identity_policy(self):
        available = {
            "cmake",
            "git",
            "pkg-config",
            "glslc",
            "custom-c",
            "custom-cxx",
        }
        missing = bootstrap.missing_dependencies(
            environ={"CC": "custom-c -flag", "CXX": "custom-cxx -flag"},
            which=lambda command: command if command in available else None,
            pkg_exists=lambda _module: True,
            library_exists=lambda _library: True,
            header_exists=lambda _header: True,
        )
        self.assertEqual(missing, [])

    def test_missing_pkg_config_reports_every_unchecked_library(self):
        available = {"cmake", "git", "glslc", "cc", "c++"}
        missing = bootstrap.missing_dependencies(
            environ={},
            which=lambda command: command if command in available else None,
            library_exists=lambda _library: True,
            header_exists=lambda _header: True,
        )
        self.assertEqual(
            missing,
            ["pkg_config", "sdl3", "sdl3_image", "freetype"],
        )

    def test_missing_zlib_header_reports_the_development_package(self):
        available = {"cmake", "git", "pkg-config", "glslc", "cc", "c++"}
        missing = bootstrap.missing_dependencies(
            environ={},
            which=lambda command: command if command in available else None,
            pkg_exists=lambda _module: True,
            library_exists=lambda _library: True,
            header_exists=lambda header: header != "zlib.h",
        )
        self.assertEqual(missing, ["zlib"])
        self.assertEqual(
            bootstrap.install_instructions(missing, family="dnf"),
            ["sudo dnf install zlib-devel"],
        )


class ProductWorkflowTest(unittest.TestCase):
    def test_help_exits_before_dependency_or_product_discovery(self):
        for option in ("-h", "--help"):
            with (
                self.subTest(option=option),
                mock.patch.object(bootstrap, "check_native_dependencies") as check,
                mock.patch.object(bootstrap, "prepare_product") as prepare,
                mock.patch.object(bootstrap.os, "execve") as execute,
                contextlib.redirect_stdout(io.StringIO()) as output,
                self.assertRaises(SystemExit) as stopped,
            ):
                bootstrap.main([option])
            self.assertEqual(stopped.exception.code, 0)
            self.assertIn("usage:", output.getvalue().lower())
            check.assert_not_called()
            prepare.assert_not_called()
            execute.assert_not_called()

    def test_prepare_uses_locked_python_and_required_dynarec_target(self):
        (ROOT / "scratch").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as directory:
            temporary = Path(directory)
            paths = bootstrap.ProductPaths(
                build=temporary / "build",
                port=temporary / "bin" / "crashbash_port",
                executable=temporary / "bin" / "crashbash" / "SCUS_945.70",
            )
            paths.port.parent.mkdir(parents=True)
            paths.port.touch()
            paths.executable.parent.mkdir(parents=True)
            paths.executable.touch()
            commands: list[list[str]] = []

            def record(arguments, _environment):
                commands.append(list(arguments))

            with (
                mock.patch.object(bootstrap, "check_native_dependencies"),
                mock.patch.dict(
                    bootstrap.os.environ,
                    {
                        "PSXPORT_VK_HEADLESS": "1",
                        "PSXPORT_NOAUDIO": "1",
                        "PSXPORT_NOPACE": "1",
                    },
                    clear=False,
                ),
            ):
                environment = bootstrap.prepare_product(
                    None, paths=paths, runner=record
                )

        configure = [
            command for command in commands if command[0] == "cmake" and "-S" in command
        ]
        self.assertEqual(len(configure), 1)
        interpreter_setting = f"-DPython3_EXECUTABLE={sys.executable}"
        self.assertTrue(all(interpreter_setting in command for command in configure))
        self.assertTrue(all("-DBUILD_TESTING=OFF" in command for command in configure))
        self.assertTrue(
            all("-DPSXPORT_BUILD_TESTS=OFF" in command for command in configure)
        )
        self.assertTrue(all("--fresh" not in command for command in configure))
        self.assertFalse(
            any(
                argument.startswith(("-DCMAKE_C_COMPILER=", "-DCMAKE_CXX_COMPILER="))
                for command in configure
                for argument in command
            )
        )
        python_commands = [
            command for command in commands if command[0] == sys.executable
        ]
        self.assertEqual(len(python_commands), 2)
        product_build = [
            command
            for command in commands
            if command[:2] == ["cmake", "--build"] and "crashbash_port" in command
        ]
        self.assertEqual(len(product_build), 1)
        self.assertFalse(any("ctest" in command for command in commands))
        provision = [
            command for command in python_commands if "tools/provision.py" in command
        ]
        self.assertEqual(len(provision), 1)
        self.assertEqual(
            provision[0][provision[0].index("--discdump") + 1],
            str(paths.build / "psxport_build" / "tools" / "discdump"),
        )
        self.assertEqual(environment["PSXPORT_VK_WINDOW"], "1")
        for agent_key in (
            "PSXPORT_VK_HEADLESS",
            "PSXPORT_NOAUDIO",
            "PSXPORT_NOPACE",
        ):
            self.assertNotIn(agent_key, environment)

    def test_dependency_refusal_runs_no_setup_command(self):
        commands: list[list[str]] = []
        with (
            mock.patch.object(
                bootstrap,
                "check_native_dependencies",
                side_effect=bootstrap.LaunchError("missing native dependencies"),
            ),
            self.assertRaises(bootstrap.LaunchError),
        ):
            bootstrap.prepare_product(
                None,
                runner=lambda arguments, _env: commands.append(list(arguments)),
            )
        self.assertEqual(commands, [])

    def test_zero_arguments_executes_the_current_port_with_required_executable(self):
        environment = {"LOCKED": "1"}
        with (
            mock.patch.object(bootstrap, "prepare_product", return_value=environment),
            mock.patch.object(bootstrap.os, "execve") as execute,
        ):
            self.assertEqual(bootstrap.main([]), 0)
        execute.assert_called_once_with(
            bootstrap.PORT,
            [str(bootstrap.PORT), str(bootstrap.EXECUTABLE)],
            environment,
        )

    def test_check_mode_does_not_prepare_or_launch(self):
        with (
            mock.patch.object(bootstrap, "check_native_dependencies") as check,
            mock.patch.object(bootstrap, "prepare_product") as prepare,
            mock.patch.object(bootstrap.os, "execve") as execute,
        ):
            self.assertEqual(bootstrap.main(["--check"]), 0)
        check.assert_called_once_with()
        prepare.assert_not_called()
        execute.assert_not_called()

    def test_prepare_only_builds_but_does_not_launch(self):
        with (
            mock.patch.object(bootstrap, "prepare_product", return_value={}) as prepare,
            mock.patch.object(bootstrap.os, "execve") as execute,
        ):
            self.assertEqual(bootstrap.main(["--prepare-only"]), 0)
        prepare.assert_called_once_with(None)
        execute.assert_not_called()


class ShimTest(unittest.TestCase):
    def test_run_sh_is_only_the_frozen_uv_handoff(self):
        self.assertEqual(
            (ROOT / "run.sh").read_text(encoding="utf-8"),
            '#!/usr/bin/env sh\ncd "$(dirname "$0")" || exit 1\n'
            'exec uv run --frozen python bootstrap.py "$@"\n',
        )


if __name__ == "__main__":
    unittest.main()
