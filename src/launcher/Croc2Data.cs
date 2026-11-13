using System;
using System.Collections.Generic;
using System.IO;

namespace Croc2Launcher
{
    public static class Croc2Data
    {
        private static readonly Dictionary<int, string> ItemNames = new Dictionary<int, string>();
        private static readonly Dictionary<int, string> ItemImportanceMap = new Dictionary<int, string>();
        private static readonly Dictionary<long, string> LocationNames = new Dictionary<long, string>();

        static Croc2Data()
        {
            RegisterItem(7200001, "Tail Whack", "progression");
            RegisterItem(7200002, "Butt Stomp", "progression");
            RegisterItem(7200003, "Climbing Claws", "progression");
            RegisterItem(7200004, "Power Gloves", "progression");
            RegisterItem(7200005, "Lever Wrench", "progression");
            RegisterItem(7200006, "Vehicle License", "progression");
            RegisterItem(7200007, "Airplane Ammo Feed", "progression");
            RegisterItem(7200161, "Long Jump", "progression");

            RegisterItem(7200010, "Heart Pot", "useful");
            RegisterItem(7200011, "Heart Pot", "useful");
            RegisterItem(7200012, "Heart Pot", "useful");
            RegisterItem(7200013, "Heart Pot", "useful");

            RegisterItem(7200020, "Pete's Sailor VIP Pass", "progression");
            RegisterItem(7200021, "Pete's Cossack VIP Pass", "progression");
            RegisterItem(7200022, "Pete's Caveman VIP Pass", "progression");
            RegisterItem(7200023, "Pete's Inca VIP Pass", "progression");

            RegisterItem(7200030, "Clockwork Gobbo", "useful");
            RegisterItem(7200031, "Clockwork Gobbo", "useful");
            RegisterItem(7200032, "Clockwork Gobbo", "useful");

            RegisterItem(7200040, "Red Jelly", "useful");
            RegisterItem(7200041, "Blue Jelly", "useful");
            RegisterItem(7200042, "Green Jelly", "useful");
            RegisterItem(7200043, "Binoculars", "useful");

            RegisterItem(7200050, "Silver Key", "progression");
            RegisterItem(7200051, "Gold Key", "progression");
            RegisterItem(7200052, "Skeleton Key", "progression");

            RegisterItem(7200060, "Jigsaw Piece", "progression");
            RegisterItem(7200061, "Golden Gobbo", "progression");
            RegisterItem(7200062, "Trophy", "progression");

            RegisterItem(7200070, "100 Crystals", "filler");
            RegisterItem(7200071, "50 Crystals", "filler");
            RegisterItem(7200072, "10 Crystals", "filler");
            RegisterItem(7200073, "5 Crystals", "filler");
            RegisterItem(7200074, "1 Crystal", "filler");
            RegisterItem(7200075, "Extra Life", "useful");

            // Traps
            RegisterItem(7200090, "Fire Breakout Trap", "trap");
            RegisterItem(7200091, "Ice Physics Trap", "trap");
            RegisterItem(7200092, "Reverse Controls Trap", "trap");
            RegisterItem(7200093, "Dantini Ambush Trap", "trap");
            RegisterItem(7200094, "Speed Distortion Trap", "trap");
            RegisterItem(7200095, "Health Drain Trap", "trap");

            // Colored Crystals (Red, Blue, Green, Yellow, Purple) for each stage
            string[] colors = new string[] { "Red", "Blue", "Green", "Yellow", "Purple" };
            for (int s = 0; s < 20; ++s)
            {
                for (int c = 0; c < 5; ++c)
                {
                    int id = 7200100 + s * 5 + c;
                    RegisterItem(id, colors[c] + " Crystal (Stage " + (s + 1) + ")", "progression");
                }
            }

            TryLoadJsonData();
        }

        private static void RegisterItem(int id, string name, string importance)
        {
            ItemNames[id] = name;
            ItemImportanceMap[id] = importance;
        }

        private static void TryLoadJsonData()
        {
            try
            {
                string baseDir = AppDomain.CurrentDomain.BaseDirectory;
                string itemsJson = Path.Combine(baseDir, "worlds", "croc2", "data", "items.json");
                if (File.Exists(itemsJson))
                {
                    string content = File.ReadAllText(itemsJson);
                    object root = MiniJson.Deserialize(content);
                    if (root is System.Collections.IEnumerable)
                    {
                        foreach (object item in (System.Collections.IEnumerable)root)
                        {
                            if (item is Dictionary<string, object>)
                            {
                                var dict = (Dictionary<string, object>)item;
                                int id = dict.ContainsKey("id") ? Convert.ToInt32(dict["id"]) : 0;
                                string name = dict.ContainsKey("name") ? dict["name"].ToString() : "";
                                string cls = dict.ContainsKey("classification") ? dict["classification"].ToString() : "filler";
                                if (id > 0 && !string.IsNullOrEmpty(name))
                                {
                                    RegisterItem(id, name, cls);
                                }
                            }
                        }
                    }
                }

                string locsJson = Path.Combine(baseDir, "worlds", "croc2", "data", "locations.json");
                if (File.Exists(locsJson))
                {
                    string content = File.ReadAllText(locsJson);
                    object root = MiniJson.Deserialize(content);
                    if (root is System.Collections.IEnumerable)
                    {
                        foreach (object item in (System.Collections.IEnumerable)root)
                        {
                            if (item is Dictionary<string, object>)
                            {
                                var dict = (Dictionary<string, object>)item;
                                long id = dict.ContainsKey("id") ? Convert.ToInt64(dict["id"]) : 0;
                                string name = dict.ContainsKey("name") ? dict["name"].ToString() : "";
                                if (id > 0 && !string.IsNullOrEmpty(name))
                                {
                                    LocationNames[id] = name;
                                }
                            }
                        }
                    }
                }
            }
            catch { }
        }

        public static string GetItemName(int itemId)
        {
            string name;
            if (ItemNames.TryGetValue(itemId, out name)) return name;
            return "Item #" + itemId;
        }

        public static string GetItemImportance(int itemId, int flags = 0)
        {
            if ((flags & 0x04) != 0) return "trap";
            if ((flags & 0x01) != 0) return "progression";
            if ((flags & 0x02) != 0) return "useful";

            string imp;
            if (ItemImportanceMap.TryGetValue(itemId, out imp)) return imp;
            return "filler";
        }

        public static string GetLocationName(long locId)
        {
            string name;
            if (LocationNames.TryGetValue(locId, out name)) return name;
            if (locId >= 7211000 && locId <= 7212999)
            {
                long offset = locId - 7211000;
                int stage = (int)(offset / 100) + 1;
                int crystal = (int)(offset % 100);
                return "Stage " + stage + " - White Crystal #" + crystal;
            }
            return "Location #" + locId;
        }
    }
}
