#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wayland-client-core.h>
#include <wayland-client.h>

#include "AppState.h"
#include "Desktop.h"
#include "DesktopLayerSurface.h"

#include "Vector.h"
//#include "HashSet.h"

#include "Test/DesktopLayerSurfaceTest.h"

void TestEnv(void)
{
    
}

int main(int argc, char *argv[])
{
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
        wl_display_prepare_read(state.display);

        int poll_result = poll(state.poll_fds, sizeof(state.poll_fds) / sizeof(*state.poll_fds), -1);

        if (poll_result < 0)
        {
            if (poll_result == EINTR)
                continue;
            else
            {
                fprintf(stderr, "fail while polling fds: %s\n", strerror(errno));
                break;
            }
        }
        
        // display fd
        if (state.poll_fds[0].revents & POLLIN)
        {
            if (wl_display_read_events(state.display) == -1)
            {
                fprintf(stderr, "error while reading events display: %s\n", strerror(errno));
                break;
            }

            if (wl_display_dispatch_pending(state.display) == -1)
            {
                fprintf(stderr, "error to dispatch from display: %s\n", strerror(errno));
                break;
            }

            state.poll_fds[0].revents *= ~POLLIN;
        }
        else
            wl_display_cancel_read(state.display);

        // pipe fd
        if (state.poll_fds[1].revents & POLLIN)
        {
            struct Vector_Iter iter_vec = { };
        
            wl_display_roundtrip(state.display);

            if (Vector_IterInit(&iter_vec, &state.desktops_with_surface_to_create))
            {
                struct Desktop **ptr_desktop_ptr = NULL;
                while ((ptr_desktop_ptr = (struct Desktop **)Vector_IterNext(&iter_vec)))
                {
                    struct Desktop *desktop = *ptr_desktop_ptr;

                    DesktopLayerSurface_Init(&desktop->surface, &state, desktop->wl_output);
                    TestPatternDrawOnDesktopLayerSurface(&desktop->surface);
                }
            }
            Vector_Clear(&state.desktops_with_surface_to_create);

            // clear
            char discard_buf[128];
            while (read(state.pipefds[0], discard_buf, sizeof(discard_buf)) > 0);

            state.poll_fds[1].revents *= ~POLLIN;
        }
    }

End:
    AppState_Cleanup(&state);

    return ret;
}