/* native/repl/debug_server.c — speculare a DebugServer.cs. */
#include "debug_server.h"

void gmb_debug_server_init(gmb_debug_server_t* s, const gmb_debug_driver_t* driver)
{
    s->driver = driver;
    s->running = false;
}

void gmb_debug_server_start(gmb_debug_server_t* s)
{
    s->running = true;
    if (s->driver && s->driver->clear_breakpoints) s->driver->clear_breakpoints();
}

bool gmb_debug_server_is_paused(const gmb_debug_server_t* s)
{
    return s->driver && s->driver->is_paused ? s->driver->is_paused() : false;
}

bool gmb_debug_server_is_running(const gmb_debug_server_t* s) { return s->running; }

int gmb_debug_server_current_line(const gmb_debug_server_t* s)
{
    return s->driver && s->driver->current_line ? s->driver->current_line() : 0;
}

void gmb_debug_server_continue_exec(gmb_debug_server_t* s)
{
    if (s->running && s->driver && s->driver->continue_exec) s->driver->continue_exec();
}

void gmb_debug_server_step_in(gmb_debug_server_t* s)
{
    if (s->running && s->driver && s->driver->step_in) s->driver->step_in();
}

void gmb_debug_server_step_over(gmb_debug_server_t* s)
{
    if (s->running && s->driver && s->driver->step_over) s->driver->step_over();
}

void gmb_debug_server_step_out(gmb_debug_server_t* s)
{
    if (s->running && s->driver && s->driver->step_out) s->driver->step_out();
}

void gmb_debug_server_set_breakpoint(gmb_debug_server_t* s, int line)
{
    if (s->driver && s->driver->set_breakpoint) s->driver->set_breakpoint(line);
}

void gmb_debug_server_clear_breakpoints(gmb_debug_server_t* s)
{
    if (s->driver && s->driver->clear_breakpoints) s->driver->clear_breakpoints();
}

const char* gmb_debug_server_take_output(const gmb_debug_server_t* s)
{
    return s->driver && s->driver->take_output ? s->driver->take_output() : "";
}

void gmb_debug_server_stop(gmb_debug_server_t* s)
{
    s->running = false;
}
