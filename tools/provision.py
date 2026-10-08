#!/usr/bin/env python3
"""Resolve the Crash Bash disc and provision its verified executable and code modules.

The disc selection order is CLI > process environment > .env > one repository-root
``*.chd`` drop-in.  The selected source is authoritative: a missing configured path is a
refusal, never permission to silently use a lower-priority disc.

Exit 0 means SYSTEM.CNF, the executable identity/header, the full data file, and every measured
loaded-module identity matched its tracked manifest.  Exit 1 means readable media
contradicted a manifest.  Exit 2 means the tool could not make the comparison.  Copyrighted
inputs and extracted output stay outside git; publication starts only after the entire set verifies,
and each destination is replaced atomically below ``scratch/bin/crashbash``.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass

import loaded_module

ROOT = pathlib.Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "titles" / "crashbash" / "executable.json"
DEFAULT_OUTPUT = ROOT / "scratch" / "bin" / "crashbash" / "SCUS_945.70"
DEFAULT_OVERLAYS = ROOT / "scratch" / "bin" / "crashbash" / "overlays"
DEFAULT_DISCDUMP = (
    ROOT
    / "build"
    / "maintainer"
    / "psxport_build"
    / "tools"
    / "discdump"
)
ENV_KEYS = ("PSXPORT_CRASHBASH_DISC", "PSXPORT_DISC")


class Refused(Exception):
    """The available inputs cannot support an identity comparison."""


class Mismatch(Exception):
    """A readable input contradicts the selected retail target."""


@dataclass(frozen=True)
class Identity:
    title: str
    region: str
    serial: str
    boot_path: str
    file_size: int
    sha1: str
    sha256: str
    entry: int
    gp: int
    text_address: int
    text_size: int
    stack_address: int
    stack_offset: int
    markers: tuple[str, ...]


@dataclass(frozen=True)
class ProvisionedInputs:
    executable: Identity
    executable_facts: int
    modules: tuple[loaded_module.ModuleIdentity, ...]
    module_facts: int


def _parse_dotenv(path: pathlib.Path) -> dict[str, str]:
    """Read only literal KEY=VALUE entries; never evaluate machine-local configuration."""
    if not path.is_file():
        return {}
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as exc:
        raise Refused(f"cannot read {path}: {exc}") from exc

    values: dict[str, str] = {}
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or "=" not in stripped:
            continue
        key, raw = stripped.split("=", 1)
        key = key.strip()
        if key not in ENV_KEYS:
            continue
        value = raw.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values.setdefault(key, value)
    return values


def _checked_disc(raw: str, source: str, base: pathlib.Path) -> pathlib.Path:
    candidate = pathlib.Path(raw).expanduser()
    if not candidate.is_absolute():
        candidate = base / candidate
    candidate = candidate.resolve()
    if not candidate.is_file():
        raise Refused(f"{source} names {candidate}, which is not a file")
    return candidate


def resolve_disc(
    argument: str | None,
    *,
    root: pathlib.Path = ROOT,
    environ: Mapping[str, str] = os.environ,
    cwd: pathlib.Path | None = None,
) -> tuple[pathlib.Path, str]:
    """Resolve exactly one authoritative disc source and return ``(path, source)``."""
    if argument:
        return _checked_disc(
            argument, "CLI argument", cwd or pathlib.Path.cwd()
        ), "CLI argument"

    for key in ENV_KEYS:
        value = environ.get(key)
        if value:
            return _checked_disc(value, f"${key}", root), f"${key}"

    dotenv = _parse_dotenv(root / ".env")
    for key in ENV_KEYS:
        value = dotenv.get(key)
        if value:
            return _checked_disc(value, f".env {key}", root), f".env {key}"

    dropins = sorted(
        (
            entry.resolve()
            for entry in root.iterdir()
            if entry.is_file() and entry.suffix.lower() == ".chd"
        ),
        key=lambda path: path.name.casefold(),
    )
    if len(dropins) == 1:
        return dropins[0], "repository-root *.chd drop-in"
    if len(dropins) > 1:
        names = ", ".join(path.name for path in dropins)
        raise Refused(f"multiple repository-root CHD drop-ins are ambiguous: {names}")
    raise Refused(
        "no disc image: tried a CLI argument, $PSXPORT_CRASHBASH_DISC, "
        "$PSXPORT_DISC, .env, and one repository-root *.chd drop-in"
    )


def _hex(value: object, field: str) -> int:
    if not isinstance(value, str):
        raise Refused(f"manifest field {field} must be a hexadecimal string")
    try:
        return int(value, 16)
    except ValueError as exc:
        raise Refused(f"manifest field {field} is not hexadecimal: {value!r}") from exc


def load_identity(path: pathlib.Path) -> Identity:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise Refused(f"cannot read executable manifest {path}: {exc}") from exc
    if not isinstance(raw, dict):
        raise Refused(f"executable manifest {path} must contain a JSON object")

    required = {
        "title",
        "region",
        "serial",
        "boot_path",
        "file_size",
        "sha1",
        "sha256",
        "header",
        "region_markers",
    }
    missing = sorted(required - raw.keys())
    if missing:
        raise Refused(f"executable manifest {path} is missing {', '.join(missing)}")
    header = raw["header"]
    markers = raw["region_markers"]
    text_fields = ("title", "region", "serial", "boot_path", "sha1", "sha256")
    invalid_text = [
        field
        for field in text_fields
        if not isinstance(raw[field], str) or not raw[field]
    ]
    if invalid_text:
        raise Refused(
            f"manifest fields must be non-empty strings: {', '.join(invalid_text)}"
        )
    if not isinstance(raw["file_size"], int) or raw["file_size"] <= 0:
        raise Refused("manifest field file_size must be a positive integer")
    if not isinstance(header, dict):
        raise Refused("manifest field header must be an object")
    if (
        not isinstance(markers, list)
        or not markers
        or not all(isinstance(item, str) for item in markers)
    ):
        raise Refused("manifest field region_markers must be a non-empty string list")

    return Identity(
        title=raw["title"],
        region=raw["region"],
        serial=raw["serial"],
        boot_path=raw["boot_path"],
        file_size=raw["file_size"],
        sha1=raw["sha1"].lower(),
        sha256=raw["sha256"].lower(),
        entry=_hex(header.get("entry"), "header.entry"),
        gp=_hex(header.get("gp"), "header.gp"),
        text_address=_hex(header.get("text_address"), "header.text_address"),
        text_size=_hex(header.get("text_size"), "header.text_size"),
        stack_address=_hex(header.get("stack_address"), "header.stack_address"),
        stack_offset=_hex(header.get("stack_offset"), "header.stack_offset"),
        markers=tuple(markers),
    )


def _load_psx_executable_parser(psxport: pathlib.Path):
    module_path = psxport / "tools" / "formats" / "psx_exe.py"
    if not module_path.is_file():
        raise Refused(
            f"cannot find psxport PS-X EXE loader at {module_path}; "
            "run tools/psxport_fetch.py --auto or set PSXPORT_DIR"
        )
    spec = importlib.util.spec_from_file_location(
        "crashbash_psxport_psx_exe", module_path
    )
    if spec is None or spec.loader is None:
        raise Refused(f"cannot load PS-X EXE parser from {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def verify_system_cnf(path: pathlib.Path, identity: Identity) -> None:
    try:
        text = path.read_text(encoding="ascii", errors="replace")
    except OSError as exc:
        raise Refused(f"cannot read extracted SYSTEM.CNF: {exc}") from exc
    match = re.search(
        r"^\s*BOOT\s*=\s*cdrom:\\+([^;\r\n]+);1\s*$", text, re.IGNORECASE | re.MULTILINE
    )
    if not match:
        raise Mismatch("SYSTEM.CNF has no canonical `BOOT = cdrom:\\<path>;1` entry")
    actual = match.group(1).replace("\\", "/").strip("/")
    expected = identity.boot_path.replace("\\", "/").strip("/")
    if actual.casefold() != expected.casefold():
        raise Mismatch(f"SYSTEM.CNF boots {actual!r}, expected {identity.boot_path!r}")


def verify_executable(
    path: pathlib.Path, identity: Identity, psxport: pathlib.Path
) -> int:
    try:
        data = path.read_bytes()
        image = _load_psx_executable_parser(psxport).load(path)
    except OSError as exc:
        raise Refused(f"cannot read extracted {identity.boot_path}: {exc}") from exc
    except ValueError as exc:
        raise Mismatch(str(exc)) from exc

    actual: dict[str, object] = {
        "file_size": len(data),
        "sha1": hashlib.sha1(data).hexdigest(),
        "sha256": hashlib.sha256(data).hexdigest(),
        "entry": image.entry,
        "gp": image.gp,
        "text_address": image.load,
        "text_size": image.text_size,
        "stack_address": image.sp_base,
        "stack_offset": image.sp_off,
    }
    expected: dict[str, object] = {
        "file_size": identity.file_size,
        "sha1": identity.sha1,
        "sha256": identity.sha256,
        "entry": identity.entry,
        "gp": identity.gp,
        "text_address": identity.text_address,
        "text_size": identity.text_size,
        "stack_address": identity.stack_address,
        "stack_offset": identity.stack_offset,
    }
    failures = [
        f"{field}: measured={actual[field]!r}, expected={expected[field]!r}"
        for field in expected
        if actual[field] != expected[field]
    ]
    missing_markers = [
        marker for marker in identity.markers if marker.encode("ascii") not in data
    ]
    failures.extend(f"missing region marker {marker!r}" for marker in missing_markers)
    if failures:
        raise Mismatch("executable identity mismatch:\n  " + "\n  ".join(failures))
    return len(expected) + len(identity.markers)


Runner = Callable[..., subprocess.CompletedProcess[str]]


def _extract(
    discdump: pathlib.Path,
    disc: pathlib.Path,
    disc_path: str,
    directory: pathlib.Path,
    runner: Runner,
) -> pathlib.Path:
    command = [str(discdump), "get", disc_path, str(disc), str(directory)]
    try:
        completed = runner(command, text=True, capture_output=True, check=False)
    except OSError as exc:
        raise Refused(f"cannot execute discdump at {discdump}: {exc}") from exc
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout or "no diagnostic").strip()
        raise Refused(f"discdump could not extract {disc_path}: {detail}")
    output = directory / pathlib.PurePosixPath(disc_path).name
    if not output.is_file():
        raise Refused(f"discdump reported success but did not create {output}")
    return output


def _publish(source: pathlib.Path, destination: pathlib.Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary: pathlib.Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            prefix=f".{destination.name}.",
            suffix=".partial",
            dir=destination.parent,
            delete=False,
        ) as output:
            temporary = pathlib.Path(output.name)
            with source.open("rb") as input_file:
                shutil.copyfileobj(input_file, output)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, destination)
    except OSError as exc:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        raise Refused(
            f"cannot publish verified executable to {destination}: {exc}"
        ) from exc


def provision(
    disc: pathlib.Path,
    *,
    discdump: pathlib.Path,
    manifest_path: pathlib.Path = MANIFEST,
    output: pathlib.Path = DEFAULT_OUTPUT,
    modules: Mapping[str, pathlib.Path] | None = None,
    overlays: pathlib.Path = DEFAULT_OVERLAYS,
    psxport: pathlib.Path | None = None,
    runner: Runner = subprocess.run,
) -> ProvisionedInputs:
    """Extract and verify every input, then publish each file through atomic replacement."""
    if not discdump.is_file():
        raise Refused(
            f"discdump is missing at {discdump}; configure with Clang and build target discdump"
        )
    identity = load_identity(manifest_path)
    manifests = loaded_module.MODULES if modules is None else modules
    try:
        module_identities = tuple(
            loaded_module.load_identity(path) for path in manifests.values()
        )
    except loaded_module.Refused as error:
        raise Refused(str(error)) from error
    module_sources = {module.source_path for module in module_identities}
    if len(module_sources) != 1:
        rendered = ", ".join(sorted(module_sources))
        raise Refused(
            "loaded modules must share one measured source for atomic extraction; "
            f"manifests name {rendered}"
        )
    for module_identity in module_identities:
        if module_identity.serial != identity.serial:
            raise Refused(
                f"loaded module targets {module_identity.serial}, executable targets {identity.serial}"
            )
    framework = psxport or pathlib.Path(
        os.environ.get("PSXPORT_DIR", ROOT / "external" / "psxport")
    )
    scratch = ROOT / "scratch" / "raw"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="crashbash-provision-", dir=scratch
    ) as raw_directory:
        directory = pathlib.Path(raw_directory)
        system_cnf = _extract(discdump, disc, "SYSTEM.CNF", directory, runner)
        executable = _extract(discdump, disc, identity.boot_path, directory, runner)
        module_source = _extract(
            discdump, disc, next(iter(module_sources)), directory, runner
        )
        verify_system_cnf(system_cnf, identity)
        executable_facts = verify_executable(executable, identity, framework)
        try:
            verified_modules = tuple(
                loaded_module.verify_source(module_source, module_identity)
                for module_identity in module_identities
            )
        except loaded_module.Mismatch as error:
            raise Mismatch(str(error)) from error
        except loaded_module.Refused as error:
            raise Refused(str(error)) from error
        # Stage everything before publishing so a late mismatch leaves no mix of new and stale files.
        staged = [
            (directory / f"{stem}.BIN", overlays / f"{stem}.BIN") for stem in manifests
        ]
        for (module_file, _destination), verified_module in zip(
            staged, verified_modules, strict=True
        ):
            module_file.write_bytes(verified_module.payload)
        _publish(executable, output)
        for module_file, destination in staged:
            _publish(module_file, destination)
    return ProvisionedInputs(
        identity,
        executable_facts,
        module_identities,
        sum(module.facts for module in verified_modules),
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "disc", nargs="?", help="Crash Bash USA disc; otherwise use env/.env/drop-in"
    )
    parser.add_argument("--discdump", type=pathlib.Path, default=DEFAULT_DISCDUMP)
    parser.add_argument("--manifest", type=pathlib.Path, default=MANIFEST)
    parser.add_argument("--output", type=pathlib.Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--overlays", type=pathlib.Path, default=DEFAULT_OVERLAYS)
    args = parser.parse_args(argv)
    try:
        disc, source = resolve_disc(args.disc)
        provisioned = provision(
            disc,
            discdump=args.discdump.resolve(),
            manifest_path=args.manifest.resolve(),
            output=args.output.resolve(),
            overlays=args.overlays.resolve(),
        )
        print(f"[provision] disc: {disc} ({source})")
        print(f"[provision] SYSTEM.CNF boot: 1/1 ({provisioned.executable.boot_path})")
        print(
            "[provision] executable identity/header: "
            f"{provisioned.executable_facts}/{provisioned.executable_facts}"
        )
        print(
            "[provision] loaded modules: "
            f"{provisioned.module_facts}/{provisioned.module_facts} facts"
        )
        for module in provisioned.modules:
            entry = (
                f"entry 0x{module.entry:08X}"
                if module.entry is not None
                else "no measured entry"
            )
            print(
                f"[provision]   0x{module.load_address:08X}.."
                f"0x{module.load_address + module.payload_size:08X}, {entry} "
                f"from LBA {module.payload_disc_lba}"
            )
        print(f"[provision] verified output: {args.output.resolve()}")
        for stem in loaded_module.MODULES:
            print(f"[provision] verified module: {args.overlays.resolve() / f'{stem}.BIN'}")
        print("[provision] scope: input identity only; no gameplay execution is claimed")
        return 0
    except Mismatch as exc:
        print(f"MISMATCH: {exc}", file=sys.stderr)
        return 1
    except Refused as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
