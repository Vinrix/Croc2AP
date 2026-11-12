using System;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace Croc2Launcher
{
    public class GamePaths
    {
        public string BaseDir { get; set; }
        public string ExePath { get; set; }
        public string HookDllPath { get; set; }
        public string IniPath { get; set; }

        public bool HasExe
        {
            get { return !string.IsNullOrEmpty(ExePath) && File.Exists(ExePath); }
        }

        public bool HasHookDll
        {
            get { return !string.IsNullOrEmpty(HookDllPath) && File.Exists(HookDllPath); }
        }

        public bool IsValid
        {
            get { return HasExe; }
        }
    }

    public static class GameDetector
    {
        private static readonly string[] DefaultDirs = new string[]
        {
            @"C:\Program Files (x86)\Steam\steamapps\common\Croc 2",
            @"C:\Program Files\Steam\steamapps\common\Croc 2"
        };

        public static GamePaths Detect(string preferredDir = null)
        {
            var paths = new GamePaths();

            string dir = preferredDir;
            if (!string.IsNullOrEmpty(dir) && Directory.Exists(dir))
            {
                string exe = Path.Combine(dir, "croc2.exe");
                if (File.Exists(exe))
                {
                    PopulatePaths(paths, dir);
                    return paths;
                }
            }

            // Check standard default directories
            foreach (string candidate in DefaultDirs)
            {
                if (Directory.Exists(candidate) && File.Exists(Path.Combine(candidate, "croc2.exe")))
                {
                    PopulatePaths(paths, candidate);
                    return paths;
                }
            }

            // Check current directory
            string currentDir = AppDomain.CurrentDomain.BaseDirectory;
            if (File.Exists(Path.Combine(currentDir, "croc2.exe")))
            {
                PopulatePaths(paths, currentDir);
                return paths;
            }

            // Check Steam libraryfolders.vdf
            string steamDir = GetSteamInstallDir();
            if (!string.IsNullOrEmpty(steamDir))
            {
                string vdf = Path.Combine(steamDir, "steamapps", "libraryfolders.vdf");
                if (File.Exists(vdf))
                {
                    try
                    {
                        string text = File.ReadAllText(vdf);
                        var matches = Regex.Matches(text, "\"path\"\\s+\"([^\"]+)\"");
                        foreach (Match m in matches)
                        {
                            string libPath = m.Groups[1].Value.Replace(@"\\", @"\");
                            string testDir = Path.Combine(libPath, "steamapps", "common", "Croc 2");
                            if (File.Exists(Path.Combine(testDir, "croc2.exe")))
                            {
                                PopulatePaths(paths, testDir);
                                return paths;
                            }
                        }
                    }
                    catch { }
                }
            }

            paths.BaseDir = !string.IsNullOrEmpty(preferredDir) ? preferredDir : DefaultDirs[0];
            return paths;
        }

        private static void PopulatePaths(GamePaths paths, string dir)
        {
            paths.BaseDir = Path.GetFullPath(dir);
            paths.ExePath = Path.Combine(paths.BaseDir, "croc2.exe");
            paths.HookDllPath = Path.Combine(paths.BaseDir, "version.dll");
            paths.IniPath = Path.Combine(paths.BaseDir, "croc2_mod.ini");
        }

        private static string GetSteamInstallDir()
        {
            try
            {
                using (var key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam"))
                {
                    if (key != null)
                    {
                        object val = key.GetValue("SteamPath");
                        if (val != null) return val.ToString().Replace('/', '\\');
                    }
                }
            }
            catch { }
            return null;
        }

        public static void SetModEnabledInIni(string iniPath, bool enabled)
        {
            if (string.IsNullOrEmpty(iniPath)) return;
            try
            {
                string content = "";
                if (File.Exists(iniPath))
                {
                    content = File.ReadAllText(iniPath);
                }
                else
                {
                    content = "[ArchipelagoMod]\nenabled = true\nport = 43825\n";
                }

                if (Regex.IsMatch(content, @"(?m)^enabled\s*=\s*(?:true|false|1|0)"))
                {
                    content = Regex.Replace(content, @"(?m)^enabled\s*=\s*(?:true|false|1|0)", "enabled = " + (enabled ? "true" : "false"));
                }
                else
                {
                    content = "[ArchipelagoMod]\nenabled = " + (enabled ? "true" : "false") + "\n\n" + content;
                }
                File.WriteAllText(iniPath, content);
            }
            catch { }
        }

        private static readonly string[] TargetCollectableSuffixes = new string[]
        {
            "_OBJ_006.zmod", "_OBJ_007.zmod", "_OBJ_008.zmod", "_OBJ_009.zmod",
            "_OBJ_010.zmod", "_OBJ_012.zmod", "_OBJ_013.zmod", "_OBJ_014.zmod",
            "_OBJ_020.zmod", "_OBJ_021.zmod", "_OBJ_025.zmod"
        };

        public static void ApplyArchipelagoLogoModels(string baseDir)
        {
            try
            {
                string modelsDir = Path.Combine(baseDir, "remaster", "models");
                if (!Directory.Exists(modelsDir)) return;

                string logoPath = Path.Combine(modelsDir, "original", "T0", "ap_logo.zmod");
                if (!File.Exists(logoPath)) return;

                byte[] logoData = File.ReadAllBytes(logoPath);
                if (logoData == null || logoData.Length < 48) return;

                foreach (string file in Directory.GetFiles(modelsDir, "*.zmod", SearchOption.AllDirectories))
                {
                    if (file.EndsWith(".vanilla", StringComparison.OrdinalIgnoreCase)) continue;
                    if (file.EndsWith("ap_logo.zmod", StringComparison.OrdinalIgnoreCase)) continue;

                    bool match = false;
                    foreach (string sfx in TargetCollectableSuffixes)
                    {
                        if (file.EndsWith(sfx, StringComparison.OrdinalIgnoreCase))
                        {
                            match = true;
                            break;
                        }
                    }
                    if (!match) continue;

                    string bak = file + ".vanilla";
                    if (!File.Exists(bak))
                    {
                        File.Copy(file, bak, true);
                    }

                    byte[] origHeader = new byte[48];
                    using (var fs = File.OpenRead(bak))
                    {
                        fs.Read(origHeader, 0, 48);
                    }

                    byte[] patchedData = (byte[])logoData.Clone();
                    Array.Copy(origHeader, 16, patchedData, 16, 32); // Preserve native material name
                    File.WriteAllBytes(file, patchedData);
                }
            }
            catch { }
        }

        public static void RestoreVanillaModels(string baseDir)
        {
            try
            {
                string modelsDir = Path.Combine(baseDir, "remaster", "models");
                if (!Directory.Exists(modelsDir)) return;

                foreach (string bak in Directory.GetFiles(modelsDir, "*.zmod.vanilla", SearchOption.AllDirectories))
                {
                    string orig = bak.Substring(0, bak.Length - 8);
                    try
                    {
                        File.Copy(bak, orig, true);
                    }
                    catch { }
                }
            }
            catch { }
        }

        public static Process LaunchGame(GamePaths paths, bool vanillaMode)
        {
            if (!paths.HasExe)
            {
                throw new FileNotFoundException("croc2.exe could not be found at: " + paths.ExePath);
            }

            // Ensure mod ini matches requested mode
            SetModEnabledInIni(paths.IniPath, !vanillaMode);

            // Swap models based on mode: Archipelago 3D logos for rando, vanilla original models for vanilla
            if (vanillaMode)
            {
                RestoreVanillaModels(paths.BaseDir);
            }
            else
            {
                ApplyArchipelagoLogoModels(paths.BaseDir);
            }

            var startInfo = new ProcessStartInfo
            {
                FileName = paths.ExePath,
                WorkingDirectory = paths.BaseDir,
                UseShellExecute = true,
                Arguments = vanillaMode ? "-vanilla" : ""
            };

            return Process.Start(startInfo);
        }

        public static Process FindRunningGame()
        {
            try
            {
                Process[] procs = Process.GetProcessesByName("croc2");
                if (procs != null && procs.Length > 0 && !procs[0].HasExited)
                {
                    return procs[0];
                }
            }
            catch { }
            return null;
        }
    }
}

