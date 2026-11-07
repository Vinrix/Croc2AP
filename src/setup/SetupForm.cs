using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace Croc2AP.Setup
{
    public class SetupForm : Form
    {
        private SetupEngine engine;

        private TextBox txtGameDir;
        private Label lblGameStatus;
        private TextBox txtArchipelagoDir;
        private Label lblArchipelagoStatus;
        private CheckBox chkDesktopShortcut;
        private CheckBox chkOverwriteConfig;
        private Button btnInstall;
        private Button btnLaunchLauncher;
        private Button btnOpenFolder;
        private TextBox txtLog;

        public SetupForm()
        {
            engine = new SetupEngine();
            try { this.Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
            InitializeComponent();
            UpdateValidationStatus();
        }

        private void InitializeComponent()
        {
            this.Text = "Croc 2 Archipelago Randomizer — Setup & Auto-Installer";
            this.Size = new Size(680, 620);
            this.MinimumSize = new Size(680, 620);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(30, 34, 45);
            this.ForeColor = Color.FromArgb(235, 238, 245);
            this.Font = new Font("Segoe UI", 9.5f, FontStyle.Regular);

            // 1. Header Banner
            Panel pnlHeader = new Panel
            {
                Dock = DockStyle.Top,
                Height = 85,
                BackColor = Color.FromArgb(22, 25, 34),
                Padding = new Padding(20, 15, 20, 10)
            };

            PictureBox picLogo = new PictureBox
            {
                Size = new Size(58, 58),
                Location = new Point(18, 13),
                SizeMode = PictureBoxSizeMode.Zoom,
                BackColor = Color.Transparent
            };
            byte[] iconBytes = engine.GetPayloadBytes("icon.png");
            if (iconBytes != null)
            {
                try
                {
                    using (MemoryStream ms = new MemoryStream(iconBytes))
                    {
                        picLogo.Image = new Bitmap(ms);
                    }
                }
                catch { }
            }

            Label lblTitle = new Label
            {
                Text = "Croc 2 Archipelago Randomizer",
                Font = new Font("Segoe UI Semibold", 16f, FontStyle.Bold),
                ForeColor = Color.FromArgb(129, 199, 132),
                AutoSize = true,
                Location = new Point(88, 12)
            };

            Label lblSubtitle = new Label
            {
                Text = "One-Click Mod Installer & Setup Wizard — Auto-detects Steam & Archipelago directories",
                Font = new Font("Segoe UI", 9.25f, FontStyle.Regular),
                ForeColor = Color.FromArgb(170, 178, 195),
                AutoSize = true,
                Location = new Point(90, 45)
            };

            pnlHeader.Controls.Add(picLogo);
            pnlHeader.Controls.Add(lblTitle);
            pnlHeader.Controls.Add(lblSubtitle);
            this.Controls.Add(pnlHeader);

            // 2. Main Content Container
            Panel pnlBody = new Panel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(20, 15, 20, 15),
                AutoScroll = true
            };
            this.Controls.Add(pnlBody);
            pnlBody.BringToFront();

            int top = 10;

            // --- Croc 2 Folder Section ---
            Label lblGameHeader = new Label
            {
                Text = "Croc 2 Steam Installation Folder:",
                Font = new Font("Segoe UI Semibold", 10f, FontStyle.Bold),
                Location = new Point(10, top),
                AutoSize = true
            };
            pnlBody.Controls.Add(lblGameHeader);
            top += 26;

            txtGameDir = new TextBox
            {
                Text = engine.Croc2Dir,
                Location = new Point(10, top),
                Width = 510,
                BackColor = Color.FromArgb(42, 47, 61),
                ForeColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle
            };
            txtGameDir.TextChanged += (s, e) => { engine.Croc2Dir = txtGameDir.Text; UpdateValidationStatus(); };
            pnlBody.Controls.Add(txtGameDir);

            Button btnBrowseGame = new Button
            {
                Text = "Browse...",
                Location = new Point(530, top - 2),
                Size = new Size(100, 28),
                BackColor = Color.FromArgb(50, 56, 72),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat
            };
            btnBrowseGame.FlatAppearance.BorderColor = Color.FromArgb(80, 88, 110);
            btnBrowseGame.Click += (s, e) => BrowseGameDir();
            pnlBody.Controls.Add(btnBrowseGame);
            top += 30;

            lblGameStatus = new Label
            {
                Text = "Detecting...",
                Location = new Point(12, top),
                AutoSize = true,
                Font = new Font("Segoe UI", 8.75f)
            };
            pnlBody.Controls.Add(lblGameStatus);
            top += 28;

            // --- Archipelago Folder Section ---
            Label lblApHeader = new Label
            {
                Text = "Archipelago Installation Directory:",
                Font = new Font("Segoe UI Semibold", 10f, FontStyle.Bold),
                Location = new Point(10, top),
                AutoSize = true
            };
            pnlBody.Controls.Add(lblApHeader);
            top += 26;

            txtArchipelagoDir = new TextBox
            {
                Text = engine.ArchipelagoDir,
                Location = new Point(10, top),
                Width = 510,
                BackColor = Color.FromArgb(42, 47, 61),
                ForeColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle
            };
            txtArchipelagoDir.TextChanged += (s, e) => { engine.ArchipelagoDir = txtArchipelagoDir.Text; UpdateValidationStatus(); };
            pnlBody.Controls.Add(txtArchipelagoDir);

            Button btnBrowseAp = new Button
            {
                Text = "Browse...",
                Location = new Point(530, top - 2),
                Size = new Size(100, 28),
                BackColor = Color.FromArgb(50, 56, 72),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat
            };
            btnBrowseAp.FlatAppearance.BorderColor = Color.FromArgb(80, 88, 110);
            btnBrowseAp.Click += (s, e) => BrowseArchipelagoDir();
            pnlBody.Controls.Add(btnBrowseAp);
            top += 30;

            lblArchipelagoStatus = new Label
            {
                Text = "Detecting...",
                Location = new Point(12, top),
                AutoSize = true,
                Font = new Font("Segoe UI", 8.75f)
            };
            pnlBody.Controls.Add(lblArchipelagoStatus);
            top += 28;

            // --- Options Checkboxes ---
            chkDesktopShortcut = new CheckBox
            {
                Text = "Create Desktop Shortcut for Croc 2 Archipelago Launcher",
                Checked = true,
                Location = new Point(12, top),
                AutoSize = true
            };
            chkDesktopShortcut.CheckedChanged += (s, e) => { engine.CreateDesktopShortcut = chkDesktopShortcut.Checked; };
            pnlBody.Controls.Add(chkDesktopShortcut);
            top += 25;

            chkOverwriteConfig = new CheckBox
            {
                Text = "Reset mod config (croc2_mod.ini) to clean default settings",
                Checked = false,
                Location = new Point(12, top),
                AutoSize = true
            };
            chkOverwriteConfig.CheckedChanged += (s, e) => { engine.OverwriteConfig = chkOverwriteConfig.Checked; };
            pnlBody.Controls.Add(chkOverwriteConfig);
            top += 35;

            // --- Action Buttons ---
            btnInstall = new Button
            {
                Text = "🚀  Install / Update Croc 2 AP",
                Location = new Point(10, top),
                Size = new Size(260, 42),
                BackColor = Color.FromArgb(46, 125, 50),
                ForeColor = Color.White,
                Font = new Font("Segoe UI Semibold", 11f, FontStyle.Bold),
                FlatStyle = FlatStyle.Flat,
                Cursor = Cursors.Hand
            };
            btnInstall.FlatAppearance.BorderSize = 0;
            btnInstall.Click += (s, e) => ExecuteInstall();
            pnlBody.Controls.Add(btnInstall);

            btnLaunchLauncher = new Button
            {
                Text = "🎮  Launch Launcher",
                Location = new Point(280, top),
                Size = new Size(180, 42),
                BackColor = Color.FromArgb(33, 150, 243),
                ForeColor = Color.White,
                Font = new Font("Segoe UI Semibold", 10.5f, FontStyle.Bold),
                FlatStyle = FlatStyle.Flat,
                Cursor = Cursors.Hand
            };
            btnLaunchLauncher.FlatAppearance.BorderSize = 0;
            btnLaunchLauncher.Click += (s, e) => LaunchLauncher();
            pnlBody.Controls.Add(btnLaunchLauncher);

            btnOpenFolder = new Button
            {
                Text = "📁 Open Folder",
                Location = new Point(470, top),
                Size = new Size(160, 42),
                BackColor = Color.FromArgb(60, 68, 86),
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 9.5f),
                FlatStyle = FlatStyle.Flat,
                Cursor = Cursors.Hand
            };
            btnOpenFolder.FlatAppearance.BorderSize = 0;
            btnOpenFolder.Click += (s, e) => OpenGameFolder();
            pnlBody.Controls.Add(btnOpenFolder);
            top += 55;

            // --- Installation Log Window ---
            Label lblLog = new Label
            {
                Text = "Installation Log:",
                Font = new Font("Segoe UI Semibold", 9.5f, FontStyle.Bold),
                Location = new Point(10, top),
                AutoSize = true
            };
            pnlBody.Controls.Add(lblLog);
            top += 24;

            txtLog = new TextBox
            {
                Location = new Point(10, top),
                Size = new Size(620, 160),
                Multiline = true,
                ReadOnly = true,
                ScrollBars = ScrollBars.Vertical,
                BackColor = Color.FromArgb(20, 23, 31),
                ForeColor = Color.FromArgb(190, 240, 190),
                Font = new Font("Consolas", 9f),
                BorderStyle = BorderStyle.FixedSingle
            };
            pnlBody.Controls.Add(txtLog);

            AppendLog("Ready. Click '🚀 Install / Update Croc 2 AP' to set up the randomizer.");
        }

        private void UpdateValidationStatus()
        {
            string gameExe = Path.Combine(engine.Croc2Dir ?? "", "croc2.exe");
            if (File.Exists(gameExe))
            {
                lblGameStatus.Text = "✔ Croc 2 executable detected!";
                lblGameStatus.ForeColor = Color.FromArgb(129, 199, 132);
            }
            else
            {
                lblGameStatus.Text = "⚠ croc2.exe not found in this directory. Click Browse to select your Croc 2 folder.";
                lblGameStatus.ForeColor = Color.FromArgb(255, 183, 77);
            }

            if (Directory.Exists(engine.ArchipelagoDir ?? ""))
            {
                lblArchipelagoStatus.Text = "✔ Archipelago directory detected!";
                lblArchipelagoStatus.ForeColor = Color.FromArgb(129, 199, 132);
            }
            else
            {
                lblArchipelagoStatus.Text = "ℹ Archipelago folder not found yet (will be created automatically).";
                lblArchipelagoStatus.ForeColor = Color.FromArgb(144, 202, 249);
            }

            string launcherExe = Path.Combine(engine.Croc2Dir ?? "", "Croc2ArchipelagoLauncher.exe");
            btnLaunchLauncher.Enabled = File.Exists(launcherExe);
        }

        private void BrowseGameDir()
        {
            using (FolderBrowserDialog fbd = new FolderBrowserDialog())
            {
                fbd.Description = "Select your Croc 2 installation folder (containing croc2.exe):";
                fbd.SelectedPath = Directory.Exists(engine.Croc2Dir) ? engine.Croc2Dir : @"C:\Program Files (x86)\Steam\steamapps\common";
                if (fbd.ShowDialog() == DialogResult.OK)
                {
                    txtGameDir.Text = fbd.SelectedPath;
                }
            }
        }

        private void BrowseArchipelagoDir()
        {
            using (FolderBrowserDialog fbd = new FolderBrowserDialog())
            {
                fbd.Description = "Select your Archipelago installation folder:";
                fbd.SelectedPath = Directory.Exists(engine.ArchipelagoDir) ? engine.ArchipelagoDir : @"C:\ProgramData";
                if (fbd.ShowDialog() == DialogResult.OK)
                {
                    txtArchipelagoDir.Text = fbd.SelectedPath;
                }
            }
        }

        private void ExecuteInstall()
        {
            btnInstall.Enabled = false;
            txtLog.Clear();
            AppendLog(string.Format("[{0:HH:mm:ss}] Starting installation...", DateTime.Now));

            bool success = engine.PerformInstall(msg => AppendLog(msg));

            btnInstall.Enabled = true;
            UpdateValidationStatus();

            if (success)
            {
                MessageBox.Show(
                    "Croc 2 Archipelago Randomizer has been successfully installed!\n\n" +
                    "Files placed:\n" +
                    "• Hook & Config: " + engine.Croc2Dir + "\n" +
                    "• Launcher: " + engine.Croc2Dir + "\\Croc2ArchipelagoLauncher.exe\n" +
                    "• APWorld: " + engine.ArchipelagoDir + "\\custom_worlds\\croc2.apworld\n" +
                    "• Player YAML: " + engine.ArchipelagoDir + "\\Players\\Templates\\Croc 2.yaml\n\n" +
                    "You can now launch the Croc 2 AP Launcher directly!",
                    "Installation Complete",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information
                );
            }
            else
            {
                MessageBox.Show(
                    "Installation finished with warnings or errors. Please check the log below.",
                    "Installation Notice",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning
                );
            }
        }

        private void LaunchLauncher()
        {
            string launcherExe = Path.Combine(engine.Croc2Dir ?? "", "Croc2ArchipelagoLauncher.exe");
            if (File.Exists(launcherExe))
            {
                try
                {
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = launcherExe,
                        WorkingDirectory = engine.Croc2Dir
                    });
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Could not start launcher: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
            else
            {
                MessageBox.Show("Launcher executable not found. Please click 'Install / Update Croc 2 AP' first.", "Not Found", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }

        private void OpenGameFolder()
        {
            if (Directory.Exists(engine.Croc2Dir))
            {
                try { Process.Start("explorer.exe", engine.Croc2Dir); } catch { }
            }
        }

        private void AppendLog(string message)
        {
            if (this.InvokeRequired)
            {
                this.Invoke(new Action<string>(AppendLog), message);
                return;
            }

            txtLog.AppendText(message + Environment.NewLine);
        }
    }
}

