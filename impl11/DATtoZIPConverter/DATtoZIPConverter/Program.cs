using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Media.Imaging;
using DATReader;
using JeremyAnsel.Xwa.Dat;
using System.Runtime.InteropServices;
using System.Windows.Media;

namespace DATtoZIPConverter
{
    public class Converter
    {
        static string m_sTempDirectory = null;
        static string m_sZIPFileName = null;
        static string m_sLastError = null;
        static bool m_bKeepTempDirectory = false;
        static bool m_bConvertToJpeg = false;

        static void MakeZIPFile(string sTempDirectory, string sDATFileName)
        {
            string sDirectory = Path.GetDirectoryName(sDATFileName);
            m_sZIPFileName = Path.GetFileNameWithoutExtension(sDATFileName);
            m_sZIPFileName = Path.Combine(sDirectory, m_sZIPFileName + ".ZIP");

            if (File.Exists(m_sZIPFileName))
            {
                Console.WriteLine("Overwriting ZIP file: " + m_sZIPFileName);
                File.Delete(m_sZIPFileName);
            }
            else
                Console.WriteLine("Creating ZIP file on: " + m_sZIPFileName + "...");
      
            ZipFile.CreateFromDirectory(sTempDirectory, m_sZIPFileName);
            Console.WriteLine("Done");
        }

        /*
         * Creates a Bitmap from a byte[] array.
         * Requires: m_sTempDirectory
         */
        static Bitmap FromByteArray(byte[] data, short Width, short Height)
        {
            System.Drawing.Imaging.PixelFormat pixelFormat = System.Drawing.Imaging.PixelFormat.Format32bppArgb;
            Bitmap result = new Bitmap(Width, Height, pixelFormat);
            BitmapData bmpData = result.LockBits(new Rectangle(0, 0, Width, Height), ImageLockMode.WriteOnly, pixelFormat);

            // Create a Bitmap from data:
            IntPtr ptr = bmpData.Scan0;
            int RowLength = 4 * Width;
            for (int y = 0; y < Height; y++)
            {
                Marshal.Copy(data, y * RowLength, ptr, RowLength);
                ptr += bmpData.Stride;
            }
            result.UnlockBits(bmpData);
            string sTempFile = Path.Combine(m_sTempDirectory, "tmp.png");
            result.Save(sTempFile);
            result.Dispose(); result = null;

            // Now create an sRGB Bitmap from the previously-saved bitmap: this will fix the gamma
            // correction.
            Stream imageStream = new FileStream(sTempFile, FileMode.Open, FileAccess.Read, FileShare.Read);
            /*
            BitmapSource myBitmapSource = BitmapFrame.Create(imageStream);
            ColorContext sourceColorContext = new ColorContext(PixelFormats.Bgra32);
            ColorContext destColorContext = new ColorContext(PixelFormats.Bgra32);
            ColorConvertedBitmap ccb = new ColorConvertedBitmap(myBitmapSource, sourceColorContext, destColorContext, PixelFormats.Pbgra32);
            */
            Bitmap new_bitmap = (Bitmap)Bitmap.FromStream(imageStream);
            imageStream.Close();

            File.Delete(sTempFile);
            return new_bitmap;
        }

        /*
         * If set, the temporary directory will be left behind after the ZIP file has been
         * created. This is useful if specific images need to be updated manually, or if
         * new images will be added later.
         * Default: false
         */
        public static void SetKeepTempDirectory(bool bKeepTempDirectory)
        {
            m_bKeepTempDirectory = bKeepTempDirectory;
            Console.WriteLine("KeepTempDirectory: " + m_bKeepTempDirectory);
        }

        /*
         * Returns the value of m_bKeepTempDirectory.
         */
        public static bool GetKeepTempDirectory()
        {
            return m_bKeepTempDirectory;
        }

        /*
         * If set, all images without transparency will be converted to JPEG format. Otherwise
         * images will be saved as PNG files.
         * Default: false
         */
        public static void SetConvertImagesToJpeg(bool bConvertToJpeg)
        {
            m_bConvertToJpeg = bConvertToJpeg;
            Console.WriteLine("ConvertToJpeg: " + m_bConvertToJpeg);
        }

        public static string GetTempDirectory()
        {
            return m_sTempDirectory;
        }

        public static string GetOutputFileName()
        {
            return m_sZIPFileName;
        }

        public static string GetLastError()
        {
            return m_sLastError;
        }

