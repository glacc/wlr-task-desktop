#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wayland-client-core.h>
#include <wayland-client.h>

#include "AppState.h"

#define ENV_HOME            "HOME"
#define ENV_XDG_DATA_HOME   "XDG_DATA_HOME"
#define ENV_XDG_DATA_DIRS   "XDG_DATA_DIRS"

void PrintEnv(const char *env_name)
{
    printf("env %s=%s\n", env_name, getenv(env_name));
}

#pragma region Wayland

static void Initialization()
{
    // wayland
}

#pragma endregion

int main(int argc, char *argv[])
{
    PrintEnv(ENV_HOME);
    PrintEnv(ENV_XDG_DATA_HOME);
    PrintEnv(ENV_XDG_DATA_DIRS);

    struct AppState state = { };
    int ret = 0;

    if (!AppState_Initialize(&state))
    {
        ret = EXIT_FAILURE;
        goto End;
    }

    // main loop
    while (true)
    {
        if (!AppState_DispatchAllEvents(&state))
            break;
    }

End:
    AppState_Cleanup(&state);

    return ret;
}