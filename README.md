# Kingdom Come: Deliverance II Head Tracking

Move the in-game view with your real head while the mouse keeps the aim, no VR headset required.

> **Status: pre-release.** Basic head tracking with look+aim decoupling is in,
> however no testing beyond that has been done - game-breaking bugs may be
> present

## Features

- **Decoupled look and aim** - head tracking moves the camera, aim stays on your mouse or controller
- **6DOF positional tracking** - lean and peek with head position, limited so you never clip through Henry

## Requirements

- **The game** - [Kingdom Come: Deliverance II](https://store.steampowered.com/app/1771300/) on Steam. Other stores are untested; the mod stays dormant rather than misbehave on a build it does not recognize.
- **A tracking source** - a webcam through [OpenTrack](https://github.com/opentrack/opentrack), a VR headset, TrackIR, Tobii, or a phone app that speaks the OpenTrack UDP protocol.
- **Windows 10 or 11, 64-bit.**

## Installation

1. Download the installer ZIP from the [Releases page](https://github.com/itsloopyo/kingdom-come-deliverance-2/releases).
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

### Manual Installation

Copy two files into `<game>\Bin\Win64MasterMasterSteamPGO`, the folder holding `KingdomCome.exe`:

```
Bin\Win64MasterMasterSteamPGO\dinput8.dll                                  (from vendor\ultimate-asi-loader\)
Bin\Win64MasterMasterSteamPGO\KingdomComeDeliverance2HeadTracking.asi      (from plugins\)
```

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
| Cycle ADS mode                  | `Insert`    | `Ctrl+Shift+U` |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

`Page Down` / `Ctrl+Shift+H` switches yaw between world-locked (the default, horizon-stable) and camera-local, which follows the camera's current up-axis.

`Insert` / `Ctrl+Shift+U` cycles what happens when you aim a bow or crossbow.
Both start the same way - raising the sights swings the view onto the point the
reticle was marking, so your shot lands where you had it lined up - and they
differ in what happens for the rest of the aim:

1. **Tracking paused** (default) - the game keeps the camera for as long as you
   are aiming. The sight picture is exactly the game's, and head movement does
   nothing until you lower the weapon.
2. **Tracking on, no aim marker** - head tracking carries on from the snapped
   position, and the game's own aim reticle keeps marking where the shot lands,
   so nothing extra is drawn over the top of it. That reticle is authoritative:
   this mod moves it onto the real impact point every frame, so when it and the
   arrow appear to disagree it is the reticle that is right.

The choice is saved to `HeadTracking.ini`, so it survives a restart. This mod
draws no on-screen text of its own, so the mode you switched to is named in
`HeadTracking.log` rather than in a toast.

Every press is named in `HeadTracking.log`.

## Configuration

`HeadTracking.ini` is written next to `KingdomCome.exe`, in `<game>\Bin\Win64MasterMasterSteamPGO`, the first time you launch with the mod installed.

```ini
[HeadTracking]
UdpPort=4242
; Start with head tracking already on.
EnableOnStartup=true
; Yaw about the world up-axis so the horizon stays level. Off yaws about
; the camera's own up-axis, which leans the view on pitched turns.
WorldSpaceYaw=true
; Move the game's own crosshair to where the shot actually goes. It is
; pinned to screen centre, which stops being the aim point as soon as you
; turn your head. Off leaves the HUD untouched.
MoveCrosshair=true
; Smoothing for a tracker running on this machine (loopback). 0 = none.
LocalSmoothing=0.0
; Smoothing for a tracker reaching this machine over the network. A tracker
; sending to this PC's LAN address instead of 127.0.0.1 counts as remote -
; the classifier sees a transport, not a machine.
RemoteSmoothing=0.15
; How far past the newest tracker sample the mod may extrapolate, as a
; fraction of one sample interval. Fills frames between samples on a
; high-refresh display.
MaxExtrapolationFraction=0.5

[Position]
; 6DOF lean. Limits are metres.
Enabled=true
; Sideways lean, applied as plus or minus this value.
LimitX=0.30
; Up, and down, kept separate so a crouch can have a tighter range.
LimitY=0.20
LimitYDown=0.20
; Forward lean, then backward. Backward is deliberately small so the camera
; does not pull back through Henry's head.
LimitZ=0.40
LimitZBack=0.10

[Hotkeys]
; Windows virtual-key codes. Ctrl+Shift+Y / G / H work as alternatives.
ToggleKey=0x23
PositionKey=0x21
YawModeKey=0x22
```

There is deliberately no sensitivity or axis-inversion setting. Shape the pose in your tracker app instead, so one profile behaves the same in every game.

## Troubleshooting

Everything the mod does is written to `HeadTracking.log` next to `KingdomCome.exe`. The previous session is kept as `HeadTracking.prev.log`. Both start over on each launch, so neither grows over time; copy one aside before your next launch if you want to keep it. The `heartbeat` line is written whenever the tracker or a toggle changes state, and otherwise once every five minutes.

**Mod not loading**

- Check `HeadTracking.log` exists. If it does not, the ASI loader is not loading. Confirm `dinput8.dll` and the `.asi` are both in `Bin\Win64MasterMasterSteamPGO`.
- If the log says "staying dormant", the mod did not recognize your `WHGame.dll`. The same line says whether the game is newer or older than the builds it knows about. Open an issue quoting it.

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

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod DLLs. The ASI loader is only removed if the installer put it there. Use `uninstall.cmd /force` to remove it anyway.

## Building from Source

Needs CMake and Visual Studio. The build has no dependency on a game install; it produces the installer ZIP on a clean checkout of a machine that does not own the game.

```powershell
git clone --recursive https://github.com/itsloopyo/kingdom-come-deliverance-2
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
