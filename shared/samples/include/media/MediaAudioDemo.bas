' MediaAudioDemo.bas — namespace MEDIA (gmbmedia): lettura/scrittura/DSP audio (WAV PCM/float).
' La decodifica audio e le DSP sono il valore unico di gmbmedia (non coperte da SDL_image/mixer).
IMPORT MEDIA WITH ALIAS

SUB Main()
    PRINT "wav rate=" + STR(MEDIA.WAVRATE("sample16.wav")) + " bits=" + STR(MEDIA.WAVBITS("sample16.wav")) + " canali=" + STR(MEDIA.WAVCHANNELS("sample16.wav"))

    a = MEDIA.AUDIOREAD("sample16.wav")
    IF a < 1 THEN
        PRINT "ERRORE lettura audio"
        EXIT SUB
    END IF
    frames = MEDIA.AUDIOFRAMES(a)
    ch = MEDIA.AUDIOCHANNELS(a)
    PRINT "read: handle=" + STR(a) + " frames=" + STR(frames) + " canali=" + STR(ch) + " rate=" + STR(MEDIA.AUDIORATE(a))
    PRINT "campione(0,0) = " + STR(MEDIA.AUDIOSAMPLE(a, 0, 0))

    ' DSP
    PRINT "gain(+6dB)   = " + STR(MEDIA.AUDIOGAIN(a, 600))
    PRINT "normalize    = " + STR(MEDIA.AUDIONORMALIZE(a))
    PRINT "reverse      = " + STR(MEDIA.AUDIOREVERSE(a))
    PRINT "fade(100,100)= " + STR(MEDIA.AUDIOFADE(a, 100, 100))

    ' resample 2x (sinc)
    r = MEDIA.AUDIORESAMPLE(a, frames * 2, 1)
    PRINT "resample 2x: frames=" + STR(MEDIA.AUDIOFRAMES(r))

    ' mono
    m = MEDIA.AUDIOTOMONO(a)
    PRINT "to mono: canali=" + STR(MEDIA.AUDIOCHANNELS(m)) + " frames=" + STR(MEDIA.AUDIOFRAMES(m))

    ok = MEDIA.AUDIOWRITE(r, "work/out_resampled.wav")
    PRINT "write wav=" + STR(ok)

    MEDIA.AUDIOFREE(a) : MEDIA.AUDIOFREE(r) : MEDIA.AUDIOFREE(m)
    PRINT "done"
END SUB
