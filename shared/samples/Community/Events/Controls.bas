' FILE: samples/Community/Events/Controls.bas
' Widget TUI — CHECKBOX, LISTBOX, MENU, TOGGLE, SLIDER
' Esempio completo di controlli interattivi
IMPORT CONTROLS
SUB Main()
  PRINT "Test controlli TUI"
  DIM r
  
  ' CHECKBOX row, col, "caption" INTO var
  CHECKBOX 1, 1, "Opzione A" INTO r
  PRINT "Checkbox: "; r
  
  ' TOGGLE row, col, "caption" INTO var
  TOGGLE 1, 3, "Attiva" INTO r
  PRINT "Toggle: "; r
  
  ' MENU row, col, "items" INTO var
  MENU 1, 5, "Rosso,Verde,Blu" INTO r
  PRINT "Menu: "; r
  
  ' LISTBOX row, col, width, height, "items" INTO var
  LISTBOX 1, 7, 20, 5, "Uno,Due,Tre" INTO r
  PRINT "Listbox: "; r
  
  PRINT "Fine test controlli"
END SUB