        /*
         * Main entry point. Call this with a valid DAT file name to create a ZIP file.
         * Returns true if the conversion was successful.
         */
        public static bool ConvertDATFile(string sDATFileName)
        {
            DatFile m_DATFile = null;
            bool bResult = false;

            try
            {
                m_DATFile = DatFile.FromFile(sDATFileName);

                // Generate a new temporary path to make the conversion
                m_sTempDirectory = Path.Combine(Path.GetDirectoryName(sDATFileName),
                                        Path.GetFileNameWithoutExtension(sDATFileName)) + "_dat_to_zip\\";

                // If the temporary directory exists, delete it
                if (Directory.Exists(m_sTempDirectory))
                    Directory.Delete(m_sTempDirectory, true);
                // Create the temporary directory
                Directory.CreateDirectory(m_sTempDirectory);

                // Iterate over each group and dump all its images
                foreach (var Group in m_DATFile.Groups)
                {
                    string sSubDir = Path.Combine(m_sTempDirectory, Group.GroupId.ToString());
                    Directory.CreateDirectory(sSubDir);
                    Console.WriteLine("Processing Group: " + Group.GroupId);

                    // Flip and dump each image in the current group
                    foreach (var Image in Group.Images)
                    {
                        // For some reason, we still have to flip the images upside down before saving
                        // them.
                        // Convert all images to Format25, that way the flipping algorithm is the same for
                        // all formats.
                        bool bHasAlpha = false;
                        Image.ConvertToFormat25();
                        short W = Image.Width;
                        short H = Image.Height;
                        byte[] data = Image.GetImageData();
                        byte[] data_out = new byte[data.Length];
                        UInt32 OfsOut = 0, OfsIn = 0, RowStride = (UInt32)W * 4, RowOfs = (UInt32)(H - 1) * RowStride;
                        //Console.Write("Image: " + Group.GroupId + "-" + Image.ImageId + ", (W,H): (" + W + ", " + H + "), ");
                        //Console.WriteLine("Format: " + Image.Format + ", ColorCount: " + Image.ColorsCount);

                        for (int y = 0; y < H; y++)
                        {
                            OfsIn = RowOfs; // Flip the image
                            for (int x = 0; x < W; x++)
                            {
                                data_out[OfsOut + 0] = data[OfsIn + 0]; // B
                                data_out[OfsOut + 1] = data[OfsIn + 1]; // G
                                data_out[OfsOut + 2] = data[OfsIn + 2]; // R
                                data_out[OfsOut + 3] = data[OfsIn + 3]; // A
                                if (data_out[OfsOut + 3] < 255)
                                    bHasAlpha = true;
                                OfsIn += 4;
                                OfsOut += 4;
                            }
                            // Flip the image and prevent underflows:
                            RowOfs -= RowStride;
                            if (RowOfs < 0) RowOfs = 0;
                        }
                        Console.WriteLine("\tSaving Image: " + Group.GroupId + "-" + Image.ImageId);
                        //Console.WriteLine("\tOutput: " + Path.Combine(sSubDir, Image.ImageId.ToString()) + ".jpg");

                        // The gamma correction is applied in FromByteArray
                        Bitmap new_image = FromByteArray(data_out, W, H);
                        if (m_bConvertToJpeg && !bHasAlpha)
                        {
                            Console.WriteLine("Image " + Group.GroupId + "-" + Image.ImageId + " has no transparency. Saving as Jpeg");
                            new_image.Save(Path.Combine(sSubDir, Image.ImageId.ToString()) + ".jpg", ImageFormat.Jpeg);
                        }
                        else
                            new_image.Save(Path.Combine(sSubDir, Image.ImageId.ToString()) + ".png", ImageFormat.Png);
                        new_image.Dispose(); new_image = null;

                        data = null; data_out = null;
                    }
                }

                // ZIP the new directories into a new ZIP file
                MakeZIPFile(m_sTempDirectory, sDATFileName);

                // Erase the temporary directory
                if (!m_bKeepTempDirectory)
                    Directory.Delete(m_sTempDirectory, true);
                else
                    Console.WriteLine("Kept temporary directory: " + m_sTempDirectory);
                bResult = true;
            }
            catch (Exception e)
            {
                m_sLastError = e.StackTrace;
                Console.WriteLine("Error " + e + " when processing: " + sDATFileName);
                Console.WriteLine(m_sLastError);
                bResult = false;
            }
            finally
            {
                m_DATFile = null;
            }
            return bResult;
        }

        static void Main(string[] args)
        {
            if (args.Length < 1)
            {
                Console.WriteLine("Usage:\n");
                Console.WriteLine("\tDATtoZIPConverter <DATFile> [KeepTempDir] [ConvertToJpeg]\n");
                Console.WriteLine("The output will be a ZIP file with the same name as DATFile, but ending in .ZIP.");
                Console.WriteLine("The ZIP file will be written on the same path as DATFile");
                return;
            }
            Console.WriteLine("Converting " + args[0]);

            if (args.Length > 1)
                SetKeepTempDirectory(bool.Parse(args[1]));

            if (args.Length > 2)
                SetConvertImagesToJpeg(bool.Parse(args[2]));

            ConvertDATFile(args[0]);
        }
    }
}
