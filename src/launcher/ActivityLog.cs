using System;
using System.IO;

namespace Croc2Launcher
{
    /// <summary>
    /// The plain-text copy of the launcher's activity list: croc2ap_activity.log beside croc2_mod.ini.
    /// Kept apart from the window code so it can be tested without a window.
    /// </summary>
    public static class ActivityLog
    {
        public const string FileName = "croc2ap_activity.log";

        /// <summary>One line, as it reads in the activity list (without the time).</summary>
        public static string Format(bool isOutgoing, string itemName, string sender, string receiver, string location)
        {
            bool own = sender == receiver || (sender == "Croc" && receiver == "Croc");
            string text = own
                ? sender + " got his own " + itemName + "!"
                : (isOutgoing ? "[OUT] " : "[IN] ") + sender + " sent " + itemName + " to " + receiver + "!";
            if (!string.IsNullOrEmpty(location)) text += "  @ " + location;
            return text;
        }

        /// <summary>Appends a dated line to the log in the folder that holds iniPath. Never throws.</summary>
        public static void Append(string iniPath, string text)
        {
            try
            {
                if (string.IsNullOrEmpty(iniPath)) return;
                string dir = Path.GetDirectoryName(iniPath);
                if (string.IsNullOrEmpty(dir)) return;
                File.AppendAllText(Path.Combine(dir, FileName),
                                   DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "  " + text + Environment.NewLine);
            }
            catch
            {
                // A log that can't be written (read-only folder, file open elsewhere) must not disturb play.
            }
        }
    }
}
