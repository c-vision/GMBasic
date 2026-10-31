namespace Basic;

public static class Opt
{
    public static bool VerboseOrchestrator;
    public static bool VerboseLexer;
}

public static class SettingsHelper
{
    public static CommonLib.EffectiveSettings LoadSettings(string path)
    {
        return new CommonLib.EffectiveSettings();
    }
}
