#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <errno.h>
#include <wayland-client.h>
#include "ext-idle-notify-v1-client-protocol.h"

#define LOCK_TIMEOUT_MS 10000
#define CMD_DISPLAY_OFF "wlopm --off \"*\""
#define CMD_DISPLAY_ON "wlopm --on \"*\""
#define DEFAULT_LOCKER_ARGS "swaylock", "-c", "000000", "--font", "IBM Plex Sans"

static struct wl_seat *seat = NULL;
static struct ext_idle_notifier_v1 *notifier = NULL;
static struct ext_idle_notification_v1 *notification = NULL;
static pid_t locker_pid = -1;

static void run_command(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

static void notification_idled(void *data, struct ext_idle_notification_v1 *notif) {
    (void)data;
    (void)notif;
    run_command(CMD_DISPLAY_OFF);
}

static void notification_resumed(void *data, struct ext_idle_notification_v1 *notif) {
    (void)data;
    (void)notif;
    run_command(CMD_DISPLAY_ON);
}

static const struct ext_idle_notification_v1_listener notification_listener = {
    .idled = notification_idled,
    .resumed = notification_resumed,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version) {
    (void)data;
    if (strcmp(interface, "wl_seat") == 0) {
        if (!seat) {
            seat = wl_registry_bind(registry, name, &wl_seat_interface, 2);
        }
    } else if (strcmp(interface, "ext_idle_notifier_v1") == 0) {
        uint32_t bind_ver = version > 2 ? 2 : version;
        notifier = wl_registry_bind(registry, name, &ext_idle_notifier_v1_interface, bind_ver);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

int main(int argc, char *argv[]) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        perror("sigprocmask");
        return 1;
    }

    int sigfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sigfd == -1) {
        perror("signalfd");
        return 1;
    }

    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        close(sigfd);
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_roundtrip(display);
    wl_display_roundtrip(display);

    if (!notifier) {
        fprintf(stderr, "Compositor does not support ext-idle-notify-v1\n");
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        close(sigfd);
        return 1;
    }
    if (!seat) {
        fprintf(stderr, "No seat found\n");
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        close(sigfd);
        return 1;
    }

    uint32_t notifier_version = ext_idle_notifier_v1_get_version(notifier);
    if (notifier_version >= 2) {
        notification = ext_idle_notifier_v1_get_input_idle_notification(notifier, LOCK_TIMEOUT_MS, seat);
    } else {
        notification = ext_idle_notifier_v1_get_idle_notification(notifier, LOCK_TIMEOUT_MS, seat);
    }

    if (!notification) {
        fprintf(stderr, "Failed to create idle notification\n");
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        close(sigfd);
        return 1;
    }

    ext_idle_notification_v1_add_listener(notification, &notification_listener, NULL);
    wl_display_flush(display);

    locker_pid = fork();
    if (locker_pid < 0) {
        perror("fork");
        ext_idle_notification_v1_destroy(notification);
        wl_registry_destroy(registry);
        wl_display_disconnect(display);
        close(sigfd);
        return 1;
    }
    if (locker_pid == 0) {
        sigset_t empty_mask;
        sigemptyset(&empty_mask);
        sigprocmask(SIG_SETMASK, &empty_mask, NULL);

        char **exec_args;
        if (argc > 1) {
            exec_args = &argv[1];
        } else {
            static char *default_args[] = {DEFAULT_LOCKER_ARGS, NULL};
            exec_args = default_args;
        }
        execvp(exec_args[0], exec_args);
        perror("execvp");
        _exit(1);
    }

    int wl_fd = wl_display_get_fd(display);
    struct pollfd fds[2] = {
        { .fd = wl_fd, .events = POLLIN },
        { .fd = sigfd, .events = POLLIN }
    };

    int running = 1;
    while (running) {
        while (wl_display_prepare_read(display) != 0) {
            if (wl_display_dispatch_pending(display) < 0) {
                running = 0;
                break;
            }
        }
        if (!running) break;

        if (wl_display_flush(display) < 0) {
            wl_display_cancel_read(display);
            break;
        }

        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            wl_display_cancel_read(display);
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (fds[0].revents & POLLIN) {
            if (wl_display_read_events(display) < 0) {
                break;
            }
            if (wl_display_dispatch_pending(display) < 0) {
                break;
            }
        } else {
            wl_display_cancel_read(display);
        }

        if (fds[1].revents & POLLIN) {
            struct signalfd_siginfo fdsi;
            ssize_t s = read(sigfd, &fdsi, sizeof(fdsi));
            if (s == sizeof(fdsi)) {
                if (fdsi.ssi_signo == SIGCHLD) {
                    int status;
                    pid_t waited = waitpid(locker_pid, &status, WNOHANG);
                    if (waited == locker_pid) {
                        running = 0;
                    }
                    while (waitpid(-1, &status, WNOHANG) > 0);
                }
            }
        }
    }

    run_command(CMD_DISPLAY_ON);

    ext_idle_notification_v1_destroy(notification);
    ext_idle_notifier_v1_destroy(notifier);
    if (seat) {
        wl_seat_destroy(seat);
    }
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    close(sigfd);

    return 0;
}
