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
#include <time.h>
#include <stdarg.h>
#include <wayland-client.h>
#include "ext-idle-notify-v1-client-protocol.h"
#include "config.h"

#define N_STAGES (sizeof(idle_stages) / sizeof(idle_stages[0]))

static struct wl_seat              *seat          = NULL;
static struct ext_idle_notifier_v1 *notifier      = NULL;
static struct ext_idle_notification_v1 *notifications[N_STAGES];
static pid_t locker_pid    = -1;
static int   locker_running = 1;
static int   verbose        = 0;

static void log_info(const char *fmt, ...) {
    if (!verbose) return;
    time_t t; struct tm tm; char buf[32];
    time(&t);
    localtime_r(&t, &tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(stderr, "%s - ", buf);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static void run_command(const char *cmd) {
    if (!cmd) return;
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

static void notification_idled(void *data, struct ext_idle_notification_v1 *notif) {
    (void)notif;
    if (!locker_running) return;
    const struct idle_stage *s = data;
    log_info("idle [%ums]: exec: %s", s->timeout_ms, s->cmd_idle);
    run_command(s->cmd_idle);
}

static void notification_resumed(void *data, struct ext_idle_notification_v1 *notif) {
    (void)notif;
    if (!locker_running) return;
    const struct idle_stage *s = data;
    log_info("resume [%ums]: exec: %s", s->timeout_ms, s->cmd_resume ? s->cmd_resume : "(none)");
    run_command(s->cmd_resume);
}

static const struct ext_idle_notification_v1_listener notification_listener = {
    .idled   = notification_idled,
    .resumed = notification_resumed,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface, uint32_t version) {
    (void)data;
    if (strcmp(interface, "wl_seat") == 0) {
        if (!seat)
            seat = wl_registry_bind(registry, name, &wl_seat_interface, 2);
    } else if (strcmp(interface, "ext_idle_notifier_v1") == 0) {
        uint32_t v = version > 2 ? 2 : version;
        notifier = wl_registry_bind(registry, name, &ext_idle_notifier_v1_interface, v);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global        = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static void cleanup_notifications(size_t count) {
    for (size_t i = 0; i < count; i++)
        ext_idle_notification_v1_destroy(notifications[i]);
}

int main(int argc, char *argv[]) {
    char **exec_args = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--") == 0) {
            if (i + 1 < argc) exec_args = &argv[i + 1];
            break;
        } else {
            exec_args = &argv[i];
            break;
        }
    }

    if (!exec_args) {
        static char *default_args[] = {DEFAULT_LOCKER_ARGS, NULL};
        exec_args = default_args;
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) { perror("sigprocmask"); return 1; }

    int sigfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sigfd == -1) { perror("signalfd"); return 1; }

    log_info("connecting to wayland display");
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "failed to connect to wayland display\n");
        close(sigfd);
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);

    if (!notifier) {
        fprintf(stderr, "compositor does not support ext-idle-notify-v1\n");
        goto fail;
    }
    if (!seat) {
        fprintf(stderr, "no seat found\n");
        goto fail;
    }

    uint32_t notifier_ver = ext_idle_notifier_v1_get_version(notifier);
    int use_input_notif = notifier_ver >= 2;

    for (size_t s = 0; s < N_STAGES; s++) {
        if (use_input_notif) {
            log_info("stage %zu: input idle notification at %ums (inhibitors bypassed)",
                     s, idle_stages[s].timeout_ms);
            notifications[s] = ext_idle_notifier_v1_get_input_idle_notification(
                notifier, idle_stages[s].timeout_ms, seat);
        } else {
            log_info("stage %zu: idle notification at %ums", s, idle_stages[s].timeout_ms);
            notifications[s] = ext_idle_notifier_v1_get_idle_notification(
                notifier, idle_stages[s].timeout_ms, seat);
        }

        if (!notifications[s]) {
            fprintf(stderr, "failed to create idle notification for stage %zu\n", s);
            cleanup_notifications(s);
            goto fail;
        }

        ext_idle_notification_v1_add_listener(notifications[s], &notification_listener,
                                              (void *)&idle_stages[s]);
    }

    wl_display_flush(display);

    if (CMD_PRE_LOCK) {
        log_info("pre-lock: exec: %s", CMD_PRE_LOCK);
        run_command(CMD_PRE_LOCK);
    }

    log_info("spawning locker: %s", exec_args[0]);
    locker_pid = fork();
    if (locker_pid < 0) {
        perror("fork");
        cleanup_notifications(N_STAGES);
        goto fail;
    }
    if (locker_pid == 0) {
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);
        execvp(exec_args[0], exec_args);
        perror("execvp");
        _exit(1);
    }

    log_info("locker pid %d", locker_pid);

    int wl_fd = wl_display_get_fd(display);
    struct pollfd fds[2] = {
        { .fd = wl_fd, .events = POLLIN },
        { .fd = sigfd,  .events = POLLIN },
    };

    log_info("entering event loop");
    int running = 1;
    while (running) {
        while (wl_display_prepare_read(display) != 0) {
            if (wl_display_dispatch_pending(display) < 0) { running = 0; break; }
        }
        if (!running) break;

        if (wl_display_flush(display) < 0) { wl_display_cancel_read(display); break; }

        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            wl_display_cancel_read(display);
            if (errno == EINTR) continue;
            break;
        }

        if (fds[0].revents & POLLIN) {
            if (wl_display_read_events(display) < 0) break;
            if (wl_display_dispatch_pending(display) < 0) break;
        } else {
            wl_display_cancel_read(display);
        }

        if (fds[1].revents & POLLIN) {
            struct signalfd_siginfo fdsi;
            if (read(sigfd, &fdsi, sizeof(fdsi)) == sizeof(fdsi) &&
                fdsi.ssi_signo == SIGCHLD) {
                int status;
                if (waitpid(locker_pid, &status, WNOHANG) == locker_pid) {
                    log_info("locker pid %d exited", locker_pid);
                    locker_running = 0;
                    running = 0;
                }
                while (waitpid(-1, &status, WNOHANG) > 0);
            }
        }
    }

    for (size_t s = 0; s < N_STAGES; s++) {
        if (idle_stages[s].cmd_resume) {
            log_info("cleanup [stage %zu]: exec: %s", s, idle_stages[s].cmd_resume);
            run_command(idle_stages[s].cmd_resume);
        }
    }

    if (CMD_POST_UNLOCK) {
        log_info("post-unlock: exec: %s", CMD_POST_UNLOCK);
        run_command(CMD_POST_UNLOCK);
    }

    cleanup_notifications(N_STAGES);
    ext_idle_notifier_v1_destroy(notifier);
    if (seat) wl_seat_destroy(seat);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    close(sigfd);
    return 0;

fail:
    if (notifier) ext_idle_notifier_v1_destroy(notifier);
    if (seat) wl_seat_destroy(seat);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    close(sigfd);
    return 1;
}
