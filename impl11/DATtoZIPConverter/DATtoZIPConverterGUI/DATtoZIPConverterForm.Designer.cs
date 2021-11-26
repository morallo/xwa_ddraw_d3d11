namespace DATtoZIPConverterGUI
{
    partial class DATtoZIPConverterForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(DATtoZIPConverterForm));
            this.SelectFileButton = new System.Windows.Forms.Button();
            this.DATFileNameTextBox = new System.Windows.Forms.TextBox();
            this.ConvertToZIPButton = new System.Windows.Forms.Button();
            this.KeepTempDirChkBox = new System.Windows.Forms.CheckBox();
            this.ConvertToJpegChkBox = new System.Windows.Forms.CheckBox();
            this.OpenDATFileDialog = new System.Windows.Forms.OpenFileDialog();
            this.ToolTipContainer = new System.Windows.Forms.ToolTip(this.components);
            this.SuspendLayout();
            // 
            // SelectFileButton
            // 
            this.SelectFileButton.Location = new System.Drawing.Point(639, 12);
            this.SelectFileButton.Name = "SelectFileButton";
            this.SelectFileButton.Size = new System.Drawing.Size(108, 23);
            this.SelectFileButton.TabIndex = 0;
            this.SelectFileButton.Text = "&Open DAT File";
            this.SelectFileButton.UseVisualStyleBackColor = true;
            this.SelectFileButton.Click += new System.EventHandler(this.SelectFileButton_Click);
            // 
            // DATFileNameTextBox
            // 
            this.DATFileNameTextBox.Location = new System.Drawing.Point(12, 12);
            this.DATFileNameTextBox.Name = "DATFileNameTextBox";
            this.DATFileNameTextBox.Size = new System.Drawing.Size(621, 20);
            this.DATFileNameTextBox.TabIndex = 1;
            // 
            // ConvertToZIPButton
            // 
            this.ConvertToZIPButton.Location = new System.Drawing.Point(639, 41);
            this.ConvertToZIPButton.Name = "ConvertToZIPButton";
            this.ConvertToZIPButton.Size = new System.Drawing.Size(108, 23);
            this.ConvertToZIPButton.TabIndex = 2;
            this.ConvertToZIPButton.Text = "&Convert to ZIP";
            this.ConvertToZIPButton.UseVisualStyleBackColor = true;
            this.ConvertToZIPButton.Click += new System.EventHandler(this.ConvertToZIPButton_Click);
            // 
            // KeepTempDirChkBox
            // 
            this.KeepTempDirChkBox.AutoSize = true;
            this.KeepTempDirChkBox.Location = new System.Drawing.Point(12, 42);
            this.KeepTempDirChkBox.Name = "KeepTempDirChkBox";
            this.KeepTempDirChkBox.Size = new System.Drawing.Size(128, 17);
            this.KeepTempDirChkBox.TabIndex = 3;
            this.KeepTempDirChkBox.Text = "Keep &Temporary Files";
            this.ToolTipContainer.SetToolTip(this.KeepTempDirChkBox, "Use this to manually edit the contents of the ZIP file later");
            this.KeepTempDirChkBox.UseVisualStyleBackColor = true;
            this.KeepTempDirChkBox.CheckedChanged += new System.EventHandler(this.KeepTempDirChkBox_CheckedChanged);
            // 
            // ConvertToJpegChkBox
            // 
            this.ConvertToJpegChkBox.AutoSize = true;
            this.ConvertToJpegChkBox.Location = new System.Drawing.Point(200, 42);
            this.ConvertToJpegChkBox.Name = "ConvertToJpegChkBox";
            this.ConvertToJpegChkBox.Size = new System.Drawing.Size(154, 17);
            this.ConvertToJpegChkBox.TabIndex = 4;
            this.ConvertToJpegChkBox.Text = "Convert to &JPEG if possible";
            this.ToolTipContainer.SetToolTip(this.ConvertToJpegChkBox, "Converts images with no transparency to JPG format, otherwise images are saved as" +
        " PNG. If not set, all images will be saved as PNGs");
            this.ConvertToJpegChkBox.UseVisualStyleBackColor = true;
            this.ConvertToJpegChkBox.CheckedChanged += new System.EventHandler(this.ConvertToJpegChkBox_CheckedChanged);
            // 
            // OpenDATFileDialog
            // 
            this.OpenDATFileDialog.DefaultExt = "DAT";
            this.OpenDATFileDialog.Filter = "DAT files|*.dat|All files|*.*";
            // 
            // DATtoZIPConverterForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(759, 74);
            this.Controls.Add(this.ConvertToJpegChkBox);
            this.Controls.Add(this.KeepTempDirChkBox);
            this.Controls.Add(this.ConvertToZIPButton);
            this.Controls.Add(this.DATFileNameTextBox);
            this.Controls.Add(this.SelectFileButton);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "DATtoZIPConverterForm";
            this.Text = "DAT to ZIP Converter";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button SelectFileButton;
        private System.Windows.Forms.TextBox DATFileNameTextBox;
        private System.Windows.Forms.Button ConvertToZIPButton;
        private System.Windows.Forms.CheckBox KeepTempDirChkBox;
        private System.Windows.Forms.ToolTip ToolTipContainer;
        private System.Windows.Forms.CheckBox ConvertToJpegChkBox;
        private System.Windows.Forms.OpenFileDialog OpenDATFileDialog;
    }
}

