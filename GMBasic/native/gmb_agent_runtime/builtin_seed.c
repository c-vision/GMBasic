/* builtin_seed.c — semina ToolRegistry con i tool noti (browser.*, computer.*, files.*, ui.*)
 * Chiamato al boot della VM/C (gmb_vm_host.c) e dell'IL. Un file/dominio. */
#include "tool_registry.h"
#include <string.h>

static int seeded = 0;

void gmb_tool_registry_seed_builtins(void) {
    if (seeded) return;
    /* Browser semantic tools (vanno al BrowserProvider via CDP) */
    gmb_tool_registry_register_simple("browser.navigate", "Navigate the browser to a URL", "BrowserProvider");
    gmb_tool_registry_register_simple("browser.click",    "Click an element identified by selector/text", "BrowserProvider");
    gmb_tool_registry_register_simple("browser.fill",     "Fill a form field with text", "BrowserProvider");
    gmb_tool_registry_register_simple("browser.snapshot", "Take an a11y snapshot of the current page", "BrowserProvider");
    gmb_tool_registry_register_simple("browser.evaluate", "Evaluate JavaScript in page context", "BrowserProvider");
    gmb_tool_registry_register_simple("browser.login",    "Login using credentials from the vault (model never sees password)", "BrowserProvider");
    /* Computer use fallback */
    gmb_tool_registry_register_simple("computer.screenshot", "Take a screenshot of the screen", "ComputerProvider");
    gmb_tool_registry_register_simple("computer.click",      "Click at absolute coordinates x,y", "ComputerProvider");
    gmb_tool_registry_register_simple("computer.type",       "Type text at the current focus", "ComputerProvider");
    gmb_tool_registry_register_simple("computer.key",        "Press a key (e.g. Return, Tab)", "ComputerProvider");
    /* Filesystem tools (L2 native — preferred over computer.*) */
    gmb_tool_registry_register_simple("files.read",   "Read a text file (sandboxed)", "FilesProvider");
    gmb_tool_registry_register_simple_risk("files.write",  "Write text to a file (sandboxed)", "FilesProvider", 1);
    gmb_tool_registry_register_simple("files.list",   "List files in a directory", "FilesProvider");
    /* UI semantic (ActionResolver) */
    gmb_tool_registry_register_simple("ui.find",  "Find a UI element by text/selector (API→AX→DOM→CUA)", "ActionResolver");
    gmb_tool_registry_register_simple_risk("ui.click", "Click a UI element by text/selector", "ActionResolver", 1);
    gmb_tool_registry_register_simple_risk("ui.type",  "Type text into a UI element by text/selector", "ActionResolver", 1);
    seeded = 1;
}
