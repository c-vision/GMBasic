' MiniTest NAMESPACE TOOL — tutte le funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: 0|[{"name":"browser.navigate","description":"Navigate the browser to a URL","provider":"BrowserProvider","risk":"low"},{"name":"browser.click","description":"Click an element identified by selector/text","provider":"BrowserProvider","risk":"low"},{"name":"browser.fill","description":"Fill a form field with text","provider":"BrowserProvider","risk":"low"},{"name":"browser.snapshot","description":"Take an a11y snapshot of the current page","provider":"BrowserProvider","risk":"low"},{"name":"browser.evaluate","description":"Evaluate JavaScript in page context","provider":"BrowserProvider","risk":"low"},{"name":"browser.login","description":"Login using credentials from the vault (model never sees password)","provider":"BrowserProvider","risk":"low"},{"name":"computer.screenshot","description":"Take a screenshot of the screen","provider":"ComputerProvider","risk":"low"},{"name":"computer.click","description":"Click at absolute coordinates x,y","provider":"ComputerProvider","risk":"low"},{"name":"computer.type","description":"Type text at the current focus","provider":"ComputerProvider","risk":"low"},{"name":"computer.key","description":"Press a key (e.g. Return, Tab)","provider":"ComputerProvider","risk":"low"},{"name":"files.read","description":"Read a text file (sandboxed)","provider":"FilesProvider","risk":"low"},{"name":"files.write","description":"Write text to a file (sandboxed)","provider":"FilesProvider","risk":"medium"},{"name":"files.list","description":"List files in a directory","provider":"FilesProvider","risk":"low"},{"name":"ui.find","description":"Find a UI element by text/selector (API\u2192AX\u2192DOM\u2192CUA)","provider":"ActionResolver","risk":"low"},{"name":"ui.click","description":"Click a UI element by text/selector","provider":"ActionResolver","risk":"medium"},{"name":"ui.type","description":"Type text into a UI element by text/selector","provider":"ActionResolver","risk":"medium"}]
IMPORT TOOL

SUB T_reg()
    PRINT TOOL.REGISTER("", "")
END SUB

SUB T_search()
    PRINT TOOL.SEARCH("")
END SUB

SUB Main()
  T_reg()
  T_search()
END SUB
