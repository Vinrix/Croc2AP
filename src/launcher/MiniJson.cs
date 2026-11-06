using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.Web.Script.Serialization;

namespace Croc2Launcher
{
    public static class MiniJson
    {
        private static readonly JavaScriptSerializer Serializer = new JavaScriptSerializer();

        public static object Deserialize(string json)
        {
            if (string.IsNullOrEmpty(json)) return null;
            return Serializer.DeserializeObject(json);
        }

        public static T Deserialize<T>(string json)
        {
            if (string.IsNullOrEmpty(json)) return default(T);
            return Serializer.Deserialize<T>(json);
        }

        public static string Serialize(object obj)
        {
            if (obj == null) return "null";
            return Serializer.Serialize(obj);
        }
    }
}

