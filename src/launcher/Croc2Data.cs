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
            // Names come from the same data files the mod and the apworld use, compiled in by
            // tools/gen_launcher_data.py (Croc2DataGenerated.cs). A hand-written table used to live here and its
            // ids had drifted: the activity log named most items wrongly.
            foreach (object[] item in Croc2DataGenerated.Items)
            {
                RegisterItem((int)item[0], (string)item[1], (string)item[2]);
            }
            foreach (object[] loc in Croc2DataGenerated.Locations)
            {
                LocationNames[(long)loc[0]] = (string)loc[1];
            }

            // If the data files sit next to the launcher (a development checkout), they take precedence.
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
            if (locId >= 7211001 && locId <= 7213000)
            {
                long offset = locId - 7211000;
                // 7211000 + stage * 100 + number, with numbers 1..100 (so stage 0 number 100 is offset 100).
                int stage = (int)((offset - 1) / 100);
                int crystal = (int)((offset - 1) % 100) + 1;
                string stageName = (stage >= 0 && stage < Croc2DataGenerated.Stages.Length)
                    ? Croc2DataGenerated.Stages[stage] : "Stage " + (stage + 1);
                return stageName + " - White Crystal " + crystal;
            }
            return "Location #" + locId;
        }
    }
}
