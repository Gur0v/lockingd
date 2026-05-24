static const struct idle_stage {
    unsigned int  timeout_ms;
    const char    *cmd_idle;
    const char    *cmd_resume;
} idle_stages[] = {
    {  10000, "wlopm --off \"*\"", "wlopm --on \"*\"" },
 // { 300000, "systemctl suspend", NULL               },
};

#define CMD_PRE_LOCK    NULL
#define CMD_POST_UNLOCK NULL

#define DEFAULT_LOCKER_ARGS "swaylock", "-c", "000000", "--font", "IBM Plex Sans"
