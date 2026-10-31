// FILE: Interpreter.Compiler.Compiler.Audio.cs



namespace Compiler;

/// <summary>
/// Contiene i metodi di visita per le istruzioni audio.
/// </summary>
public sealed partial class Compiler
{
    private const int PLAY_ACTION_STANDARD = 0;
    private const int PLAY_ACTION_CONCURRENT = 1;
    private const int PLAY_ACTION_QUEUE = 2;
    private const int PLAY_ACTION_LOOP = 3;
    private const int PLAY_ACTION_NAMED = 4;
    private const int PLAY_ACTION_CONTROL = 5;

    /// <summary>
    /// Compila un'istruzione SOUND con tutti i parametri (voice, waveform, ADSR, pw).
    /// </summary>
    private void VisitSoundStatement(SoundStmt stmt)
    {
        var argCount = 0;

        if (stmt.Voice is not null) { VisitExpression(stmt.Voice); argCount++; }
        else { EmitByte((byte)OpCode.OP_PUSH_NIL); argCount++; }

        VisitExpression(stmt.Frequency); argCount++;
        VisitExpression(stmt.Duration); argCount++;

        if (stmt.Volume is not null) { VisitExpression(stmt.Volume); argCount++; }
        else { EmitByte((byte)OpCode.OP_PUSH_NIL); argCount++; }

        if (stmt.Waveform is not null) { VisitExpression(stmt.Waveform); argCount++; }
        else { EmitByte((byte)OpCode.OP_PUSH_NIL); argCount++; }

        if (stmt.Attack is not null) { VisitExpression(stmt.Attack); argCount++; }
        else { EmitByte((byte)OpCode.OP_PUSH_NIL); argCount++; }

        if (stmt.Decay is not null) { VisitExpression(stmt.Decay); argCount++; }
        else { EmitByte((byte)OpCode.OP_PUSH_NIL); argCount++; }

        if (stmt.Sustain is not null) { VisitExpression(stmt.Sustain); argCount++; }
        else { EmitByte((byte)OpCode.OP_PUSH_NIL); argCount++; }

        if (stmt.Release is not null) { VisitExpression(stmt.Release); argCount++; }
        else { EmitByte((byte)OpCode.OP_PUSH_NIL); argCount++; }

        if (stmt.PulseWidth is not null) { VisitExpression(stmt.PulseWidth); argCount++; }
        else { EmitByte((byte)OpCode.OP_PUSH_NIL); argCount++; }

        EmitBytes((byte)OpCode.OP_AUDIO_CMD, (byte)AudioSubOpCode.SOUND, (byte)argCount);
    }

    /// <summary>
    /// Compila un'istruzione PLAY, gestendo tutte le sue varianti.
    /// Il caso per le azioni di controllo (PAUSE/RESUME/STOP) è stato spostato
    /// in VisitAudioControlStatement.
    /// </summary>
    private void VisitPlayStatement(PlayStmt stmt)
    {
        byte argCount = 0;
        switch (stmt.Action)
        {
            case StandardPlayAction spa:
                EmitConstant((double)PLAY_ACTION_STANDARD);
                VisitExpression(spa.MusicString);
                argCount = 2; // Codice Azione + MusicString
                break;

            case ConcurrentPlayAction cpa:
                EmitConstant((double)PLAY_ACTION_CONCURRENT);
                foreach (var musicString in cpa.MusicStrings)
                {
                    VisitExpression(musicString);
                }
                EmitConstant((double)cpa.MusicStrings.Length);
                argCount = (byte)(2 + cpa.MusicStrings.Length); // Codice Azione + N stringhe + Conteggio
                break;
                
            case QueuePlayAction qpa:
                EmitConstant((double)PLAY_ACTION_QUEUE);
                VisitExpression(qpa.MusicString);
                argCount = 2; // Codice Azione + MusicString
                break;

            case NamedPlayAction npa:
                EmitConstant((double)PLAY_ACTION_NAMED);
                VisitExpression(npa.MusicString);
                VisitExpression(npa.Channel);
                argCount = 3; // Codice Azione + MusicString + Canale
                break;

            case LoopPlayAction lpa:
                EmitConstant((double)PLAY_ACTION_LOOP);
                VisitExpression(lpa.MusicString);
                VisitExpression(lpa.Channel);
                if (lpa.Times is not null) VisitExpression(lpa.Times);
                else EmitByte((byte)OpCode.OP_PUSH_NIL);
                argCount = 4; // Codice Azione + MusicString + Canale + Times
                break;
        }
        
        // Emette l'opcode audio con il sottocomando PLAY e il numero di argomenti totali.
        EmitBytes((byte)OpCode.OP_AUDIO_CMD, (byte)AudioSubOpCode.PLAY, argCount);
    }

    /// <summary>
    /// Compila un'istruzione di controllo audio (SOUND/PLAY PAUSE, RESUME, STOP).
    /// Genera un P-Code unificato che usa OP_AUDIO_CMD con il sottocomando PLAY,
    /// come richiesto dalla logica dell'Executor.
    /// </summary>
    /// <summary>
    /// Compila un'istruzione FILTER voice, type, cutoff, resonance.
    /// </summary>
    private void VisitSetFilterStatement(SetFilterStmt stmt)
    {
        VisitExpression(stmt.Voice);
        VisitExpression(stmt.Type);
        VisitExpression(stmt.Cutoff);
        VisitExpression(stmt.Resonance);
        EmitBytes((byte)OpCode.OP_AUDIO_CMD, (byte)AudioSubOpCode.FILTER, 4);
    }

    private void VisitAudioControlStatement(AudioControlStmt stmt)
    {
        // L'Executor si aspetta la stessa struttura di un'azione di controllo PLAY.
        EmitConstant((double)PLAY_ACTION_CONTROL);
        
        // Emette il comando di controllo (es. TokenKind.Pause, TokenKind.Stop).
        EmitConstant((double)stmt.Action.Kind);
        
        // Emette l'argomento (canale o "ALL"), se presente.
        if (stmt.Argument is not null)
        {
            VisitExpression(stmt.Argument);
        }
        else
        {
            EmitByte((byte)OpCode.OP_PUSH_NIL);
        }

        // Il sottocomando è sempre PLAY per le azioni di controllo,
        // indipendentemente dal fatto che l'istruzione originale fosse SOUND o PLAY.
        EmitBytes((byte)OpCode.OP_AUDIO_CMD, (byte)AudioSubOpCode.PLAY, 3);
    }
}