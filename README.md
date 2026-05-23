# qtile-lock
A small frankenstein to lock qtile's Wayland session nicely.
## How it works
This utility spawns a screen locker (such as [`swaylock`](https://github.com/swaywm/swaylock)) and communicates with the Wayland compositor via [`ext-idle-notify-v1`](https://wayland.app/protocols/ext-idle-notify-v1) to control display sleep timeouts:
- When idle for 10 seconds, it powers down the displays using [`wlopm`](https://sr.ht/~leon_plickat/wlopm) `--off "*"`.
- On user input activity, it wakes up the displays with `wlopm --on "*"`.
- When the screen locker process exits (whether successfully unlocked, crashed, or killed), the utility wakes up the displays, cleans up Wayland references, and terminates.

## Customization
You can configure compile-time settings directly at the top of `qtile_lock.c`:
```c
#define LOCK_TIMEOUT_MS 10000
#define CMD_DISPLAY_OFF "wlopm --off \"*\""
#define CMD_DISPLAY_ON "wlopm --on \"*\""
#define DEFAULT_LOCKER_ARGS "swaylock", "-c", "000000", "--font", "IBM Plex Sans"
```
Recompile the project after making any modifications.
> [!NOTE]
> Passing a custom locker command on the command line (using the `--` separator) completely overrides `DEFAULT_LOCKER_ARGS` dynamically without requiring recompilation.
## Build and Install
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
Install:
```bash
sudo make install
```
Uninstall:
```bash
sudo make uninstall
```
## Usage
Lock using the default locker command:
```bash
qtile-lock
```
Use the `-v` or `--verbose` flags to output debug logs prefixed with a timestamp (identical to [`dozed`](https://github.com/Gur0v/dozed) logging):
```bash
qtile-lock -v
```
To pass custom locker commands and arguments (without flag conflict), use the standard `--` separator:
```bash
# Lock with custom swaylock options (overriding DEFAULT_LOCKER_ARGS):
qtile-lock -- swaylock -c 112233
# Lock with custom options and verbose logging:
qtile-lock -v -- swaylock -c 112233
```
