# lockingd
_Formerly known as qtile-lock._

A tiny, compositor-agnostic daemon that pairs with your Wayland screen locker to run commands on idle, like turning off your displays.

## How it works

`lockingd` spawns a screen locker of your choice (e.g. `swaylock`) and talks to your Wayland compositor via `ext-idle-notify-v1` to manage display power.

- **On idle:** powers down displays via `wlopm` after a configurable timeout
- **On activity:** wakes displays back up immediately
- **On locker exit:** whether unlocked, crashed, or killed, displays wake up, Wayland references are cleaned up, and the daemon exits

On compositors that support `ext-idle-notify-v1` v2+, `lockingd` uses `get_input_idle_notification`, which bypasses idle inhibitors. On older compositors it falls back to `get_idle_notification`.

## Dependencies

- `wayland-client`
- `wayland-scanner`
- `wayland-protocols`
- `gcc`
- `make`

## Build & Install

```bash
make
sudo make install
```

```bash
# To uninstall:
sudo make uninstall
```

## Usage

Lock with the default locker command:

```bash
lockingd
```

Enable verbose logging (timestamped debug output):

```bash
lockingd -v
```

Pass a custom locker command using the `--` separator:

```bash
lockingd -- swaylock -c 112233
lockingd -v -- swaylock -c 112233
```

> **Note:** Arguments after `--` completely override `DEFAULT_LOCKER_ARGS` at runtime, no recompilation needed.

## Configuration

Compile-time defaults live at the top of `lockingd.c`:

```c
#define LOCK_TIMEOUT_MS    10000
#define CMD_DISPLAY_OFF    "wlopm --off \"*\""
#define CMD_DISPLAY_ON     "wlopm --on \"*\""
#define DEFAULT_LOCKER_ARGS "swaylock", "-c", "000000", "--font", "IBM Plex Sans"
```

Recompile after any changes:

```bash
make
```