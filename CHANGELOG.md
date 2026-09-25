# Changelog

## [Unreleased]

### Added

- Added support for the Xbox Game Pass / Microsoft Store version of the game.
  It ships its own build, so it gets its own build profile; the Steam profile
  is untouched and a player on either store matches their own entry.
- The installer and the manual instructions now put the loader and the mod
  beside `KingdomCome.exe` wherever the store keeps it. The Game Pass package
  has no `Bin\Win64MasterMasterSteamPGO`; its executable sits directly in the
  package `Content` folder, and so do `HeadTracking.ini` and
  `HeadTracking.log`.

### Changed

- Head tracking now carries straight on while you aim a bow or crossbow.
  Raising the sights no longer moves the view, and head rotation is never
  paused or measured from where the aim began. Only the lean eases out while
  the sights are up, and back in when they come down.

### Removed

- The aim-down-sights mode cycle (paused, marker, tracked) on `Insert` /
  `Ctrl+Shift+U`, the aim marker it could draw, and the `[ADS] AdsMode` and
  `[Hotkeys] AdsModeKey` settings. A config that still carries them loads as
  before and the two keys are ignored.

### Fixed

- Fixed a launcher install on the Game Pass version putting the loader and the
  mod in a folder nothing reads. The package now anchors both files to the
  directory the game's executable is in rather than to a fixed path under the
  game root, which is the same folder on Steam and the correct one on Game
  Pass. A Steam install lands exactly where it did before.

## [0.0.0] - 2026-08-24

### Added

- Added head tracking for Kingdom Come: Deliverance II as an Ultimate ASI
  Loader plugin. The head moves the view while the mouse still controls aim:
  the tracked pose is composed onto the render camera only, so what the game
  reads for aim, raycasts and weapon direction is untouched.
- Added OpenTrack UDP intake on port 4242 with sample-rate estimation,
  frame-rate interpolation and per-connection smoothing (`LocalSmoothing` 0.0
  for a tracker on this machine, `RemoteSmoothing` 0.15 for one reaching the
  game over the network).
- Added positional lean with asymmetric limits, applied through the camera's
  original basis so leaning follows where the body faces rather than where the
  head is looking.
- Added horizon-locked (world-space) yaw by default, switchable to
  camera-local yaw.
- Added crosshair compensation so the game's own `CursorCross` follows the real
  aim point instead of staying pinned to screen centre. The crosshair keeps the
  weapon, stamina and interaction state it normally carries. Turn it off with
  `MoveCrosshair`.
- Added hotkeys on the nav cluster (`End`, `Page Up`, `Page Down`) with
  `Ctrl+Shift+Y/G/H` chord alternatives for keyboards without one. `Page Up`
  cycles the tracking mode: 6DOF, then rotation only, then lean only.
- Added a per-build profile registry that fingerprints `WHGame.dll` and leaves
  the mod fully dormant on an unrecognised build, so a Warhorse patch cannot
  leave a player with a crashing game. `pixi run check-fingerprint` reports
  which profile an install matches.
- Added `HeadTracking.ini` beside `KingdomCome.exe` on first run, plus
  `HeadTracking.log` and `HeadTracking.prev.log` next to it for diagnostics.
  The log starts empty on every launch and the previous run is kept as
  `.prev.log`, so neither file grows across sessions. It records startup, the
  matched build profile, hook installation, hotkey presses and tracker state
  changes; the periodic status line only repeats every five minutes while
  nothing changes, so a long session does not bury the interesting lines.

### Security

- The camera matrix is now checked for finite values before it is written into
  the game. The tracking socket binds every interface, and a single datagram
  carrying a pose near the top of the float range overflows the interpolator's
  extrapolation to infinity, after which the smoothing filter reports NaN for
  the rest of the session - which would have been written straight into the
  live camera and handed to the frustum rebuild. The mod now leaves the camera
  alone and says so once in the log.
- The vertical field of view and projection ratio read off the live camera are
  range-checked before the crosshair offset is scaled by them, the same way the
  back buffer size already was. A pinned offset a patch has moved can leave a
  positive float that is not a field of view, and the crosshair projection
  divides by its tangent.
- MinHook is fetched by commit rather than by the `v1.3.3` tag. A tag is a
  movable ref, so whoever controls that repository could point it at different
  code and every clean build would compile it into a DLL that loads inside the
  player's game.
- The daily patch-watch workflow validates the Steam buildid it parses out of
  SteamCMD's output as a decimal number and passes it to later steps through
  the environment instead of interpolating it into shell and a commit message.

### Fixed

- Hotkey codes in `HeadTracking.ini` are validated like every other setting.
  `ToggleKey=0` used to leave the player with no way to turn tracking off and
  nothing in the log explaining why, because the poller reads 0 as "no
  binding"; out-of-range codes are also rejected now, with the substitution
  logged.
- A failed or short write of the default `HeadTracking.ini` is reported instead
  of leaving a truncated file that the next launch parses as though it were
  whole.
- The bootstrap thread now holds a reference on the mod's own image and gives
  it back on the way out, so unloading the mod while it is still waiting for
  `WHGame.dll` or installing a trampoline cannot pull the code out from under
  the running thread.
