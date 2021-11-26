using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using DATtoZIPConverter;

namespace DATtoZIPConverterGUI
{
    public partial class DATtoZIPConverterForm : Form
    {
        public DATtoZIPConverterForm()
        {
            InitializeComponent();
        }

        private void SelectFileButton_Click(object sender, EventArgs e)
        {
            if (OpenDATFileDialog.ShowDialog() == DialogResult.OK)
            {
                DATFileNameTextBox.Text = OpenDATFileDialog.FileName;
            }
        }

        private void KeepTempDirChkBox_CheckedChanged(object sender, EventArgs e)
        {
            Converter.SetKeepTempDirectory(KeepTempDirChkBox.Checked);
        }

        private void ConvertToJpegChkBox_CheckedChanged(object sender, EventArgs e)
        {
            Converter.SetConvertImagesToJpeg(ConvertToJpegChkBox.Checked);
        }

        private void ConvertToZIPButton_Click(object sender, EventArgs e)
        {
            // Apply the settings from the check boxes
            Converter.SetKeepTempDirectory(KeepTempDirChkBox.Checked);
            Converter.SetConvertImagesToJpeg(ConvertToJpegChkBox.Checked);

            // Input validation
            string sFileName = DATFileNameTextBox.Text;
            if (sFileName == null || sFileName.Length < 1)
            {
                MessageBox.Show("A DAT file must be selected first", "Missing Data", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            // The text box is not empty, check if the file exists
            if (!File.Exists(sFileName))
            {
                MessageBox.Show("File: " + sFileName + " does not exist", "Invalid File", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            // The file exists, do the conversion
            if (Converter.ConvertDATFile(sFileName))
            {
                // The conversion was successful
                // The user requested to keep the temporary directory.
                if (Converter.GetKeepTempDirectory())
                    MessageBox.Show("File converted to:\n" + Converter.GetOutputFileName() +
                        "\n\nThe temporary directory is at:\n" + Converter.GetTempDirectory(),
                        "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
                else
                    // No temporary directory, we're done.
                    MessageBox.Show("File converted to:\n" + Converter.GetOutputFileName(), "Success",
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
            } else
            {
                // The file could not be converted.
                MessageBox.Show("File " + sFileName + " could not be converted. Error:\n" + Converter.GetLastError(),
                    "Conversion Failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
