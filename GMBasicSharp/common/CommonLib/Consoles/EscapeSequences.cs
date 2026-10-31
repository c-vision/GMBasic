using CommonLib.Screens;
using CommonLib.Consoles;
// FILE: Interpreter.Terminal.EscapeSequences.cs

using System.Text;

// ReSharper disable InconsistentNaming

namespace CommonGraphics.Terminal;

/// <summary>
/// Processa un flusso di caratteri, interpreta le sequenze di escape ANSI/VT100
/// e invoca i comandi corrispondenti su un'istanza di ITerminalBuffer.
/// </summary>
public sealed class EscapeSequences
{
    /// <summary>
    /// Gli stati possibili del parser della sequenza di escape.
    /// </summary>
    private enum ParserState
    {
        Normal,
        Escape,
        ControlSequenceIntroducer
    }

    private readonly ITerminalBuffer _buffer; // MODIFICATO: Ora dipende dall'interfaccia
    private ParserState _currentState = ParserState.Normal;
        
    private readonly List<int> _params = [];
    private readonly StringBuilder _paramBuilder = new();

    /// <summary>
    /// Inizializza una nuova istanza della classe <see cref="EscapeSequences"/>.
    /// </summary>
    /// <param name="buffer">L'istanza di ITerminalBuffer su cui operare.</param>
    public EscapeSequences(ITerminalBuffer buffer) // MODIFICATO: Costruttore
    {
        _buffer = buffer;
    }

    /// <summary>
    /// Processa un singolo carattere. Se è un carattere normale, lo stampa.
    /// Se fa parte di una sequenza di escape, aggiorna lo stato del parser.
    /// </summary>
    /// <param name="ch">Il carattere da processare.</param>
    public void ProcessChar(char ch)
    {
        switch (_currentState)
        {
            case ParserState.Normal:
                if (ch == '\x1B') // ESC
                {
                    _currentState = ParserState.Escape;
                }
                else
                {
                    _buffer.Print(ch); // MODIFICATO
                }
                break;

            case ParserState.Escape:
                if (ch == '[')
                {
                    ResetParams();
                    _currentState = ParserState.ControlSequenceIntroducer;
                }
                else
                {
                    ResetState();
                }
                break;

            case ParserState.ControlSequenceIntroducer:
                if (char.IsDigit(ch))
                {
                    _paramBuilder.Append(ch);
                }
                else switch (ch)
                {
                    case ';':
                        FinalizeParam();
                        break;
                    // Carattere finale del comando
                    case >= '@' and <= '~':
                        FinalizeParam();
                        ExecuteCommand(ch);
                        ResetState();
                        break;
                    default:
                        ResetState();
                        break;
                }
                break;
        }
    }
        
    private void ExecuteCommand(char command)
    {
        switch (command)
        {
            case 'H': // CUP - Cursor Position
            case 'f': // HVP - Horizontal Vertical Position
                var row = GetParam(0, 1);
                var col = GetParam(1, 1);
                _buffer.SetCursorPosition(row, col); // MODIFICATO
                break;
                
            case 'J': // ED - Erase in Display
                if (GetParam(0, 0) == 2)
                    _buffer.ClearScreen(); // MODIFICATO
                break;

            case 'K': // EL - Erase in Line
                if (GetParam(0, 0) == 0)
                    _buffer.EraseToEndOfLine(); // MODIFICATO
                break;
                    
            case 'A': // CUU - Cursor Up
                _buffer.SetCursorPosition(_buffer.CursorRow - GetParam(0, 1) + 1, _buffer.CursorCol + 1); // MODIFICATO
                break;

            case 'B': // CUD - Cursor Down
                _buffer.SetCursorPosition(_buffer.CursorRow + GetParam(0, 1) + 1, _buffer.CursorCol + 1); // MODIFICATO
                break;

            case 'C': // CUF - Cursor Forward
                _buffer.SetCursorPosition(_buffer.CursorRow + 1, _buffer.CursorCol + GetParam(0, 1) + 1); // MODIFICATO
                break;

            case 'D': // CUB - Cursor Back
                _buffer.SetCursorPosition(_buffer.CursorRow + 1, _buffer.CursorCol - GetParam(0, 1) + 1); // MODIFICATO
                break;

            case 'm': // SGR - Select Graphic Rendition
                HandleSGR();
                break;
                    
            case 'h': // SM - Set Mode
            case 'l': // RM - Reset Mode
                if (GetParam(0, 0) == 25) 
                    _buffer.IsCursorVisible = command == 'h'; // MODIFICATO
                break;
        }
    }
        
    private void HandleSGR()
    {
        if (_params.Count == 0) _params.Add(0);

        foreach (var code in _params)
        {
            switch (code)
            {
                case 0: // Reset all attributes
                    _buffer.CurrentForegroundColor = 14; // Azzurro
                    _buffer.CurrentBackgroundColor = 6;  // Blu
                    // MODIFICATO: Usa il nuovo tipo unificato
                    _buffer.CurrentAttributes = ScreenCellAttributes.None;
                    continue;
                // MODIFICATO: Usa il nuovo tipo unificato
                case 1: _buffer.CurrentAttributes |= ScreenCellAttributes.Bold; continue;
                case 4: _buffer.CurrentAttributes |= ScreenCellAttributes.Underline; continue;
                case 5: _buffer.CurrentAttributes |= ScreenCellAttributes.Blink; continue;
                case 7: _buffer.CurrentAttributes |= ScreenCellAttributes.Inverse; continue;
                case 22: _buffer.CurrentAttributes &= ~ScreenCellAttributes.Bold; continue;
                case 24: _buffer.CurrentAttributes &= ~ScreenCellAttributes.Underline; continue;
                case 25: _buffer.CurrentAttributes &= ~ScreenCellAttributes.Blink; continue;
                case 27: _buffer.CurrentAttributes &= ~ScreenCellAttributes.Inverse; continue;
            }

            switch (code)
            {
                case >= 30 and <= 37:
                    _buffer.CurrentForegroundColor = code - 30;
                    break;
                case >= 40 and <= 47:
                    _buffer.CurrentBackgroundColor = code - 40;
                    break;
                case >= 90 and <= 97:
                    _buffer.CurrentForegroundColor = code - 90 + 8;
                    break;
                case >= 100 and <= 107:
                    _buffer.CurrentBackgroundColor = code - 100 + 8;
                    break;
            }
        }
    }        
    
    private void ResetState()
    {
        _currentState = ParserState.Normal;
        ResetParams();
    }

    private void ResetParams()
    {
        _paramBuilder.Clear();
        _params.Clear();
    }

    private void FinalizeParam()
    {
        if (_paramBuilder.Length > 0)
        {
            if (int.TryParse(_paramBuilder.ToString(), out var value))
                _params.Add(value);
            _paramBuilder.Clear();
        }
        else
        {
            _params.Add(-1); 
        }
    }

    private int GetParam(int index, int defaultValue)
    {
        if (index >= _params.Count) return defaultValue;
        var val = _params[index];
        return val <= 0 ? defaultValue : val;
    }
}