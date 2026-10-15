# 0042 — the packaged first run is a separate launcher, not psxport's ProductHost

**Status:** resolved · **State:** S018

The AppImage shipped its own `main` (`packaging/linux/launcher_main.cpp`), an SDL message box and file
dialog, `install_media.cpp` extracting and hashing the chosen `.chd`/`.zip`, and `user_paths.cpp` reading
XDG variables, then `exec`ed the game with the disc, settings and card paths in the environment.

Fixed in psxport: with no argument and no usable disc, `ProductHost::runOnlyTitle` shows
`FirstRunSetup` (shared setup-ui, one judged image, `Placement::Adopt`). Its validator opens the image
and authenticates `SCUS_945.70` inside it; the accepted path is saved by `SavedDisc` in
`<user data>/crashbash/disc-path` and read after the disc keys. A packaged run (`APPDIR`) defaults to a
window and the shipped assets and works in that user-data directory. The launcher, `install_media.*`,
`user_paths.*` and the accepted entry are deleted; `packaging/appimage.py` packages `crashbash_port` alone.

Evidence: psxport `tests/test_first_run_setup.cpp`; a zero-argument packaged run with no disc refused by
name (hidden), and with a saved choice extracted the executable into user data and booted (recordcheck
mismatched=0 on 301 presents).

Left: a `.zip` holding the `.chd` is no longer accepted (the launcher took one); the windowed screen has
not been driven by a player click and has no handset-size capture; no AppImage was built.
