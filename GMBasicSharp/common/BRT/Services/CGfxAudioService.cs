// FILE: Basic.Runtime.CGfxAudioService.cs
// Audio via motore C (SDL_mixer): SOUND/PLAY/SOUND LOAD/PLAY/VOLUME + filtro biquad.
// Usato con GMB_GFX_C=1. In headless senza SDL_mixer le funzioni C sono no-op sicure.
using System;
using System.Runtime.InteropServices;
using Basic.Runtime;

namespace Basic.Runtime;

public sealed class CGfxAudioService : IAudioService
{
    private const string Lib = CGfxNative.Lib;

    [DllImport(Lib)] private static extern void gmb_audio_sound(int frequency, double duration, int volume);
    [DllImport(Lib)] private static extern void gmb_audio_play_mml(string mml);
    [DllImport(Lib)] private static extern int gmb_audio_load_sound(string path);
    [DllImport(Lib)] private static extern void gmb_audio_play_sound(int handle, int channel, int loops);
    [DllImport(Lib)] private static extern void gmb_audio_set_volume(int volume, int channel);
    [DllImport(Lib)] private static extern void gmb_audio_control(int action, int channel);
    [DllImport(Lib)] private static extern void gmb_audio_set_filter(int voice, int type, double cutoff, double resonance);

    static CGfxAudioService() => CGfxNative.EnsureResolver();

    public void Sound(double frequency, double duration, int volume)
    {
        try { gmb_audio_sound((int)frequency, duration, volume); } catch { }
    }

    public void SetFilter(int voice, int type, double cutoff, double resonance)
    {
        try { gmb_audio_set_filter(voice, type, cutoff, resonance); } catch { }
    }

    public int LoadSound(string filePath)
    {
        try { return gmb_audio_load_sound(filePath); } catch { return 0; }
    }

    public void PlaySound(int handle, int channel, int loops)
    {
        try { gmb_audio_play_sound(handle, channel, loops); } catch { }
    }

    public void SetVolume(int volume, int? channel)
    {
        try { gmb_audio_set_volume(volume, channel ?? -1); } catch { }
    }

    public void Play(string musicString)
    {
        try { gmb_audio_play_mml(musicString ?? string.Empty); } catch { }
    }

    public void AudioControl(int action, object? argument)
    {
        try
        {
            // Canale: numero → canale; nome/altro → tutti (il motore C non ha canali
            // nominati: le sequenze sono bloccanti e non accodate).
            var channel = -1;
            switch (argument)
            {
                case int i: channel = i; break;
                case long l: channel = (int)l; break;
                case double d: channel = (int)d; break;
            }
            gmb_audio_control(action, channel);
        }
        catch { }
    }
}
