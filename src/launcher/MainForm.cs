using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Croc2Launcher
{
    public class MainForm : Form
    {
        // Tab Control
        private TabControl _tabs;
        private TabPage _tabPlay;
        private TabPage _tabOptions;

        // Connection controls (Tab 1)
        private TextBox _txtHost;
        private TextBox _txtPort;
        private TextBox _txtSlot;
        private TextBox _txtPassword;
        private CheckBox _chkRemember;

        // Game Path controls (Tab 1)
        private TextBox _txtGamePath;
        private Button _btnBrowse;
        private Label _lblDetectedStatus;

        // Action buttons (Tab 1)
        private Button _btnLaunchVanilla;
        private Button _btnLaunchSoloRando;
        private Button _btnLaunchAp;
        private Button _btnAttach;
        private Button _btnDisconnect;
        private Button _btnSyncItems;

        // Status badges (Tab 1)
        private Label _lblStatusAp;
        private Label _lblStatusGame;
        private Label _lblStatusHook;
        private Label _lblChecksCount;
        private Label _lblItemsCount;

        // Activity log (Tab 1)
        private RichTextBox _rtbLog;
        private Button _btnClearLog;

        // Randomizer & YAML Options Controls (Tab 2)
        private TextBox _txtRandoSlot;
        private ComboBox _cboGoal;
        private NumericUpDown _numJigsaws;
        private NumericUpDown _numTrophies;
        private ComboBox _cboWhiteCrystals;
        private ComboBox _cboPeteStore;
        private ComboBox _cboDoorShuffle;
        private CheckBox _chkLockDoors;
        private CheckBox _chkAbilities;
        private CheckBox _chkLevers;
        private CheckBox _chkVehicles;
        private CheckBox _chkPlaneGuns;
        private NumericUpDown _numTrapWeight;
        private CheckBox _chkFireTrap;
        private CheckBox _chkDeathLink;
        private Button _btnExportYaml;
        private Button _btnOpenPlayersFolder;
        private Label _lblExportStatus;

        // Auto-updater controls
        private Label _lblVersion;
        private Button _btnCheckUpdates;
        private TextBox _txtGitHubRepo;
        private CheckBox _chkAutoCheckUpdates;
        private Button _btnCheckUpdatesTab2;
        private Label _lblUpdateStatusTab2;
        private string[] _startupArgs;

        // Background timer & client subsystems
        private Timer _pollTimer;
        private readonly ApClient _apClient;
        private readonly HookBridgeClient _hookClient;
        private LauncherSettings _settings;
        private GamePaths _paths;

        private readonly List<ApNetworkItem> _receivedItemsQueue = new List<ApNetworkItem>();
        private int _itemsAppliedCount = 0;
        private int _soloItemsReceivedCount = 0;
        private readonly HashSet<long> _checkedLocations = new HashSet<long>();
        private Dictionary<string, object> _cachedSlotData = null;
        private bool _slotDataConfigured = false;
        private bool _goalReported = false;
        private bool _deathLinkActive = false;
        private readonly List<KeyValuePair<string, string>> _pendingDeathLinkKills = new List<KeyValuePair<string, string>>();

        public MainForm() : this(new string[0])
        {
        }

        public MainForm(string[] args)
        {
            _startupArgs = args ?? new string[0];
            try { this.Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
            InitializeComponent();

            _apClient = new ApClient();
            _hookClient = new HookBridgeClient();

            WireEvents();

            _pollTimer = new Timer();
            _pollTimer.Interval = 150;
            _pollTimer.Tick += OnPollTick;

            LoadSettingsAndDetectGame();

            this.Shown += OnFormShown;
            this.FormClosing += (s, e) => SaveSettings();
        }

        private void InitializeComponent()
        {
            this.Text = "Croc 2 Archipelago Launcher";
            this.Size = new Size(860, 780);
            this.MinimumSize = new Size(820, 720);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Font = new Font("Segoe UI", 9F, FontStyle.Regular, GraphicsUnit.Point);

            _tabs = new TabControl
            {
                Dock = DockStyle.Fill,
                Font = new Font("Segoe UI", 9.5F, FontStyle.Regular, GraphicsUnit.Point)
            };

            _tabPlay = new TabPage("  🎮 Play & Connect  ");
            _tabOptions = new TabPage("  ⚙️ Randomizer Settings & YAML Export  ");

            BuildPlayTab();
            BuildOptionsTab();

            _tabs.TabPages.Add(_tabPlay);
            _tabs.TabPages.Add(_tabOptions);

            this.Controls.Add(_tabs);
        }

        private void BuildPlayTab()
        {
            _tabPlay.AutoScroll = true;

            // Group: Archipelago Connection Details
            var grpConnection = new GroupBox
            {
                Text = "Archipelago Room Details",
                Location = new Point(12, 10),
                Size = new Size(815, 105),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            var lblHost = new Label { Text = "Server Host:", Location = new Point(15, 25), AutoSize = true };
            _txtHost = new TextBox { Location = new Point(95, 22), Size = new Size(200, 23), Text = "archipelago.gg" };

            var lblPort = new Label { Text = "Port:", Location = new Point(310, 25), AutoSize = true };
            _txtPort = new TextBox { Location = new Point(350, 22), Size = new Size(75, 23), Text = "38281" };

            var lblSlot = new Label { Text = "Slot Name:", Location = new Point(445, 25), AutoSize = true };
            _txtSlot = new TextBox { Location = new Point(520, 22), Size = new Size(275, 23), Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right, Text = "WillOfTheVinny" };

            var lblPass = new Label { Text = "Password:", Location = new Point(15, 62), AutoSize = true };
            _txtPassword = new TextBox { Location = new Point(95, 59), Size = new Size(200, 23), UseSystemPasswordChar = true };

            _chkRemember = new CheckBox
            {
                Text = "Remember Settings",
                Location = new Point(315, 61),
                AutoSize = true,
                Checked = true
            };

            _lblVersion = new Label
            {
                Text = "v" + UpdateManager.CurrentVersion,
                Location = new Point(480, 62),
                AutoSize = true,
                ForeColor = Color.DimGray,
                Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };

            _btnCheckUpdates = new Button
            {
                Text = "🔄 Check for Updates",
                Location = new Point(545, 57),
                Size = new Size(150, 27),
                BackColor = Color.FromArgb(240, 244, 248),
                ForeColor = Color.FromArgb(30, 30, 30),
                FlatStyle = FlatStyle.Standard,
                Font = new Font("Segoe UI", 8.5F)
            };
            _btnCheckUpdates.Click += async (s, e) => await CheckForUpdatesAsync(true);

            grpConnection.Controls.AddRange(new Control[] {
                lblHost, _txtHost, lblPort, _txtPort, lblSlot, _txtSlot, lblPass, _txtPassword, _chkRemember,
                _lblVersion, _btnCheckUpdates
            });

            // Group: Croc 2 Game Path Configuration
            var grpGamePath = new GroupBox
            {
                Text = "Croc 2 Game Path Configuration",
                Location = new Point(12, 122),
                Size = new Size(815, 85),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            var lblPath = new Label { Text = "Install Directory:", Location = new Point(15, 24), AutoSize = true };
            _txtGamePath = new TextBox { Location = new Point(125, 21), Size = new Size(585, 23), Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right };
            _txtGamePath.TextChanged += (s, e) => UpdateDetectedPaths();

            _btnBrowse = new Button { Text = "Browse...", Location = new Point(720, 20), Size = new Size(80, 25), Anchor = AnchorStyles.Top | AnchorStyles.Right };
            _btnBrowse.Click += OnBrowseClicked;

            _lblDetectedStatus = new Label
            {
                Text = "Scanning for croc2.exe...",
                Location = new Point(125, 50),
                AutoSize = true,
                ForeColor = Color.DimGray
            };

            grpGamePath.Controls.AddRange(new Control[] { lblPath, _txtGamePath, _btnBrowse, _lblDetectedStatus });

            // Group: Launch & Mode Controls
            var grpActions = new GroupBox
            {
                Text = "Launch & Mode Controls",
                Location = new Point(12, 215),
                Size = new Size(815, 130),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            _btnLaunchVanilla = new Button
            {
                Text = "🎮 Launch Vanilla",
                Location = new Point(15, 25),
                Size = new Size(160, 38),
                BackColor = Color.FromArgb(100, 115, 130),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };
            _btnLaunchVanilla.Click += OnLaunchVanillaClicked;

            _btnLaunchSoloRando = new Button
            {
                Text = "🎲 Launch Solo Rando",
                Location = new Point(185, 25),
                Size = new Size(170, 38),
                BackColor = Color.FromArgb(46, 139, 87),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };
            _btnLaunchSoloRando.Click += OnLaunchSoloRandoClicked;

            _btnLaunchAp = new Button
            {
                Text = "🌐 Play Archipelago",
                Location = new Point(365, 25),
                Size = new Size(170, 38),
                BackColor = Color.FromArgb(0, 120, 215),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };
            _btnLaunchAp.Click += OnLaunchApClicked;

            _btnAttach = new Button
            {
                Text = "🔗 Attach Running",
                Location = new Point(545, 25),
                Size = new Size(140, 38),
                BackColor = Color.FromArgb(50, 150, 90),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };
            _btnAttach.Click += OnAttachClicked;

            _btnDisconnect = new Button
            {
                Text = "⏹ Disconnect",
                Location = new Point(695, 25),
                Size = new Size(105, 38),
                Enabled = false
            };
            _btnDisconnect.Click += OnDisconnectClicked;

            _btnSyncItems = new Button
            {
                Text = "🔄 Sync / Replay Items",
                Location = new Point(655, 75),
                Size = new Size(145, 28),
                Enabled = false
            };
            _btnSyncItems.Click += OnSyncItemsClicked;

            // Badges
            _lblStatusAp = CreateBadge("AP Server: Disconnected", Color.DarkRed, new Point(15, 75));
            _lblStatusGame = CreateBadge("Game Process: Not Detected", Color.DarkRed, new Point(220, 75));
            _lblStatusHook = CreateBadge("Hook TCP (43825): Disconnected", Color.DarkRed, new Point(415, 75));

            _lblChecksCount = new Label { Text = "Checks Sent: 0", Location = new Point(15, 100), AutoSize = true, Font = new Font("Segoe UI", 9F, FontStyle.Bold) };
            _lblItemsCount = new Label { Text = "Items Received: 0", Location = new Point(220, 100), AutoSize = true, Font = new Font("Segoe UI", 9F, FontStyle.Bold) };

            grpActions.Controls.AddRange(new Control[] {
                _btnLaunchVanilla, _btnLaunchSoloRando, _btnLaunchAp, _btnAttach, _btnDisconnect, _btnSyncItems,
                _lblStatusAp, _lblStatusGame, _lblStatusHook,
                _lblChecksCount, _lblItemsCount
            });

            // Group: Activity Log
            var grpLog = new GroupBox
            {
                Text = "Activity Log (Archipelago Sends & Receives)",
                Location = new Point(12, 352),
                Size = new Size(815, 345),
                Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right
            };

            _rtbLog = new RichTextBox
            {
                Location = new Point(15, 22),
                Size = new Size(785, 280),
                Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right,
                ReadOnly = true,
                BackColor = Color.FromArgb(24, 28, 32),
                ForeColor = Color.FromArgb(220, 220, 220),
                Font = new Font("Consolas", 9.5F)
            };

            _btnClearLog = new Button
            {
                Text = "Clear Log",
                Location = new Point(15, 310),
                Size = new Size(90, 26),
                Anchor = AnchorStyles.Bottom | AnchorStyles.Left
            };
            _btnClearLog.Click += (s, e) => _rtbLog.Clear();

            grpLog.Controls.AddRange(new Control[] { _rtbLog, _btnClearLog });

            _tabPlay.Controls.AddRange(new Control[] { grpConnection, grpGamePath, grpActions, grpLog });
        }

        private void BuildOptionsTab()
        {
            _tabOptions.AutoScroll = true;

            int padLeft = 14;
            int width = 810;

            // Header Banner
            var pnlBanner = new Panel
            {
                Location = new Point(padLeft, 10),
                Size = new Size(width, 58),
                BackColor = Color.FromArgb(235, 243, 250),
                BorderStyle = BorderStyle.FixedSingle,
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            var lblTitle = new Label
            {
                Text = "⚙️ Croc 2 Randomizer Options & Archipelago YAML Generator",
                Location = new Point(10, 6),
                AutoSize = true,
                Font = new Font("Segoe UI", 10F, FontStyle.Bold),
                ForeColor = Color.FromArgb(20, 50, 90)
            };

            var lblSubtitle = new Label
            {
                Text = "Configure seed rules, white crystal check density, Pete's store shuffle, door shuffle, abilities and traps.\nExport directly to C:\\ProgramData\\Archipelago\\Players\\<SlotName>.yaml ready for multiworld generation!",
                Location = new Point(10, 26),
                AutoSize = true,
                ForeColor = Color.FromArgb(50, 60, 70),
                Font = new Font("Segoe UI", 8.5F)
            };
            pnlBanner.Controls.AddRange(new Control[] { lblTitle, lblSubtitle });

            // 1. Player Identification Group
            var grpSlot = new GroupBox
            {
                Text = "Player / Slot Name",
                Location = new Point(padLeft, 75),
                Size = new Size(width, 58),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            var lblRandoSlot = new Label { Text = "Slot / Player Name:", Location = new Point(15, 23), AutoSize = true };
            _txtRandoSlot = new TextBox { Location = new Point(140, 20), Size = new Size(240, 23), Text = "WillOfTheVinny" };
            var lblSlotHint = new Label
            {
                Text = "← Used as the file name (<SlotName>.yaml) and in-game peer name for check toasts",
                Location = new Point(390, 23),
                AutoSize = true,
                ForeColor = Color.DimGray
            };
            grpSlot.Controls.AddRange(new Control[] { lblRandoSlot, _txtRandoSlot, lblSlotHint });

            // Keep Slot Names synchronized between Tab 1 and Tab 2
            _txtSlot.TextChanged += (s, e) => { if (_txtRandoSlot.Text != _txtSlot.Text) _txtRandoSlot.Text = _txtSlot.Text; };
            _txtRandoSlot.TextChanged += (s, e) => { if (_txtSlot.Text != _txtRandoSlot.Text) _txtSlot.Text = _txtRandoSlot.Text; };

            // 2. Goal & Win Condition Group
            var grpGoals = new GroupBox
            {
                Text = "Goal & Win Conditions",
                Location = new Point(padLeft, 138),
                Size = new Size(width, 82),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            var lblGoal = new Label { Text = "Seed Goal:", Location = new Point(15, 23), AutoSize = true };
            _cboGoal = new ComboBox
            {
                Location = new Point(95, 20),
                Size = new Size(320, 23),
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            _cboGoal.Items.AddRange(new object[] {
                "Defeat Baron Dante (Dante's Final Fight) [Recommended]",
                "Collect Jigsaw Pieces (Hub Portals)",
                "Trophy Hunt (Village Races & Minigames)",
                "100% Mastery (Dante + 4 Jigsaws + 12 Trophies)"
            });
            _cboGoal.SelectedIndex = 0;

            var lblJigsaws = new Label { Text = "Jigsaw Pieces Req (1-4):", Location = new Point(435, 23), AutoSize = true };
            _numJigsaws = new NumericUpDown
            {
                Location = new Point(585, 20),
                Size = new Size(55, 23),
                Minimum = 1,
                Maximum = 4,
                Value = 4
            };

            var lblTrophies = new Label { Text = "Trophies Req (1-16):", Location = new Point(655, 23), AutoSize = true };
            _numTrophies = new NumericUpDown
            {
                Location = new Point(780, 20),
                Size = new Size(55, 23),
                Minimum = 1,
                Maximum = 16,
                Value = 12
            };

            var lblGoalDesc = new Label
            {
                Text = "Note: Collecting all 5 Colored Crystals in a stage activates that stage's Golden Gobbo check automatically.",
                Location = new Point(15, 52),
                AutoSize = true,
                ForeColor = Color.DarkSlateGray
            };
            grpGoals.Controls.AddRange(new Control[] { lblGoal, _cboGoal, lblJigsaws, _numJigsaws, lblTrophies, _numTrophies, lblGoalDesc });

            // 3. Location Pool & Hub Door Locks Group
            var grpPool = new GroupBox
            {
                Text = "Location Pool, Crystals & Door Shuffling",
                Location = new Point(padLeft, 226),
                Size = new Size(width, 120),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            var lblCrystals = new Label { Text = "White Crystals:", Location = new Point(15, 23), AutoSize = true };
            _cboWhiteCrystals = new ComboBox
            {
                Location = new Point(115, 20),
                Size = new Size(270, 23),
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            _cboWhiteCrystals.Items.AddRange(new object[] {
                "None (Vanilla Currency Only)",
                "50 & 100 Milestones (40 Checks Total)",
                "Every 10 Milestone (200 Checks Total)",
                "All 100 Per Stage (2,000 Checks) [Recommended]"
            });
            _cboWhiteCrystals.SelectedIndex = 3; // all_100

            var lblPete = new Label { Text = "Pete's Store:", Location = new Point(410, 23), AutoSize = true };
            _cboPeteStore = new ComboBox
            {
                Location = new Point(495, 20),
                Size = new Size(300, 23),
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            _cboPeteStore.Items.AddRange(new object[] {
                "Vanilla Store (Pete Sells for Crystals)",
                "Full Shop Shuffle (VIP Pass + Shop Items Randomized) [Default]"
            });
            _cboPeteStore.SelectedIndex = 1; // full shop shuffle

            var lblDoor = new Label { Text = "Door Shuffle:", Location = new Point(15, 57), AutoSize = true };
            _cboDoorShuffle = new ComboBox
            {
                Location = new Point(115, 54),
                Size = new Size(270, 23),
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            _cboDoorShuffle.Items.AddRange(new object[] {
                "Off (Vanilla Doors)",
                "Within Village Hubs [Default / Recommended]",
                "Any Village (Full Cross-Village Chaos)"
            });
            _cboDoorShuffle.SelectedIndex = 1; // within_village

            _chkLockDoors = new CheckBox
            {
                Text = "🔒 Lock Stage Doors behind Level Keys (Sailor Door 1 starts open)",
                Location = new Point(410, 56),
                Size = new Size(390, 22),
                Checked = true
            };

            var lblGobboNote = new Label
            {
                Text = "✓ Normal Gobbos are excluded from check logic to prevent logic breaks; Golden Gobbos remain full progression.",
                Location = new Point(15, 90),
                AutoSize = true,
                ForeColor = Color.DarkOliveGreen
            };
            grpPool.Controls.AddRange(new Control[] { lblCrystals, _cboWhiteCrystals, lblPete, _cboPeteStore, lblDoor, _cboDoorShuffle, _chkLockDoors, lblGobboNote });

            // 4. Abilities & Interactions Group
            var grpAbilities = new GroupBox
            {
                Text = "Abilities & World Interactions",
                Location = new Point(padLeft, 352),
                Size = new Size(width, 78),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            _chkAbilities = new CheckBox
            {
                Text = "Movement Abilities (Butt Stomp, Tail Whack, Claws, Power Gloves)",
                Location = new Point(15, 22),
                Size = new Size(430, 22),
                Checked = true
            };

            _chkLevers = new CheckBox
            {
                Text = "Shuffle Levers (Lever Wrench)",
                Location = new Point(470, 22),
                Size = new Size(220, 22),
                Checked = true
            };

            _chkVehicles = new CheckBox
            {
                Text = "Shuffle Vehicles (Vehicle License)",
                Location = new Point(15, 48),
                Size = new Size(260, 22),
                Checked = true
            };

            _chkPlaneGuns = new CheckBox
            {
                Text = "Shuffle Plane Guns (Airplane Ammo Feed)",
                Location = new Point(470, 48),
                Size = new Size(280, 22),
                Checked = true
            };
            grpAbilities.Controls.AddRange(new Control[] { _chkAbilities, _chkLevers, _chkVehicles, _chkPlaneGuns });

            // 5. Traps Group
            var grpTraps = new GroupBox
            {
                Text = "Traps & Gameplay Modifiers",
                Location = new Point(padLeft, 436),
                Size = new Size(width, 100),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            var lblTrapWeight = new Label { Text = "Trap Frequency %:", Location = new Point(15, 23), AutoSize = true };
            _numTrapWeight = new NumericUpDown
            {
                Location = new Point(135, 20),
                Size = new Size(55, 23),
                Minimum = 0,
                Maximum = 100,
                Value = 15
            };

            _chkFireTrap = new CheckBox
            {
                Text = "Enable Fire Breakout Trap (Instant Heart Down & Knockback)",
                Location = new Point(220, 21),
                Size = new Size(350, 22),
                Checked = true
            };

            _chkDeathLink = new CheckBox
            {
                Text = "Enable DeathLink (Share Deaths)",
                Location = new Point(580, 21),
                Size = new Size(220, 22),
                Checked = false
            };

            var lblTrapsList = new Label
            {
                Text = "Active Traps: Fire Breakout (instant -1 heart & knockback), Tail-Knot (10s), Lead Boots (10s), Ice Physics (10s), Reverse Controls (10s), Darkness (10s), Dantini Ambush (10s), Pete's Crystal Tax.",
                Location = new Point(15, 52),
                AutoSize = true,
                ForeColor = Color.DimGray
            };

            var lblTrapToastNote = new Label
            {
                Text = "All ill effects last a flat 10 seconds and cannot be cleared early. In-game bottom toast HUD handles all alerts.",
                Location = new Point(15, 74),
                AutoSize = true,
                ForeColor = Color.DarkSlateBlue
            };
            grpTraps.Controls.AddRange(new Control[] { lblTrapWeight, _numTrapWeight, _chkFireTrap, _chkDeathLink, lblTrapsList, lblTrapToastNote });

            // 6. Export Actions
            var grpExport = new GroupBox
            {
                Text = "Export & Synchronization",
                Location = new Point(padLeft, 542),
                Size = new Size(width, 115),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            _btnExportYaml = new Button
            {
                Text = "💾 Save & Export YAML to Archipelago Players",
                Location = new Point(15, 24),
                Size = new Size(420, 44),
                BackColor = Color.FromArgb(0, 120, 215),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 10F, FontStyle.Bold)
            };
            _btnExportYaml.Click += (s, e) => ExportYaml();

            _btnOpenPlayersFolder = new Button
            {
                Text = "📂 Open Players Folder",
                Location = new Point(450, 24),
                Size = new Size(180, 44),
                BackColor = Color.FromArgb(70, 80, 95),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 9.5F, FontStyle.Regular)
            };
            _btnOpenPlayersFolder.Click += (s, e) => OpenPlayersFolder();

            _lblExportStatus = new Label
            {
                Text = "Click above to save directly to C:\\ProgramData\\Archipelago\\Players\\<SlotName>.yaml and update croc2_mod.ini",
                Location = new Point(15, 78),
                AutoSize = true,
                ForeColor = Color.FromArgb(60, 60, 60),
                Font = new Font("Segoe UI", 9F, FontStyle.Italic)
            };
            grpExport.Controls.AddRange(new Control[] { _btnExportYaml, _btnOpenPlayersFolder, _lblExportStatus });

            // 7. Auto-Updater & GitHub Repository
            var grpUpdates = new GroupBox
            {
                Text = "Launcher Auto-Updates & GitHub Repository",
                Location = new Point(padLeft, 665),
                Size = new Size(width, 70),
                Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right
            };

            var lblRepo = new Label { Text = "GitHub Repo:", Location = new Point(15, 26), AutoSize = true };
            _txtGitHubRepo = new TextBox { Location = new Point(105, 23), Size = new Size(200, 23), Text = "Vinrix/Croc2AP" };

            _chkAutoCheckUpdates = new CheckBox
            {
                Text = "Auto-check on startup",
                Location = new Point(320, 24),
                AutoSize = true,
                Checked = true
            };

            _btnCheckUpdatesTab2 = new Button
            {
                Text = "🔄 Check Updates Now",
                Location = new Point(485, 20),
                Size = new Size(160, 29),
                BackColor = Color.FromArgb(40, 110, 180),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 8.5F, FontStyle.Bold)
            };
            _btnCheckUpdatesTab2.Click += async (s, e) => await CheckForUpdatesAsync(true);

            _lblUpdateStatusTab2 = new Label
            {
                Text = "Version: v" + UpdateManager.CurrentVersion,
                Location = new Point(660, 26),
                AutoSize = true,
                ForeColor = Color.DimGray,
                Font = new Font("Segoe UI", 8.5F, FontStyle.Italic)
            };

            grpUpdates.Controls.AddRange(new Control[] { lblRepo, _txtGitHubRepo, _chkAutoCheckUpdates, _btnCheckUpdatesTab2, _lblUpdateStatusTab2 });

            _tabOptions.Controls.AddRange(new Control[] {
                pnlBanner, grpSlot, grpGoals, grpPool, grpAbilities, grpTraps, grpExport, grpUpdates
            });
        }

        private void ExportYaml()
        {
            string slotName = (_txtRandoSlot.Text ?? "").Trim();
            if (string.IsNullOrEmpty(slotName))
            {
                MessageBox.Show("Please enter a valid Slot Name / Player Name before exporting.", "Invalid Slot Name", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            _txtSlot.Text = slotName;

            int goal = _cboGoal.SelectedIndex;
            int jigsaws = (int)_numJigsaws.Value;
            int trophies = (int)_numTrophies.Value;
            int whiteCrystals = _cboWhiteCrystals.SelectedIndex;
            int peteStore = _cboPeteStore.SelectedIndex;
            int doorShuffle = _cboDoorShuffle.SelectedIndex;
            bool lockDoors = _chkLockDoors.Checked;
            bool abilities = _chkAbilities.Checked;
            bool levers = _chkLevers.Checked;
            bool vehicles = _chkVehicles.Checked;
            bool planeGuns = _chkPlaneGuns.Checked;
            int trapWeight = (int)_numTrapWeight.Value;
            bool fireTrap = _chkFireTrap.Checked;
            bool deathLink = _chkDeathLink.Checked;

            string wcStr = (whiteCrystals == 0) ? "none" : (whiteCrystals == 1) ? "milestones_50" : (whiteCrystals == 2) ? "milestones_10" : "all_100";
            string psStr = (peteStore == 0) ? "vanilla" : "shuffle_shop_items";
            string dsStr = (doorShuffle == 0) ? "off" : (doorShuffle == 1) ? "within_village" : "any_village";
            string goalStr = (goal == 0) ? "defeat_baron_dante" : (goal == 1) ? "collect_jigsaw_pieces" : (goal == 2) ? "trophy_hunt" : "all_of_the_above";

            var sb = new StringBuilder();
            sb.AppendLine("# Archipelago Multiworld YAML Configuration for Croc 2: Kingdom of the Gobbos");
            sb.AppendLine("# Generated by Croc 2 Archipelago Standalone Launcher");
            sb.AppendLine();
            sb.AppendLine("name: " + slotName);
            sb.AppendLine("description: Croc 2 Multiworld Settings for " + slotName);
            sb.AppendLine("game: Croc 2");
            sb.AppendLine("requires:");
            sb.AppendLine("  version: 0.6.0");
            sb.AppendLine("  game:");
            sb.AppendLine("    Croc 2: 0.1.0");
            sb.AppendLine();
            sb.AppendLine("Croc 2:");
            sb.AppendLine("  progression_balancing: 50");
            sb.AppendLine("  accessibility: full");
            sb.AppendLine();
            sb.AppendLine("  # Seed Settings & Goals");
            sb.AppendLine("  goal:");
            sb.AppendLine("    " + goalStr + ": 50");
            sb.AppendLine("  jigsaw_pieces_required: " + jigsaws);
            sb.AppendLine("  trophies_required: " + trophies);
            sb.AppendLine();
            sb.AppendLine("  # Colored Crystals Mode: shuffled into multiworld pool (5 crystals activate Golden Gobbo)");
            sb.AppendLine("  colored_crystals_mode:");
            sb.AppendLine("    shuffled_in_pool: 50");
            sb.AppendLine();
            sb.AppendLine("  # White Crystals checks granularity");
            sb.AppendLine("  shuffle_white_crystals:");
            sb.AppendLine("    " + wcStr + ": 50");
            sb.AppendLine();
            sb.AppendLine("  # Stage Door locks & Door Shuffle");
            sb.AppendLine("  lock_level_access: " + (lockDoors ? "true" : "false"));
            sb.AppendLine("  door_shuffle:");
            sb.AppendLine("    " + dsStr + ": 50");
            sb.AppendLine();
            sb.AppendLine("  # Abilities & Access Locks");
            sb.AppendLine("  shuffle_abilities: " + (abilities ? "true" : "false"));
            sb.AppendLine("  shuffle_levers: " + (levers ? "true" : "false"));
            sb.AppendLine("  shuffle_vehicles: " + (vehicles ? "true" : "false"));
            sb.AppendLine("  shuffle_plane_guns: " + (planeGuns ? "true" : "false"));
            sb.AppendLine("  shuffle_pete_store:");
            sb.AppendLine("    " + psStr + ": 50");
            sb.AppendLine();
            sb.AppendLine("  # Traps");
            sb.AppendLine("  trap_weight: " + trapWeight);
            sb.AppendLine("  fire_breakout_trap: " + (fireTrap ? "true" : "false"));
            sb.AppendLine();
            sb.AppendLine("  death_link: " + (deathLink ? "true" : "false"));

            string apPlayersDir = @"C:\ProgramData\Archipelago\Players";
            try
            {
                if (!Directory.Exists(apPlayersDir))
                {
                    Directory.CreateDirectory(apPlayersDir);
                }
                string yamlFilePath = Path.Combine(apPlayersDir, slotName + ".yaml");
                File.WriteAllText(yamlFilePath, sb.ToString(), Encoding.UTF8);

                // Synchronize croc2_mod.ini in local game directory for offline solo play
                SaveToLocalModIni(slotName, goal, jigsaws, trophies, whiteCrystals, peteStore, doorShuffle, lockDoors, abilities, levers, vehicles, planeGuns, trapWeight, fireTrap, deathLink);

                SaveSettings();

                Log("Exported Archipelago YAML: " + yamlFilePath, Color.LimeGreen);
                _lblExportStatus.Text = "Saved: " + yamlFilePath;
                _lblExportStatus.ForeColor = Color.DarkGreen;

                MessageBox.Show(
                    "Successfully exported Archipelago player configuration!\n\n" +
                    "File: " + yamlFilePath + "\n\n" +
                    "This file is placed directly in Archipelago's Players folder and is ready for Multiworld generation!\n" +
                    "Your settings have also been synchronized to croc2_mod.ini for offline randomizer play.",
                    "Archipelago YAML Exported",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information
                );
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to write YAML file:\n" + ex.Message, "Export Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void OpenPlayersFolder()
        {
            string dir = @"C:\ProgramData\Archipelago\Players";
            try
            {
                if (!Directory.Exists(dir))
                {
                    Directory.CreateDirectory(dir);
                }
                Process.Start("explorer.exe", dir);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Could not open folder:\n" + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void SaveToLocalModIni(string slotName, int goal, int jigsaws, int trophies, int whiteCrystals, int peteStore, int doorShuffle, bool lockDoors, bool abilities, bool levers, bool vehicles, bool planeGuns, int trapWeight, bool fireTrap, bool deathLink)
        {
            try
            {
                string iniPath = _paths != null ? _paths.IniPath : null;
                if (string.IsNullOrEmpty(iniPath) || !File.Exists(iniPath))
                {
                    if (!string.IsNullOrEmpty(_settings.Croc2Path))
                    {
                        iniPath = Path.Combine(_settings.Croc2Path, "croc2_mod.ini");
                    }
                }
                if (string.IsNullOrEmpty(iniPath)) return;

                var sb = new StringBuilder();
                sb.AppendLine("[ArchipelagoMod]");
                sb.AppendLine("enabled = true");
                sb.AppendLine("port = 43825");
                sb.AppendLine("toggle_hotkey_vk = 0x79");
                sb.AppendLine("menu_hotkey_vk = 0x78");
                sb.AppendLine("fire_trap_tick_ms = 1500");
                sb.AppendLine();
                sb.AppendLine("[ServerConnection]");
                sb.AppendLine("server_host = " + (_txtHost.Text.Trim()));
                sb.AppendLine("server_port = " + (_txtPort.Text.Trim()));
                sb.AppendLine("slot_name = " + slotName);
                sb.AppendLine("password = " + (_txtPassword.Text));
                sb.AppendLine();
                sb.AppendLine("[RandomizerSettings]");
                sb.AppendLine("lock_level_access = " + (lockDoors ? "true" : "false"));
                sb.AppendLine("shuffle_white_crystals = " + whiteCrystals);
                sb.AppendLine("shuffle_pete_store = " + peteStore);
                sb.AppendLine("shuffle_abilities = " + (abilities ? "true" : "false"));
                sb.AppendLine("shuffle_levers = " + (levers ? "true" : "false"));
                sb.AppendLine("shuffle_vehicles = " + (vehicles ? "true" : "false"));
                sb.AppendLine("shuffle_plane_guns = " + (planeGuns ? "true" : "false"));
                sb.AppendLine("door_shuffle = " + doorShuffle);
                sb.AppendLine("goal = " + goal);
                sb.AppendLine("jigsaw_pieces_required = " + jigsaws);
                sb.AppendLine("trophies_required = " + trophies);
                sb.AppendLine("trap_weight = " + trapWeight);
                sb.AppendLine("fire_breakout_trap = " + (fireTrap ? "true" : "false"));
                sb.AppendLine("death_link = " + (deathLink ? "true" : "false"));

                File.WriteAllText(iniPath, sb.ToString(), Encoding.UTF8);
                Log("Synchronized settings to " + iniPath, Color.CornflowerBlue);
            }
            catch { }
        }

        private Label CreateBadge(string text, Color color, Point loc)
        {
            return new Label
            {
                Text = text,
                Location = loc,
                AutoSize = true,
                ForeColor = color,
                Font = new Font("Segoe UI", 9F, FontStyle.Bold)
            };
        }

        private void WireEvents()
        {
            _apClient.OnLog += (msg, isErr) => Log("[Archipelago] " + msg, isErr ? Color.Salmon : Color.LightGray);
            _apClient.OnStatusChanged += status =>
            {
                SafeInvoke(() =>
                {
                    bool isConn = _apClient.IsConnected;
                    _lblStatusAp.Text = "AP Server: " + status;
                    _lblStatusAp.ForeColor = isConn ? Color.DarkGreen : Color.DarkRed;
                    _btnDisconnect.Enabled = isConn || _hookClient.IsConnected;
                    _btnSyncItems.Enabled = isConn && _hookClient.IsConnected;
                });
            };

            _apClient.OnConnected += slotData =>
            {
                _cachedSlotData = slotData;
                _slotDataConfigured = false;
                _goalReported = false;
                SafeInvoke(() =>
                {
                    LogFormatted("[Archipelago] Room Joined successfully!", Color.LimeGreen);
                    if (_hookClient.IsConnected && _apClient.SlotToPlayerName.Count > 0)
                    {
                        var names = new List<string>(_apClient.SlotToPlayerName.Values);
                        Task.Run(() => _hookClient.SetYamlPlayersAsync(names));
                    }
                });
            };

            _apClient.OnDeathLinkReceived += (source, cause) =>
            {
                SafeInvoke(() =>
                {
                    if (!_deathLinkActive) return;
                    _pendingDeathLinkKills.Add(new KeyValuePair<string, string>(source, cause));
                    LogFormatted("💀 [DeathLink] " + source + (string.IsNullOrEmpty(cause) ? "" : ": " + cause), Color.OrangeRed);
                });
            };

            _apClient.OnItemsReceived += (items, index) =>
            {
                SafeInvoke(() =>
                {
                    // "index" is where this batch starts in the full received-items list. On every
                    // (re)connect the server resends everything from index 0, so only append items we
                    // don't already have; otherwise the queue doubles and every item is applied twice.
                    int position = index;
                    foreach (var item in items)
                    {
                        if (position >= _receivedItemsQueue.Count)
                        {
                            _receivedItemsQueue.Add(item);
                        }
                        position++;
                    }
                    _lblItemsCount.Text = "Items Received: " + _receivedItemsQueue.Count;
                });
            };

            _apClient.OnItemSent += info =>
            {
                SafeInvoke(() =>
                {
                    string itemName = Croc2Data.GetItemName(info.ItemId);
                    string senderName = _apClient.GetPlayerName(info.FinderSlot);
                    string receiverName = _apClient.GetPlayerName(info.ReceivingSlot);
                    string locName = Croc2Data.GetLocationName(info.LocationId);
                    string importance = Croc2Data.GetItemImportance(info.ItemId, info.Flags);

                    if (info.FinderSlot == _apClient.OurSlot && info.ReceivingSlot != _apClient.OurSlot)
                    {
                        LogItemEvent(true, itemName, importance, senderName, receiverName, locName);

                        if (_hookClient.IsConnected)
                        {
                            Task.Run(() => _hookClient.NotifyItemSentAsync(itemName, receiverName, locName, importance));
                        }
                    }
                });
            };
        }

        private void LoadSettingsAndDetectGame()
        {
            _settings = SettingsManager.Load();
            _txtHost.Text = _settings.Host;
            _txtPort.Text = _settings.Port.ToString();
            _txtSlot.Text = _settings.SlotName;
            _txtRandoSlot.Text = _settings.SlotName;
            _txtPassword.Text = _settings.Password;
            _chkRemember.Checked = _settings.RememberSettings;

            // Load Randomizer Options
            if (_settings.Goal >= 0 && _settings.Goal < _cboGoal.Items.Count)
                _cboGoal.SelectedIndex = _settings.Goal;
            if (_settings.JigsawPiecesRequired >= 1 && _settings.JigsawPiecesRequired <= 4)
                _numJigsaws.Value = _settings.JigsawPiecesRequired;
            if (_settings.TrophiesRequired >= 1 && _settings.TrophiesRequired <= 16)
                _numTrophies.Value = _settings.TrophiesRequired;
            if (_settings.WhiteCrystals >= 0 && _settings.WhiteCrystals < _cboWhiteCrystals.Items.Count)
                _cboWhiteCrystals.SelectedIndex = _settings.WhiteCrystals;
            if (_settings.PeteStore >= 0 && _settings.PeteStore < _cboPeteStore.Items.Count)
                _cboPeteStore.SelectedIndex = _settings.PeteStore;
            if (_settings.DoorShuffle >= 0 && _settings.DoorShuffle < _cboDoorShuffle.Items.Count)
                _cboDoorShuffle.SelectedIndex = _settings.DoorShuffle;

            _chkLockDoors.Checked = _settings.LockStageDoors;
            _chkAbilities.Checked = _settings.ShuffleAbilities;
            _chkLevers.Checked = _settings.ShuffleLevers;
            _chkVehicles.Checked = _settings.ShuffleVehicles;
            _chkPlaneGuns.Checked = _settings.ShufflePlaneGuns;

            if (_settings.TrapWeight >= 0 && _settings.TrapWeight <= 100)
                _numTrapWeight.Value = _settings.TrapWeight;
            _chkFireTrap.Checked = _settings.FireBreakoutTrap;
            _chkDeathLink.Checked = _settings.DeathLink;

            _txtGamePath.Text = !string.IsNullOrEmpty(_settings.Croc2Path) ? _settings.Croc2Path : "";
            if (_txtGitHubRepo != null) _txtGitHubRepo.Text = !string.IsNullOrEmpty(_settings.GitHubRepo) ? _settings.GitHubRepo : UpdateManager.DefaultRepo;
            if (_chkAutoCheckUpdates != null) _chkAutoCheckUpdates.Checked = _settings.AutoCheckUpdates;

            UpdateDetectedPaths();
        }

        private void SaveSettings()
        {
            if (!_chkRemember.Checked) return;
            _settings.Host = _txtHost.Text.Trim();
            int p;
            if (int.TryParse(_txtPort.Text.Trim(), out p)) _settings.Port = p;
            _settings.SlotName = _txtSlot.Text.Trim();
            _settings.Password = _txtPassword.Text;
            _settings.Croc2Path = _txtGamePath.Text.Trim();
            _settings.RememberSettings = _chkRemember.Checked;

            if (_txtGitHubRepo != null) _settings.GitHubRepo = _txtGitHubRepo.Text.Trim();
            if (_chkAutoCheckUpdates != null) _settings.AutoCheckUpdates = _chkAutoCheckUpdates.Checked;

            _settings.Goal = _cboGoal.SelectedIndex;
            _settings.JigsawPiecesRequired = (int)_numJigsaws.Value;
            _settings.TrophiesRequired = (int)_numTrophies.Value;
            _settings.WhiteCrystals = _cboWhiteCrystals.SelectedIndex;
            _settings.PeteStore = _cboPeteStore.SelectedIndex;
            _settings.DoorShuffle = _cboDoorShuffle.SelectedIndex;
            _settings.LockStageDoors = _chkLockDoors.Checked;
            _settings.ShuffleAbilities = _chkAbilities.Checked;
            _settings.ShuffleLevers = _chkLevers.Checked;
            _settings.ShuffleVehicles = _chkVehicles.Checked;
            _settings.ShufflePlaneGuns = _chkPlaneGuns.Checked;
            _settings.TrapWeight = (int)_numTrapWeight.Value;
            _settings.FireBreakoutTrap = _chkFireTrap.Checked;
            _settings.DeathLink = _chkDeathLink.Checked;

            SettingsManager.Save(_settings);
        }

        private async void OnFormShown(object sender, EventArgs e)
        {
            if (_startupArgs != null && Array.IndexOf(_startupArgs, "--updated") >= 0)
            {
                Log("🎉 Croc 2 Archipelago Launcher updated successfully to v" + UpdateManager.CurrentVersion + "!", Color.LimeGreen);
                MessageBox.Show(
                    "Croc 2 Archipelago Launcher has been updated to version " + UpdateManager.CurrentVersion + "!",
                    "Update Complete",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information
                );
            }
            else if (_settings != null && _settings.AutoCheckUpdates)
            {
                await CheckForUpdatesAsync(false);
            }
        }

        private async Task CheckForUpdatesAsync(bool isManual)
        {
            try
            {
                if (isManual)
                {
                    Log("Checking for launcher updates from GitHub...", Color.CornflowerBlue);
                    if (_btnCheckUpdates != null) _btnCheckUpdates.Enabled = false;
                    if (_btnCheckUpdatesTab2 != null) _btnCheckUpdatesTab2.Enabled = false;
                }

                string repo = (_txtGitHubRepo != null && !string.IsNullOrEmpty(_txtGitHubRepo.Text.Trim()))
                    ? _txtGitHubRepo.Text.Trim()
                    : (!string.IsNullOrEmpty(_settings.GitHubRepo) ? _settings.GitHubRepo : UpdateManager.DefaultRepo);

                UpdateInfo info = await UpdateManager.CheckForUpdatesAsync(repo);

                if (info.UpdateAvailable)
                {
                    string msg = "A new version of Croc 2 Archipelago Launcher is available!\n\n" +
                                 "Current version: v" + info.CurrentVersion + "\n" +
                                 "Latest version: v" + info.LatestVersion + "\n\n" +
                                 (!string.IsNullOrEmpty(info.ReleaseNotes) ? ("What's new:\n" + info.ReleaseNotes + "\n\n") : "") +
                                 "Would you like to download and install this update now?";

                    Log("✨ New version v" + info.LatestVersion + " is available! (Current: v" + info.CurrentVersion + ")", Color.Gold);

                    if (_lblUpdateStatusTab2 != null)
                    {
                        _lblUpdateStatusTab2.Text = "Update available: v" + info.LatestVersion;
                        _lblUpdateStatusTab2.ForeColor = Color.OrangeRed;
                    }

                    DialogResult res = MessageBox.Show(msg, "Update Available", MessageBoxButtons.YesNo, MessageBoxIcon.Information);
                    if (res == DialogResult.Yes)
                    {
                        Log("Starting update installation...", Color.CornflowerBlue);
                        bool applied = await UpdateManager.DownloadAndApplyUpdateAsync(info, _settings.Croc2Path, progress => Log(progress, Color.Orange));
                        if (applied)
                        {
                            Application.Exit();
                        }
                    }
                }
                else
                {
                    if (!string.IsNullOrEmpty(info.ErrorMessage))
                    {
                        if (isManual)
                        {
                            Log("Update check notice: " + info.ErrorMessage, Color.DimGray);
                            MessageBox.Show("Could not check for updates:\n" + info.ErrorMessage, "Update Check", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        }
                    }
                    else
                    {
                        if (isManual)
                        {
                            Log("Launcher is up to date (v" + UpdateManager.CurrentVersion + ").", Color.LimeGreen);
                            MessageBox.Show("You are running the latest version of Croc 2 Archipelago Launcher (v" + UpdateManager.CurrentVersion + ").", "Up to Date", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        }
                        if (_lblUpdateStatusTab2 != null)
                        {
                            _lblUpdateStatusTab2.Text = "Up to date (v" + UpdateManager.CurrentVersion + ")";
                            _lblUpdateStatusTab2.ForeColor = Color.Green;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                if (isManual)
                {
                    Log("Update check error: " + ex.Message, Color.Salmon);
                }
            }
            finally
            {
                if (_btnCheckUpdates != null) _btnCheckUpdates.Enabled = true;
                if (_btnCheckUpdatesTab2 != null) _btnCheckUpdatesTab2.Enabled = true;
            }
        }

        private void UpdateDetectedPaths()
        {
            string dir = _txtGamePath.Text.Trim();
            _paths = GameDetector.Detect(dir);

            if (_paths.HasExe)
            {
                if (_paths.HasHookDll)
                {
                    _lblDetectedStatus.Text = "✓ Ready: croc2.exe & version.dll found in " + _paths.BaseDir;
                    _lblDetectedStatus.ForeColor = Color.DarkGreen;
                }
                else
                {
                    _lblDetectedStatus.Text = "⚠ Found croc2.exe, but version.dll (mod hook) is missing. Vanilla mode only.";
                    _lblDetectedStatus.ForeColor = Color.DarkOrange;
                }
            }
            else
            {
                _lblDetectedStatus.Text = "✗ croc2.exe not found in this folder. Click 'Browse...' to locate it.";
                _lblDetectedStatus.ForeColor = Color.DarkRed;
            }
        }

        private void OnBrowseClicked(object sender, EventArgs e)
        {
            using (var fbd = new FolderBrowserDialog())
            {
                fbd.Description = "Select your Croc 2 Game Install Directory (containing croc2.exe)";
                if (Directory.Exists(_txtGamePath.Text)) fbd.SelectedPath = _txtGamePath.Text;
                if (fbd.ShowDialog() == DialogResult.OK)
                {
                    _txtGamePath.Text = fbd.SelectedPath;
                    UpdateDetectedPaths();
                    SaveSettings();
                }
            }
        }

        private void OnLaunchVanillaClicked(object sender, EventArgs e)
        {
            UpdateDetectedPaths();
            if (!_paths.HasExe)
            {
                MessageBox.Show("croc2.exe was not found. Please browse to your Croc 2 install directory.", "Game Not Found", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                LogFormatted("Launching Croc 2 in 100% Vanilla Mode...", Color.Yellow);
                GameDetector.LaunchGame(_paths, true);
                LogFormatted("Croc 2 Vanilla started successfully.", Color.LightGreen);
                _pollTimer.Start();
            }
            catch (Exception ex)
            {
                Log("Failed to launch game: " + ex.Message, Color.Salmon);
            }
        }

        private void OnLaunchSoloRandoClicked(object sender, EventArgs e)
        {
            UpdateDetectedPaths();
            if (!_paths.HasExe)
            {
                MessageBox.Show("croc2.exe was not found. Please browse to your Croc 2 install directory.", "Game Not Found", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                SaveToLocalModIni(
                    _txtRandoSlot.Text.Trim(),
                    _cboGoal.SelectedIndex,
                    (int)_numJigsaws.Value,
                    (int)_numTrophies.Value,
                    _cboWhiteCrystals.SelectedIndex,
                    _cboPeteStore.SelectedIndex,
                    _cboDoorShuffle.SelectedIndex,
                    _chkLockDoors.Checked,
                    _chkAbilities.Checked,
                    _chkLevers.Checked,
                    _chkVehicles.Checked,
                    _chkPlaneGuns.Checked,
                    (int)_numTrapWeight.Value,
                    _chkFireTrap.Checked,
                    _chkDeathLink.Checked
                );

                LogFormatted("Launching Croc 2 in Standalone Randomizer Mode (Offline Seed)...", Color.DeepSkyBlue);
                _soloItemsReceivedCount = 0;
                _checkedLocations.Clear();
                _lblChecksCount.Text = "Checks: 0";
                _lblItemsCount.Text = "Items Received: 0";
                GameDetector.LaunchGame(_paths, false);
                LogFormatted("Croc 2 Randomizer started. In-game bottom toasts and rando mechanics active!", Color.LightGreen);
                _pollTimer.Start();
            }
            catch (Exception ex)
            {
                Log("Failed to launch game: " + ex.Message, Color.Salmon);
            }
        }

        private async void OnLaunchApClicked(object sender, EventArgs e)
        {
            SaveSettings();
            UpdateDetectedPaths();
            if (!_paths.HasExe)
            {
                MessageBox.Show("croc2.exe was not found. Please browse to your Croc 2 install directory.", "Game Not Found", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                SaveToLocalModIni(
                    _txtSlot.Text.Trim(),
                    _cboGoal.SelectedIndex,
                    (int)_numJigsaws.Value,
                    (int)_numTrophies.Value,
                    _cboWhiteCrystals.SelectedIndex,
                    _cboPeteStore.SelectedIndex,
                    _cboDoorShuffle.SelectedIndex,
                    _chkLockDoors.Checked,
                    _chkAbilities.Checked,
                    _chkLevers.Checked,
                    _chkVehicles.Checked,
                    _chkPlaneGuns.Checked,
                    (int)_numTrapWeight.Value,
                    _chkFireTrap.Checked,
                    _chkDeathLink.Checked
                );

                LogFormatted("Starting Croc 2 with Archipelago Randomizer Hook...", Color.Cyan);
                GameDetector.LaunchGame(_paths, false);

                await ConnectToApServerAsync();

                _pollTimer.Start();
            }
            catch (Exception ex)
            {
                Log("Launch & Connect Error: " + ex.Message, Color.Salmon);
            }
        }

        private async void OnAttachClicked(object sender, EventArgs e)
        {
            SaveSettings();
            try
            {
                await ConnectToApServerAsync();
                _pollTimer.Start();
            }
            catch (Exception ex)
            {
                Log("Attach Error: " + ex.Message, Color.Salmon);
            }
        }

        private async Task ConnectToApServerAsync()
        {
            string host = _txtHost.Text.Trim();
            int port;
            if (!int.TryParse(_txtPort.Text.Trim(), out port)) port = 38281;
            string slot = _txtSlot.Text.Trim();
            string pass = _txtPassword.Text;

            LogFormatted("Connecting to Archipelago Server at " + host + ":" + port + " as '" + slot + "'...", Color.LightSkyBlue);
            await _apClient.ConnectAsync(host, port, slot, pass);
        }

        private async void OnDisconnectClicked(object sender, EventArgs e)
        {
            _pollTimer.Stop();
            await _apClient.DisconnectAsync();
            _hookClient.Disconnect();

            _btnDisconnect.Enabled = false;
            _btnSyncItems.Enabled = false;
            _lblStatusAp.Text = "AP Server: Disconnected";
            _lblStatusAp.ForeColor = Color.DarkRed;
            _lblStatusHook.Text = "Hook TCP (43825): Disconnected";
            _lblStatusHook.ForeColor = Color.DarkRed;
            LogFormatted("Disconnected from Archipelago.", Color.Orange);
        }

        private async void OnSyncItemsClicked(object sender, EventArgs e)
        {
            if (!_hookClient.IsConnected) return;

            LogFormatted("Re-synchronizing all received items with game...", Color.Yellow);
            int count = 0;
            for (int i = 0; i < _receivedItemsQueue.Count; i++)
            {
                var item = _receivedItemsQueue[i];
                string itemName = Croc2Data.GetItemName(item.ItemId);
                string senderName = _apClient.GetPlayerName(item.Player);
                string locName = Croc2Data.GetLocationName(item.LocationId);
                string importance = Croc2Data.GetItemImportance(item.ItemId, item.Flags);

                await _hookClient.GiveItemAsync((int)item.ItemId, senderName, locName, importance, i + 1);
                count++;
            }
            LogFormatted("Re-synced " + count + " items to Croc 2.", Color.LightGreen);
        }

        private async void OnPollTick(object sender, EventArgs e)
        {
            bool gameRunning = GameDetector.FindRunningGame() != null;
            _lblStatusGame.Text = gameRunning ? "Game Process: Running (croc2.exe)" : "Game Process: Not Detected";
            _lblStatusGame.ForeColor = gameRunning ? Color.DarkGreen : Color.DarkRed;

            if (gameRunning && !_hookClient.IsConnected)
            {
                bool connected = await _hookClient.ConnectAsync();
                _lblStatusHook.Text = connected ? "Hook TCP (43825): Connected" : "Hook TCP (43825): Waiting...";
                _lblStatusHook.ForeColor = connected ? Color.DarkGreen : Color.DarkGoldenrod;
                if (connected)
                {
                    LogFormatted("[Hook] Connected to Croc 2 In-Game Bridge on port 43825!", Color.LimeGreen);

                    if (_apClient.IsConnected && _apClient.SlotToPlayerName.Count > 0)
                    {
                        var names = new List<string>(_apClient.SlotToPlayerName.Values);
                        await _hookClient.SetYamlPlayersAsync(names);
                    }
                }
            }

            if (_hookClient.IsConnected && _cachedSlotData != null && !_slotDataConfigured)
            {
                _slotDataConfigured = true;
                await _hookClient.ConfigureSessionAsync(_cachedSlotData);
                LogFormatted("[Hook] Configured session using Archipelago Slot Data.", Color.MediumSpringGreen);

                object dl;
                _deathLinkActive = _cachedSlotData.TryGetValue("death_link", out dl) && dl != null && Convert.ToInt32(dl) != 0;
                await _hookClient.SetDeathLinkAsync(_deathLinkActive);
                if (_apClient.IsConnected) await _apClient.SetDeathLinkTagAsync(_deathLinkActive);

                // Resend checks the save already holds, including ones made while the launcher was closed
                var stored = await _hookClient.GetClaimedLocationsAsync();
                var resend = new List<long>();
                foreach (long id in stored)
                {
                    if (_checkedLocations.Add(id)) resend.Add(id);
                }
                if (resend.Count > 0 && _apClient.IsConnected)
                {
                    await _apClient.SendLocationChecksAsync(resend);
                    LogFormatted("[Hook] Re-sent " + resend.Count + " check(s) stored in the save.", Color.MediumSpringGreen);
                }
            }

            if (_hookClient.IsConnected && _itemsAppliedCount < _receivedItemsQueue.Count)
            {
                while (_itemsAppliedCount < _receivedItemsQueue.Count)
                {
                    int itemIndex = _itemsAppliedCount; // 0-based position in the server's item list
                    var item = _receivedItemsQueue[itemIndex];
                    _itemsAppliedCount++;

                    string itemName = Croc2Data.GetItemName(item.ItemId);
                    string senderName = _apClient.GetPlayerName(item.Player);
                    string locName = Croc2Data.GetLocationName(item.LocationId);
                    string importance = Croc2Data.GetItemImportance(item.ItemId, item.Flags);

                    LogItemEvent(false, itemName, importance, senderName, _apClient.OurSlotName, locName);

                    await _hookClient.GiveItemAsync((int)item.ItemId, senderName, locName, importance, itemIndex);
                }
            }

            if (_hookClient.IsConnected)
            {
                var events = await _hookClient.PollEventsDetailedAsync();
                if (events != null && events.Count > 0)
                {
                    var checksToSend = new List<long>();
                    foreach (var ev in events)
                    {
                        string evType = ev.ContainsKey("event") ? ev["event"].ToString() : "";
                        if (evType == "location_checked")
                        {
                            long locId = ev.ContainsKey("location_id") ? Convert.ToInt64(ev["location_id"]) : 0;
                            string desc = ev.ContainsKey("description") ? ev["description"].ToString() : ("Location #" + locId);
                            if (locId > 0 && !_checkedLocations.Contains(locId))
                            {
                                _checkedLocations.Add(locId);
                                if (_apClient.IsConnected)
                                {
                                    checksToSend.Add(locId);
                                }
                            }
                            LogFormatted("⭐ [CHECK] " + desc, Color.LimeGreen);
                            _lblChecksCount.Text = "Checks: " + _checkedLocations.Count;
                        }
                        else if (evType == "death")
                        {
                            if (_deathLinkActive && _apClient.IsConnected)
                            {
                                string where = ev.ContainsKey("map") && ev["map"] != null ? ev["map"].ToString() : "";
                                await _apClient.SendDeathLinkAsync(_apClient.OurSlotName + " was squashed in Croc 2" +
                                                                   (string.IsNullOrEmpty(where) ? "" : " (" + where + ")"));
                                LogFormatted("💀 [DeathLink] Sent your death to the multiworld.", Color.OrangeRed);
                            }
                        }
                        else if (evType == "item_received")
                        {
                            string itemName = ev.ContainsKey("item_name") ? ev["item_name"].ToString() : "Unknown Item";
                            string senderName = ev.ContainsKey("sender") ? ev["sender"].ToString() : "Croc";
                            string locName = ev.ContainsKey("location") ? ev["location"].ToString() : "";
                            string importance = "progression";
                            if (ev.ContainsKey("importance"))
                            {
                                int impVal = Convert.ToInt32(ev["importance"]);
                                if (impVal == 0) importance = "progression";
                                else if (impVal == 1) importance = "useful";
                                else if (impVal == 2) importance = "filler";
                                else if (impVal == 3) importance = "trap";
                            }
                            LogItemEvent(false, itemName, importance, senderName, "Croc", locName);
                            if (!_apClient.IsConnected)
                            {
                                _soloItemsReceivedCount++;
                                _lblItemsCount.Text = "Items Received: " + _soloItemsReceivedCount;
                            }
                        }
                        else if (evType == "key_received")
                        {
                            string levelName = ev.ContainsKey("level_name") ? ev["level_name"].ToString() : "Stage";
                            LogFormatted("🗝️ [KEY] Unlocked " + levelName + " Key!", Color.Gold);
                        }
                        else if (evType == "door_locked")
                        {
                            string levelName = ev.ContainsKey("level_name") ? ev["level_name"].ToString() : "Stage";
                            LogFormatted("🔒 [LOCKED] " + levelName + " is locked! Stage Key required.", Color.Crimson);
                        }
                        else if (evType == "vehicle_required")
                        {
                            string levelName = ev.ContainsKey("level_name") ? ev["level_name"].ToString() : "Vehicle Stage";
                            LogFormatted("🚗 [VEHICLE] " + levelName + " requires Vehicle License!", Color.OrangeRed);
                        }
                        else if (evType == "climb_blocked")
                        {
                            LogFormatted("🧗 [ABILITY] Climbing surface requires Climbing Claws!", Color.OrangeRed);
                        }
                        else if (evType == "door_entered")
                        {
                            string origName = ev.ContainsKey("orig_name") ? ev["orig_name"].ToString() : "";
                            string targetName = ev.ContainsKey("target_name") ? ev["target_name"].ToString() : "";
                            LogFormatted("🚪 [DOOR] Shuffled: " + origName + " -> " + targetName, Color.DeepSkyBlue);
                        }
                        else if (evType == "trap_applied")
                        {
                            string desc = ev.ContainsKey("description") ? ev["description"].ToString() : (ev.ContainsKey("trap_type") ? ev["trap_type"].ToString() : "Trap Triggered");
                            LogFormatted("⚠️ [TRAP] " + desc, Color.Red);
                        }
                        else if (evType == "golden_gobbo_activated")
                        {
                            string wad = ev.ContainsKey("wad") ? ev["wad"].ToString() : "";
                            LogFormatted("🏆 [GOLDEN GOBBO] 5/5 Crystals collected! Golden Gobbo activated for " + wad + "!", Color.Gold);
                        }
                    }

                    if (checksToSend.Count > 0 && _apClient.IsConnected)
                    {
                        await _apClient.SendLocationChecksAsync(checksToSend);
                    }

                    while (_pendingDeathLinkKills.Count > 0 && _hookClient.IsConnected)
                    {
                        var kill = _pendingDeathLinkKills[0];
                        if (!await _hookClient.DeathLinkKillAsync(kill.Key, kill.Value)) break;
                        _pendingDeathLinkKills.RemoveAt(0);
                    }

                    if (!_goalReported && _apClient.IsConnected && IsGoalReached())
                    {
                        _goalReported = true;
                        await _apClient.SendStatusUpdateAsync(ApClient.ClientGoal);
                        LogFormatted("🏁 Goal complete! Victory reported to the Archipelago server.", Color.Gold);
                    }
                }
            }
        }

        // Mirrors the completion_condition in worlds/croc2/rules.py (IDs from data/items.json / locations.json).
        private bool IsGoalReached()
        {
            if (_cachedSlotData == null) return false;
            Func<string, int, int> slotInt = (key, fallback) =>
            {
                object v;
                return _cachedSlotData.TryGetValue(key, out v) && v != null ? Convert.ToInt32(v) : fallback;
            };
            int goal = slotInt("goal", 0);
            int jigsaws = 0, trophies = 0;
            foreach (var item in _receivedItemsQueue)
            {
                if (item.ItemId >= 7200012 && item.ItemId <= 7200016) jigsaws++;
                else if (item.ItemId == 7200017) trophies++;
            }
            bool dante = _checkedLocations.Contains(7210287);
            bool hasJigsaws = jigsaws >= slotInt("jigsaw_pieces_required", 4);
            bool hasTrophies = trophies >= slotInt("trophies_required", 16);
            switch (goal)
            {
                case 0: return dante;
                case 1: return hasJigsaws;
                case 2: return hasTrophies;
                case 3: return dante && hasJigsaws && hasTrophies;
                default: return false;
            }
        }

        private void LogItemEvent(bool isOutgoing, string itemName, string importance, string sender, string receiver, string location)
        {
            SafeInvoke(() =>
            {
                string time = DateTime.Now.ToString("HH:mm:ss");
                _rtbLog.SelectionStart = _rtbLog.TextLength;
                _rtbLog.SelectionColor = Color.DarkGray;
                _rtbLog.AppendText("[" + time + "] ");

                if (sender == receiver || (sender == "Croc" && receiver == "Croc"))
                {
                    _rtbLog.SelectionColor = Color.FromArgb(255, 215, 0); // Gold
                    _rtbLog.AppendText(sender);
                    _rtbLog.SelectionColor = Color.FromArgb(220, 220, 220);
                    _rtbLog.AppendText(" got his own ");
                    _rtbLog.SelectionColor = GetImportanceColor(importance);
                    _rtbLog.AppendText(itemName);
                    _rtbLog.SelectionColor = Color.FromArgb(220, 220, 220);
                    _rtbLog.AppendText("!\n");
                }
                else
                {
                    _rtbLog.SelectionColor = isOutgoing ? Color.CornflowerBlue : Color.Orange;
                    _rtbLog.AppendText(isOutgoing ? "[OUT] " : "[IN] ");

                    _rtbLog.SelectionColor = Color.FromArgb(255, 215, 0); // Gold
                    _rtbLog.AppendText(sender);
                    _rtbLog.SelectionColor = Color.FromArgb(220, 220, 220);
                    _rtbLog.AppendText(" sent ");
                    _rtbLog.SelectionColor = GetImportanceColor(importance);
                    _rtbLog.AppendText(itemName);
                    _rtbLog.SelectionColor = Color.FromArgb(220, 220, 220);
                    _rtbLog.AppendText(" to ");
                    _rtbLog.SelectionColor = Color.FromArgb(255, 215, 0); // Gold
                    _rtbLog.AppendText(receiver);
                    _rtbLog.SelectionColor = Color.FromArgb(220, 220, 220);
                    _rtbLog.AppendText("!\n");
                }
                _rtbLog.ScrollToCaret();
            });
        }

        private Color GetImportanceColor(string imp)
        {
            switch (imp)
            {
                case "progression": return Color.FromArgb(195, 145, 255); // Orchid
                case "useful": return Color.FromArgb(110, 165, 255);      // Slate Blue
                case "trap": return Color.FromArgb(255, 115, 95);         // Salmon
                default: return Color.FromArgb(0, 238, 238);              // Cyan
            }
        }

        private void Log(string msg, Color color)
        {
            SafeInvoke(() =>
            {
                string time = DateTime.Now.ToString("HH:mm:ss");
                _rtbLog.SelectionStart = _rtbLog.TextLength;
                _rtbLog.SelectionColor = Color.DarkGray;
                _rtbLog.AppendText("[" + time + "] ");
                _rtbLog.SelectionColor = color;
                _rtbLog.AppendText(msg + "\n");
                _rtbLog.ScrollToCaret();
            });
        }

        private void LogFormatted(string msg, Color color)
        {
            Log(msg, color);
        }

        private void SafeInvoke(Action act)
        {
            if (this.IsDisposed || !this.IsHandleCreated) return;
            if (this.InvokeRequired)
            {
                try { this.BeginInvoke(act); } catch { }
            }
            else
            {
                act();
            }
        }
    }
}
