namespace EsOdbcDsnEditor
{
    partial class DsnEditorForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(DsnEditorForm));
            this.saveButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.testButton = new System.Windows.Forms.Button();
            this.header = new System.Windows.Forms.PictureBox();
            this.certificatePathButton = new System.Windows.Forms.Button();
            this.textCertificatePath = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.radioEnabledFull = new System.Windows.Forms.RadioButton();
            this.radioEnabledHostname = new System.Windows.Forms.RadioButton();
            this.radioEnabledNoHostname = new System.Windows.Forms.RadioButton();
            this.radioEnabledNoValidation = new System.Windows.Forms.RadioButton();
            this.radioButtonDisabled = new System.Windows.Forms.RadioButton();
            this.numericUpDownPort = new System.Windows.Forms.NumericUpDown();
            this.textPassword = new System.Windows.Forms.TextBox();
            this.labelPassword = new System.Windows.Forms.Label();
            this.textUsername = new System.Windows.Forms.TextBox();
            this.labelUsername = new System.Windows.Forms.Label();
            this.textHostname = new System.Windows.Forms.TextBox();
            this.labelPort = new System.Windows.Forms.Label();
            this.labelHostname = new System.Windows.Forms.Label();
            this.certificateFileDialog = new System.Windows.Forms.OpenFileDialog();
            this.tabConfiguration = new System.Windows.Forms.TabControl();
            this.tabBasic = new System.Windows.Forms.TabPage();
            this.textDescription = new System.Windows.Forms.TextBox();
            this.label2 = new System.Windows.Forms.Label();
            this.textName = new System.Windows.Forms.TextBox();
            this.labelName = new System.Windows.Forms.Label();
            this.tabPage2 = new System.Windows.Forms.TabPage();
            ((System.ComponentModel.ISupportInitialize)(this.header)).BeginInit();
            this.groupBox1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownPort)).BeginInit();
            this.tabConfiguration.SuspendLayout();
            this.tabBasic.SuspendLayout();
            this.tabPage2.SuspendLayout();
            this.SuspendLayout();
            // 
            // saveButton
            // 
            this.saveButton.Location = new System.Drawing.Point(457, 541);
            this.saveButton.Margin = new System.Windows.Forms.Padding(4);
            this.saveButton.Name = "saveButton";
            this.saveButton.Size = new System.Drawing.Size(100, 28);
            this.saveButton.TabIndex = 17;
            this.saveButton.Text = "Save";
            this.saveButton.UseVisualStyleBackColor = true;
            this.saveButton.Click += new System.EventHandler(this.SaveButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(560, 541);
            this.cancelButton.Margin = new System.Windows.Forms.Padding(4);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(100, 28);
            this.cancelButton.TabIndex = 18;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.CancelButton_Click);
            // 
            // testButton
            // 
            this.testButton.Location = new System.Drawing.Point(17, 541);
            this.testButton.Margin = new System.Windows.Forms.Padding(4);
            this.testButton.Name = "testButton";
            this.testButton.Size = new System.Drawing.Size(156, 28);
            this.testButton.TabIndex = 16;
            this.testButton.Text = "Test Connection";
            this.testButton.UseVisualStyleBackColor = true;
            this.testButton.Click += new System.EventHandler(this.TestConnectionButton_Click);
            // 
            // header
            // 
            this.header.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("header.BackgroundImage")));
            this.header.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Zoom;
            this.header.InitialImage = null;
            this.header.Location = new System.Drawing.Point(0, 0);
            this.header.Margin = new System.Windows.Forms.Padding(0);
            this.header.Name = "header";
            this.header.Size = new System.Drawing.Size(680, 58);
            this.header.TabIndex = 5;
            this.header.TabStop = false;
            // 
            // certificatePathButton
            // 
            this.certificatePathButton.Location = new System.Drawing.Point(524, 206);
            this.certificatePathButton.Margin = new System.Windows.Forms.Padding(4);
            this.certificatePathButton.Name = "certificatePathButton";
            this.certificatePathButton.Size = new System.Drawing.Size(100, 28);
            this.certificatePathButton.TabIndex = 15;
            this.certificatePathButton.Text = "Browse...";
            this.certificatePathButton.UseVisualStyleBackColor = true;
            this.certificatePathButton.Click += new System.EventHandler(this.CertificatePathButton_Click);
            // 
            // textCertificatePath
            // 
            this.textCertificatePath.Location = new System.Drawing.Point(124, 209);
            this.textCertificatePath.Margin = new System.Windows.Forms.Padding(4);
            this.textCertificatePath.Name = "textCertificatePath";
            this.textCertificatePath.Size = new System.Drawing.Size(392, 22);
            this.textCertificatePath.TabIndex = 14;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(16, 212);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(101, 17);
            this.label1.TabIndex = 11;
            this.label1.Text = "Certificate File:";
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this.radioEnabledFull);
            this.groupBox1.Controls.Add(this.radioEnabledHostname);
            this.groupBox1.Controls.Add(this.radioEnabledNoHostname);
            this.groupBox1.Controls.Add(this.radioEnabledNoValidation);
            this.groupBox1.Controls.Add(this.radioButtonDisabled);
            this.groupBox1.Location = new System.Drawing.Point(16, 16);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(608, 176);
            this.groupBox1.TabIndex = 10;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Secure Sockets Layer (SSL):";
            // 
            // radioEnabledFull
            // 
            this.radioEnabledFull.AutoSize = true;
            this.radioEnabledFull.Location = new System.Drawing.Point(16, 139);
            this.radioEnabledFull.Name = "radioEnabledFull";
            this.radioEnabledFull.Size = new System.Drawing.Size(304, 21);
            this.radioEnabledFull.TabIndex = 13;
            this.radioEnabledFull.TabStop = true;
            this.radioEnabledFull.Text = "Enabled. Certificate Identity chain validated.";
            this.radioEnabledFull.UseVisualStyleBackColor = true;
            // 
            // radioEnabledHostname
            // 
            this.radioEnabledHostname.AutoSize = true;
            this.radioEnabledHostname.Location = new System.Drawing.Point(16, 112);
            this.radioEnabledHostname.Name = "radioEnabledHostname";
            this.radioEnabledHostname.Size = new System.Drawing.Size(362, 21);
            this.radioEnabledHostname.TabIndex = 12;
            this.radioEnabledHostname.TabStop = true;
            this.radioEnabledHostname.Text = "Enabled. Certificate is validated; hostname validated.";
            this.radioEnabledHostname.UseVisualStyleBackColor = true;
            // 
            // radioEnabledNoHostname
            // 
            this.radioEnabledNoHostname.AutoSize = true;
            this.radioEnabledNoHostname.Location = new System.Drawing.Point(16, 85);
            this.radioEnabledNoHostname.Name = "radioEnabledNoHostname";
            this.radioEnabledNoHostname.Size = new System.Drawing.Size(386, 21);
            this.radioEnabledNoHostname.TabIndex = 11;
            this.radioEnabledNoHostname.TabStop = true;
            this.radioEnabledNoHostname.Text = "Enabled. Certificate is validated; hostname not validated.";
            this.radioEnabledNoHostname.UseVisualStyleBackColor = true;
            // 
            // radioEnabledNoValidation
            // 
            this.radioEnabledNoValidation.AutoSize = true;
            this.radioEnabledNoValidation.Location = new System.Drawing.Point(16, 58);
            this.radioEnabledNoValidation.Name = "radioEnabledNoValidation";
            this.radioEnabledNoValidation.Size = new System.Drawing.Size(241, 21);
            this.radioEnabledNoValidation.TabIndex = 10;
            this.radioEnabledNoValidation.TabStop = true;
            this.radioEnabledNoValidation.Text = "Enabled. Certificate not validated.";
            this.radioEnabledNoValidation.UseVisualStyleBackColor = true;
            // 
            // radioButtonDisabled
            // 
            this.radioButtonDisabled.AutoSize = true;
            this.radioButtonDisabled.Location = new System.Drawing.Point(16, 31);
            this.radioButtonDisabled.Name = "radioButtonDisabled";
            this.radioButtonDisabled.Size = new System.Drawing.Size(299, 21);
            this.radioButtonDisabled.TabIndex = 9;
            this.radioButtonDisabled.TabStop = true;
            this.radioButtonDisabled.Text = "Disabled. All communications unencrypted.";
            this.radioButtonDisabled.UseVisualStyleBackColor = true;
            // 
            // numericUpDownPort
            // 
            this.numericUpDownPort.Location = new System.Drawing.Point(112, 246);
            this.numericUpDownPort.Maximum = new decimal(new int[] {
            65535,
            0,
            0,
            0});
            this.numericUpDownPort.Name = "numericUpDownPort";
            this.numericUpDownPort.Size = new System.Drawing.Size(104, 22);
            this.numericUpDownPort.TabIndex = 5;
            this.numericUpDownPort.Value = new decimal(new int[] {
            9200,
            0,
            0,
            0});
            // 
            // textPassword
            // 
            this.textPassword.Location = new System.Drawing.Point(112, 324);
            this.textPassword.Margin = new System.Windows.Forms.Padding(4);
            this.textPassword.Name = "textPassword";
            this.textPassword.Size = new System.Drawing.Size(229, 22);
            this.textPassword.TabIndex = 7;
            // 
            // labelPassword
            // 
            this.labelPassword.AutoSize = true;
            this.labelPassword.Location = new System.Drawing.Point(16, 327);
            this.labelPassword.Name = "labelPassword";
            this.labelPassword.Size = new System.Drawing.Size(73, 17);
            this.labelPassword.TabIndex = 6;
            this.labelPassword.Text = "Password:";
            // 
            // textUsername
            // 
            this.textUsername.Location = new System.Drawing.Point(112, 284);
            this.textUsername.Margin = new System.Windows.Forms.Padding(4);
            this.textUsername.Name = "textUsername";
            this.textUsername.Size = new System.Drawing.Size(229, 22);
            this.textUsername.TabIndex = 6;
            // 
            // labelUsername
            // 
            this.labelUsername.AutoSize = true;
            this.labelUsername.Location = new System.Drawing.Point(16, 287);
            this.labelUsername.Name = "labelUsername";
            this.labelUsername.Size = new System.Drawing.Size(77, 17);
            this.labelUsername.TabIndex = 4;
            this.labelUsername.Text = "Username:";
            // 
            // textHostname
            // 
            this.textHostname.Location = new System.Drawing.Point(112, 208);
            this.textHostname.Margin = new System.Windows.Forms.Padding(4);
            this.textHostname.Name = "textHostname";
            this.textHostname.Size = new System.Drawing.Size(506, 22);
            this.textHostname.TabIndex = 4;
            // 
            // labelPort
            // 
            this.labelPort.AutoSize = true;
            this.labelPort.Location = new System.Drawing.Point(16, 248);
            this.labelPort.Name = "labelPort";
            this.labelPort.Size = new System.Drawing.Size(38, 17);
            this.labelPort.TabIndex = 2;
            this.labelPort.Text = "Port:";
            // 
            // labelHostname
            // 
            this.labelHostname.AutoSize = true;
            this.labelHostname.Location = new System.Drawing.Point(16, 211);
            this.labelHostname.Name = "labelHostname";
            this.labelHostname.Size = new System.Drawing.Size(76, 17);
            this.labelHostname.TabIndex = 0;
            this.labelHostname.Text = "Hostname:";
            // 
            // certificateFileDialog
            // 
            this.certificateFileDialog.Filter = "X509 Certificate|*.pem;*.der|All Files|*.*";
            // 
            // tabConfiguration
            // 
            this.tabConfiguration.Controls.Add(this.tabBasic);
            this.tabConfiguration.Controls.Add(this.tabPage2);
            this.tabConfiguration.Location = new System.Drawing.Point(16, 74);
            this.tabConfiguration.Name = "tabConfiguration";
            this.tabConfiguration.SelectedIndex = 0;
            this.tabConfiguration.Size = new System.Drawing.Size(648, 460);
            this.tabConfiguration.TabIndex = 8;
            // 
            // tabBasic
            // 
            this.tabBasic.Controls.Add(this.textDescription);
            this.tabBasic.Controls.Add(this.label2);
            this.tabBasic.Controls.Add(this.textName);
            this.tabBasic.Controls.Add(this.labelName);
            this.tabBasic.Controls.Add(this.textHostname);
            this.tabBasic.Controls.Add(this.labelHostname);
            this.tabBasic.Controls.Add(this.labelPort);
            this.tabBasic.Controls.Add(this.labelUsername);
            this.tabBasic.Controls.Add(this.textUsername);
            this.tabBasic.Controls.Add(this.numericUpDownPort);
            this.tabBasic.Controls.Add(this.labelPassword);
            this.tabBasic.Controls.Add(this.textPassword);
            this.tabBasic.Location = new System.Drawing.Point(4, 25);
            this.tabBasic.Name = "tabBasic";
            this.tabBasic.Padding = new System.Windows.Forms.Padding(3);
            this.tabBasic.Size = new System.Drawing.Size(640, 431);
            this.tabBasic.TabIndex = 0;
            this.tabBasic.Text = "Basic";
            this.tabBasic.UseVisualStyleBackColor = true;
            // 
            // textDescription
            // 
            this.textDescription.Location = new System.Drawing.Point(112, 56);
            this.textDescription.Margin = new System.Windows.Forms.Padding(4);
            this.textDescription.Multiline = true;
            this.textDescription.Name = "textDescription";
            this.textDescription.Size = new System.Drawing.Size(506, 132);
            this.textDescription.TabIndex = 3;
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(16, 59);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(83, 17);
            this.label2.TabIndex = 10;
            this.label2.Text = "Description:";
            // 
            // textName
            // 
            this.textName.Location = new System.Drawing.Point(112, 16);
            this.textName.Margin = new System.Windows.Forms.Padding(4);
            this.textName.Name = "textName";
            this.textName.Size = new System.Drawing.Size(506, 22);
            this.textName.TabIndex = 2;
            // 
            // labelName
            // 
            this.labelName.AutoSize = true;
            this.labelName.Location = new System.Drawing.Point(16, 21);
            this.labelName.Name = "labelName";
            this.labelName.Size = new System.Drawing.Size(49, 17);
            this.labelName.TabIndex = 8;
            this.labelName.Text = "Name:";
            // 
            // tabPage2
            // 
            this.tabPage2.Controls.Add(this.certificatePathButton);
            this.tabPage2.Controls.Add(this.groupBox1);
            this.tabPage2.Controls.Add(this.textCertificatePath);
            this.tabPage2.Controls.Add(this.label1);
            this.tabPage2.Location = new System.Drawing.Point(4, 25);
            this.tabPage2.Name = "tabPage2";
            this.tabPage2.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage2.Size = new System.Drawing.Size(640, 431);
            this.tabPage2.TabIndex = 1;
            this.tabPage2.Text = "Security";
            this.tabPage2.UseVisualStyleBackColor = true;
            // 
            // DsnEditorForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(682, 588);
            this.Controls.Add(this.tabConfiguration);
            this.Controls.Add(this.header);
            this.Controls.Add(this.testButton);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.saveButton);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(4);
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "DsnEditorForm";
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.Text = "Elasticsearch ODBC DSN Configuration";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.DsnEditorForm_FormClosing);
            ((System.ComponentModel.ISupportInitialize)(this.header)).EndInit();
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownPort)).EndInit();
            this.tabConfiguration.ResumeLayout(false);
            this.tabBasic.ResumeLayout(false);
            this.tabBasic.PerformLayout();
            this.tabPage2.ResumeLayout(false);
            this.tabPage2.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion
        private System.Windows.Forms.Button saveButton;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.Button testButton;
        private System.Windows.Forms.PictureBox header;
        private System.Windows.Forms.TextBox textHostname;
        private System.Windows.Forms.Label labelPort;
        private System.Windows.Forms.Label labelHostname;
        private System.Windows.Forms.TextBox textPassword;
        private System.Windows.Forms.Label labelPassword;
        private System.Windows.Forms.TextBox textUsername;
        private System.Windows.Forms.Label labelUsername;
        private System.Windows.Forms.NumericUpDown numericUpDownPort;
        private System.Windows.Forms.RadioButton radioButtonDisabled;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.RadioButton radioEnabledFull;
        private System.Windows.Forms.RadioButton radioEnabledHostname;
        private System.Windows.Forms.RadioButton radioEnabledNoHostname;
        private System.Windows.Forms.RadioButton radioEnabledNoValidation;
        private System.Windows.Forms.Button certificatePathButton;
        private System.Windows.Forms.TextBox textCertificatePath;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.OpenFileDialog certificateFileDialog;
        private System.Windows.Forms.TabControl tabConfiguration;
        private System.Windows.Forms.TabPage tabBasic;
        private System.Windows.Forms.TabPage tabPage2;
        private System.Windows.Forms.TextBox textDescription;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.TextBox textName;
        private System.Windows.Forms.Label labelName;
    }
}

