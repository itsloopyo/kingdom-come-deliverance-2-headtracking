# Kingdom Come: Deliverance II Head Tracking

Move the in-game view with your real head while the mouse keeps the aim, no VR headset required.

> **Status: pre-release.** Basic head tracking with look+aim decoupling is in,
> however no testing beyond that has been done - game-breaking bugs may be
> present

> **Updating from an earlier version?** Settings now live in `CameraUnlock.ini`,
> beside `KingdomCome.exe`. The first time this version starts it reads your
> settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It
> never changes `HeadTracking.ini`, and does not read it again while
> `CameraUnlock.ini` exists. See [Configuration](#configuration) for what is and
> is not carried over.

## Features

- **Decoupled look and aim** - head tracking moves the camera, aim stays on your mouse or controller
- **6DOF positional tracking** - lean and peek with head position, limited so you never clip through Henry

## Requirements

- **The game** - [Kingdom Come: Deliverance II](https://store.steampowered.com/app/1771300/) on Steam, or the Xbox Game Pass version. Each store ships its own build of the game and the mod carries a profile for each; on a build it does not recognize it stays dormant rather than misbehave.
- **A tracking source** - a webcam through [OpenTrack](https://github.com/opentrack/opentrack), a VR headset, TrackIR, Tobii, or a phone app that speaks the OpenTrack UDP protocol.
- **Windows 10 or 11, 64-bit.**

## Installation

1. Download the installer ZIP from the [Releases page](https://github.com/itsloopyo/kingdom-come-deliverance-2-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`.
4. Configure OpenTrack (or your phone app) to output UDP to `127.0.0.1:4242`.
5. Launch the game.

If the installer cannot find your game, point it at the install folder yourself, either with an environment variable or a positional argument:

```powershell
$env:KINGDOM_COME_DELIVERANCE_2_PATH = "D:\Games\KingdomComeDeliverance2"
.\install.cmd
```

```powershell
.\install.cmd "D:\Games\KingdomComeDeliverance2"
```

Give it the game's own top folder, not the folder the executable is in. On
Steam and GOG that is the folder containing
`Bin\Win64MasterMasterSteamPGO\KingdomCome.exe`; on Xbox Game Pass it is the
`Content` folder, which holds `KingdomCome.exe` directly.

### Manual Installation

Copy two files in beside `KingdomCome.exe`:

```
dinput8.dll                                  (from vendor\ultimate-asi-loader\)
KingdomComeDeliverance2HeadTracking.asi      (from plugins\)
```

The stores put `KingdomCome.exe` in different places, and the loader only looks
in the folder the executable is in, so this is the one detail worth checking
before you copy:

| Store | Where the two files go |
|-------|------------------------|
| Steam, GOG | `<game>\Bin\Win64MasterMasterSteamPGO\` |
| Xbox Game Pass | `<XboxGames>\Kingdom Come- Deliverance II\Content\` |

The installer ZIP mirrors the Steam layout, so its files sit under
`Bin\Win64MasterMasterSteamPGO\` inside the archive. For an Xbox Game Pass install, take
them out of that folder and drop them straight into `Content`.

`dinput8.dll` is Ultimate ASI Loader. `WHGame.dll` imports DirectInput 8 directly and the game folder is searched before System32, so the loader picks itself up with no launch options. If you already run another ASI loader there, keep yours and copy only the `.asi`.

The Nexus ZIP ships only the `.asi` for exactly that case, since Nexus users manage their own loader.

ReturnOfModding / KCD2ModLoader, if you use it, proxies `d3d12.dll` in the same folder. That is a different slot, so the two coexist and neither needs removing.

## Setting Up OpenTrack

- **Input:** your tracker.
- **Output:** *UDP over network*, IP `127.0.0.1`, port `4242`.
- Press **Start**, then center with OpenTrack's Center bind while sitting how you normally play.

Recenter in OpenTrack or with your phone app's recenter button

### VR Headset Setup

Connect the headset over Air Link or [Virtual Desktop](https://www.vrdesktop.net/), start SteamVR, then select OpenTrack's **SteamVR** input. The headset reports head pose to OpenTrack, which relays it to the mod on `127.0.0.1:4242`. 

### Webcam Setup

Select OpenTrack's **neuralnet tracker** input. It needs no markers and no IR clip, just a webcam and reasonable lighting. Output stays *UDP over network* on `127.0.0.1:4242`.

### Phone App Setup

A phone app can reach the mod directly, with no OpenTrack on the PC, if it sends
the OpenTrack UDP datagram. Point it at this PC's IP address (run `ipconfig` to
find it) on port `4242`. Not every phone tracker speaks this protocol, so check
yours for an OpenTrack or UDP output option first. [Headcam](https://headcam.app)
sends it, and I wrote it so decent tracking is free for anyone who already owns
a phone.

If you want OpenTrack's mapping curves, point the app at OpenTrack instead and
let OpenTrack relay to `127.0.0.1:4242`.

## Controls

Two equivalent binding sets, use whichever your keyboard has:

| Action                          | Nav-cluster | Chord          |
|---------------------------------|-------------|----------------|
| Toggle tracking                 | `End`       | `Ctrl+Shift+Y` |
| Cycle tracking mode             | `Page Up`   | `Ctrl+Shift+G` |
| Toggle yaw mode (world / local) | `Page Down` | `Ctrl+Shift+H` |
| Toggle true free look           | `Insert`    | `Ctrl+Shift+U` |

Head tracking pauses while the game's pause menu is open, including when opened
with `Backspace`. Closing the menu restores tracking if you have it enabled.

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

`Page Down` / `Ctrl+Shift+H` switches yaw between world-locked (the default, horizon-stable) and camera-local, which follows the camera's current up-axis.

Every press is named in `HeadTracking.log`. Every hotkey is a list of keys in
`CameraUnlock.ini`, the chords included, so you can rebind or remove any of them
there.

### Aiming down sights

Head tracking stays on while you aim a bow or crossbow: turning your head still
turns the view.

By default the lean eases out while the sights are up and eases back in when
you lower the weapon. `Insert` /
`Ctrl+Shift+U` switches to **true free look**, where the lean stays in full
while you aim. It is off by default. The mod saves the mode you pick, so it
holds the next time you start the game.

## Configuration

<!-- cameraunlock:config -->
The mod reads its settings from `CameraUnlock.ini` in the game folder, at one of these paths depending on the store the game came from:

- `Bin\Win64MasterMasterSteamPGO\CameraUnlock.ini`
- `CameraUnlock.ini`

It creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

Earlier versions of the mod kept these settings in `HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.

A setting that the defaults below set to `default` is written as `default` when the value imported for it equals its default at that start, which is the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none. It then follows `Defaults.ini`. Every other setting is written with the value imported for it. `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.

Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:

- Reticle settings, and a key that toggled the reticle.
- A sensitivity, scale, deadzone, response curve or axis inversion you changed from its default. Set these in your tracker instead.
- The setting for a feature that earlier versions shipped switched off while it was untested. It now follows the mod's default.

An older version of the mod reads `HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `HeadTracking.ini`.

Deleting only `CameraUnlock.ini` makes the next start read `HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults below. Every setting they set to `default` then follows `Defaults.ini`.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `WorldSpaceYaw=true`
- `RotationEnabled=true`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `TrueFreeLook=false`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`
- `YawModeKey=PageDown, Ctrl+Shift+H`
- `TrueFreeLookKey=Insert, Ctrl+Shift+U`

With every setting at its default, the file reads:

```ini
; Kingdom Come: Deliverance II head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default
; How far past the newest tracker sample the view may carry on moving,
; as a fraction of the time between samples. 0 only moves between samples.
MaxExtrapolationFraction=0.5

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; false: while you aim down the sights, leaning keeps your eye on the sights.
; true: the weapon stays put and your head moves freely around it (true free look).
TrueFreeLook=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=default
; Switches between keeping your eye on the sights and true free look (TrueFreeLook).
TrueFreeLookKey=default
```
<!-- /cameraunlock:config -->

These settings from earlier versions are gone:

- `MoveCrosshair`. The game's crosshair now always follows your aim, and no
  setting turns that off. Where `HeadTracking.ini` set it to `false`, the
  import logs that value as not carried.
- `[ADS] AdsMode` and `[Hotkeys] AdsModeKey`. The paused, marker and tracked
  aiming modes are gone, including paused, which was the default. Head tracking
  now carries on while you aim and the mod draws no aim marker. `Insert` /
  `Ctrl+Shift+U` toggle true free look instead. The import logs both keys as
  not carried.
- Where `HeadTracking.ini` still has the older `ShowReticle`, the import logs a
  line telling you to use `MoveCrosshair`. That advice is out of date, since
  `MoveCrosshair` is gone as well.

The mod saves the tracking mode (`Page Up`), the yaw mode (`Page Down`) and
true free look (`Insert`) to `CameraUnlock.ini` the moment you change them, so each one
comes back the next time you start the game. Turning head tracking on or off
with `End` lasts for the session only: `EnableOnStartup` decides whether it is
on when the game starts.

There is deliberately no sensitivity or axis-inversion setting. Shape the pose in your tracker app instead, so one profile behaves the same in every game.

## Troubleshooting

Everything the mod does is written to `HeadTracking.log` next to `KingdomCome.exe`. The previous session is kept as `HeadTracking.prev.log`. Both start over on each launch, so neither grows over time; copy one aside before your next launch if you want to keep it. The `heartbeat` line is written whenever the tracker or a toggle changes state, and otherwise once every five minutes.

**Mod not loading**

- Check `HeadTracking.log` exists. If it does not, the ASI loader is not loading. Confirm `dinput8.dll` and the `.asi` are both in the folder `KingdomCome.exe` is in - `Bin\Win64MasterMasterSteamPGO` on Steam and GOG, `Content` on Xbox Game Pass. Neither belongs in the game's top folder.
- If the log says "staying dormant", the mod did not recognize your `WHGame.dll`. The same line says whether the game is newer or older than the builds it knows about. Open an issue quoting it. On a working install the line above it names the profile that matched, `steam-win64-...` or `gdk-win64-...` according to where you bought the game.

**No tracking response**

- The log shows `udpData=NO`: read the `udpPort=` field on the same line. `4242/bound` means the mod has the port and nothing is sending, so check OpenTrack is started and its output is UDP `127.0.0.1:4242`.
- The log shows `udpPort=4242/WAITING (held by another app)`: something else already has the port, usually another modded game left running, or OpenTrack itself configured as a receiver. Close it and leave the game running. The mod rechecks twice a second and picks up within about a second, no restart needed.
- Tracking is suppressed outside gameplay, so check in the world rather than in a menu.

**Jittery or unstable tracking**

- Raise `RemoteSmoothing` if the tracker is a phone or anything else coming over WiFi.
- Raise `LocalSmoothing` above 0 if a webcam tracker on this machine is noisy. Better lighting usually beats more smoothing.
- Fix the tracker first. Sensitivity curves and deadzones belong in OpenTrack or the phone app, not in the mod.

**Wrong rotation axis, or the view drifts**

- The view drifts or sits off-center: center in your tracker app, the mod has no center of its own.
- Yaw feels wrong at extreme up or down angles: toggle world-locked and camera-local yaw with `Page Down`. World-locked is horizon-stable, camera-local follows the camera's up-axis and leans the view on steeply pitched turns.
- An axis moves the view the wrong way: fix the axis direction in your tracker profile. The mod exposes no inversion setting on purpose, so one tracker profile stays correct across every game.

**Leaning still moves the view while I aim**

- You are in true free look, which keeps the lean while you aim. Press `Insert` / `Ctrl+Shift+U` to go back to the default, which eases the lean out while the sights are up.

## Updating

Download the new release and run `install.cmd` again. The installer never writes `CameraUnlock.ini` or `HeadTracking.ini`, so your settings stay as they are.

## Uninstalling

Run `uninstall.cmd`. This removes the mod DLLs and leaves `CameraUnlock.ini` and `HeadTracking.ini` in place, so a reinstall keeps your settings. The ASI loader is only removed if the installer put it there. Use `uninstall.cmd /force` to remove it anyway.

## Building from Source

Needs CMake and Visual Studio. The build has no dependency on a game install; it produces the installer ZIP on a clean checkout of a machine that does not own the game.

```powershell
git clone --recursive https://github.com/itsloopyo/kingdom-come-deliverance-2-headtracking
cd kingdom-come-deliverance-2
pixi run package
```

Other tasks: `pixi run build | test | install | update-deps | check-fingerprint | release`.

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

Third-party components keep their own licenses, with full texts in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md), which ships at the root of every release ZIP alongside `LICENSE`. No Kingdom Come: Deliverance II code or assets are included here or in any release.

## Credits

- **Warhorse Studios** for Kingdom Come: Deliverance II, and for shipping a camera whose view parameters and render matrix are cleanly separated. That separation is the whole reason look and aim can be decoupled here.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) (MIT) for getting the mod into the process.
- [OpenTrack](https://github.com/opentrack/opentrack) (ISC) for the tracking protocol.
- [MinHook](https://github.com/TsudaKageyu/minhook) (BSD-2-Clause) for the function hooks.

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Warhorse Studios or Deep Silver. Use at your own risk.
