using System;
using System.Collections.Generic;
using System.IO;

namespace Croc2Launcher
{
    public class LauncherSettings
    {
        public string Host { get; set; }
        public int Port { get; set; }
        public string SlotName { get; set; }
        public string Password { get; set; }
        public string Croc2Path { get; set; }
        public bool RememberSettings { get; set; }

        // Auto-Update & GitHub
        public string GitHubRepo { get; set; }
        public bool AutoCheckUpdates { get; set; }

        // Randomizer & YAML Options
        public int Goal { get; set; }
        public int JigsawPiecesRequired { get; set; }
        public int TrophiesRequired { get; set; }
        public int WhiteCrystals { get; set; }
        public int PeteStore { get; set; }
        public int DoorShuffle { get; set; }
        public bool LockStageDoors { get; set; }
        public bool ShuffleAbilities { get; set; }
        public bool ShuffleLevers { get; set; }
        public bool ShuffleVehicles { get; set; }
        public bool ShufflePlaneGuns { get; set; }
        public int TrapWeight { get; set; }
        public bool FireBreakoutTrap { get; set; }
        public bool DeathLink { get; set; }
        public string Theme { get; set; } // "system", "light" or "dark"

        public LauncherSettings()
        {
            Host = "archipelago.gg";
            Port = 38281;
            SlotName = "WillOfTheVinny";
            Password = "";
            Croc2Path = @"C:\Program Files (x86)\Steam\steamapps\common\Croc 2";
            RememberSettings = true;
            GitHubRepo = "Vinrix/Croc2AP";
            AutoCheckUpdates = true;

            Goal = 0;
            JigsawPiecesRequired = 4;
            TrophiesRequired = 12;
            WhiteCrystals = 3; // all_100
            PeteStore = 1;     // shuffle_shop_items
            DoorShuffle = 1;   // within_village
            LockStageDoors = true;
            ShuffleAbilities = true;
            ShuffleLevers = true;
            ShuffleVehicles = true;
            ShufflePlaneGuns = true;
            TrapWeight = 15;
            FireBreakoutTrap = true;
            DeathLink = false;
            Theme = "system";
        }
    }

