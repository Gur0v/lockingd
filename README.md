# lockingd
_Formerly known as qtile-lock._
A tiny, compositor-agnostic daemon that pairs with your Wayland screen locker to run commands on idle, like turning off your displays.

## How it works
`lockingd` spawns a screen locker of your choice (e.g. `swaylock`) and talks to your Wayland compositor via `ext-idle-notify-v1` to manage display power.

- **On idle:** runs configurable commands after one or more timeouts (e.g. powering down displays)
- **On activity:** runs resume commands immediately (e.g. waking displays back up)
- **On locker exit:** whether unlocked, crashed, or killed, all resume commands are run, Wayland references are cleaned up, and the daemon exits

On compositors that support `ext-idle-notify-v1` v2+, `lockingd` uses `get_input_idle_notification`, which bypasses idle inhibitors. On older compositors it falls back to `get_idle_notification`.

## License
`lockingd` is free software, released under the [GNU General Public License v3](LICENSE).

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
> **Note:** Arguments after `--` completely override `DEFAULT_LOCKER_ARGS` at runtime — no recompilation needed.

## Configuration
All compile-time configuration lives in `config.h`. Edit it, then recompile:
```bash
make
```

### Idle stages
The heart of `lockingd`'s configuration is the `idle_stages` array. Each entry defines a timeout and a pair of commands to run when the system goes idle and when activity resumes:

```c
static const struct idle_stage {
    unsigned int  timeout_ms;
    const char   *cmd_idle;
    const char   *cmd_resume;
} idle_stages[] = {
    {  10000, "wlopm --off \"*\"",    "wlopm --on \"*\""  },
 // { 300000, "systemctl suspend",    NULL                 },
};
```

You can define as many stages as you like. Each one registers an independent idle notification with the compositor — when the idle threshold is crossed, `cmd_idle` runs; when the user becomes active again, `cmd_resume` runs. Setting `cmd_resume` to `NULL` skips the resume step for that stage.

On locker exit, all `cmd_resume` commands are run automatically so your displays always come back on cleanly.

### Hooks
Two optional hooks fire around the locker's lifetime, regardless of idle state:

| Hook | When it runs | Typical use |
|---|---|---|
| `CMD_PRE_LOCK` | Just before the locker process is spawned | Send a notification, pause media, etc. |
| `CMD_POST_UNLOCK` | After the locker process exits | Resume media, log the unlock event, etc. |

```c
#define CMD_PRE_LOCK    NULL   // e.g. "notify-send 'Locking...'"
#define CMD_POST_UNLOCK NULL   // e.g. "playerctl play"
```

Set either to `NULL` to disable it.

### Default locker command
```c
#define DEFAULT_LOCKER_ARGS "swaylock", "-c", "000000", "--font", "IBM Plex Sans"
```
This is used when `lockingd` is invoked without a `--` argument. Override it at runtime without recompiling by passing your command after `--`:
```bash
lockingd -- swaylock -c 112233
```

## Compositor-specific examples

### wlroots compositors (wlopm)
```c
{  10000, "wlopm --off \"*\"",  "wlopm --on \"*\""  },
```

### Sway (swaymsg)
```c
{  10000, "swaymsg \"output * power off\"",  "swaymsg \"output * power on\""  },
```

### Displays off, then suspend
```c
{  10000, "wlopm --off \"*\"",   "wlopm --on \"*\""  },
{ 300000, "systemctl suspend",   NULL                 },
```
The second stage fires five minutes after the first. Because `cmd_resume` is `NULL`, there's no resume command for suspend — the system simply wakes up and the display stage handles turning screens back on.
