#include "ui/app.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   LPSTR command_line, int show_command)
{
    (void)previous;
    (void)command_line;
    return cf_app_run(instance, show_command);
}
