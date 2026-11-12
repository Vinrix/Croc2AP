using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Croc2Launcher
{
    public class ApNetworkItem
    {
        public int ItemId { get; set; }
        public long LocationId { get; set; }
        public int Player { get; set; }
        public int Flags { get; set; }
    }

    public class ApItemSendInfo
    {
        public int ItemId { get; set; }
        public long LocationId { get; set; }
        public int FinderSlot { get; set; }
        public int ReceivingSlot { get; set; }
        public int Flags { get; set; }
    }

    public class ApPacket
    {
        public string Cmd { get; set; }
        public Dictionary<string, object> Data { get; set; }

        public ApPacket(string cmd, Dictionary<string, object> data)
        {
            Cmd = cmd;
            Data = data ?? new Dictionary<string, object>();
        }

        public string GetString(string key, string fallback = "")
        {
            object val;
            if (Data.TryGetValue(key, out val) && val != null) return val.ToString();
            return fallback;
        }

        public int GetInt(string key, int fallback = 0)
        {
            object val;
            if (Data.TryGetValue(key, out val) && val != null)
            {
                int res;
                if (int.TryParse(val.ToString(), out res)) return res;
            }
            return fallback;
        }

        public Dictionary<string, object> GetDict(string key)
        {
            object val;
            if (Data.TryGetValue(key, out val) && val is Dictionary<string, object>)
            {
                return (Dictionary<string, object>)val;
            }
            return new Dictionary<string, object>();
        }
    }

    public class ApClient
    {
        private ClientWebSocket _ws;
        private CancellationTokenSource _cts;
        private Task _receiveTask;

        public bool IsConnected { get; private set; }
        public int OurSlot { get; private set; }
        public string OurSlotName { get; private set; }
        public Dictionary<int, string> SlotToPlayerName { get; private set; }

        public event Action<string, bool> OnLog;
        public event Action<string> OnStatusChanged;
        public event Action<Dictionary<string, object>> OnConnected;
        public event Action<List<ApNetworkItem>, int> OnItemsReceived;
        public event Action<ApItemSendInfo> OnItemSent;

        public ApClient()
        {
            SlotToPlayerName = new Dictionary<int, string>();
        }

        private void Log(string msg, bool isErr = false)
        {
            if (OnLog != null) OnLog(msg, isErr);
        }

        private void SetStatus(string status)
        {
            if (OnStatusChanged != null) OnStatusChanged(status);
        }

        public async Task<bool> ConnectAsync(string host, int port, string slotName, string password)
        {
            await DisconnectAsync();

            OurSlotName = slotName;
            OurSlot = -1;
            SlotToPlayerName.Clear();

            _cts = new CancellationTokenSource();
            _ws = new ClientWebSocket();

            string[] schemes = new string[] { "ws", "wss" };
            if (host.IndexOf(".gg", StringComparison.OrdinalIgnoreCase) >= 0 ||
                host.IndexOf(".com", StringComparison.OrdinalIgnoreCase) >= 0)
            {
                schemes = new string[] { "wss", "ws" };
            }

            bool connected = false;
            foreach (string scheme in schemes)
            {
                try
                {
                    string uriStr = scheme + "://" + host + ":" + port;
                    Log("Connecting to " + uriStr + "...");
                    SetStatus("Connecting (" + uriStr + ")...");
                    var uri = new Uri(uriStr);
                    await _ws.ConnectAsync(uri, _cts.Token);
                    connected = true;
                    Log("WebSocket connection established!");
                    break;
                }
                catch (Exception ex)
                {
                    Log("Failed with " + scheme + "://: " + ex.Message, true);
                    _ws.Dispose();
                    _ws = new ClientWebSocket();
                }
            }

            if (!connected)
            {
                SetStatus("Connection Failed");
                return false;
            }

            IsConnected = true;
            SetStatus("Handshaking...");
            _receiveTask = Task.Run(() => ReceiveLoopAsync(slotName, password, _cts.Token));
            return true;
        }

        public async Task DisconnectAsync()
        {
            IsConnected = false;
            if (_cts != null)
            {
                _cts.Cancel();
            }

            if (_ws != null)
            {
                try
                {
                    if (_ws.State == WebSocketState.Open || _ws.State == WebSocketState.CloseReceived)
                    {
                        await _ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "Client closing", CancellationToken.None);
                    }
                }
                catch { }
                _ws.Dispose();
                _ws = null;
            }

            SetStatus("Disconnected");
        }

        private async Task ReceiveLoopAsync(string slotName, string password, CancellationToken ct)
        {
            var buffer = new byte[16384];
            var ms = new MemoryStream();

            try
            {
                while (_ws != null && _ws.State == WebSocketState.Open && !ct.IsCancellationRequested)
                {
                    WebSocketReceiveResult result;
                    do
                    {
                        result = await _ws.ReceiveAsync(new ArraySegment<byte>(buffer), ct);
                        if (result.MessageType == WebSocketMessageType.Close)
                        {
                            await _ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "Acknowledge Close", CancellationToken.None);
                            IsConnected = false;
                            SetStatus("Server Closed Connection");
                            return;
                        }
                        ms.Write(buffer, 0, result.Count);
                    } while (!result.EndOfMessage);

                    string text = Encoding.UTF8.GetString(ms.ToArray());
                    ms.SetLength(0);

                    ProcessMessage(text, slotName, password);
                }
            }
            catch (Exception ex)
            {
                if (!ct.IsCancellationRequested)
                {
                    Log("WebSocket Error: " + ex.Message, true);
                    SetStatus("Connection Lost");
                }
            }
            finally
            {
                IsConnected = false;
                ms.Dispose();
            }
        }

        private void ProcessMessage(string json, string slotName, string password)
        {
            object root = MiniJson.Deserialize(json);
            if (!(root is IEnumerable) || (root is string) || (root is IDictionary)) return;

            foreach (object item in (IEnumerable)root)
            {
                if (!(item is Dictionary<string, object>)) continue;
                var dict = (Dictionary<string, object>)item;
                string cmd = dict.ContainsKey("cmd") ? dict["cmd"].ToString() : "";
                var packet = new ApPacket(cmd, dict);
                HandlePacket(packet, slotName, password);
            }
        }

        private void HandlePacket(ApPacket packet, string slotName, string password)
        {
            switch (packet.Cmd)
            {
                case "RoomInfo":
                    Log("RoomInfo received. Sending Connect packet for '" + slotName + "'...");
                    SendConnect(slotName, password);
                    break;

                case "Connected":
                    OurSlot = packet.GetInt("slot", 1);
                    Log("Connected successfully to Archipelago room as slot " + OurSlot + " ('" + slotName + "')!");
                    SetStatus("Connected (" + slotName + ")");

                    // Parse players list
                    object playersObj;
                    if (packet.Data.TryGetValue("players", out playersObj) && playersObj is IEnumerable)
                    {
                        foreach (object p in (IEnumerable)playersObj)
                        {
                            if (p is Dictionary<string, object>)
                            {
                                var pdict = (Dictionary<string, object>)p;
                                int s = pdict.ContainsKey("slot") ? Convert.ToInt32(pdict["slot"]) : 0;
                                string name = pdict.ContainsKey("name") ? pdict["name"].ToString() : "";
                                string alias = pdict.ContainsKey("alias") ? pdict["alias"].ToString() : "";
                                if (s > 0)
                                {
                                    SlotToPlayerName[s] = !string.IsNullOrEmpty(alias) ? alias : name;
                                }
                            }
                        }
                    }

                    var slotData = packet.GetDict("slot_data");
                    if (OnConnected != null) OnConnected(slotData);
                    break;

                case "ConnectionRefused":
                    object errors;
                    string errMsg = "Connection Refused";
                    if (packet.Data.TryGetValue("errors", out errors) && errors is IEnumerable)
                    {
                        var list = new List<string>();
                        foreach (object err in (IEnumerable)errors) list.Add(err.ToString());
                        errMsg += ": " + string.Join(", ", list.ToArray());
                    }
                    Log(errMsg, true);
                    SetStatus("Refused: " + errMsg);
                    break;

                case "ReceivedItems":
                    int index = packet.GetInt("index", 0);
                    object itemsObj;
                    if (packet.Data.TryGetValue("items", out itemsObj) && itemsObj is IEnumerable)
                    {
                        var items = new List<ApNetworkItem>();
                        foreach (object itm in (IEnumerable)itemsObj)
                        {
                            if (itm is Dictionary<string, object>)
                            {
                                var d = (Dictionary<string, object>)itm;
                                var netItem = new ApNetworkItem
                                {
                                    ItemId = d.ContainsKey("item") ? Convert.ToInt32(d["item"]) : 0,
                                    LocationId = d.ContainsKey("location") ? Convert.ToInt64(d["location"]) : 0,
                                    Player = d.ContainsKey("player") ? Convert.ToInt32(d["player"]) : 0,
                                    Flags = d.ContainsKey("flags") ? Convert.ToInt32(d["flags"]) : 0
                                };
                                items.Add(netItem);
                            }
                        }
                        if (OnItemsReceived != null) OnItemsReceived(items, index);
                    }
                    break;

                case "PrintJSON":
                    string type = packet.GetString("type");
                    if (type == "ItemSend")
                    {
                        object itemObj;
                        if (packet.Data.TryGetValue("item", out itemObj) && itemObj is Dictionary<string, object>)
                        {
                            var itmDict = (Dictionary<string, object>)itemObj;
                            var info = new ApItemSendInfo
                            {
                                ItemId = itmDict.ContainsKey("item") ? Convert.ToInt32(itmDict["item"]) : 0,
                                LocationId = itmDict.ContainsKey("location") ? Convert.ToInt64(itmDict["location"]) : 0,
                                FinderSlot = itmDict.ContainsKey("player") ? Convert.ToInt32(itmDict["player"]) : 0,
                                ReceivingSlot = packet.GetInt("receiving", 0),
                                Flags = itmDict.ContainsKey("flags") ? Convert.ToInt32(itmDict["flags"]) : 0
                            };
                            if (OnItemSent != null) OnItemSent(info);
                        }
                    }
                    break;
            }
        }

        private async void SendConnect(string slotName, string password)
        {
            var dict = new Dictionary<string, object>();
            dict["cmd"] = "Connect";
            dict["password"] = string.IsNullOrEmpty(password) ? "" : password;
            dict["game"] = "Croc 2";
            dict["name"] = slotName;
            dict["uuid"] = "croc2-ap-launcher-" + Environment.TickCount;
            var ver = new Dictionary<string, object>();
            ver["major"] = 0;
            ver["minor"] = 6;
            ver["build"] = 7;
            ver["class"] = "Version";
            dict["version"] = ver;
            dict["items_handling"] = 7;
            dict["tags"] = new string[] { "AP", "Launcher" };
            dict["slot_data"] = true;

            var list = new List<object> { dict };
            string json = MiniJson.Serialize(list);
            await SendRawAsync(json);
        }

        public async Task SendLocationChecksAsync(IEnumerable<long> locations)
        {
            var listLocs = new List<long>(locations);
            if (listLocs.Count == 0) return;

            var dict = new Dictionary<string, object>();
            dict["cmd"] = "LocationChecks";
            dict["locations"] = listLocs;

            var list = new List<object> { dict };
            string json = MiniJson.Serialize(list);
            await SendRawAsync(json);
        }

        public const int ClientGoal = 30; // Archipelago ClientStatus.CLIENT_GOAL

        public async Task SendStatusUpdateAsync(int status)
        {
            var dict = new Dictionary<string, object>();
            dict["cmd"] = "StatusUpdate";
            dict["status"] = status;
            await SendRawAsync(MiniJson.Serialize(new List<object> { dict }));
        }

        public async Task SendRawAsync(string json)
        {
            if (_ws != null && _ws.State == WebSocketState.Open)
            {
                byte[] bytes = Encoding.UTF8.GetBytes(json);
                await _ws.SendAsync(new ArraySegment<byte>(bytes), WebSocketMessageType.Text, true, CancellationToken.None);
            }
        }

        public string GetPlayerName(int slot)
        {
            if (slot == OurSlot && !string.IsNullOrEmpty(OurSlotName)) return OurSlotName;
            string name;
            if (SlotToPlayerName.TryGetValue(slot, out name) && !string.IsNullOrEmpty(name)) return name;
            return "Player " + slot;
        }
    }
}

