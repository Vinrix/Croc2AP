using System;
using System.Diagnostics;
using System.IO;
using System.Security.Principal;
using System.Windows.Forms;

namespace Croc2AP.Setup
{
    static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            bool silent = false;
            foreach (string arg in args)
            {
                if (arg.Equals("/silent", StringComparison.OrdinalIgnoreCase) ||
                    arg.Equals("/s", StringComparison.OrdinalIgnoreCase) ||
                    arg.Equals("/install", StringComparison.OrdinalIgnoreCase))
                {
                    silent = true;
                }
            }

            if (!IsAdministrator() && NeedsElevation())
            {
                try
                {
                    ProcessStartInfo psi = new ProcessStartInfo
                    {
                        FileName = Application.ExecutablePath,
                        Arguments = string.Join(" ", args),
                        Verb = "runas",
                        UseShellExecute = true
                    };
                    Process.Start(psi);
                    return;
                }
                catch (Exception)
                {
                    // User declined UAC elevation; proceed anyway (might fail on protected paths)
                }
            }

            if (silent)
            {
                SetupEngine engine = new SetupEngine();
                bool success = engine.PerformInstall(null);
                Environment.Exit(success ? 0 : 1);
            }
            else
            {
                Application.Run(new SetupForm());
            }
        }

        public static bool IsAdministrator()
        {
            try
            {
                WindowsIdentity identity = WindowsIdentity.GetCurrent();
                WindowsPrincipal principal = new WindowsPrincipal(identity);
                return principal.IsInRole(WindowsBuiltInRole.Administrator);
            }
            catch
            {
                return false;
            }
        }

        private static bool NeedsElevation()
        {
            // If running on Windows Vista+, elevate so we can write to Program Files & ProgramData
            return Environment.OSVersion.Version.Major >= 6;
        }
    }
}

