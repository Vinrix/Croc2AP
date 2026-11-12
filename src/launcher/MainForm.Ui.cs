using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace Croc2Launcher
{
    // Guided-setup launcher UI: a step rail (Game, Mode, Settings, Play) on the left and one page per step.
    // Every control the launcher logic in MainForm.cs reads or writes is created here with the same field
    // name, so the logic is unchanged.
    public partial class MainForm
    {
        // ---- palette (switched by SetPalette; custom controls read these at paint time) ----
        private static Color CAccent, CAccentHover, CAccentSoft, CAccentText, CText, CMuted, CBorder, CRail,
                             CPage, CInput, CToggleOff, CWarnSoft, CBadSoft;
        // Status colours, also used by the launcher logic in MainForm.cs
        private static Color StatusOk, StatusBad, StatusWarn;
        private static bool _darkTheme;

        static MainForm()
        {
            SetPalette(false);
        }

        private static void SetPalette(bool dark)
        {
            _darkTheme = dark;
            if (!dark)
            {
                CAccent = Color.FromArgb(31, 122, 80);
                CAccentHover = Color.FromArgb(24, 100, 65);
                CAccentSoft = Color.FromArgb(232, 244, 237);
                CAccentText = Color.FromArgb(31, 122, 80);
                CText = Color.FromArgb(28, 31, 35);
                CMuted = Color.FromArgb(100, 108, 118);
                CBorder = Color.FromArgb(222, 225, 230);
                CRail = Color.FromArgb(246, 247, 249);
                CPage = Color.White;
                CInput = Color.White;
                CToggleOff = Color.FromArgb(205, 209, 214);
                CWarnSoft = Color.FromArgb(253, 243, 224);
                CBadSoft = Color.FromArgb(252, 233, 233);
                StatusOk = Color.FromArgb(22, 120, 60);
                StatusBad = Color.FromArgb(180, 35, 35);
                StatusWarn = Color.FromArgb(166, 92, 0);
            }
            else
            {
                CAccent = Color.FromArgb(38, 140, 92);
                CAccentHover = Color.FromArgb(48, 160, 106);
                CAccentSoft = Color.FromArgb(27, 50, 39);
                CAccentText = Color.FromArgb(96, 200, 142);
                CText = Color.FromArgb(230, 232, 235);
                CMuted = Color.FromArgb(150, 157, 166);
                CBorder = Color.FromArgb(58, 62, 68);
                CRail = Color.FromArgb(30, 32, 36);
                CPage = Color.FromArgb(22, 24, 27);
                CInput = Color.FromArgb(34, 37, 41);
                CToggleOff = Color.FromArgb(72, 77, 84);
                CWarnSoft = Color.FromArgb(58, 45, 22);
                CBadSoft = Color.FromArgb(62, 30, 30);
                StatusOk = Color.FromArgb(96, 200, 130);
                StatusBad = Color.FromArgb(240, 112, 112);
                StatusWarn = Color.FromArgb(236, 172, 82);
            }
        }

        private const string IconFont = "Segoe MDL2 Assets";
        private const int ContentLeft = 36;
        private const int ContentWidth = 672;

        private enum Step { Game = 0, Mode = 1, Settings = 2, Play = 3, About = 4 }

        private Step _step = Step.Game;
        private bool _multiworld;
        private bool _applyingPreset;

        private Panel _content;
        private Panel[] _pages;
        private StepRailItem[] _railItems;
        private Panel _footer;
        private Button _btnBack;
        private Button _btnNext;
        private ChoiceCard _cardSolo;
        private ChoiceCard _cardMulti;
        private ChoiceCard[] _presetCards;
        private Label _lblPresetState;
        private Panel _playSolo;
        private Panel _playMulti;
        private Label _lblSoloSummary;
        private Panel _gameStatusCard;
        private Label _lblSettingsSub;
        private Panel _rail;
        private ThemeSwitch _themeSwitch;
        private static readonly string[] ThemeValues = { "light", "dark", "system" };

        private static readonly string[] PresetNames = { "Relaxed", "Standard", "Chaos" };

        // goal, jigsaws, trophies, whiteCrystals, peteStore, doorShuffle, lockDoors, abilities, levers,
        // vehicles, planeGuns, trapWeight, fireTrap
        private static readonly object[][] Presets =
        {
            new object[] { 0, 4, 12, 1, 0, 0, false, false, false, false, false, 0, false },
            new object[] { 0, 4, 12, 2, 1, 1, true, true, true, true, true, 15, true },
            new object[] { 3, 4, 12, 3, 1, 2, true, true, true, true, true, 40, true },
        };

        private void InitializeComponent()
        {
            SuspendLayout();
            AutoScaleMode = AutoScaleMode.Dpi;
            AutoScaleDimensions = new SizeF(96F, 96F);
            Text = "Croc 2 Archipelago";
            ClientSize = new Size(940, 660);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.White;
            ForeColor = CText;
            Font = new Font("Segoe UI", 9.5F, FontStyle.Regular, GraphicsUnit.Point);

            BuildRail();

            _content = new Panel { Location = new Point(200, 0), Size = new Size(740, 660), BackColor = Color.White };
            Controls.Add(_content);

            _pages = new Panel[5];
            for (int i = 0; i < _pages.Length; i++)
            {
                _pages[i] = new Panel { Location = new Point(0, 0), Size = new Size(740, 590), BackColor = Color.White, Visible = false };
                _content.Controls.Add(_pages[i]);
            }

            BuildFooter();
            BuildGamePage(_pages[(int)Step.Game]);
            BuildModePage(_pages[(int)Step.Mode]);
            BuildSettingsPage(_pages[(int)Step.Settings]);
            BuildPlayPage(_pages[(int)Step.Play]);
            BuildAboutPage(_pages[(int)Step.About]);

            ResumeLayout(false);
            GoTo(Step.Game);
        }

        // ------------------------------------------------------------------ rail

        private void BuildRail()
        {
            var rail = new Panel { Location = new Point(0, 0), Size = new Size(200, 660), BackColor = CRail, Tag = "rail" };
            _rail = rail;
            rail.Paint += (s, e) => e.Graphics.DrawLine(new Pen(CBorder), rail.Width - 1, 0, rail.Width - 1, rail.Height);
            Controls.Add(rail);

            rail.Controls.Add(new Label
            {
                Text = "Croc 2",
                Font = new Font("Segoe UI Semibold", 13F),
                ForeColor = CText,
                Location = new Point(22, 22),
                AutoSize = true
            });
            rail.Controls.Add(new Label
            {
                Text = "Archipelago randomizer",
                ForeColor = CMuted,
                Location = new Point(23, 48),
                AutoSize = true
            });

            string[] names = { "Game", "Mode", "Settings", "Play" };
            _railItems = new StepRailItem[names.Length];
            for (int i = 0; i < names.Length; i++)
            {
                var step = (Step)i;
                var item = new StepRailItem(i + 1, names[i]) { Location = new Point(12, 96 + i * 46), Size = new Size(176, 40) };
                item.Click += (s, e) => GoTo(step);
                _railItems[i] = item;
                rail.Controls.Add(item);
            }

            _themeSwitch = new ThemeSwitch(new[] { "Light", "Dark", "System" }) { Location = new Point(18, 536), Size = new Size(164, 30) };
            _themeSwitch.Picked += (s, e) => SetThemeChoice(ThemeValues[_themeSwitch.SelectedIndex]);
            rail.Controls.Add(_themeSwitch);

            var about = LinkButton("About and updates");
            about.Location = new Point(18, 580);
            about.Click += (s, e) => GoTo(Step.About);
            rail.Controls.Add(about);

            _lblVersion = new Label { Text = "v" + UpdateManager.CurrentVersion, ForeColor = CMuted, Location = new Point(23, 612), AutoSize = true };
            rail.Controls.Add(_lblVersion);

            _btnCheckUpdates = new Button { Visible = false }; // kept for the update logic; the visible one is on the About page
            _btnCheckUpdates.Click += async (s, e) => await CheckForUpdatesAsync(true);
            rail.Controls.Add(_btnCheckUpdates);
        }

        private void BuildFooter()
        {
            _footer = new Panel { Location = new Point(0, 590), Size = new Size(740, 70), BackColor = Color.White };
            _footer.Paint += (s, e) => e.Graphics.DrawLine(new Pen(CBorder), 0, 0, _footer.Width, 0);
            _content.Controls.Add(_footer);

            _btnBack = SecondaryButton("Back", 96);
            _btnBack.Location = new Point(ContentLeft, 16);
            _btnBack.Click += (s, e) => { if (_step > Step.Game) GoTo(_step - 1); };
            _footer.Controls.Add(_btnBack);

            _btnNext = PrimaryButton("Next", 120);
            _btnNext.Location = new Point(ContentLeft + ContentWidth - 120, 16);
            _btnNext.Click += (s, e) => { if (_step < Step.Play) GoTo(_step + 1); };
            _footer.Controls.Add(_btnNext);

            _btnExportYaml = SecondaryButton("Save YAML", 140);
            _btnExportYaml.Location = new Point(ContentLeft + ContentWidth - 120 - 12 - 140, 16);
            _btnExportYaml.Click += (s, e) => ExportYaml();
            _footer.Controls.Add(_btnExportYaml);
        }

        private void GoTo(Step step)
        {
            _step = step;
            for (int i = 0; i < _pages.Length; i++) _pages[i].Visible = i == (int)step;
            for (int i = 0; i < _railItems.Length; i++)
            {
                _railItems[i].State = i < (int)step && step != Step.About ? StepState.Done
                                    : i == (int)step ? StepState.Current : StepState.Upcoming;
                _railItems[i].Detail = i == (int)Step.Mode && (int)step > i && step != Step.About
                    ? (_multiworld ? "Multiworld" : "Solo") : "";
            }

            _btnBack.Visible = step != Step.Game && step != Step.About;
            _btnNext.Visible = step < Step.Play;
            _btnExportYaml.Visible = step == Step.Settings && _multiworld;
            _footer.Visible = step != Step.About;

            if (step == Step.Settings)
            {
                _lblSettingsSub.Text = _multiworld
                    ? "These go into your YAML. Save it and send it to whoever is generating the room."
                    : "Pick a preset, or fine-tune it below. You can change these before every run.";
            }
            if (step == Step.Play)
            {
                _playSolo.Visible = !_multiworld;
                _playMulti.Visible = _multiworld;
                _lblStatusAp.Visible = _multiworld;
                _btnSyncItems.Visible = _multiworld;
                _lblStatusGame.Left = _multiworld ? 214 : 14;
                _lblStatusHook.Left = _multiworld ? 394 : 194;
                _lblSoloSummary.Text = BuildSoloSummary();
            }
        }

        // ------------------------------------------------------------------ step 1: game

        private void BuildGamePage(Panel page)
        {
            AddHeading(page, "Find your game", "Point the launcher at your Croc 2 install. Steam installs are found automatically.");

            page.Controls.Add(new Label { Text = "Install folder", ForeColor = CMuted, Location = new Point(ContentLeft, 112), AutoSize = true });
            _txtGamePath = new TextBox
            {
                Location = new Point(ContentLeft, 134),
                Size = new Size(ContentWidth - 110, 28),
                BorderStyle = BorderStyle.FixedSingle
            };
            _txtGamePath.TextChanged += (s, e) => { UpdateDetectedPaths(); RefreshGameStatusCard(); };
            page.Controls.Add(_txtGamePath);

            _btnBrowse = SecondaryButton("Browse", 96);
            _btnBrowse.Location = new Point(ContentLeft + ContentWidth - 96, 131);
            _btnBrowse.Click += (s, e) => { OnBrowseClicked(s, e); RefreshGameStatusCard(); };
            page.Controls.Add(_btnBrowse);

            _gameStatusCard = new RoundedPanel { Location = new Point(ContentLeft, 184), Size = new Size(ContentWidth, 64), FillColor = CAccentSoft, BorderColor = CAccentSoft };
            _lblDetectedStatus = new Label
            {
                Location = new Point(16, 12),
                Size = new Size(ContentWidth - 32, 40),
                BackColor = Color.Transparent,
                TextAlign = ContentAlignment.MiddleLeft,
                UseMnemonic = false,
                Tag = "status"
            };
            _gameStatusCard.Controls.Add(_lblDetectedStatus);
            page.Controls.Add(_gameStatusCard);

            _btnLaunchVanilla = LinkButton("Play the original game without the mod");
            _btnLaunchVanilla.Location = new Point(ContentLeft - 6, 268);
            _btnLaunchVanilla.Click += OnLaunchVanillaClicked;
            page.Controls.Add(_btnLaunchVanilla);
        }

        private void RefreshGameStatusCard()
        {
            if (_gameStatusCard == null || _lblDetectedStatus == null) return;
            // Driven by the detection result itself, not the label colour (which may still be the previous
            // theme's colour while a theme switch is being applied).
            bool ok = _paths != null && _paths.HasExe && _paths.HasHookDll;
            bool warn = _paths != null && _paths.HasExe && !_paths.HasHookDll;
            _lblDetectedStatus.ForeColor = ok ? StatusOk : warn ? StatusWarn : StatusBad;
            var rp = (RoundedPanel)_gameStatusCard;
            rp.FillColor = ok ? CAccentSoft : warn ? CWarnSoft : CBadSoft;
            rp.BorderColor = rp.FillColor;
            rp.Invalidate();
        }

        // ------------------------------------------------------------------ step 2: mode

        private void BuildModePage(Panel page)
        {
            AddHeading(page, "How do you want to play?", "You can switch any time; your settings are kept.");

            _cardSolo = new ChoiceCard("", "Solo randomizer", "Play offline with a fresh shuffle. No server needed.")
            {
                Location = new Point(ContentLeft, 112),
                Size = new Size(ContentWidth, 92),
                Selected = true
            };
            _cardMulti = new ChoiceCard("", "Archipelago multiworld", "Join a room and trade items with other players and games.")
            {
                Location = new Point(ContentLeft, 218),
                Size = new Size(ContentWidth, 92)
            };
            _cardSolo.Picked += (s, e) => SetMode(false);
            _cardMulti.Picked += (s, e) => SetMode(true);
            page.Controls.Add(_cardSolo);
            page.Controls.Add(_cardMulti);
        }

        private void SetMode(bool multiworld)
        {
            _multiworld = multiworld;
            _cardSolo.Selected = !multiworld;
            _cardMulti.Selected = multiworld;
        }

        // ------------------------------------------------------------------ step 3: settings

        private void BuildSettingsPage(Panel page)
        {
            _lblSettingsSub = AddHeading(page, "Choose your settings", "");

            page.Controls.Add(new Label { Text = "Player name", ForeColor = CMuted, Location = new Point(ContentLeft, 104), AutoSize = true });
            _txtRandoSlot = new TextBox { Location = new Point(ContentLeft + 100, 101), Size = new Size(240, 28), BorderStyle = BorderStyle.FixedSingle };
            page.Controls.Add(_txtRandoSlot);

            _presetCards = new ChoiceCard[3];
            string[] blurbs = { "Abilities from the start, vanilla doors", "Doors shuffled within villages", "Any door, every crystal, more traps" };
            int cardW = (ContentWidth - 24) / 3;
            for (int i = 0; i < 3; i++)
            {
                int idx = i;
                var card = new ChoiceCard(null, PresetNames[i], blurbs[i])
                {
                    Location = new Point(ContentLeft + i * (cardW + 12), 142),
                    Size = new Size(cardW, 72)
                };
                card.Picked += (s, e) => ApplyPreset(idx);
                _presetCards[i] = card;
                page.Controls.Add(card);
            }

            page.Controls.Add(new Label
            {
                Text = "Fine-tune",
                Font = new Font("Segoe UI Semibold", 10.5F),
                Location = new Point(ContentLeft, 232),
                AutoSize = true
            });
            _lblPresetState = new Label { ForeColor = CMuted, Location = new Point(ContentLeft + 80, 234), AutoSize = true };
            page.Controls.Add(_lblPresetState);

            var list = new Panel { Location = new Point(ContentLeft, 260), Size = new Size(ContentWidth + 20, 326), AutoScroll = true, BackColor = Color.White };
            page.Controls.Add(list);

            _cboGoal = Combo(new[] { "Defeat Baron Dante", "Collect Jigsaw Pieces", "Trophy hunt", "All of the above" });
            _numJigsaws = Number(1, 4, 4);
            _numTrophies = Number(1, 16, 12);
            _cboWhiteCrystals = Combo(new[] { "None", "Every 50th (40 checks)", "Every 10th (200 checks)", "All 100 per stage (2,000 checks)" });
            _cboPeteStore = Combo(new[] { "Vanilla", "Shuffled (VIP passes)" });
            _cboDoorShuffle = Combo(new[] { "Off", "Within each village", "Any village" });
            _chkLockDoors = new ToggleSwitch();
            _chkAbilities = new ToggleSwitch();
            _chkLevers = new ToggleSwitch();
            _chkVehicles = new ToggleSwitch();
            _chkPlaneGuns = new ToggleSwitch();
            _numTrapWeight = Number(0, 100, 15);
            _chkFireTrap = new ToggleSwitch();
            _chkDeathLink = new ToggleSwitch();
            _cboGoal.SelectedIndex = 0;
            _cboWhiteCrystals.SelectedIndex = 2;
            _cboPeteStore.SelectedIndex = 1;
            _cboDoorShuffle.SelectedIndex = 1;

            int y = 0;
            y = Row(list, y, "Goal", "What finishes the seed", _cboGoal);
            y = Row(list, y, "Jigsaw pieces needed", "For the jigsaw goal (1 to 4)", _numJigsaws);
            y = Row(list, y, "Trophies needed", "For the trophy goal (1 to 16)", _numTrophies);
            y = Row(list, y, "White crystals as checks", "More checks means a longer run", _cboWhiteCrystals);
            y = Row(list, y, "Swap Meet Pete's shop", "Shuffle the shop's stock", _cboPeteStore);
            y = Row(list, y, "Door shuffle", "Where each stage door leads", _cboDoorShuffle);
            y = Row(list, y, "Lock stage doors", "Each door needs its key item (Sailor door 1 starts open)", _chkLockDoors);
            y = Row(list, y, "Shuffle abilities", "Tail whack, butt stomp, climbing, carrying", _chkAbilities);
            y = Row(list, y, "Shuffle levers", "Levers need the Lever Wrench", _chkLevers);
            y = Row(list, y, "Shuffle vehicles", "Vehicles need the Vehicle License", _chkVehicles);
            y = Row(list, y, "Shuffle plane guns", "Shooting needs the Airplane Ammo Feed", _chkPlaneGuns);
            y = Row(list, y, "Trap frequency", "Percent of filler that becomes a trap", _numTrapWeight);
            y = Row(list, y, "Fire breakout trap", "Instant heart loss and knockback", _chkFireTrap);
            Row(list, y, "DeathLink", "When anyone dies, everyone dies (multiworld)", _chkDeathLink);

            EventHandler changed = (s, e) => { if (!_applyingPreset) RefreshPresetState(); };
            foreach (Control c in new Control[] { _cboGoal, _cboWhiteCrystals, _cboPeteStore, _cboDoorShuffle })
                ((ComboBox)c).SelectedIndexChanged += changed;
            foreach (var n in new[] { _numJigsaws, _numTrophies, _numTrapWeight }) n.ValueChanged += changed;
            foreach (var t in new[] { _chkLockDoors, _chkAbilities, _chkLevers, _chkVehicles, _chkPlaneGuns, _chkFireTrap })
                t.CheckedChanged += changed;

            // Status line for YAML export (shown under the footer buttons' row, inside the page)
            _lblExportStatus = new Label { Tag = "status", ForeColor = CMuted, Location = new Point(ContentLeft + 360, 104), Size = new Size(ContentWidth - 360, 20), TextAlign = ContentAlignment.MiddleRight };
            page.Controls.Add(_lblExportStatus);
            _btnOpenPlayersFolder = new Button { Visible = false };
            _btnOpenPlayersFolder.Click += (s, e) => OpenPlayersFolder();
            page.Controls.Add(_btnOpenPlayersFolder);

            RefreshPresetState();
        }

        private void ApplyPreset(int idx)
        {
            object[] p = Presets[idx];
            _applyingPreset = true;
            _cboGoal.SelectedIndex = (int)p[0];
            _numJigsaws.Value = (int)p[1];
            _numTrophies.Value = (int)p[2];
            _cboWhiteCrystals.SelectedIndex = (int)p[3];
            _cboPeteStore.SelectedIndex = (int)p[4];
            _cboDoorShuffle.SelectedIndex = (int)p[5];
            _chkLockDoors.Checked = (bool)p[6];
            _chkAbilities.Checked = (bool)p[7];
            _chkLevers.Checked = (bool)p[8];
            _chkVehicles.Checked = (bool)p[9];
            _chkPlaneGuns.Checked = (bool)p[10];
            _numTrapWeight.Value = (int)p[11];
            _chkFireTrap.Checked = (bool)p[12];
            _applyingPreset = false;
            RefreshPresetState();
        }

        private int MatchingPreset()
        {
            for (int i = 0; i < Presets.Length; i++)
            {
                object[] p = Presets[i];
                if (_cboGoal.SelectedIndex == (int)p[0] && (int)_numJigsaws.Value == (int)p[1] &&
                    (int)_numTrophies.Value == (int)p[2] && _cboWhiteCrystals.SelectedIndex == (int)p[3] &&
                    _cboPeteStore.SelectedIndex == (int)p[4] && _cboDoorShuffle.SelectedIndex == (int)p[5] &&
                    _chkLockDoors.Checked == (bool)p[6] && _chkAbilities.Checked == (bool)p[7] &&
                    _chkLevers.Checked == (bool)p[8] && _chkVehicles.Checked == (bool)p[9] &&
                    _chkPlaneGuns.Checked == (bool)p[10] && (int)_numTrapWeight.Value == (int)p[11] &&
                    _chkFireTrap.Checked == (bool)p[12])
                {
                    return i;
                }
            }
            return -1;
        }

        private void RefreshPresetState()
        {
            if (_presetCards == null) return;
            int match = MatchingPreset();
            for (int i = 0; i < _presetCards.Length; i++) _presetCards[i].Selected = i == match;
            _lblPresetState.Text = match >= 0 ? "Using the " + PresetNames[match] + " preset" : "Custom settings";
        }

        private string BuildSoloSummary()
        {
            int match = MatchingPreset();
            string preset = match >= 0 ? PresetNames[match] : "Custom";
            return preset + " settings\n" +
                   "Goal: " + _cboGoal.Text + "\n" +
                   "Doors: " + _cboDoorShuffle.Text + "\n" +
                   "White crystal checks: " + _cboWhiteCrystals.Text + "\n" +
                   "Traps: " + (int)_numTrapWeight.Value + "%";
        }

        // ------------------------------------------------------------------ step 4: play

        private void BuildPlayPage(Panel page)
        {
            // Solo
            _playSolo = new Panel { Location = new Point(0, 0), Size = new Size(740, 300), BackColor = Color.White };
            AddHeading(_playSolo, "Ready to play", "Start the game, then pick an empty save slot and choose New Game.");
            var summary = new RoundedPanel { Location = new Point(ContentLeft, 106), Size = new Size(380, 128), FillColor = CRail, BorderColor = CRail };
            _lblSoloSummary = new Label { Location = new Point(16, 12), Size = new Size(350, 106), BackColor = Color.Transparent };
            summary.Controls.Add(_lblSoloSummary);
            _playSolo.Controls.Add(summary);
            _btnLaunchSoloRando = PrimaryButton("Start solo run", 200);
            _btnLaunchSoloRando.Font = new Font("Segoe UI Semibold", 10.5F);
            _btnLaunchSoloRando.Height = 44;
            _btnLaunchSoloRando.Location = new Point(ContentLeft + 400, 106);
            _btnLaunchSoloRando.Click += OnLaunchSoloRandoClicked;
            _playSolo.Controls.Add(_btnLaunchSoloRando);
            page.Controls.Add(_playSolo);

            // Multiworld
            _playMulti = new Panel { Location = new Point(0, 0), Size = new Size(740, 300), BackColor = Color.White };
            AddHeading(_playMulti, "Join your room", "Use the address and slot name from your room page.");
            int fy = 104;
            _playMulti.Controls.Add(new Label { Text = "Room address", ForeColor = CMuted, Location = new Point(ContentLeft, fy), AutoSize = true });
            _playMulti.Controls.Add(new Label { Text = "Port", ForeColor = CMuted, Location = new Point(ContentLeft + 262, fy), AutoSize = true });
            _playMulti.Controls.Add(new Label { Text = "Slot name", ForeColor = CMuted, Location = new Point(ContentLeft + 352, fy), AutoSize = true });
            _playMulti.Controls.Add(new Label { Text = "Password", ForeColor = CMuted, Location = new Point(ContentLeft + 528, fy), AutoSize = true });
            _txtHost = new TextBox { Location = new Point(ContentLeft, fy + 22), Size = new Size(250, 28), BorderStyle = BorderStyle.FixedSingle, Text = "archipelago.gg" };
            _txtPort = new TextBox { Location = new Point(ContentLeft + 262, fy + 22), Size = new Size(78, 28), BorderStyle = BorderStyle.FixedSingle, Text = "38281" };
            _txtSlot = new TextBox { Location = new Point(ContentLeft + 352, fy + 22), Size = new Size(164, 28), BorderStyle = BorderStyle.FixedSingle };
            _txtPassword = new TextBox { Location = new Point(ContentLeft + 528, fy + 22), Size = new Size(144, 28), BorderStyle = BorderStyle.FixedSingle, UseSystemPasswordChar = true };
            _playMulti.Controls.AddRange(new Control[] { _txtHost, _txtPort, _txtSlot, _txtPassword });
            _txtSlot.TextChanged += (s, e) => { if (_txtRandoSlot.Text != _txtSlot.Text) _txtRandoSlot.Text = _txtSlot.Text; };
            _txtRandoSlot.TextChanged += (s, e) => { if (_txtSlot.Text != _txtRandoSlot.Text) _txtSlot.Text = _txtRandoSlot.Text; };

            _chkRemember = new CheckBox { Text = "Remember room details", Location = new Point(ContentLeft, fy + 60), AutoSize = true, Checked = true, ForeColor = CMuted };
            _playMulti.Controls.Add(_chkRemember);

            _btnLaunchAp = PrimaryButton("Launch game and connect", 230);
            _btnLaunchAp.Location = new Point(ContentLeft, fy + 92);
            _btnLaunchAp.Click += OnLaunchApClicked;
            _btnAttach = SecondaryButton("Connect to running game", 200);
            _btnAttach.Location = new Point(ContentLeft + 242, fy + 92);
            _btnAttach.Click += OnAttachClicked;
            _playMulti.Controls.Add(_btnLaunchAp);
            _playMulti.Controls.Add(_btnAttach);
            page.Controls.Add(_playMulti);

            // Session status (shared)
            int sy = 290;
            var statusCard = new RoundedPanel { Location = new Point(ContentLeft, sy), Size = new Size(ContentWidth, 64), FillColor = CRail, BorderColor = CRail };
            _lblStatusAp = StatusLabel("Archipelago: disconnected", new Point(14, 10));
            _lblStatusGame = StatusLabel("Game: not running", new Point(214, 10));
            _lblStatusHook = StatusLabel("Mod link: off", new Point(394, 10));
            _lblChecksCount = new Label { Text = "Checks: 0", Location = new Point(14, 36), AutoSize = true, BackColor = Color.Transparent, ForeColor = CMuted };
            _lblItemsCount = new Label { Text = "Items: 0", Location = new Point(214, 36), AutoSize = true, BackColor = Color.Transparent, ForeColor = CMuted };
            statusCard.Controls.AddRange(new Control[] { _lblStatusAp, _lblStatusGame, _lblStatusHook, _lblChecksCount, _lblItemsCount });

            _btnSyncItems = SmallButton("Resend items");
            _btnSyncItems.Location = new Point(ContentWidth - 214, 32);
            _btnSyncItems.Enabled = false;
            _btnSyncItems.Click += OnSyncItemsClicked;
            _btnDisconnect = SmallButton("Disconnect");
            _btnDisconnect.Location = new Point(ContentWidth - 106, 32);
            _btnDisconnect.Enabled = false;
            _btnDisconnect.Click += OnDisconnectClicked;
            statusCard.Controls.Add(_btnSyncItems);
            statusCard.Controls.Add(_btnDisconnect);
            page.Controls.Add(statusCard);

            page.Controls.Add(new Label { Text = "Activity", Font = new Font("Segoe UI Semibold", 10.5F), Location = new Point(ContentLeft, sy + 78), AutoSize = true });
            _btnClearLog = SmallButton("Clear");
            _btnClearLog.Location = new Point(ContentLeft + ContentWidth - _btnClearLog.Width, sy + 72);
            _btnClearLog.Click += (s, e) => _rtbLog.Clear();
            page.Controls.Add(_btnClearLog);

            _rtbLog = new RichTextBox
            {
                Location = new Point(ContentLeft, sy + 106),
                Size = new Size(ContentWidth, 184),
                ReadOnly = true,
                BorderStyle = BorderStyle.None,
                BackColor = Color.FromArgb(24, 28, 32),
                ForeColor = Color.Gainsboro,
                Font = new Font("Consolas", 9.5F)
            };
            page.Controls.Add(_rtbLog);
        }

        // ------------------------------------------------------------------ about

        private void BuildAboutPage(Panel page)
        {
            AddHeading(page, "About and updates", "Croc 2 Archipelago launcher v" + UpdateManager.CurrentVersion);

            page.Controls.Add(new Label { Text = "Update source (GitHub repository)", ForeColor = CMuted, Location = new Point(ContentLeft, 112), AutoSize = true });
            _txtGitHubRepo = new TextBox { Location = new Point(ContentLeft, 134), Size = new Size(280, 28), BorderStyle = BorderStyle.FixedSingle, Text = UpdateManager.DefaultRepo };
            page.Controls.Add(_txtGitHubRepo);
            _chkAutoCheckUpdates = new CheckBox { Text = "Check for updates when the launcher opens", Location = new Point(ContentLeft, 176), AutoSize = true, Checked = true };
            page.Controls.Add(_chkAutoCheckUpdates);
            _btnCheckUpdatesTab2 = SecondaryButton("Check for updates", 170);
            _btnCheckUpdatesTab2.Location = new Point(ContentLeft, 214);
            _btnCheckUpdatesTab2.Click += async (s, e) => await CheckForUpdatesAsync(true);
            page.Controls.Add(_btnCheckUpdatesTab2);
            _lblUpdateStatusTab2 = new Label { ForeColor = CMuted, Location = new Point(ContentLeft + 184, 222), Size = new Size(480, 20) };
            page.Controls.Add(_lblUpdateStatusTab2);

            var players = LinkButton("Open the Archipelago Players folder");
            players.Location = new Point(ContentLeft - 6, 266);
            players.Click += (s, e) => OpenPlayersFolder();
            page.Controls.Add(players);

            var back = SecondaryButton("Back to setup", 140);
            back.Location = new Point(ContentLeft, 520);
            back.Click += (s, e) => GoTo(Step.Game);
            page.Controls.Add(back);
        }

        // ------------------------------------------------------------------ helpers

        private Label AddHeading(Control parent, string title, string subtitle)
        {
            parent.Controls.Add(new Label
            {
                Text = title,
                Font = new Font("Segoe UI Semibold", 16F),
                ForeColor = CText,
                Location = new Point(ContentLeft - 2, 30),
                AutoSize = true
            });
            var sub = new Label { Text = subtitle, ForeColor = CMuted, Location = new Point(ContentLeft, 66), Size = new Size(ContentWidth, 22) };
            parent.Controls.Add(sub);
            return sub;
        }

        private int Row(Panel list, int y, string title, string hint, Control control)
        {
            var row = new Panel { Location = new Point(0, y), Size = new Size(ContentWidth, 52), BackColor = Color.White };
            row.Paint += (s, e) => e.Graphics.DrawLine(new Pen(CBorder), 0, row.Height - 1, row.Width, row.Height - 1);
            row.Controls.Add(new Label { Text = title, Location = new Point(0, 7), AutoSize = true, ForeColor = CText });
            row.Controls.Add(new Label { Text = hint, Location = new Point(0, 27), AutoSize = true, ForeColor = CMuted, Font = new Font("Segoe UI", 8.5F) });
            int w = control is ToggleSwitch ? 40 : control is NumericUpDown ? 70 : 250;
            control.Size = new Size(w, control is ToggleSwitch ? 22 : control.Height);
            control.Location = new Point(ContentWidth - w, (52 - control.Height) / 2);
            row.Controls.Add(control);
            list.Controls.Add(row);
            return y + 52;
        }

        private ComboBox Combo(string[] items)
        {
            var c = new ComboBox
            {
                DropDownStyle = ComboBoxStyle.DropDownList,
                FlatStyle = FlatStyle.Flat,
                DrawMode = DrawMode.OwnerDrawFixed,
                ItemHeight = 22
            };
            c.Items.AddRange(items);
            // Owner-drawn so the closed box and the open list both follow the theme.
            c.DrawItem += (s, e) =>
            {
                if (e.Index < 0) return;
                bool highlighted = (e.State & DrawItemState.Selected) != 0 && (e.State & DrawItemState.ComboBoxEdit) == 0;
                using (var bg = new SolidBrush(highlighted ? CAccentSoft : CInput)) e.Graphics.FillRectangle(bg, e.Bounds);
                TextRenderer.DrawText(e.Graphics, c.Items[e.Index].ToString(), c.Font,
                    new Rectangle(e.Bounds.X + 6, e.Bounds.Y, e.Bounds.Width - 6, e.Bounds.Height), CText,
                    TextFormatFlags.VerticalCenter | TextFormatFlags.Left | TextFormatFlags.EndEllipsis);
            };
            return c;
        }

        private NumericUpDown Number(int min, int max, int value)
        {
            return new NumericUpDown { Minimum = min, Maximum = max, Value = value, BorderStyle = BorderStyle.FixedSingle, TextAlign = HorizontalAlignment.Center };
        }

        private Label StatusLabel(string text, Point loc)
        {
            return new Label { Text = text, Location = loc, AutoSize = true, BackColor = Color.Transparent, ForeColor = StatusBad, Font = new Font("Segoe UI Semibold", 9F), Tag = "status" };
        }

        private Button PrimaryButton(string text, int width)
        {
            var b = new ThemedButton
            {
                Text = text,
                Size = new Size(width, 38),
                FlatStyle = FlatStyle.Flat,
                BackColor = CAccent,
                ForeColor = Color.White,
                Font = new Font("Segoe UI Semibold", 9.5F),
                Cursor = Cursors.Hand
            };
            b.FlatAppearance.BorderSize = 0;
            b.FlatAppearance.MouseOverBackColor = CAccentHover;
            return b;
        }

        private Button SecondaryButton(string text, int width)
        {
            var b = new ThemedButton
            {
                Text = text,
                Size = new Size(width, 38),
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.White,
                ForeColor = CText,
                Cursor = Cursors.Hand
            };
            b.FlatAppearance.BorderColor = CBorder;
            b.FlatAppearance.MouseOverBackColor = CRail;
            return b;
        }

        private Button SmallButton(string text)
        {
            var b = SecondaryButton(text, 96);
            b.Height = 26;
            b.Font = new Font("Segoe UI", 8.5F);
            return b;
        }

        private Button LinkButton(string text)
        {
            var b = new Button
            {
                Text = text,
                AutoSize = true,
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.Transparent,
                ForeColor = CAccent,
                Cursor = Cursors.Hand,
                TextAlign = ContentAlignment.MiddleLeft
            };
            b.FlatAppearance.BorderSize = 0;
            b.FlatAppearance.MouseOverBackColor = Color.Transparent;
            b.FlatAppearance.MouseDownBackColor = Color.Transparent;
            return b;
        }

        // ------------------------------------------------------------------ custom controls

        private enum StepState { Upcoming, Current, Done }

        private class StepRailItem : Control
        {
            private readonly int _number;
            private readonly string _name;
            private StepState _state;
            private string _detail = "";

            public StepRailItem(int number, string name)
            {
                _number = number;
                _name = name;
                DoubleBuffered = true;
                Cursor = Cursors.Hand;
                SetStyle(ControlStyles.SupportsTransparentBackColor, true);
                BackColor = Color.Transparent;
            }

            public StepState State { get { return _state; } set { _state = value; Invalidate(); } }
            public string Detail { get { return _detail; } set { _detail = value ?? ""; Invalidate(); } }

            protected override void OnPaint(PaintEventArgs e)
            {
                var g = e.Graphics;
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
                if (_state == StepState.Current)
                {
                    using (var bg = new SolidBrush(CAccentSoft))
                    using (var path = RoundRect(new Rectangle(0, 0, Width - 1, Height - 1), 8))
                        g.FillPath(bg, path);
                }
                var circle = new Rectangle(10, (Height - 24) / 2, 24, 24);
                if (_state == StepState.Upcoming)
                {
                    using (var pen = new Pen(CBorder, 1.5f)) g.DrawEllipse(pen, circle);
                }
                else
                {
                    using (var b = new SolidBrush(CAccent)) g.FillEllipse(b, circle);
                }
                string mark = _state == StepState.Done ? "" : _number.ToString();
                using (var f = _state == StepState.Done ? new Font(IconFont, 9F) : new Font("Segoe UI Semibold", 9F))
                {
                    TextRenderer.DrawText(g, mark, f, circle, _state == StepState.Upcoming ? CMuted : Color.White,
                        TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
                }
                using (var f = new Font(_state == StepState.Current ? "Segoe UI Semibold" : "Segoe UI", 10F))
                {
                    var rect = _detail.Length > 0 ? new Rectangle(44, 2, Width - 48, 20) : new Rectangle(44, 0, Width - 48, Height);
                    TextRenderer.DrawText(g, _name, f, rect, _state == StepState.Upcoming ? CMuted : CText,
                        TextFormatFlags.VerticalCenter | TextFormatFlags.Left);
                }
                if (_detail.Length > 0)
                {
                    using (var f = new Font("Segoe UI", 8.5F))
                        TextRenderer.DrawText(g, _detail, f, new Rectangle(44, 21, Width - 48, 16), CMuted, TextFormatFlags.Left);
                }
            }
        }

        private class RoundedPanel : Panel
        {
            public Color FillColor = Color.White;
            public Color BorderColor = CBorder;
            public int BorderWidth = 1;

            public RoundedPanel()
            {
                DoubleBuffered = true;
                SetStyle(ControlStyles.ResizeRedraw, true);
                BackColor = Color.White;
            }

            protected override void OnPaintBackground(PaintEventArgs e)
            {
                e.Graphics.Clear(Parent != null ? Parent.BackColor : CPage);
                e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
                var r = new Rectangle(BorderWidth / 2, BorderWidth / 2, Width - 1 - BorderWidth, Height - 1 - BorderWidth);
                using (var path = RoundRect(r, 10))
                using (var fill = new SolidBrush(FillColor))
                using (var pen = new Pen(BorderColor, BorderWidth))
                {
                    e.Graphics.FillPath(fill, path);
                    e.Graphics.DrawPath(pen, path);
                }
            }
        }

        private class ChoiceCard : RoundedPanel
        {
            private readonly string _glyph;
            private readonly string _title;
            private readonly string _subtitle;
            private bool _selected;
            private bool _hover;

            public event EventHandler Picked;

            public ChoiceCard(string glyph, string title, string subtitle)
            {
                _glyph = glyph;
                _title = title;
                _subtitle = subtitle;
                Cursor = Cursors.Hand;
                ApplyLook();
            }

            public bool Selected { get { return _selected; } set { _selected = value; ApplyLook(); } }

            public void ApplyLook()
            {
                FillColor = _selected ? CAccentSoft : _hover ? CRail : CPage;
                BorderColor = _selected ? CAccent : CBorder;
                BorderWidth = _selected ? 2 : 1;
                Invalidate();
            }

            protected override void OnMouseEnter(EventArgs e) { _hover = true; ApplyLook(); base.OnMouseEnter(e); }
            protected override void OnMouseLeave(EventArgs e) { _hover = false; ApplyLook(); base.OnMouseLeave(e); }
            protected override void OnClick(EventArgs e) { base.OnClick(e); if (Picked != null) Picked(this, EventArgs.Empty); }

            protected override void OnPaint(PaintEventArgs e)
            {
                var g = e.Graphics;
                int x = 16;
                if (!string.IsNullOrEmpty(_glyph))
                {
                    using (var f = new Font(IconFont, 18F))
                        TextRenderer.DrawText(g, _glyph, f, new Rectangle(16, 0, 36, Height), _selected ? CAccent : CMuted,
                            TextFormatFlags.VerticalCenter | TextFormatFlags.HorizontalCenter);
                    x = 64;
                }
                bool tall = Height >= 80;
                using (var tf = new Font("Segoe UI Semibold", tall ? 11.5F : 10F))
                using (var sf = new Font("Segoe UI", tall ? 9.5F : 8.5F))
                {
                    int titleH = tall ? 24 : 20;
                    int top = (Height - titleH - (tall ? 22 : 34)) / 2;
                    TextRenderer.DrawText(g, _title, tf, new Rectangle(x, top, Width - x - 12, titleH), CText, TextFormatFlags.Left);
                    TextRenderer.DrawText(g, _subtitle, sf, new Rectangle(x, top + titleH, Width - x - 12, tall ? 22 : 34), CMuted,
                        TextFormatFlags.Left | TextFormatFlags.WordBreak);
                }
            }
        }

        private class ToggleSwitch : CheckBox
        {
            public ToggleSwitch()
            {
                SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, true);
                Size = new Size(40, 22);
                Cursor = Cursors.Hand;
                Text = "";
            }

            protected override void OnPaint(PaintEventArgs e)
            {
                var g = e.Graphics;
                g.Clear(Parent != null ? Parent.BackColor : CPage);
                g.SmoothingMode = SmoothingMode.AntiAlias;
                var track = new Rectangle(0, 0, Width - 1, Height - 1);
                using (var path = RoundRect(track, Height / 2))
                using (var b = new SolidBrush(Checked ? CAccent : CToggleOff))
                    g.FillPath(b, path);
                int d = Height - 6;
                int kx = Checked ? Width - d - 4 : 3;
                using (var k = new SolidBrush(Color.White)) g.FillEllipse(k, kx, 3, d, d);
            }
        }

        // ------------------------------------------------------------------ theme

        [System.Runtime.InteropServices.DllImport("dwmapi.dll")]
        private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);

        private static bool SystemPrefersDark()
        {
            try
            {
                using (var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize"))
                {
                    object v = key != null ? key.GetValue("AppsUseLightTheme") : null;
                    return v is int && (int)v == 0;
                }
            }
            catch { return false; }
        }

        private bool _themeHooksInstalled;

        // Called once settings are loaded, and whenever the switch or the Windows theme changes.
        private void InitTheme()
        {
            string choice = _settings != null && !string.IsNullOrEmpty(_settings.Theme) ? _settings.Theme : "system";
            int idx = Array.IndexOf(ThemeValues, choice);
            _themeSwitch.SelectedIndex = idx < 0 ? 2 : idx;

            if (!_themeHooksInstalled)
            {
                _themeHooksInstalled = true;
                TagThemeRoles(this);
                Microsoft.Win32.SystemEvents.UserPreferenceChanged += (s, e) =>
                {
                    if (_settings != null && (_settings.Theme ?? "system") == "system") SafeInvoke(InitTheme);
                };
                FormClosed += (s, e) => { };
                HandleCreated += (s, e) => ApplyTitleBar();
            }

            bool dark = choice == "dark" || (choice == "system" && SystemPrefersDark());
            Color oldOk = StatusOk, oldBad = StatusBad, oldWarn = StatusWarn, oldMuted = CMuted;
            SetPalette(dark);
            ApplyTheme(this, oldOk, oldBad, oldWarn, oldMuted);
            ApplyTitleBar();
            Invalidate(true);
        }

        private void SetThemeChoice(string choice)
        {
            if (_settings == null) return;
            _settings.Theme = choice;
            try { SettingsManager.Save(_settings); } catch { }
            InitTheme();
        }

        private void ApplyTitleBar()
        {
            if (!IsHandleCreated) return;
            int on = _darkTheme ? 1 : 0;
            try
            {
                if (DwmSetWindowAttribute(Handle, 20, ref on, sizeof(int)) != 0)
                    DwmSetWindowAttribute(Handle, 19, ref on, sizeof(int)); // older Windows 10 builds
            }
            catch { }
        }

        // Records each standard control's role once, from the light palette it was built with.
        private void TagThemeRoles(Control root)
        {
            foreach (Control c in root.Controls)
            {
                if (c.Tag == null)
                {
                    if (c is Label) c.Tag = c.ForeColor == CMuted ? "muted" : "text";
                    else if (c is Button) c.Tag = c.BackColor == CAccent ? "primary" : c.BackColor == Color.Transparent ? "link" : "secondary";
                    else if (c is ToggleSwitch || c is ThemeSwitch || c is StepRailItem) c.Tag = "custom";
                    else if (c is CheckBox) c.Tag = c.ForeColor == CMuted ? "muted" : "text";
                    else if (c is RichTextBox) c.Tag = "log";
                    else if (c is TextBox || c is NumericUpDown || c is ComboBox) c.Tag = "input";
                    else if (c is ChoiceCard) c.Tag = "choice";
                    else if (c is RoundedPanel) c.Tag = c == _gameStatusCard ? "gamecard" : "card";
                    else if (c is Panel) c.Tag = "page";
                }
                TagThemeRoles(c);
            }
        }

        private void ApplyTheme(Control root, Color oldOk, Color oldBad, Color oldWarn, Color oldMuted)
        {
            if (root == this)
            {
                BackColor = CPage;
                ForeColor = CText;
            }
            foreach (Control c in root.Controls)
            {
                switch (c.Tag as string)
                {
                    case "rail": c.BackColor = CRail; break;
                    case "page": c.BackColor = CPage; break;
                    case "muted": c.ForeColor = CMuted; break;
                    case "text": c.ForeColor = CText; break;
                    case "status":
                        c.ForeColor = c.ForeColor == oldOk ? StatusOk : c.ForeColor == oldWarn ? StatusWarn
                                    : c.ForeColor == oldBad ? StatusBad : c.ForeColor == oldMuted ? CMuted : c.ForeColor;
                        break;
                    case "input": c.BackColor = CInput; c.ForeColor = CText; break;
                    case "primary":
                    {
                        var b = (Button)c;
                        b.BackColor = CAccent;
                        b.ForeColor = Color.White;
                        b.FlatAppearance.MouseOverBackColor = CAccentHover;
                        break;
                    }
                    case "secondary":
                    {
                        var b = (Button)c;
                        b.BackColor = CPage;
                        b.ForeColor = CText;
                        b.FlatAppearance.BorderColor = CBorder;
                        b.FlatAppearance.MouseOverBackColor = CRail;
                        break;
                    }
                    case "link": c.ForeColor = CAccentText; break;
                    case "log": c.BackColor = _darkTheme ? Color.FromArgb(14, 16, 18) : Color.FromArgb(24, 28, 32); break;
                    case "card": ((RoundedPanel)c).FillColor = CRail; ((RoundedPanel)c).BorderColor = CRail; break;
                    case "gamecard": RefreshGameStatusCard(); break;
                    case "choice": ((ChoiceCard)c).ApplyLook(); break;
                }
                c.Invalidate();
                ApplyTheme(c, oldOk, oldBad, oldWarn, oldMuted);
            }
        }

        // Flat button whose disabled state stays readable in both themes (Windows draws disabled flat
        // buttons in a fixed grey that disappears on a dark background).
        private class ThemedButton : Button
        {
            protected override void OnPaint(PaintEventArgs e)
            {
                if (Enabled) { base.OnPaint(e); return; }
                var g = e.Graphics;
                g.Clear(BackColor == Color.Transparent && Parent != null ? Parent.BackColor : BackColor);
                if (FlatAppearance.BorderSize > 0)
                {
                    using (var pen = new Pen(CBorder)) g.DrawRectangle(pen, 0, 0, Width - 1, Height - 1);
                }
                TextRenderer.DrawText(g, Text, Font, ClientRectangle,
                    _darkTheme ? Color.FromArgb(108, 114, 122) : Color.FromArgb(160, 166, 173),
                    TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            }
        }

        // Small segmented control (Light / Dark / System) for the rail.
        private class ThemeSwitch : Control
        {
            private readonly string[] _options;
            private int _selected;

            public event EventHandler Picked;

            public ThemeSwitch(string[] options)
            {
                _options = options;
                DoubleBuffered = true;
                Cursor = Cursors.Hand;
                Font = new Font("Segoe UI", 8.5F);
            }

            public int SelectedIndex { get { return _selected; } set { _selected = value; Invalidate(); } }

            protected override void OnMouseClick(MouseEventArgs e)
            {
                base.OnMouseClick(e);
                int idx = Math.Min(_options.Length - 1, Math.Max(0, e.X * _options.Length / Math.Max(1, Width)));
                if (idx == _selected) return;
                _selected = idx;
                Invalidate();
                if (Picked != null) Picked(this, EventArgs.Empty);
            }

            protected override void OnPaint(PaintEventArgs e)
            {
                var g = e.Graphics;
                g.Clear(Parent != null ? Parent.BackColor : CRail);
                g.SmoothingMode = SmoothingMode.AntiAlias;
                using (var path = RoundRect(new Rectangle(0, 0, Width - 1, Height - 1), 7))
                using (var bg = new SolidBrush(CInput))
                using (var pen = new Pen(CBorder))
                {
                    g.FillPath(bg, path);
                    g.DrawPath(pen, path);
                }
                int w = Width / _options.Length;
                for (int i = 0; i < _options.Length; i++)
                {
                    var r = new Rectangle(i * w + 2, 2, w - 4, Height - 5);
                    if (i == _selected)
                    {
                        using (var path = RoundRect(r, 5))
                        using (var sel = new SolidBrush(CAccentSoft))
                            g.FillPath(sel, path);
                    }
                    TextRenderer.DrawText(g, _options[i], Font, r, i == _selected ? CAccentText : CMuted,
                        TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
                }
            }
        }

        private static GraphicsPath RoundRect(Rectangle r, int radius)
        {
            int d = radius * 2;
            var path = new GraphicsPath();
            path.AddArc(r.X, r.Y, d, d, 180, 90);
            path.AddArc(r.Right - d, r.Y, d, d, 270, 90);
            path.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
            path.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
            path.CloseFigure();
            return path;
        }
    }
}
