#!/usr/bin/env python3
"""Provision, build, and launch the Crash Bash native/Lightrec product."""

from __future__ import annotations

import os
import sys
from collections.abc import Sequence
from pathlib import Path

from port import launcher
from port.launch_environment import player_environment

ROOT = Path(__file__).resolve().parents[1]
EXECUTABLE = ROOT / "scratch" / "bin" / "crashbash" / "SCUS_945.70"
DISC_VARIABLE = "PSXPORT_CRASHBASH_DISC"


def launch(disc: str | None, prepare_only: bool) -> None:
    environment = dict(os.environ)
    if disc:
        environment[DISC_VARIABLE] = str(Path(disc).resolve())
    launcher.preflight()
    discdump = launcher.build_discdump(ROOT, environment)
    launcher.run([sys.executable, "-B", "tools/provision.py", "--discdump", discdump, *([disc] if disc else [])],
                 environment, cwd=ROOT)
    build = launcher.build_product(ROOT, "crashbash_port", environment)
    if prepare_only:
        launcher.say("Crash Bash is built and ready.")
        return
    launcher.exec_product(build / "bin" / "crashbash_port", [str(EXECUTABLE)],
                          player_environment(environment, product="crashbash"), ROOT)


def main(argv: Sequence[str] | None = None) -> int:
    args = launcher.argument_parser(__doc__).parse_args(argv)
    return launcher.main(lambda: launch(args.disc, args.prepare_only))


if __name__ == "__main__":
    raise SystemExit(main())