    public static class SettingsManager
    {
        public static LauncherSettings Load(string filePath = "launcher_settings.json")
        {
            var settings = new LauncherSettings();
            try
            {
                if (File.Exists(filePath))
                {
                    string json = File.ReadAllText(filePath);
                    object root = MiniJson.Deserialize(json);
                    if (root is Dictionary<string, object>)
                    {
                        var dict = (Dictionary<string, object>)root;
                        if (dict.ContainsKey("Host") && dict["Host"] != null)
                            settings.Host = dict["Host"].ToString();
                        if (dict.ContainsKey("Port") && dict["Port"] != null)
                        {
                            int port;
                            if (int.TryParse(dict["Port"].ToString(), out port))
                                settings.Port = port;
                        }
                        if (dict.ContainsKey("SlotName") && dict["SlotName"] != null)
                            settings.SlotName = dict["SlotName"].ToString();
                        if (dict.ContainsKey("Password") && dict["Password"] != null)
                            settings.Password = dict["Password"].ToString();
                        if (dict.ContainsKey("Croc2Path") && dict["Croc2Path"] != null)
                            settings.Croc2Path = dict["Croc2Path"].ToString();
                        if (dict.ContainsKey("RememberSettings") && dict["RememberSettings"] != null)
                        {
                            bool remember;
                            if (bool.TryParse(dict["RememberSettings"].ToString(), out remember))
                                settings.RememberSettings = remember;
                        }
                        if (dict.ContainsKey("GitHubRepo") && dict["GitHubRepo"] != null)
                            settings.GitHubRepo = dict["GitHubRepo"].ToString();
                        if (dict.ContainsKey("AutoCheckUpdates") && dict["AutoCheckUpdates"] != null)
                        {
                            bool autoUp;
                            if (bool.TryParse(dict["AutoCheckUpdates"].ToString(), out autoUp))
                                settings.AutoCheckUpdates = autoUp;
                        }

                        // Parse Rando options
                        if (dict.ContainsKey("Goal") && dict["Goal"] != null)
                        {
                            int v;
                            if (int.TryParse(dict["Goal"].ToString(), out v)) settings.Goal = v;
                        }
                        if (dict.ContainsKey("JigsawPiecesRequired") && dict["JigsawPiecesRequired"] != null)
                        {
                            int v;
                            if (int.TryParse(dict["JigsawPiecesRequired"].ToString(), out v)) settings.JigsawPiecesRequired = v;
                        }
                        if (dict.ContainsKey("TrophiesRequired") && dict["TrophiesRequired"] != null)
                        {
                            int v;
                            if (int.TryParse(dict["TrophiesRequired"].ToString(), out v)) settings.TrophiesRequired = v;
                        }
                        if (dict.ContainsKey("WhiteCrystals") && dict["WhiteCrystals"] != null)
                        {
                            int v;
                            if (int.TryParse(dict["WhiteCrystals"].ToString(), out v)) settings.WhiteCrystals = v;
                        }
                        if (dict.ContainsKey("PeteStore") && dict["PeteStore"] != null)
                        {
                            int v;
                            if (int.TryParse(dict["PeteStore"].ToString(), out v)) settings.PeteStore = v;
                        }
                        if (dict.ContainsKey("DoorShuffle") && dict["DoorShuffle"] != null)
                        {
                            int v;
                            if (int.TryParse(dict["DoorShuffle"].ToString(), out v)) settings.DoorShuffle = v;
                        }
                        if (dict.ContainsKey("LockStageDoors") && dict["LockStageDoors"] != null)
                        {
                            bool b;
                            if (bool.TryParse(dict["LockStageDoors"].ToString(), out b)) settings.LockStageDoors = b;
                        }
                        if (dict.ContainsKey("ShuffleAbilities") && dict["ShuffleAbilities"] != null)
                        {
                            bool b;
                            if (bool.TryParse(dict["ShuffleAbilities"].ToString(), out b)) settings.ShuffleAbilities = b;
                        }
                        if (dict.ContainsKey("ShuffleLevers") && dict["ShuffleLevers"] != null)
                        {
                            bool b;
                            if (bool.TryParse(dict["ShuffleLevers"].ToString(), out b)) settings.ShuffleLevers = b;
                        }
                        if (dict.ContainsKey("ShuffleVehicles") && dict["ShuffleVehicles"] != null)
                        {
                            bool b;
                            if (bool.TryParse(dict["ShuffleVehicles"].ToString(), out b)) settings.ShuffleVehicles = b;
                        }
                        if (dict.ContainsKey("ShufflePlaneGuns") && dict["ShufflePlaneGuns"] != null)
                        {
                            bool b;
                            if (bool.TryParse(dict["ShufflePlaneGuns"].ToString(), out b)) settings.ShufflePlaneGuns = b;
                        }
                        if (dict.ContainsKey("TrapWeight") && dict["TrapWeight"] != null)
                        {
                            int v;
                            if (int.TryParse(dict["TrapWeight"].ToString(), out v)) settings.TrapWeight = v;
                        }
                        if (dict.ContainsKey("FireBreakoutTrap") && dict["FireBreakoutTrap"] != null)
                        {
                            bool b;
                            if (bool.TryParse(dict["FireBreakoutTrap"].ToString(), out b)) settings.FireBreakoutTrap = b;
                        }
                        if (dict.ContainsKey("Theme") && dict["Theme"] != null)
                        {
                            string th = dict["Theme"].ToString();
                            if (th == "light" || th == "dark" || th == "system") settings.Theme = th;
                        }
                        if (dict.ContainsKey("DeathLink") && dict["DeathLink"] != null)
                        {
                            bool b;
                            if (bool.TryParse(dict["DeathLink"].ToString(), out b)) settings.DeathLink = b;
                        }
                    }
                }
            }
            catch { }
            return settings;
        }

        public static void Save(LauncherSettings settings, string filePath = "launcher_settings.json")
        {
            try
            {
                if (settings == null) return;
                var dict = new Dictionary<string, object>();
                dict["Host"] = settings.Host;
                dict["Port"] = settings.Port;
                dict["SlotName"] = settings.SlotName;
                dict["Password"] = settings.Password;
                dict["Croc2Path"] = settings.Croc2Path;
                dict["RememberSettings"] = settings.RememberSettings;
                dict["GitHubRepo"] = settings.GitHubRepo;
                dict["AutoCheckUpdates"] = settings.AutoCheckUpdates;

                dict["Goal"] = settings.Goal;
                dict["JigsawPiecesRequired"] = settings.JigsawPiecesRequired;
                dict["TrophiesRequired"] = settings.TrophiesRequired;
                dict["WhiteCrystals"] = settings.WhiteCrystals;
                dict["PeteStore"] = settings.PeteStore;
                dict["DoorShuffle"] = settings.DoorShuffle;
                dict["LockStageDoors"] = settings.LockStageDoors;
                dict["ShuffleAbilities"] = settings.ShuffleAbilities;
                dict["ShuffleLevers"] = settings.ShuffleLevers;
                dict["ShuffleVehicles"] = settings.ShuffleVehicles;
                dict["ShufflePlaneGuns"] = settings.ShufflePlaneGuns;
                dict["TrapWeight"] = settings.TrapWeight;
                dict["FireBreakoutTrap"] = settings.FireBreakoutTrap;
                dict["DeathLink"] = settings.DeathLink;
                dict["Theme"] = settings.Theme ?? "system";

                string json = MiniJson.Serialize(dict);
                File.WriteAllText(filePath, json);
            }
            catch { }
        }
    }
}

