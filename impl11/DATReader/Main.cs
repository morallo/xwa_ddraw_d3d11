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
        // Cached DatFile var. This must be loaded before we start inspecting the file
        static DatFile m_DATFile = null;
        // Cached image var. This must be loaded before accessing raw image data.
        // This var is loaded in GetDATImageMetadata below
        static DatImage m_DATImage = null; 

        [DllExport(CallingConvention.Cdecl)]
        public static bool LoadDATFile([MarshalAs(UnmanagedType.LPStr)] string sDatFileName)
        {
            Trace.WriteLine("[DBG] [C#] Loading File: " + sDatFileName);
            try
            {
                m_DATImage = null; // Release any previous instances
                m_DATFile = null; // Release any previous instances
                m_DATFile = DatFile.FromFile(sDatFileName);
            }
            catch (Exception e)
            {
                Trace.WriteLine("[DBG] [C#] Exception " + e + " when opening " + sDatFileName);
                m_DATFile = null;
            }
            return true;
        }

        [DllExport(CallingConvention.Cdecl)]
        public static unsafe bool GetDATImageMetadata(int GroupId, int ImageId, short *Width, short *Height, byte *Format)
        {
            if (m_DATFile == null)
            {
                Trace.WriteLine("[DBG] [C#] Load a DAT file first");
                *Width = 0; *Height = 0; *Format = 0;
                return false;
            }

            //DatFile datFile = DatFile.FromFile(sDatFileName);
            IList<DatGroup> groups = m_DATFile.Groups;
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
                            // Cache the current image
                            m_DATImage = image;
                            *Width = image.Width;
                            *Height = image.Height;
                            *Format = (byte)image.Format;
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
