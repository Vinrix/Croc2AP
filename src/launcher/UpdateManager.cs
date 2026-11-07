using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Text;
using System.Threading.Tasks;

namespace Croc2Launcher
{
    public class UpdateInfo
    {
        public bool UpdateAvailable { get; set; }
        public string CurrentVersion { get; set; }
        public string LatestVersion { get; set; }
        public string ReleaseNotes { get; set; }
        public string ReleaseUrl { get; set; }
        public string LauncherDownloadUrl { get; set; }
        public string SetupDownloadUrl { get; set; }
        public string ZipDownloadUrl { get; set; }
        public string ApworldDownloadUrl { get; set; }
        public string YamlDownloadUrl { get; set; }
        public string HookDllDownloadUrl { get; set; }
        public string ErrorMessage { get; set; }
    }

    public static class UpdateManager
    {
        public const string CurrentVersion = "1.0.7";
        public const string DefaultRepo = "Vinrix/Croc2AP";

        static UpdateManager()
        {
            try
            {
                // Ensure TLS 1.2 is enabled for GitHub API
                ServicePointManager.SecurityProtocol |= (SecurityProtocolType)3072;
            }
            catch { }
        }

        public static async Task<UpdateInfo> CheckForUpdatesAsync(string repo = DefaultRepo)
        {
            var info = new UpdateInfo();
            info.CurrentVersion = CurrentVersion;
            info.LatestVersion = CurrentVersion;
            info.UpdateAvailable = false;
            info.ReleaseNotes = "";
            info.ReleaseUrl = "";
            info.ErrorMessage = "";

            if (string.IsNullOrEmpty(repo)) repo = DefaultRepo;

            try
            {
                string apiUrl = "https://api.github.com/repos/" + repo + "/releases/latest";
                var request = (HttpWebRequest)WebRequest.Create(apiUrl);
                request.Method = "GET";
                request.UserAgent = "Croc2ArchipelagoLauncher-" + CurrentVersion;
                request.Accept = "application/vnd.github.v3+json";
                request.Timeout = 8000;

                using (var response = (HttpWebResponse)await Task.Factory.FromAsync<WebResponse>(request.BeginGetResponse, request.EndGetResponse, null))
                using (var stream = response.GetResponseStream())
                using (var reader = new StreamReader(stream, Encoding.UTF8))
                {
                    string json = await reader.ReadToEndAsync();
                    object parsed = MiniJson.Deserialize(json);
                    if (parsed is Dictionary<string, object>)
                    {
                        var dict = (Dictionary<string, object>)parsed;
                        string tag = dict.ContainsKey("tag_name") && dict["tag_name"] != null ? dict["tag_name"].ToString() : "";
                        info.LatestVersion = tag.Trim().TrimStart('v', 'V');
                        info.ReleaseNotes = dict.ContainsKey("body") && dict["body"] != null ? dict["body"].ToString() : "";
                        info.ReleaseUrl = dict.ContainsKey("html_url") && dict["html_url"] != null ? dict["html_url"].ToString() : "";

                        // Parse release assets
                        if (dict.ContainsKey("assets") && dict["assets"] is System.Collections.IEnumerable)
                        {
                            foreach (object aObj in (System.Collections.IEnumerable)dict["assets"])
                            {
                                if (aObj is Dictionary<string, object>)
                                {
                                    var aDict = (Dictionary<string, object>)aObj;
                                    string aName = aDict.ContainsKey("name") && aDict["name"] != null ? aDict["name"].ToString() : "";
                                    string aUrl = aDict.ContainsKey("browser_download_url") && aDict["browser_download_url"] != null ? aDict["browser_download_url"].ToString() : "";

                                    if (aName.IndexOf("setup", StringComparison.OrdinalIgnoreCase) >= 0 || aName.IndexOf("installer", StringComparison.OrdinalIgnoreCase) >= 0)
                                    {
                                        info.SetupDownloadUrl = aUrl;
                                    }
                                    else if (aName.IndexOf("launcher", StringComparison.OrdinalIgnoreCase) >= 0 && aName.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
                                    {
                                        info.LauncherDownloadUrl = aUrl;
                                    }
                                    else if (aName.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
                                    {
                                        info.ZipDownloadUrl = aUrl;
                                    }
                                    else if (aName.EndsWith(".apworld", StringComparison.OrdinalIgnoreCase))
                                    {
                                        info.ApworldDownloadUrl = aUrl;
                                    }
                                    else if (aName.EndsWith(".yaml", StringComparison.OrdinalIgnoreCase) || aName.EndsWith(".yml", StringComparison.OrdinalIgnoreCase))
                                    {
                                        info.YamlDownloadUrl = aUrl;
                                    }
                                    else if (aName.Equals("version.dll", StringComparison.OrdinalIgnoreCase) || aName.Equals("croc2_ap_hook.dll", StringComparison.OrdinalIgnoreCase))
                                    {
                                        info.HookDllDownloadUrl = aUrl;
                                    }
                                }
                            }
                        }

                        // Compare semantic versions
                        info.UpdateAvailable = IsNewerVersion(CurrentVersion, info.LatestVersion);
                    }
                }
            }
            catch (Exception ex)
            {
                info.ErrorMessage = ex.Message;
            }

            return info;
        }

        public static bool IsNewerVersion(string current, string remote)
        {
            try
            {
                Version vCur = ParseVersion(current);
                Version vRem = ParseVersion(remote);
                return vRem > vCur;
            }
            catch
            {
                return false;
            }
        }

        private static Version ParseVersion(string vStr)
        {
            if (string.IsNullOrEmpty(vStr)) return new Version(0, 0, 0);
            vStr = vStr.Trim().TrimStart('v', 'V');
            string[] parts = vStr.Split(new char[] { '.', '-', '+' }, StringSplitOptions.RemoveEmptyEntries);
            int major = parts.Length > 0 ? int.Parse(parts[0]) : 0;
            int minor = parts.Length > 1 ? int.Parse(parts[1]) : 0;
            int build = parts.Length > 2 ? int.Parse(parts[2]) : 0;
            return new Version(major, minor, build);
        }

        public static async Task<bool> DownloadAndApplyUpdateAsync(UpdateInfo info, string gameDir = null, Action<string> onProgress = null)
        {
            if (info == null || string.IsNullOrEmpty(info.LauncherDownloadUrl))
                return false;

            string currentExe = Process.GetCurrentProcess().MainModule.FileName;
            string exeDir = Path.GetDirectoryName(currentExe);
            string newExe = Path.Combine(exeDir, "Croc2ArchipelagoLauncher.exe.new");

            try
            {
                if (onProgress != null) onProgress("Downloading latest launcher (" + info.LatestVersion + ")...");

                using (var client = new WebClient())
                {
                    client.Headers["User-Agent"] = "Croc2ArchipelagoLauncher-" + CurrentVersion;
                    await client.DownloadFileTaskAsync(new Uri(info.LauncherDownloadUrl), newExe);
                }

                // Download updated croc2.apworld if available
                if (!string.IsNullOrEmpty(info.ApworldDownloadUrl))
                {
                    try
                    {
                        string apDir = @"C:\ProgramData\Archipelago\custom_worlds";
                        if (Directory.Exists(apDir))
                        {
                            if (onProgress != null) onProgress("Downloading updated croc2.apworld...");
                            using (var client = new WebClient())
                            {
                                client.Headers["User-Agent"] = "Croc2ArchipelagoLauncher-" + CurrentVersion;
                                string dstApworld = Path.Combine(apDir, "croc2.apworld");
                                await client.DownloadFileTaskAsync(new Uri(info.ApworldDownloadUrl), dstApworld);
                            }
                        }
                    }
                    catch { }
                }

                // Download updated hook DLL to game directory if detected
                if (!string.IsNullOrEmpty(info.HookDllDownloadUrl) && !string.IsNullOrEmpty(gameDir) && Directory.Exists(gameDir))
                {
                    try
                    {
                        if (onProgress != null) onProgress("Downloading updated hook DLL (version.dll)...");
                        using (var client = new WebClient())
                        {
                            client.Headers["User-Agent"] = "Croc2ArchipelagoLauncher-" + CurrentVersion;
                            string dstDll = Path.Combine(gameDir, "version.dll");
                            await client.DownloadFileTaskAsync(new Uri(info.HookDllDownloadUrl), dstDll);
                        }
                    }
                    catch { }
                }

                if (onProgress != null) onProgress("Applying update and restarting launcher...");

                // Create self-replacing updater batch script
                string batchPath = Path.Combine(exeDir, "update_launcher.bat");
                var sb = new StringBuilder();
                sb.AppendLine("@echo off");
                sb.AppendLine("timeout /t 1 /nobreak > nul");
                sb.AppendLine("move /y \"" + newExe + "\" \"" + currentExe + "\" > nul");
                sb.AppendLine("start \"\" \"" + currentExe + "\" --updated");
                sb.AppendLine("del \"%~f0\"");

                File.WriteAllText(batchPath, sb.ToString(), Encoding.Default);

                var psi = new ProcessStartInfo
                {
                    FileName = batchPath,
                    WorkingDirectory = exeDir,
                    CreateNoWindow = true,
                    UseShellExecute = false
                };
                Process.Start(psi);

                return true;
            }
            catch (Exception ex)
            {
                if (onProgress != null) onProgress("Update Error: " + ex.Message);
                return false;
            }
        }
    }
}

