using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using JeremyAnsel.Xwa.Dat;

/*
 
 DAT Image Formats:

 - Format 7: 8-bit indexed colors and 1-bit alpha, RLE compressed. 
   Use: concourse or in-flight.

 - Format 23: 8-bit indexed colors and 8-bit alpha, RLE compressed.
   Use: concourse.

 - Format 24: 8-bit indexed colors and 8-bit alpha (uncompressed?) Use: in-flight.

 - Format 25: 32-bit ARGB (uncompressed?) Use: in-flight.

 */

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
                            // Release the previous cached image
                            m_DATImage = null;
                            // Cache the current image
                            m_DATImage = image;
                            // Populate the output values
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

        [DllExport(CallingConvention.Cdecl)]
        public static unsafe bool ReadDATImageData(byte *RawData_out, int RawDataSize)
        {
            if (m_DATImage == null) {
                Trace.WriteLine("[DBG] [C#] Must cache an image first");
                return false;
            }
            short W = m_DATImage.Width;
            short H = m_DATImage.Height;
            byte[] raw_data = m_DATImage.GetImageData();
            Trace.WriteLine("[DBG] [C#] raw_data.len: " + raw_data.Length + ", Format: " + m_DATImage.Format);

            m_DATImage.ConvertToFormat25();
            byte[] data = m_DATImage.GetImageData();
            int len = data.Length;
            Trace.WriteLine("[DBG] [C#] RawData, W*H*4 = " + (W * H * 4) + ", len: " + len + ", Format: " + m_DATImage.Format);

            if (RawData_out == null)
            {
                Trace.WriteLine("[DBG] [C#] ReadDATImageData: output buffer should not be NULL");
                return false;
            }

            try
            {
                int min_len = RawDataSize;
                if (data.Length < min_len) min_len = data.Length;
                for (UInt32 i = 0; i < min_len; i++)
                    RawData_out[i] = data[i];
            }
            catch (Exception e)
            {
                Trace.WriteLine("[DBG] [C#] Exception: " + e + ", caught in ReadDATImageData");
                return false;
            }

            return true;
        }
    }
}
