#!/usr/bin/env python3
"""Verify and extract measured Crash Bash code modules from CRASHBSH.DAT."""

from __future__ import annotations

import hashlib
import json
import pathlib
from dataclasses import dataclass

ROOT = pathlib.Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "titles" / "crashbash" / "boot_module.json"
MENU_MANIFEST = ROOT / "titles" / "crashbash" / "menu_module.json"
DAT28272_MANIFEST = ROOT / "titles" / "crashbash" / "dat28272_module.json"
DAT28241_MANIFEST = ROOT / "titles" / "crashbash" / "dat28241_module.json"
DAT28136_MANIFEST = ROOT / "titles" / "crashbash" / "dat28136_module.json"
DAT28382_MANIFEST = ROOT / "titles" / "crashbash" / "dat28382_module.json"
DAT22510_MANIFEST = ROOT / "titles" / "crashbash" / "dat22510_module.json"

# Measured code modules in CRASHBSH.DAT, keyed by the image name used for identity, override scope and cache invalidation.
#
# MENU and the DAT images are alternatives in one nested slot at 0x800B32B4; the router tells them apart by
# the content signature at the base. DAT stems are named by disc LBA, not by inferred role.
MODULES: dict[str, pathlib.Path] = {
    "BOOT": MANIFEST,
    "MENU": MENU_MANIFEST,
    "DAT28272": DAT28272_MANIFEST,
    "DAT28241": DAT28241_MANIFEST,
    "DAT28136": DAT28136_MANIFEST,
    "DAT28382": DAT28382_MANIFEST,
    "DAT22510": DAT22510_MANIFEST,
}


class Refused(RuntimeError):
    """The source or manifest cannot support an identity comparison."""


class Mismatch(RuntimeError):
    """A readable source contradicts the measured module identity."""


@dataclass(frozen=True)
class ModuleIdentity:
    title: str
    serial: str
    source_path: str
    source_size: int
    source_sha256: str
    source_file_lba: int
    payload_disc_lba: int
    sector_size: int
    sector_count: int
    payload_offset: int
    payload_size: int
    payload_sha256: str
    load_address: int
    # Modules without a header entry pointer (resource-style images) have None for both fields.
    entry_pointer_offset: int | None
    entry: int | None


@dataclass(frozen=True)
class VerifiedModule:
    identity: ModuleIdentity
    payload: bytes
    facts: int


def _positive_integer(raw: object, field: str) -> int:
    if not isinstance(raw, int) or raw <= 0:
        raise Refused(f"module manifest field {field} must be a positive integer")
    return raw


def _hexadecimal(raw: object, field: str) -> int:
    if not isinstance(raw, str):
        raise Refused(f"module manifest field {field} must be a hexadecimal string")
    try:
        value = int(raw, 16)
    except ValueError as error:
        raise Refused(
            f"module manifest field {field} is not hexadecimal: {raw!r}"
        ) from error
    if value < 0:
        raise Refused(f"module manifest field {field} must not be negative")
    return value


def load_identity(path: pathlib.Path = MANIFEST) -> ModuleIdentity:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise Refused(f"cannot read loaded-module manifest {path}: {error}") from error
    if not isinstance(raw, dict):
        raise Refused(f"loaded-module manifest {path} must contain an object")

    manifest_fields = set(ModuleIdentity.__dataclass_fields__)
    missing = sorted(manifest_fields - raw.keys())
    if missing:
        raise Refused(f"loaded-module manifest {path} is missing {', '.join(missing)}")
    extra = sorted(raw.keys() - manifest_fields)
    if extra:
        raise Refused(
            f"loaded-module manifest {path} has unknown fields: {', '.join(extra)}"
        )
    for field in ("title", "serial", "source_path", "source_sha256", "payload_sha256"):
        if not isinstance(raw[field], str) or not raw[field]:
            raise Refused(f"module manifest field {field} must be a non-empty string")

    identity = ModuleIdentity(
        title=raw["title"],
        serial=raw["serial"],
        source_path=raw["source_path"],
        source_size=_positive_integer(raw["source_size"], "source_size"),
        source_sha256=raw["source_sha256"].lower(),
        source_file_lba=_positive_integer(raw["source_file_lba"], "source_file_lba"),
        payload_disc_lba=_positive_integer(raw["payload_disc_lba"], "payload_disc_lba"),
        sector_size=_positive_integer(raw["sector_size"], "sector_size"),
        sector_count=_positive_integer(raw["sector_count"], "sector_count"),
        payload_offset=_hexadecimal(raw["payload_offset"], "payload_offset"),
        payload_size=_hexadecimal(raw["payload_size"], "payload_size"),
        payload_sha256=raw["payload_sha256"].lower(),
        load_address=_hexadecimal(raw["load_address"], "load_address"),
        entry_pointer_offset=(
            None
            if raw.get("entry_pointer_offset") is None
            else _hexadecimal(raw["entry_pointer_offset"], "entry_pointer_offset")
        ),
        entry=None if raw.get("entry") is None else _hexadecimal(raw["entry"], "entry"),
    )
    expected_offset = (
        identity.payload_disc_lba - identity.source_file_lba
    ) * identity.sector_size
    if identity.payload_offset != expected_offset:
        raise Refused(
            "module manifest payload_offset disagrees with its measured file/disc LBA relation"
        )
    if identity.payload_size != identity.sector_count * identity.sector_size:
        raise Refused(
            "module manifest payload_size disagrees with sector_count * sector_size"
        )
    if (
        identity.entry is not None
        and not identity.load_address
        <= identity.entry
        < identity.load_address + identity.payload_size
    ):
        raise Refused("module entry lies outside the measured load range")
    if (identity.entry is None) != (identity.entry_pointer_offset is None):
        raise Refused(
            "module manifest ships an entry without an entry pointer or the reverse — both are "
            "measured, or neither"
        )
    if (
        identity.entry_pointer_offset is not None
        and identity.entry_pointer_offset + 4 > identity.payload_size
    ):
        raise Refused("module entry pointer lies outside the measured payload")
    return identity


def verify_source(
    path: pathlib.Path, identity: ModuleIdentity | None = None
) -> VerifiedModule:
    measured = identity or load_identity()
    try:
        source = path.read_bytes()
    except OSError as error:
        raise Refused(
            f"cannot read extracted {measured.source_path}: {error}"
        ) from error
    source_digest = hashlib.sha256(source).hexdigest()
    if len(source) != measured.source_size or source_digest != measured.source_sha256:
        raise Mismatch(
            f"{measured.source_path} identity mismatch: {len(source)} bytes sha256 "
            f"{source_digest}; expected {measured.source_size} bytes sha256 "
            f"{measured.source_sha256}"
        )
    end = measured.payload_offset + measured.payload_size
    if end > len(source):
        raise Mismatch(
            f"measured module slice 0x{measured.payload_offset:X}..0x{end:X} exceeds "
            f"the {len(source)}-byte source"
        )
    payload = source[measured.payload_offset : end]
    payload_digest = hashlib.sha256(payload).hexdigest()
    if payload_digest != measured.payload_sha256:
        raise Mismatch(
            f"loaded-module sha256 {payload_digest} disagrees with {measured.payload_sha256}"
        )
    pointer = measured.entry_pointer_offset
    if pointer is None:
        return VerifiedModule(measured, payload, 6)
    entry = int.from_bytes(payload[pointer : pointer + 4], "little")
    if entry != measured.entry:
        raise Mismatch(
            f"loaded-module entry pointer at +0x{pointer:X} is 0x{entry:08X}, "
            f"expected 0x{measured.entry:08X}"
        )
    return VerifiedModule(measured, payload, 7)
