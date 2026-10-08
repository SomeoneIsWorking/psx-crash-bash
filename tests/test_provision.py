#!/usr/bin/env python3
"""Both-answer tests for Crash Bash's shipping disc/executable provisioner."""

from __future__ import annotations

import hashlib
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import provision


def synthetic_executable() -> bytes:
    text = bytearray(0x80)
    markers = (b"North America fixture", b"BASCUS fixture")
    text[0x10 : 0x10 + len(markers[0])] = markers[0]
    text[0x40 : 0x40 + len(markers[1])] = markers[1]
    image = bytearray(0x800) + text
    image[0:8] = b"PS-X EXE"
    struct.pack_into("<II", image, 0x10, 0x80010020, 0x80018000)
    struct.pack_into("<II", image, 0x18, 0x80010000, len(text))
    struct.pack_into("<II", image, 0x30, 0x801FFFF0, 0x10)
    return bytes(image)


def write_manifest(path: pathlib.Path, executable: bytes) -> None:
    path.write_text(
        json.dumps(
            {
                "title": "Crash Bash fixture",
                "region": "test",
                "serial": "SCUS-TEST",
                "boot_path": "SCUS_TEST.00",
                "file_size": len(executable),
                "sha1": hashlib.sha1(executable).hexdigest(),
                "sha256": hashlib.sha256(executable).hexdigest(),
                "header": {
                    "entry": "0x80010020",
                    "gp": "0x80018000",
                    "text_address": "0x80010000",
                    "text_size": "0x00000080",
                    "stack_address": "0x801FFFF0",
                    "stack_offset": "0x00000010",
                },
                "region_markers": ["North America fixture", "BASCUS fixture"],
            }
        ),
        encoding="utf-8",
    )


def synthetic_module_source() -> tuple[bytes, bytes, bytes]:
    boot = bytearray(0x80)
    menu = bytearray(0x80)
    struct.pack_into("<I", boot, 0, 0x80018020)
    struct.pack_into("<I", menu, 0, 0x80020020)
    boot[4:] = bytes(range(4, 0x80))
    menu[4:] = bytes(reversed(range(4, 0x80)))
    source = bytes(0x80) + bytes(boot) + bytes(menu)
    return source, bytes(boot), bytes(menu)


def write_module_manifest(
    path: pathlib.Path, source: bytes, payload: bytes, load_address: int, entry: int
) -> None:
    offset = source.index(payload)
    path.write_text(
        json.dumps(
            {
                "title": "Crash Bash fixture module",
                "serial": "SCUS-TEST",
                "source_path": "DATA/CRASHBSH.DAT",
                "source_size": len(source),
                "source_sha256": hashlib.sha256(source).hexdigest(),
                "source_file_lba": 1,
                "payload_disc_lba": 1 + offset // len(payload),
                "sector_size": len(payload),
                "sector_count": 1,
                "payload_offset": f"0x{offset:08X}",
                "payload_size": f"0x{len(payload):08X}",
                "payload_sha256": hashlib.sha256(payload).hexdigest(),
                "load_address": f"0x{load_address:08X}",
                "entry_pointer_offset": "0x00000000",
                "entry": f"0x{entry:08X}",
            }
        ),
        encoding="utf-8",
    )


class FakeDiscdump:
    def __init__(
        self,
        executable: bytes,
        module_source: bytes,
        boot: str = "SCUS_TEST.00",
        returncode: int = 0,
    ):
        self.executable = executable
        self.module_source = module_source
        self.boot = boot
        self.returncode = returncode
        self.requests: list[str] = []

    def __call__(self, command, **_kwargs):
        disc_path = command[2]
        self.requests.append(disc_path)
        if self.returncode:
            return subprocess.CompletedProcess(
                command, self.returncode, "", "fixture extraction failed"
            )
        output = pathlib.Path(command[4]) / pathlib.PurePosixPath(disc_path).name
        if disc_path == "SYSTEM.CNF":
            output.write_text(f"BOOT = cdrom:\\{self.boot};1\r\n", encoding="ascii")
        elif disc_path.endswith("CRASHBSH.DAT"):
            output.write_bytes(self.module_source)
        else:
            output.write_bytes(self.executable)
        return subprocess.CompletedProcess(command, 0, f"dumped {output}", "")


