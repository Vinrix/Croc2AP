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

        public static bool IsTargetCollectable(string filePath)
        {
            string fname = Path.GetFileNameWithoutExtension(filePath);
            if (!fname.Contains("_OBJ_")) return false;

            // Never touch gems (white crystals and 5 colored crystals)
            // OBJ_011: White Crystal
            // OBJ_015: Red Crystal
            // OBJ_016: Blue Crystal
            // OBJ_017: Green Crystal
            // OBJ_018: Yellow Crystal
            // OBJ_019: Pink/Purple Crystal
            if (fname.EndsWith("_OBJ_011", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_015", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_016", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_017", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_018", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_019", StringComparison.OrdinalIgnoreCase))
            {
                return false;
            }

            // Exclude Croc and major scenery/infrastructure
            // OBJ_000: Croc
            // OBJ_001..OBJ_006, OBJ_008, OBJ_009, OBJ_014
            if (fname.EndsWith("_OBJ_000", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_001", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_002", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_003", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_004", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_005", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_006", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_008", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_009", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_014", StringComparison.OrdinalIgnoreCase))
            {
                if (!fname.Equals("EU_T0L1M007_OBJ_004", StringComparison.OrdinalIgnoreCase))
                {
                    return false;
                }
            }

            // Exclude large scenery meshes (scenery/terrain/village buildings are > 700KB)
            try
            {
                var fi = new FileInfo(filePath);
                if (fi.Length > 700 * 1024) return false;
            }
            catch { return false; }

            // Target collectables & crates:
            // OBJ_007: Quest pickups / binoculars / items
            // OBJ_010: Heart pots
            // OBJ_012: Small smashables / crates (e.g. EU_T0L1M003_OBJ_012)
            // OBJ_013: Pete's store crate & smashables (e.g. EU_T0L1M005_OBJ_013)
            // OBJ_020: Clockwork Gobbos
            // OBJ_021: Keys
            // OBJ_025: Gobbos / Jellies
            // OBJ_027: Crates (e.g. EU_T0L1M003_OBJ_027)
            if (fname.EndsWith("_OBJ_007", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_010", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_012", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_013", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_020", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_021", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_025", StringComparison.OrdinalIgnoreCase) ||
                fname.EndsWith("_OBJ_027", StringComparison.OrdinalIgnoreCase) ||
                fname.Equals("EU_T0L1M007_OBJ_004", StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }

            return false;
        }

        public static byte[] BuildArchipelagoLogoZmod(byte[] origBytes, byte[] vdata, byte[] idata)
        {
            if (origBytes == null || origBytes.Length < 16) return null;
            if (origBytes[0] != 'Z' || origBytes[1] != 'M' || origBytes[2] != 'O' || origBytes[3] != 'D') return null;

            int numSub = BitConverter.ToInt32(origBytes, 12);
            if (numSub <= 0) return null;

            int headerSize = 16 + numSub * 84;
            if (origBytes.Length < headerSize) return null;

            byte[] newBytes = new byte[headerSize + 4 + vdata.Length + 4 + idata.Length];
            Array.Copy(origBytes, 0, newBytes, 0, headerSize);

            // Submesh 0: index_count = 504, index_offset = 0, bounding box: [-0.1, -0.1, -0.05] to [0.1, 0.1, 0.05]
            int sub0 = 16;
            BitConverter.GetBytes(504).CopyTo(newBytes, sub0 + 32);
            BitConverter.GetBytes(0).CopyTo(newBytes, sub0 + 36);
            BitConverter.GetBytes(-0.1f).CopyTo(newBytes, sub0 + 40);
            BitConverter.GetBytes(-0.1f).CopyTo(newBytes, sub0 + 44);
            BitConverter.GetBytes(-0.05f).CopyTo(newBytes, sub0 + 48);
            BitConverter.GetBytes(0.1f).CopyTo(newBytes, sub0 + 52);
            BitConverter.GetBytes(0.1f).CopyTo(newBytes, sub0 + 56);
            BitConverter.GetBytes(0.05f).CopyTo(newBytes, sub0 + 60);

            // Submeshes 1..numSub-1: index_count = 0, index_offset = 0 (draw 0 triangles, avoiding material errors)
            for (int i = 1; i < numSub; i++)
            {
                int subI = 16 + i * 84;
                BitConverter.GetBytes(0).CopyTo(newBytes, subI + 32);
                BitConverter.GetBytes(0).CopyTo(newBytes, subI + 36);
                BitConverter.GetBytes(0.0f).CopyTo(newBytes, subI + 40);
                BitConverter.GetBytes(0.0f).CopyTo(newBytes, subI + 44);
                BitConverter.GetBytes(0.0f).CopyTo(newBytes, subI + 48);
                BitConverter.GetBytes(0.0f).CopyTo(newBytes, subI + 52);
                BitConverter.GetBytes(0.0f).CopyTo(newBytes, subI + 56);
                BitConverter.GetBytes(0.0f).CopyTo(newBytes, subI + 60);
            }

            int offset = headerSize;
            BitConverter.GetBytes(vdata.Length).CopyTo(newBytes, offset);
            offset += 4;
            Array.Copy(vdata, 0, newBytes, offset, vdata.Length);
            offset += vdata.Length;

            BitConverter.GetBytes(idata.Length).CopyTo(newBytes, offset);
            offset += 4;
            Array.Copy(idata, 0, newBytes, offset, idata.Length);

            return newBytes;
        }

        public static void ApplyArchipelagoLogoModels(string baseDir)
        {
            try
            {
                string modelsDir = Path.Combine(baseDir, "remaster", "models");
                if (!Directory.Exists(modelsDir)) return;

                byte[] vdata = Convert.FromBase64String(LogoData.VertexDataBase64);
                byte[] idata = Convert.FromBase64String(LogoData.IndexDataBase64);

                foreach (string file in Directory.GetFiles(modelsDir, "*.zmod", SearchOption.AllDirectories))
                {
                    if (file.EndsWith(".vanilla", StringComparison.OrdinalIgnoreCase)) continue;
                    if (file.EndsWith("ap_logo.zmod", StringComparison.OrdinalIgnoreCase)) continue;

                    if (IsTargetCollectable(file))
                    {
                        string bak = file + ".vanilla";
                        if (!File.Exists(bak))
                        {
                            File.Copy(file, bak, true);
                        }

                        byte[] origBytes = File.ReadAllBytes(bak);
                        byte[] patched = BuildArchipelagoLogoZmod(origBytes, vdata, idata);
                        if (patched != null)
                        {
                            File.WriteAllBytes(file, patched);
                        }
                    }
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
                        File.Delete(bak);
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

            // Apply custom Archipelago logo models in mod mode, or restore clean vanilla models in vanilla mode
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

