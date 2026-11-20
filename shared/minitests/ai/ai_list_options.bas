' MiniTest: AI LIST OPTIONS — comando-query del registro opzioni generico.
' Comando (senza parentesi): AI LIST OPTIONS
' Stampa una riga per voce DICHIARATA: nome | descrizione | = valore corrente.
' Elenca le voci dichiarate del registro: core, ponte AI, GRAFICA e CONSOLE.
' EXPECT: moe_swap_dir | dir modello per lo streaming esperti MoE (vuoto = off) | = work/modeldir|path.models_root | radice dei modelli per `%MODELS%` e `~/ai/Models` (vuoto = ~/ai/Models) | = (vuoto)|GMB_TUI_SCRIPT | typeahead scriptato per TUI/CI: tasti iniettati nell'input (vuoto = input reale) | = (vuoto)|GMB_PKG_CACHE | dir cache dei pacchetti per REQUIRE (vuoto = <progetto>/packages) | = (vuoto)|GMB_TORCH_PATH | dir dei runtime ML (torch/mlx) quando i settings non la indicano | = (vuoto)|GMB_CONSOLE | modo della console testuale (0/assente = auto; valore = modo forzato) | = (vuoto)|GMB_CONSOLE_SIZE | griglia della console in colonne,righe (sovrascrive i settings) | = (vuoto)|GMB_CONSOLE_FONT | font della console: nome[,dimensione] (sovrascrive i settings) | = (vuoto)|GMB_SDLCONSOLE | console TUI su superficie SDL ('0' = disattivata) | = (vuoto)|GMB_FONT_PATH | file del font usato da grafica e console | = (vuoto)|GMB_UI_FONT_SIZE | dimensione in px del font UI (6..48, default 16) | = (vuoto)|GMB_GFX_BACKEND | backend grafico: 'vulkan' o 'auto' (dove compilato) | = (vuoto)|GMB_GFX_NULL | grafica headless senza finestra ('0' = disattivata) | = (vuoto)|GMB_GFX_SCALE | scala di render forzata (rapporto drawable/logico) | = (vuoto)|GMB_GFX_SHOT | file PNG della cattura della superficie grafica | = (vuoto)|GMB_MOUSE_SCRIPT | sequenza di movimenti/click del mouse iniettati (test TUI) | = (vuoto)|GMB_FORM_AUTOKEY | tasti automatici sui form (sequenza, test) | = (vuoto)|GMB_FORM_AUTOCLICK | click automatici sui form (sequenza, test) | = (vuoto)|GMB_FORM_SHOT | file PNG della cattura del form (test) | = (vuoto)|GMB_FORM_SHOT_FRAME | frame in cui scattare GMB_FORM_SHOT (default 20) | = (vuoto)|MAXXLIB_GRAPHICS | protocollo grafico del terminale: kitty o sixel (assente = dedotto da TERM) | = (vuoto)|debug.memory | contabilità memoria del runtime C per tipo, report su stderr a fine run (1 = attiva) | = (vuoto)|engine.present.max_fps | cap del present grafico in fps (0 = segue il clock) | = (vuoto)
IMPORT AI WITH ALIAS
SUB Main()
  ' Le due opzioni di CONSOLE hanno come valore corrente quello dei SETTINGS della macchina
  ' (consoleConfiguration) e la gamba C applica il bridge dei settings mentre le managed no:
  ' senza azzerarle qui, l'elenco contiene valori locali (es. "80,25" / "Monaco,14") e il
  ' test non è confrontabile né fra macchine né fra gambe. Il test verifica il REGISTRO
  ' (nomi, ordine, descrizioni), non la configurazione locale.
  AI SET OPTION "GMB_CONSOLE_SIZE", ""
  AI SET OPTION "GMB_CONSOLE_FONT", ""
  AI LIST OPTIONS
END SUB