class ResolveDiscTest(unittest.TestCase):
    def test_default_discdump_uses_the_maintainer_build_root(self):
        self.assertEqual(
            provision.DEFAULT_DISCDUMP,
            ROOT / "build" / "maintainer" / "psxport_build" / "tools" / "discdump",
        )

    def test_cli_environment_dotenv_and_dropin_precedence(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as directory:
            root = pathlib.Path(directory)
            configured = root / "configured"
            configured.mkdir()
            cli = configured / "cli.chd"
            env = configured / "env.chd"
            generic = configured / "generic.chd"
            dotenv = configured / "dotenv.chd"
            dropin = root / "dropin.chd"
            for path in (cli, env, generic, dotenv, dropin):
                path.touch()
            (root / ".env").write_text(
                f"PSXPORT_CRASHBASH_DISC={dotenv}\nPSXPORT_DISC={generic}\n",
                encoding="utf-8",
            )

            self.assertEqual(
                provision.resolve_disc(str(cli), root=root, environ={})[0],
                cli.resolve(),
            )
            self.assertEqual(
                provision.resolve_disc(
                    None,
                    root=root,
                    environ={
                        "PSXPORT_CRASHBASH_DISC": str(env),
                        "PSXPORT_DISC": str(generic),
                    },
                )[0],
                env.resolve(),
            )
            self.assertEqual(
                provision.resolve_disc(None, root=root, environ={})[0], dotenv.resolve()
            )
            (root / ".env").unlink()
            self.assertEqual(
                provision.resolve_disc(None, root=root, environ={})[0], dropin.resolve()
            )

    def test_missing_authoritative_path_refuses_instead_of_falling_through(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as directory:
            root = pathlib.Path(directory)
            (root / "fallback.chd").touch()
            with self.assertRaises(provision.Refused):
                provision.resolve_disc(str(root / "missing.chd"), root=root, environ={})

    def test_multiple_dropins_refuse_ambiguity(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "scratch") as directory:
            root = pathlib.Path(directory)
            (root / "a.chd").touch()
            (root / "b.CHD").touch()
            with self.assertRaises(provision.Refused):
                provision.resolve_disc(None, root=root, environ={})


class ProvisionTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(dir=ROOT / "scratch")
        self.directory = pathlib.Path(self.temporary.name)
        self.disc = self.directory / "disc.chd"
        self.disc.touch()
        self.discdump = self.directory / "discdump"
        self.discdump.touch()
        self.output = self.directory / "output" / "SCUS_TEST.00"
        self.manifest = self.directory / "executable.json"
        self.module_manifest = self.directory / "boot_module.json"
        self.menu_manifest = self.directory / "menu_module.json"
        self.executable = synthetic_executable()
        self.module_source, self.module, self.menu_module = synthetic_module_source()
        write_manifest(self.manifest, self.executable)
        write_module_manifest(
            self.module_manifest,
            self.module_source,
            self.module,
            0x80018000,
            0x80018020,
        )
        write_module_manifest(
            self.menu_manifest,
            self.module_source,
            self.menu_module,
            0x80020000,
            0x80020020,
        )
        self.overlays = self.directory / "output" / "overlays"
        self.modules = {"BOOT": self.module_manifest, "MENU": self.menu_manifest}
        self.module_output = self.overlays / "BOOT.BIN"
        self.menu_output = self.overlays / "MENU.BIN"

    def tearDown(self):
        self.temporary.cleanup()

    def run_provision(self, fake: FakeDiscdump):
        return provision.provision(
            self.disc,
            discdump=self.discdump,
            manifest_path=self.manifest,
            output=self.output,
            modules=self.modules,
            overlays=self.overlays,
            psxport=ROOT / "external" / "psxport",
            runner=fake,
        )

    def test_matching_disc_publishes_verified_executable(self):
        fake = FakeDiscdump(self.executable, self.module_source)
        provisioned = self.run_provision(fake)
        self.assertEqual(provisioned.executable.boot_path, "SCUS_TEST.00")
        self.assertEqual(provisioned.executable_facts, 11)
        self.assertEqual(provisioned.module_facts, 14)
        self.assertEqual(
            fake.requests, ["SYSTEM.CNF", "SCUS_TEST.00", "DATA/CRASHBSH.DAT"]
        )
        self.assertEqual(self.output.read_bytes(), self.executable)
        self.assertEqual(self.module_output.read_bytes(), self.module)
        self.assertEqual(self.menu_output.read_bytes(), self.menu_module)

    def test_wrong_system_cnf_is_a_mismatch_and_preserves_prior_output(self):
        previous = b"previous verified output"
        self.output.parent.mkdir(parents=True)
        self.output.write_bytes(previous)
        with self.assertRaises(provision.Mismatch):
            self.run_provision(
                FakeDiscdump(self.executable, self.module_source, boot="OTHER.EXE")
            )
        self.assertEqual(self.output.read_bytes(), previous)

    def test_mutated_executable_is_a_mismatch_and_is_not_published(self):
        mutated = bytearray(self.executable)
        mutated[-1] ^= 1
        with self.assertRaises(provision.Mismatch):
            self.run_provision(FakeDiscdump(bytes(mutated), self.module_source))
        self.assertFalse(self.output.exists())

    def test_mutated_loaded_module_is_a_mismatch_and_publishes_neither_input(self):
        mutated = bytearray(self.module_source)
        mutated[-1] ^= 1
        with self.assertRaises(provision.Mismatch):
            self.run_provision(FakeDiscdump(self.executable, bytes(mutated)))
        self.assertFalse(self.output.exists())
        self.assertFalse(self.module_output.exists())
        self.assertFalse(self.menu_output.exists())

    def test_different_module_sources_refuse_before_extraction(self):
        changed = json.loads(self.menu_manifest.read_text(encoding="utf-8"))
        changed["source_path"] = "DATA/OTHER.DAT"
        self.menu_manifest.write_text(json.dumps(changed), encoding="utf-8")
        fake = FakeDiscdump(self.executable, self.module_source)
        with self.assertRaises(provision.Refused):
            self.run_provision(fake)
        self.assertEqual(fake.requests, [])
        self.assertFalse(self.output.exists())
        self.assertFalse(self.module_output.exists())
        self.assertFalse(self.menu_output.exists())

    def test_negative_module_pointer_offset_refuses_before_extraction(self):
        changed = json.loads(self.menu_manifest.read_text(encoding="utf-8"))
        changed["entry_pointer_offset"] = "-0x1"
        self.menu_manifest.write_text(json.dumps(changed), encoding="utf-8")
        fake = FakeDiscdump(self.executable, self.module_source)
        with self.assertRaises(provision.Refused):
            self.run_provision(fake)
        self.assertEqual(fake.requests, [])
        self.assertFalse(self.output.exists())
        self.assertFalse(self.module_output.exists())
        self.assertFalse(self.menu_output.exists())

    def test_module_without_a_measured_entry_verifies_and_publishes(self):
        """A module the game dispatches through runtime-patched pointers has no entry pointer in
        its image, so it ships entry/entry_pointer_offset as null and carries one fact fewer."""
        changed = json.loads(self.menu_manifest.read_text(encoding="utf-8"))
        changed["entry_pointer_offset"] = None
        changed["entry"] = None
        self.menu_manifest.write_text(json.dumps(changed), encoding="utf-8")
        fake = FakeDiscdump(self.executable, self.module_source)
        provisioned = self.run_provision(fake)
        # 7 facts for BOOT plus 6 for the entry-less MENU fixture, not 14.
        self.assertEqual(provisioned.module_facts, 13)
        self.assertEqual(self.menu_output.read_bytes(), self.menu_module)

    def test_module_entry_without_its_pointer_refuses_before_extraction(self):
        """Both entry facts are measured together or neither is; a lone one is unverifiable."""
        original = self.menu_manifest.read_text(encoding="utf-8")
        for dropped in ("entry_pointer_offset", "entry"):
            with self.subTest(dropped=dropped):
                changed = json.loads(original)
                changed[dropped] = None
                self.menu_manifest.write_text(json.dumps(changed), encoding="utf-8")
                fake = FakeDiscdump(self.executable, self.module_source)
                with self.assertRaises(provision.Refused):
                    self.run_provision(fake)
                self.assertEqual(fake.requests, [])
                self.assertFalse(self.output.exists())

    def test_unknown_module_field_refuses_before_extraction(self):
        changed = json.loads(self.menu_manifest.read_text(encoding="utf-8"))
        changed["unmeasured_hint"] = "must not be ignored"
        self.menu_manifest.write_text(json.dumps(changed), encoding="utf-8")
        fake = FakeDiscdump(self.executable, self.module_source)
        with self.assertRaises(provision.Refused):
            self.run_provision(fake)
        self.assertEqual(fake.requests, [])
        self.assertFalse(self.output.exists())
        self.assertFalse(self.module_output.exists())
        self.assertFalse(self.menu_output.exists())

    def test_each_of_the_eleven_manifest_facts_can_fail(self):
        original = json.loads(self.manifest.read_text(encoding="utf-8"))
        mutations = [
            ("file_size", None, original["file_size"] + 1),
            ("sha1", None, "0" * 40),
            ("sha256", None, "0" * 64),
            ("header", "entry", "0x80010024"),
            ("header", "gp", "0x80018004"),
            ("header", "text_address", "0x80010004"),
            ("header", "text_size", "0x00000084"),
            ("header", "stack_address", "0x801FFFE0"),
            ("header", "stack_offset", "0x00000014"),
            ("region_markers", 0, "missing first marker"),
            ("region_markers", 1, "missing second marker"),
        ]
        for field, child, wrong in mutations:
            with self.subTest(field=field, child=child):
                changed = json.loads(json.dumps(original))
                if child is None:
                    changed[field] = wrong
                else:
                    changed[field][child] = wrong
                self.manifest.write_text(json.dumps(changed), encoding="utf-8")
                with self.assertRaises(provision.Mismatch):
                    self.run_provision(
                        FakeDiscdump(self.executable, self.module_source)
                    )
                self.assertFalse(self.output.exists())
        self.manifest.write_text(json.dumps(original), encoding="utf-8")

    def test_extraction_failure_refuses_without_publishing(self):
        with self.assertRaises(provision.Refused):
            self.run_provision(
                FakeDiscdump(self.executable, self.module_source, returncode=1)
            )
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
