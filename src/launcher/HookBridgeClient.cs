using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Croc2Launcher
{
    public class HookBridgeClient
    {
        private TcpClient _client;
        private NetworkStream _stream;
        private StreamReader _reader;
        private StreamWriter _writer;
        private readonly SemaphoreSlim _lock = new SemaphoreSlim(1, 1);

        public bool IsConnected
        {
            get { return _client != null && _client.Connected; }
        }

        public string Host { get; set; }
        public int Port { get; set; }

        public HookBridgeClient(string host = "127.0.0.1", int port = 43825)
        {
            Host = host;
            Port = port;
        }

        public async Task<bool> ConnectAsync()
        {
            Disconnect();
            try
            {
                _client = new TcpClient();
                await _client.ConnectAsync(Host, Port);
                _stream = _client.GetStream();
                _reader = new StreamReader(_stream, Encoding.UTF8);
                _writer = new StreamWriter(_stream, Encoding.UTF8) { AutoFlush = true };

                var req = new Dictionary<string, object>
                {
                    { "command", "client_hello" },
                    { "version", "0.2.0" }
                };

                var resp = await SendCommandAsync(req);
                return resp != null;
            }
            catch
            {
                Disconnect();
                return false;
            }
        }

        public void Disconnect()
        {
            try { if (_reader != null) _reader.Dispose(); } catch { }
            try { if (_writer != null) _writer.Dispose(); } catch { }
            try { if (_stream != null) _stream.Dispose(); } catch { }
            try { if (_client != null) _client.Close(); } catch { }

            _reader = null;
            _writer = null;
            _stream = null;
            _client = null;
        }

        public async Task<Dictionary<string, object>> SendCommandAsync(Dictionary<string, object> payload)
        {
            if (!IsConnected || _writer == null || _reader == null) return null;

            await _lock.WaitAsync();
            try
            {
                string line = MiniJson.Serialize(payload) + "\n";
                await _writer.WriteAsync(line);
                string respLine = await _reader.ReadLineAsync();
                if (string.IsNullOrEmpty(respLine))
                {
                    Disconnect();
                    return null;
                }

                object root = MiniJson.Deserialize(respLine);
                if (root is Dictionary<string, object>)
                {
                    return (Dictionary<string, object>)root;
                }
                return null;
            }
            catch
            {
                Disconnect();
                return null;
            }
            finally
            {
                _lock.Release();
            }
        }

        public async Task<bool> ConfigureSessionAsync(Dictionary<string, object> slotData)
        {
            if (slotData == null) return false;

            // Handle door map if present
            object doorMapObj;
            if (slotData.TryGetValue("door_map", out doorMapObj) && doorMapObj is Dictionary<string, object>)
            {
                var doorMap = (Dictionary<string, object>)doorMapObj;
                foreach (var kvp in doorMap)
                {
                    var doorReq = new Dictionary<string, object>
                    {
                        { "command", "set_door_destination" },
                        { "source_door", kvp.Key },
                        { "target_wad", kvp.Value != null ? kvp.Value.ToString() : "" }
                    };
                    await SendCommandAsync(doorReq);
                }
            }

            var cfgReq = new Dictionary<string, object>
            {
                { "command", "configure_session" },
                { "colored_crystals_mode", slotData.ContainsKey("colored_crystals_mode") ? Convert.ToInt32(slotData["colored_crystals_mode"]) : 1 },
                { "lock_level_access", slotData.ContainsKey("lock_level_access") ? Convert.ToInt32(slotData["lock_level_access"]) : 1 },
                { "shuffle_white_crystals", slotData.ContainsKey("shuffle_white_crystals") ? Convert.ToInt32(slotData["shuffle_white_crystals"]) : 1 },
                { "shuffle_abilities", slotData.ContainsKey("shuffle_abilities") ? Convert.ToInt32(slotData["shuffle_abilities"]) : 1 },
                { "shuffle_levers", slotData.ContainsKey("shuffle_levers") ? Convert.ToInt32(slotData["shuffle_levers"]) : 1 },
                { "shuffle_vehicles", slotData.ContainsKey("shuffle_vehicles") ? Convert.ToInt32(slotData["shuffle_vehicles"]) : 1 },
                { "shuffle_plane_guns", slotData.ContainsKey("shuffle_plane_guns") ? Convert.ToInt32(slotData["shuffle_plane_guns"]) : 1 },
                { "shuffle_pete_store", slotData.ContainsKey("shuffle_pete_store") ? Convert.ToInt32(slotData["shuffle_pete_store"]) : 2 }
            };

            var resp = await SendCommandAsync(cfgReq);
            return resp != null;
        }

        // Every check stored in the current save (sent again after (re)connecting)
        public async Task<List<long>> GetClaimedLocationsAsync()
        {
            var result = new List<long>();
            var resp = await SendCommandAsync(new Dictionary<string, object> { { "command", "get_claimed_locations" } });
            object locs;
            if (resp != null && resp.TryGetValue("locations", out locs) && locs is IEnumerable && !(locs is string))
            {
                foreach (object o in (IEnumerable)locs) result.Add(Convert.ToInt64(o));
            }
            return result;
        }

        public async Task SetDeathLinkAsync(bool enabled)
        {
            await SendCommandAsync(new Dictionary<string, object> { { "command", "set_death_link" }, { "enabled", enabled ? 1 : 0 } });
        }

        public async Task<bool> DeathLinkKillAsync(string source, string cause)
        {
            var resp = await SendCommandAsync(new Dictionary<string, object>
            {
                { "command", "deathlink_kill" }, { "source", source ?? "" }, { "cause", cause ?? "" }
            });
            return resp != null;
        }

        public async Task<bool> GiveItemAsync(int itemId, string sender, string location, string classification, int itemIndex)
        {
            string itemName = Croc2Data.GetItemName(itemId);
            var req = new Dictionary<string, object>
            {
                { "command", "give_item" },
                { "item_id", itemId },
                { "item_name", itemName },
                { "sender", sender },
                { "location", location },
                { "classification", classification },
                { "item_index", itemIndex }
            };

            var resp = await SendCommandAsync(req);
            return resp != null;
        }

        public async Task<bool> NotifyItemSentAsync(string itemName, string receiver, string location, string classification)
        {
            var req = new Dictionary<string, object>
            {
                { "command", "notify_item_sent" },
                { "item_name", itemName },
                { "receiver", receiver },
                { "location", location },
                { "classification", classification }
            };

            var resp = await SendCommandAsync(req);
            return resp != null;
        }

        public async Task<List<Dictionary<string, object>>> PollEventsDetailedAsync()
        {
            var list = new List<Dictionary<string, object>>();
            var req = new Dictionary<string, object>
            {
                { "command", "poll_events" }
            };

            var resp = await SendCommandAsync(req);
            if (resp != null && resp.ContainsKey("events"))
            {
                object evListObj = resp["events"];
                if (evListObj is IEnumerable)
                {
                    foreach (object evObj in (IEnumerable)evListObj)
                    {
                        if (evObj is Dictionary<string, object>)
                        {
                            list.Add((Dictionary<string, object>)evObj);
                        }
                    }
                }
            }
            return list;
        }

        public async Task<List<long>> PollCheckedLocationsAsync()
        {
            var list = new List<long>();
            var events = await PollEventsDetailedAsync();
            foreach (var evDict in events)
            {
                string evType = evDict.ContainsKey("event") ? evDict["event"].ToString() : "";
                if (evType == "location_checked" && evDict.ContainsKey("location_id"))
                {
                    long locId = Convert.ToInt64(evDict["location_id"]);
                    list.Add(locId);
                }
            }
            return list;
        }

        public async Task<bool> SetYamlPlayersAsync(IEnumerable<string> playerNames)
        {
            var list = new List<string>(playerNames);
            var req = new Dictionary<string, object>
            {
                { "command", "set_yaml_players" },
                { "players", list }
            };

            var resp = await SendCommandAsync(req);
            return resp != null;
        }
    }
}

