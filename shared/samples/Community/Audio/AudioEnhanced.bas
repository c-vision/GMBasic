' FILE: Audio/AudioEnhanced.bas
' ====================================================================================
' ENHANCED SOUND — Waveform + ADSR + Pulse Width + Multi-Voice + Filter
' ====================================================================================
' SINTASSI:
'   SOUND freq, dur [, vol [, wave [, att, dec, sus, rel [, pw]]]]
'   SOUND ON voice, freq, dur [, vol [, wave [, att, dec, sus, rel [, pw]]]]
'   FILTER voice, type, cutoff, resonance
'
' wave: 0=Sine, 1=Square, 2=Sawtooth, 3=Triangle, 4=Noise, 5=Pulse
' pw:   pulse width in % (1-99, default 25, 50=square)
' type: 0=Off, 1=LowPass, 2=HighPass, 3=BandPass
' ====================================================================================

IMPORT GRAPHICS
IMPORT CONSOLE

PRINT "--- TEST SOUND ENHANCED ---"
PRINT

' 1. Waveform demo
PRINT "1. Waveform types"
SOUND 440, 0.4, 100, 0  ' Sine
SOUND 440, 0.4, 100, 1  ' Square
SOUND 440, 0.4, 100, 2  ' Sawtooth
SOUND 440, 0.4, 100, 3  ' Triangle
SOUND 200, 0.2, 80, 4   ' Noise (short burst)
SOUND 440, 0.4, 100, 5  ' Pulse (25%)
PRINT

' 2. Pulse width variation
PRINT "2. Pulse width: 10%, 25%, 50%, 75%"
SOUND 440, 0.3, 100, 5, 10, 0, 100, 10, 10   ' 10% duty
SOUND 440, 0.3, 100, 5, 10, 0, 100, 10, 25   ' 25% duty
SOUND 440, 0.3, 100, 5, 10, 0, 100, 10, 50   ' 50% duty (square)
SOUND 440, 0.3, 100, 5, 10, 0, 100, 10, 75   ' 75% duty
PRINT

' 3. ADSR shapes
PRINT "3. ADSR envelopes"
PRINT "  - Organ: slow attack (30%)"
SOUND 220, 1.5, 100, 0, 30
PRINT "  - Staccato: fast all around"
SOUND 440, 0.5, 100, 1, 5, 10, 30, 5
PRINT "  - Pad: slow everything"
SOUND 220, 2, 100, 3, 15, 15, 80, 30
PRINT "  - Percussion: fast attack, fast decay"
SOUND 800, 0.3, 100, 4, 1, 40, 0, 5
PRINT

' 4. Multi-voice (chord: C major)
PRINT "4. Multi-voice chord (C major)"
SOUND ON 1, 261.63, 0.8, 80, 0, 10, 5, 80, 20  ' C4
SOUND ON 2, 329.63, 0.8, 80, 0, 10, 5, 80, 20  ' E4
SOUND ON 3, 392.00, 0.8, 80, 0, 10, 5, 80, 20  ' G4
SOUND ON 4, 523.25, 0.8, 60, 0, 10, 5, 80, 20  ' C5
PRINT

' 5. Multi-voice with different waveforms
PRINT "5. Mixed waveforms chord"
SOUND ON 1, 261.63, 1, 80, 1, 10, 5, 80, 20     ' C4 square
SOUND ON 2, 329.63, 1, 60, 2, 10, 5, 80, 20     ' E4 saw
SOUND ON 3, 392.00, 1, 60, 3, 10, 5, 80, 20     ' G4 triangle
PRINT

' 6. Filter sweep
PRINT "6. Filter sweep (low-pass on sawtooth)"
FILTER 1, 1, 2000, 30   ' LowPass at 2000Hz, resonance 30%
SOUND ON 1, 330, 0.5, 100, 2
FILTER 1, 1, 500, 50    ' LowPass at 500Hz, more resonance
SOUND ON 1, 330, 0.5, 100, 2
FILTER 1, 1, 100, 80    ' LowPass at 100Hz, high resonance
SOUND ON 1, 330, 0.5, 100, 2
FILTER 1, 0, 0, 0       ' Filter off
PRINT

' 7. Arpeggio (multi-voice sequence)
PRINT "7. Arpeggio C major"
FOR I = 0 TO 2
    SOUND ON 1, 261.63, 0.15, 80, 3, 2, 10, 70, 5  ' C
    SOUND ON 2, 329.63, 0.15, 80, 3, 2, 10, 70, 5  ' E
    SOUND ON 3, 392.00, 0.15, 80, 3, 2, 10, 70, 5  ' G
    SOUND ON 4, 523.25, 0.15, 60, 3, 2, 10, 70, 5  ' C5
NEXT I
PRINT

' 8. Band-pass filter
PRINT "8. Band-pass filter"
FILTER 1, 3, 1000, 40   ' BandPass at 1000Hz
SOUND ON 1, 440, 0.8, 100, 2   ' Sawtooth through BP filter
FILTER 1, 0, 0, 0
PRINT

PRINT "--- FINE TEST AUDIO ---"
END
