using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace Croc2AP.Setup
{
    public class SetupEngine
    {
        public string Croc2Dir { get; set; }
        public string ArchipelagoDir { get; set; }
        public bool CreateDesktopShortcut { get; set; }
        public bool OverwriteConfig { get; set; }

        public SetupEngine()
        {
            Croc2Dir = DetectCroc2Dir();
            ArchipelagoDir = DetectArchipelagoDir();
            CreateDesktopShortcut = true;
            OverwriteConfig = false;
        }

        public string DetectCroc2Dir()
        {
            // 1. Current directory
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            if (File.Exists(Path.Combine(baseDir, "croc2.exe")))
                return baseDir;

            // 2. Standard paths
            string[] standardPaths = new string[]
            {
                @"C:\Program Files (x86)\Steam\steamapps\common\Croc 2",
                @"C:\Program Files\Steam\steamapps\common\Croc 2",
                @"D:\SteamLibrary\steamapps\common\Croc 2",
                @"E:\SteamLibrary\steamapps\common\Croc 2",
                @"F:\SteamLibrary\steamapps\common\Croc 2"
            };

            foreach (string p in standardPaths)
            {
                if (File.Exists(Path.Combine(p, "croc2.exe")))
                    return p;
            }

            // 3. Steam libraryfolders.vdf
            try
            {
                using (RegistryKey key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam"))
                {
                    if (key != null)
                    {
                        object steamPathVal = key.GetValue("SteamPath");
                        if (steamPathVal != null)
                        {
                            string steamPath = steamPathVal.ToString().Replace('/', '\\');
                            string vdfPath = Path.Combine(steamPath, @"steamapps\libraryfolders.vdf");
                            if (File.Exists(vdfPath))
                            {
                                string content = File.ReadAllText(vdfPath);
                                MatchCollection matches = Regex.Matches(content, @"""path""\s+""([^""]+)""");
                                foreach (Match m in matches)
                                {
                                    string libPath = m.Groups[1].Value.Replace(@"\\", @"\");
                                    string candidate = Path.Combine(libPath, @"steamapps\common\Croc 2");
                                    if (File.Exists(Path.Combine(candidate, "croc2.exe")))
                                        return candidate;
                                }
                            }
                        }
                    }
                }
            }
            catch { }

            // 4. Fallback default
            return standardPaths[0];
        }

        public string DetectArchipelagoDir()
        {
            string programData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            string candidate = Path.Combine(programData, "Archipelago");
            if (Directory.Exists(candidate))
                return candidate;

            string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            string localCandidate = Path.Combine(localAppData, "Archipelago");
            if (Directory.Exists(localCandidate))
                return localCandidate;

            return candidate; // C:\ProgramData\Archipelago default
        }

        public byte[] GetPayloadBytes(string filename)
        {
            // 1. Check current directory / subdirectories for loose file
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            string[] searchPaths = new string[]
            {
                Path.Combine(baseDir, filename),
                Path.Combine(baseDir, "build", filename),
                Path.Combine(baseDir, "dist", filename),
                Path.Combine(baseDir, "templates", filename)
            };

            foreach (string p in searchPaths)
            {
                if (File.Exists(p))
                {
                    try { return File.ReadAllBytes(p); } catch { }
                }
            }

            // 2. Read embedded resource
            Assembly asm = Assembly.GetExecutingAssembly();
            string[] manifestNames = asm.GetManifestResourceNames();
            foreach (string res in manifestNames)
            {
                if (res.Equals(filename, StringComparison.OrdinalIgnoreCase) ||
                    res.EndsWith("." + filename, StringComparison.OrdinalIgnoreCase))
                {
                    using (Stream s = asm.GetManifestResourceStream(res))
                    {
                        if (s != null)
                        {
                            byte[] buffer = new byte[s.Length];
                            s.Read(buffer, 0, (int)s.Length);
                            return buffer;
                        }
                    }
                }
            }

            return null;
        }

        public bool PerformInstall(Action<string> logCallback)
        {
            Action<string> log = delegate(string msg)
            {
                if (logCallback != null) logCallback(msg);
                else Console.WriteLine(msg);
            };

            log("Starting Croc 2 Archipelago installation...");
            log(string.Format("Target Croc 2 folder: {0}", Croc2Dir));
            log(string.Format("Target Archipelago folder: {0}", ArchipelagoDir));

            bool allOk = true;

            // Validate Croc 2 dir
            if (!Directory.Exists(Croc2Dir))
            {
                try { Directory.CreateDirectory(Croc2Dir); }
                catch (Exception ex)
                {
                    log(string.Format("[ERROR] Could not create Croc 2 folder: {0}", ex.Message));
                    return false;
                }
            }

            // 1. Hook DLL (version.dll)
            byte[] dllBytes = GetPayloadBytes("version.dll");
            if (dllBytes == null) dllBytes = GetPayloadBytes("croc2_ap_hook.dll");
            if (dllBytes != null && dllBytes.Length > 0)
            {
                string targetDll = Path.Combine(Croc2Dir, "version.dll");
                string backupDll = Path.Combine(Croc2Dir, "version.dll.vanilla_backup");
                if (File.Exists(targetDll) && !File.Exists(backupDll))
                {
                    try
                    {
                        File.Copy(targetDll, backupDll, true);
                        log("[+] Backed up original version.dll -> version.dll.vanilla_backup");
                    }
                    catch { }
                }

                try
                {
                    File.WriteAllBytes(targetDll, dllBytes);
                    log(string.Format("[OK] Installed Hook DLL: {0} ({1:N0} bytes)", targetDll, dllBytes.Length));
                }
                catch (Exception ex)
                {
                    log(string.Format("[ERROR] Failed to write version.dll: {0}. Ensure Croc 2 is closed!", ex.Message));
                    allOk = false;
                }
            }
            else
            {
                log("[WARN] version.dll payload not found!");
                allOk = false;
            }

            // 2. Mod INI (croc2_mod.ini)
            byte[] iniBytes = GetPayloadBytes("croc2_mod.ini");
            if (iniBytes != null)
            {
                string targetIni = Path.Combine(Croc2Dir, "croc2_mod.ini");
                if (!File.Exists(targetIni) || OverwriteConfig)
                {
                    try
                    {
                        File.WriteAllBytes(targetIni, iniBytes);
                        log(string.Format("[OK] Installed Config: {0}", targetIni));
                    }
                    catch (Exception ex)
                    {
                        log(string.Format("[WARN] Could not write croc2_mod.ini: {0}", ex.Message));
                    }
                }
                else
                {
                    log(string.Format("[INFO] Preserved existing config: {0}", targetIni));
                }
            }

            // 3. Launcher (Croc2ArchipelagoLauncher.exe)
            byte[] launcherBytes = GetPayloadBytes("Croc2ArchipelagoLauncher.exe");
            string targetLauncher = Path.Combine(Croc2Dir, "Croc2ArchipelagoLauncher.exe");
            if (launcherBytes != null)
            {
                try
                {
                    File.WriteAllBytes(targetLauncher, launcherBytes);
                    log(string.Format("[OK] Installed Standalone Launcher: {0}", targetLauncher));

                    // Create Desktop Shortcut
                    if (CreateDesktopShortcut)
                    {
                        CreateShortcut(targetLauncher);
                        log("[OK] Created Desktop shortcut: Croc 2 Archipelago Launcher");
                    }
                }
                catch (Exception ex)
                {
                    log(string.Format("[WARN] Could not write launcher: {0}", ex.Message));
                }
            }

            // 4. Instructions (INSTALL_INSTRUCTIONS.txt)
            byte[] instrBytes = GetPayloadBytes("INSTALL_INSTRUCTIONS.txt");
            if (instrBytes != null)
            {
                try
                {
                    File.WriteAllBytes(Path.Combine(Croc2Dir, "INSTALL_INSTRUCTIONS.txt"), instrBytes);
                }
                catch { }
            }

            // 5. Archipelago World (croc2.apworld)
            byte[] apworldBytes = GetPayloadBytes("croc2.apworld");
            if (apworldBytes != null)
            {
                string customWorlds = Path.Combine(ArchipelagoDir, "custom_worlds");
                try
                {
                    if (!Directory.Exists(customWorlds))
                        Directory.CreateDirectory(customWorlds);

                    string targetApworld = Path.Combine(customWorlds, "croc2.apworld");
                    File.WriteAllBytes(targetApworld, apworldBytes);
                    log(string.Format("[OK] Installed APWorld: {0} ({1:N0} bytes)", targetApworld, apworldBytes.Length));
                }
                catch (Exception ex)
                {
                    log(string.Format("[WARN] Could not write croc2.apworld to Archipelago directory: {0}", ex.Message));
                }
            }

            // 6. YAML Template (Croc 2.yaml)
            byte[] yamlBytes = GetPayloadBytes("Croc 2.yaml");
            if (yamlBytes != null)
            {
                string templatesDir = Path.Combine(ArchipelagoDir, @"Players\Templates");
                try
                {
                    if (!Directory.Exists(templatesDir))
                        Directory.CreateDirectory(templatesDir);

                    string targetYaml = Path.Combine(templatesDir, "Croc 2.yaml");
                    File.WriteAllBytes(targetYaml, yamlBytes);
                    log(string.Format("[OK] Installed YAML Template: {0}", targetYaml));
                }
                catch (Exception ex)
                {
                    log(string.Format("[WARN] Could not write Croc 2.yaml to Templates: {0}", ex.Message));
                }

                // Also copy to Croc 2 folder for quick player access
                try
                {
                    File.WriteAllBytes(Path.Combine(Croc2Dir, "Croc 2.yaml"), yamlBytes);
                }
                catch { }
            }

            if (allOk)
            {
                log("\n============================================================");
                log("  Installation Successful! Croc 2 AP is ready to play.");
                log("============================================================");
            }

            return allOk;
        }

        private void CreateShortcut(string targetExePath)
        {
            try
            {
                string desktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
                string shortcutPath = Path.Combine(desktop, "Croc 2 Archipelago Launcher.lnk");

                // Use PowerShell to create .lnk via WScript.Shell without external dependencies
                string psCmd = string.Format(
                    "-NoProfile -Command \"$s=(New-Object -COM WScript.Shell).CreateShortcut('{0}'); $s.TargetPath='{1}'; $s.WorkingDirectory='{2}'; $s.Description='Croc 2 Archipelago Randomizer Launcher'; $s.Save()\"",
                    shortcutPath.Replace("'", "''"),
                    targetExePath.Replace("'", "''"),
                    Path.GetDirectoryName(targetExePath).Replace("'", "''")
                );

                ProcessStartInfo psi = new ProcessStartInfo("powershell.exe", psCmd);
                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                Process p = Process.Start(psi);
                p.WaitForExit(4000);
            }
            catch { }
        }
    }
}
