using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using JeremyAnsel.Xwa.Dat;

namespace DATReader
{
    public class Main
    {

        [DllExport("LoadDATImage", CallingConvention.Cdecl)]
        public static bool LoadDATImage([MarshalAs(UnmanagedType.LPStr)] string sDatFileName, int GroupId, int ImageId)
        {
            Trace.WriteLine("[DBG] [C#] Loading File: " + sDatFileName);
            DatFile datFile = DatFile.FromFile(sDatFileName);
            Trace.WriteLine("[DBG] [C#] CHECK 1");
            IList<DatGroup> groups = datFile.Groups;
            Trace.WriteLine("[DBG] [C#] groups.Count: " + groups.Count());
            foreach (var group in groups)
            {
                Trace.WriteLine("[DBG] [C#] GroupID: " + group.GroupId);
                if (group.GroupId == GroupId)
                {
                    IList<DatImage> images = group.Images;
                    foreach (var image in images)
                    {
                        if (image.ImageId == ImageId)
                        {
                            Trace.WriteLine("[DBG] [C#] Success. Found " + GroupId + "-" + ImageId + ", " +
                                "MetaData: (" + image.Width + ", " + image.Height + "), Type: " + image.Format);
                            return true;
                        }
                    }
                }
            }
            return false;
        }
    }
}
