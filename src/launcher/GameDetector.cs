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

        // Remaster model files are named EU_<map>_OBJ_<n>, where <n> is an index into THAT map's own
        // object list, so the same number is a different object in every map. Collectables are not
        // stored per level: every level preloads them from shared "library" maps (mostly T0L1M003, see
        // remaster/data_store/generated/T*/preload.json). So only those exact shared files are swapped.
        private static readonly string[] LogoTargetNames =
        {
            "EU_T0L1M003_OBJ_009", // Key
            "EU_T0L1M003_OBJ_022", // Heart
            "EU_T0L1M003_OBJ_012", // collectable-sized shared object (pair with 013) - verify in game
            "EU_T0L1M003_OBJ_013",
            "EU_T0L1M003_OBJ_021", // collectable-sized shared object - verify in game
            "EU_T0L1M003_OBJ_025", // collectable-sized shared object (pair with 026) - verify in game
            "EU_T0L1M003_OBJ_026",
            "EU_T0L1M007_OBJ_004", // kept from the original list
        };
        // Never swapped: OBJ_011 and OBJ_015..019 of T0L1M003 are the white and five coloured crystals.

        private static readonly System.Collections.Generic.HashSet<string> LogoTargets =
            new System.Collections.Generic.HashSet<string>(LogoTargetNames, StringComparer.OrdinalIgnoreCase);

        public static bool IsTargetCollectable(string filePath)
        {
            return LogoTargets.Contains(Path.GetFileNameWithoutExtension(filePath));
        }

        // ZMOD layout: "ZMOD", version, ?, submesh count; 84-byte submesh table; int32 vertex-byte count +
        // vertices (24 bytes each); int32 index-byte count + uint32 indices; then a trailer (20 bytes for
        // static models). The trailer must be preserved or the engine reads past the end of the file.
        public static byte[] BuildArchipelagoLogoZmod(byte[] origBytes, byte[] vdata, byte[] idata)
        {
            if (origBytes == null || origBytes.Length < 16) return null;
            if (origBytes[0] != 'Z' || origBytes[1] != 'M' || origBytes[2] != 'O' || origBytes[3] != 'D') return null;

            int numSub = BitConverter.ToInt32(origBytes, 12);
            if (numSub <= 0 || numSub > 64) return null;

            int headerSize = 16 + numSub * 84;
            if (origBytes.Length < headerSize + 4) return null;
            int origVBytes = BitConverter.ToInt32(origBytes, headerSize);
            int idxPos = headerSize + 4 + origVBytes;
            if (origVBytes < 0 || idxPos + 4 > origBytes.Length) return null;
            int origIBytes = BitConverter.ToInt32(origBytes, idxPos);
            int trailerPos = idxPos + 4 + origIBytes;
            if (origIBytes < 0 || trailerPos > origBytes.Length) return null;
            int trailerLen = origBytes.Length - trailerPos;
            if (trailerLen != 20) return null; // skinned/animated or unknown layout: leave it alone

            byte[] newBytes = new byte[headerSize + 4 + vdata.Length + 4 + idata.Length + trailerLen];
            Array.Copy(origBytes, 0, newBytes, 0, headerSize);

            // Submesh 0 draws the logo: index_count = all logo indices, bounding box +-0.1 x +-0.1 x +-0.05
            int sub0 = 16;
            BitConverter.GetBytes(idata.Length / 4).CopyTo(newBytes, sub0 + 32);
            BitConverter.GetBytes(0).CopyTo(newBytes, sub0 + 36);
            BitConverter.GetBytes(-0.1f).CopyTo(newBytes, sub0 + 40);
            BitConverter.GetBytes(-0.1f).CopyTo(newBytes, sub0 + 44);
            BitConverter.GetBytes(-0.05f).CopyTo(newBytes, sub0 + 48);
            BitConverter.GetBytes(0.1f).CopyTo(newBytes, sub0 + 52);
            BitConverter.GetBytes(0.1f).CopyTo(newBytes, sub0 + 56);
            BitConverter.GetBytes(0.05f).CopyTo(newBytes, sub0 + 60);

            // Other submeshes draw nothing
            for (int i = 1; i < numSub; i++)
            {
                int subI = 16 + i * 84;
                for (int b = 32; b < 64; b++) newBytes[subI + b] = 0;
            }

            int offset = headerSize;
            BitConverter.GetBytes(vdata.Length).CopyTo(newBytes, offset);
            offset += 4;
            Array.Copy(vdata, 0, newBytes, offset, vdata.Length);
            offset += vdata.Length;
            BitConverter.GetBytes(idata.Length).CopyTo(newBytes, offset);
            offset += 4;
            Array.Copy(idata, 0, newBytes, offset, idata.Length);
            offset += idata.Length;
            Array.Copy(origBytes, trailerPos, newBytes, offset, trailerLen);

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
                    string bak = file + ".vanilla";
                    try
                    {
                        if (!IsTargetCollectable(file))
                        {
                            // Undo swaps made by older launcher versions, which matched the wrong files
                            if (File.Exists(bak))
                            {
                                File.Copy(bak, file, true);
                                File.Delete(bak);
                            }
                            continue;
                        }

                        if (!File.Exists(bak))
                        {
                            File.Copy(file, bak, true);
                        }

                        byte[] patched = BuildArchipelagoLogoZmod(File.ReadAllBytes(bak), vdata, idata);
                        if (patched != null)
                        {
                            File.WriteAllBytes(file, patched);
                        }
                    }
                    catch { }
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

