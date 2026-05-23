# qtile-lock

A small frankenstein to lock qtile's wayland session nicely.

## How it works

This utility spawns a screen locker (such as `swaylock`) and communicates with the Wayland compositor via `ext-idle-notify-v1` to control display sleep timeouts:
- When idle for 10 seconds, it powers down the displays using `wlopm --off "*"`.
- On user input activity, it wakes up the displays with `wlopm --on "*"`.
- When the screen locker process exits (successful unlock), it wakes up the displays, cleans up Wayland references, and terminates.

## Customization

You can configure settings directly at the top of `qtile_lock.c`:

```c
#define LOCK_TIMEOUT_MS 10000
#define CMD_DISPLAY_OFF "wlopm --off \"*\""
#define CMD_DISPLAY_ON "wlopm --on \"*\""
#define DEFAULT_LOCKER_ARGS "swaylock", "-c", "000000", "--font", "IBM Plex Sans"
```

Recompile the project after making any modifications.

## Build

Dependencies:
- `wayland-client`
- `wayland-scanner`
- `wayland-protocols`
- `gcc`
- `make`

Build the executable:

```bash
make
```

## Usage

Lock using the default command:

```bash
./qtile-lock
```

Or pass a custom locker command and arguments:

```bash
./qtile-lock swaylock -c 112233
```
