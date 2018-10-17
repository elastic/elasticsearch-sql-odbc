/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

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
            this.labelCertificatePath = new System.Windows.Forms.Label();
            this.groupSSL = new System.Windows.Forms.GroupBox();
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
            this.pageBasic = new System.Windows.Forms.TabPage();
            this.textDescription = new System.Windows.Forms.TextBox();
            this.labelDescription = new System.Windows.Forms.Label();
            this.textName = new System.Windows.Forms.TextBox();
            this.labelName = new System.Windows.Forms.Label();
            this.pageSecurity = new System.Windows.Forms.TabPage();
            this.pageLogging = new System.Windows.Forms.TabPage();
            this.checkLoggingEnabled = new System.Windows.Forms.CheckBox();
            this.comboLogLevel = new System.Windows.Forms.ComboBox();
            this.labelLogLevel = new System.Windows.Forms.Label();
            this.logDirectoryPathButton = new System.Windows.Forms.Button();
            this.textLogDirectoryPath = new System.Windows.Forms.TextBox();
            this.labelLogDirectory = new System.Windows.Forms.Label();
            this.folderLogDirectoryDialog = new System.Windows.Forms.FolderBrowserDialog();
            ((System.ComponentModel.ISupportInitialize)(this.header)).BeginInit();
            this.groupSSL.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownPort)).BeginInit();
            this.tabConfiguration.SuspendLayout();
            this.pageBasic.SuspendLayout();
            this.pageSecurity.SuspendLayout();
            this.pageLogging.SuspendLayout();
            this.SuspendLayout();
            // 
            // saveButton
            // 
            this.saveButton.Location = new System.Drawing.Point(514, 676);
            this.saveButton.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.saveButton.Name = "saveButton";
            this.saveButton.Size = new System.Drawing.Size(112, 35);
            this.saveButton.TabIndex = 17;
            this.saveButton.Text = "Save";
            this.saveButton.UseVisualStyleBackColor = true;
            this.saveButton.Click += new System.EventHandler(this.SaveButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(630, 676);
            this.cancelButton.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(112, 35);
            this.cancelButton.TabIndex = 18;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.CancelButton_Click);
            // 
            // testButton
            // 
            this.testButton.Location = new System.Drawing.Point(19, 676);
            this.testButton.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.testButton.Name = "testButton";
            this.testButton.Size = new System.Drawing.Size(176, 35);
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
            this.header.Size = new System.Drawing.Size(765, 72);
            this.header.TabIndex = 5;
            this.header.TabStop = false;
            // 
            // certificatePathButton
            // 
            this.certificatePathButton.Location = new System.Drawing.Point(590, 258);
            this.certificatePathButton.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.certificatePathButton.Name = "certificatePathButton";
            this.certificatePathButton.Size = new System.Drawing.Size(112, 35);
            this.certificatePathButton.TabIndex = 15;
            this.certificatePathButton.Text = "Browse...";
            this.certificatePathButton.UseVisualStyleBackColor = true;
            this.certificatePathButton.Click += new System.EventHandler(this.CertificatePathButton_Click);
            // 
            // textCertificatePath
            // 
            this.textCertificatePath.Location = new System.Drawing.Point(140, 261);
            this.textCertificatePath.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.textCertificatePath.Name = "textCertificatePath";
            this.textCertificatePath.Size = new System.Drawing.Size(440, 26);
            this.textCertificatePath.TabIndex = 14;
            // 
            // labelCertificatePath
            // 
            this.labelCertificatePath.AutoSize = true;
            this.labelCertificatePath.Location = new System.Drawing.Point(18, 265);
            this.labelCertificatePath.Name = "labelCertificatePath";
            this.labelCertificatePath.Size = new System.Drawing.Size(114, 20);
            this.labelCertificatePath.TabIndex = 11;
            this.labelCertificatePath.Text = "Certificate File:";
            // 
            // groupSSL
            // 
            this.groupSSL.Controls.Add(this.radioEnabledFull);
            this.groupSSL.Controls.Add(this.radioEnabledHostname);
            this.groupSSL.Controls.Add(this.radioEnabledNoHostname);
            this.groupSSL.Controls.Add(this.radioEnabledNoValidation);
            this.groupSSL.Controls.Add(this.radioButtonDisabled);
            this.groupSSL.Location = new System.Drawing.Point(18, 20);
            this.groupSSL.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.groupSSL.Name = "groupSSL";
            this.groupSSL.Padding = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.groupSSL.Size = new System.Drawing.Size(684, 220);
            this.groupSSL.TabIndex = 10;
            this.groupSSL.TabStop = false;
            this.groupSSL.Text = "Secure Sockets Layer (SSL):";
            // 
            // radioEnabledFull
            // 
            this.radioEnabledFull.AutoSize = true;
            this.radioEnabledFull.Location = new System.Drawing.Point(18, 174);
            this.radioEnabledFull.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radioEnabledFull.Name = "radioEnabledFull";
            this.radioEnabledFull.Size = new System.Drawing.Size(340, 24);
            this.radioEnabledFull.TabIndex = 13;
            this.radioEnabledFull.TabStop = true;
            this.radioEnabledFull.Text = "Enabled. Certificate identity chain validated.";
            this.radioEnabledFull.UseVisualStyleBackColor = true;
            // 
            // radioEnabledHostname
            // 
            this.radioEnabledHostname.AutoSize = true;
            this.radioEnabledHostname.Location = new System.Drawing.Point(18, 140);
            this.radioEnabledHostname.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radioEnabledHostname.Name = "radioEnabledHostname";
            this.radioEnabledHostname.Size = new System.Drawing.Size(405, 24);
            this.radioEnabledHostname.TabIndex = 12;
            this.radioEnabledHostname.TabStop = true;
            this.radioEnabledHostname.Text = "Enabled. Certificate is validated; hostname validated.";
            this.radioEnabledHostname.UseVisualStyleBackColor = true;
            // 
            // radioEnabledNoHostname
            // 
            this.radioEnabledNoHostname.AutoSize = true;
            this.radioEnabledNoHostname.Location = new System.Drawing.Point(18, 106);
            this.radioEnabledNoHostname.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radioEnabledNoHostname.Name = "radioEnabledNoHostname";
            this.radioEnabledNoHostname.Size = new System.Drawing.Size(432, 24);
            this.radioEnabledNoHostname.TabIndex = 11;
            this.radioEnabledNoHostname.TabStop = true;
            this.radioEnabledNoHostname.Text = "Enabled. Certificate is validated; hostname not validated.";
            this.radioEnabledNoHostname.UseVisualStyleBackColor = true;
            // 
            // radioEnabledNoValidation
            // 
            this.radioEnabledNoValidation.AutoSize = true;
            this.radioEnabledNoValidation.Location = new System.Drawing.Point(18, 72);
            this.radioEnabledNoValidation.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radioEnabledNoValidation.Name = "radioEnabledNoValidation";
            this.radioEnabledNoValidation.Size = new System.Drawing.Size(271, 24);
            this.radioEnabledNoValidation.TabIndex = 10;
            this.radioEnabledNoValidation.TabStop = true;
            this.radioEnabledNoValidation.Text = "Enabled. Certificate not validated.";
            this.radioEnabledNoValidation.UseVisualStyleBackColor = true;
            // 
            // radioButtonDisabled
            // 
            this.radioButtonDisabled.AutoSize = true;
            this.radioButtonDisabled.Location = new System.Drawing.Point(18, 39);
            this.radioButtonDisabled.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.radioButtonDisabled.Name = "radioButtonDisabled";
            this.radioButtonDisabled.Size = new System.Drawing.Size(336, 24);
            this.radioButtonDisabled.TabIndex = 9;
            this.radioButtonDisabled.TabStop = true;
            this.radioButtonDisabled.Text = "Disabled. All communications unencrypted.";
            this.radioButtonDisabled.UseVisualStyleBackColor = true;
            // 
            // numericUpDownPort
            // 
            this.numericUpDownPort.Location = new System.Drawing.Point(126, 308);
            this.numericUpDownPort.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.numericUpDownPort.Maximum = new decimal(new int[] {
            65535,
            0,
            0,
            0});
            this.numericUpDownPort.Name = "numericUpDownPort";
            this.numericUpDownPort.Size = new System.Drawing.Size(117, 26);
            this.numericUpDownPort.TabIndex = 5;
            this.numericUpDownPort.Value = new decimal(new int[] {
            9200,
            0,
            0,
            0});
            this.numericUpDownPort.ValueChanged += new System.EventHandler(this.NumericUpDownPort_ValueChanged);
            // 
            // textPassword
            // 
            this.textPassword.Location = new System.Drawing.Point(126, 405);
            this.textPassword.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.textPassword.Name = "textPassword";
            this.textPassword.PasswordChar = '*';
            this.textPassword.Size = new System.Drawing.Size(257, 26);
            this.textPassword.TabIndex = 7;
            // 
            // labelPassword
            // 
            this.labelPassword.AutoSize = true;
            this.labelPassword.Location = new System.Drawing.Point(18, 409);
            this.labelPassword.Name = "labelPassword";
            this.labelPassword.Size = new System.Drawing.Size(82, 20);
            this.labelPassword.TabIndex = 6;
            this.labelPassword.Text = "Password:";
            // 
            // textUsername
            // 
            this.textUsername.Location = new System.Drawing.Point(126, 355);
            this.textUsername.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.textUsername.Name = "textUsername";
            this.textUsername.Size = new System.Drawing.Size(257, 26);
            this.textUsername.TabIndex = 6;
            // 
            // labelUsername
            // 
            this.labelUsername.AutoSize = true;
            this.labelUsername.Location = new System.Drawing.Point(18, 359);
            this.labelUsername.Name = "labelUsername";
            this.labelUsername.Size = new System.Drawing.Size(87, 20);
            this.labelUsername.TabIndex = 4;
            this.labelUsername.Text = "Username:";
            // 
            // textHostname
            // 
            this.textHostname.Location = new System.Drawing.Point(126, 260);
            this.textHostname.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.textHostname.Name = "textHostname";
            this.textHostname.Size = new System.Drawing.Size(569, 26);
            this.textHostname.TabIndex = 4;
            this.textHostname.TextChanged += new System.EventHandler(this.TextHostname_TextChanged);
            // 
            // labelPort
            // 
            this.labelPort.AutoSize = true;
            this.labelPort.Location = new System.Drawing.Point(18, 310);
            this.labelPort.Name = "labelPort";
            this.labelPort.Size = new System.Drawing.Size(42, 20);
            this.labelPort.TabIndex = 2;
            this.labelPort.Text = "Port:";
            // 
            // labelHostname
            // 
            this.labelHostname.AutoSize = true;
            this.labelHostname.Location = new System.Drawing.Point(18, 264);
            this.labelHostname.Name = "labelHostname";
            this.labelHostname.Size = new System.Drawing.Size(87, 20);
            this.labelHostname.TabIndex = 0;
            this.labelHostname.Text = "Hostname:";
            // 
            // certificateFileDialog
            // 
            this.certificateFileDialog.Filter = "X509 Certificate|*.pem;*.der|All Files|*.*";
            // 
            // tabConfiguration
            // 
            this.tabConfiguration.Controls.Add(this.pageBasic);
            this.tabConfiguration.Controls.Add(this.pageSecurity);
            this.tabConfiguration.Controls.Add(this.pageLogging);
            this.tabConfiguration.Location = new System.Drawing.Point(18, 92);
            this.tabConfiguration.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.tabConfiguration.Name = "tabConfiguration";
            this.tabConfiguration.SelectedIndex = 0;
            this.tabConfiguration.Size = new System.Drawing.Size(729, 575);
            this.tabConfiguration.TabIndex = 8;
            // 
            // pageBasic
            // 
            this.pageBasic.Controls.Add(this.textDescription);
            this.pageBasic.Controls.Add(this.labelDescription);
            this.pageBasic.Controls.Add(this.textName);
            this.pageBasic.Controls.Add(this.labelName);
            this.pageBasic.Controls.Add(this.textHostname);
            this.pageBasic.Controls.Add(this.labelHostname);
            this.pageBasic.Controls.Add(this.labelPort);
            this.pageBasic.Controls.Add(this.labelUsername);
            this.pageBasic.Controls.Add(this.textUsername);
            this.pageBasic.Controls.Add(this.numericUpDownPort);
            this.pageBasic.Controls.Add(this.labelPassword);
            this.pageBasic.Controls.Add(this.textPassword);
            this.pageBasic.Location = new System.Drawing.Point(4, 29);
            this.pageBasic.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.pageBasic.Name = "pageBasic";
            this.pageBasic.Padding = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.pageBasic.Size = new System.Drawing.Size(721, 542);
            this.pageBasic.TabIndex = 0;
            this.pageBasic.Text = "Basic";
            this.pageBasic.UseVisualStyleBackColor = true;
            // 
            // textDescription
            // 
            this.textDescription.Location = new System.Drawing.Point(126, 70);
            this.textDescription.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.textDescription.Multiline = true;
            this.textDescription.Name = "textDescription";
            this.textDescription.Size = new System.Drawing.Size(569, 164);
            this.textDescription.TabIndex = 3;
            // 
            // labelDescription
            // 
            this.labelDescription.AutoSize = true;
            this.labelDescription.Location = new System.Drawing.Point(18, 74);
            this.labelDescription.Name = "labelDescription";
            this.labelDescription.Size = new System.Drawing.Size(93, 20);
            this.labelDescription.TabIndex = 10;
            this.labelDescription.Text = "Description:";
            // 
            // textName
            // 
            this.textName.Location = new System.Drawing.Point(126, 20);
            this.textName.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.textName.Name = "textName";
            this.textName.Size = new System.Drawing.Size(569, 26);
            this.textName.TabIndex = 2;
            this.textName.TextChanged += new System.EventHandler(this.TextName_TextChanged);
            // 
            // labelName
            // 
            this.labelName.AutoSize = true;
            this.labelName.Location = new System.Drawing.Point(18, 26);
            this.labelName.Name = "labelName";
            this.labelName.Size = new System.Drawing.Size(55, 20);
            this.labelName.TabIndex = 8;
            this.labelName.Text = "Name:";
            // 
            // pageSecurity
            // 
            this.pageSecurity.Controls.Add(this.certificatePathButton);
            this.pageSecurity.Controls.Add(this.groupSSL);
            this.pageSecurity.Controls.Add(this.textCertificatePath);
            this.pageSecurity.Controls.Add(this.labelCertificatePath);
            this.pageSecurity.Location = new System.Drawing.Point(4, 29);
            this.pageSecurity.Margin = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.pageSecurity.Name = "pageSecurity";
            this.pageSecurity.Padding = new System.Windows.Forms.Padding(3, 4, 3, 4);
            this.pageSecurity.Size = new System.Drawing.Size(721, 542);
            this.pageSecurity.TabIndex = 1;
            this.pageSecurity.Text = "Security";
            this.pageSecurity.UseVisualStyleBackColor = true;
            // 
            // pageLogging
            // 
            this.pageLogging.Controls.Add(this.checkLoggingEnabled);
            this.pageLogging.Controls.Add(this.comboLogLevel);
            this.pageLogging.Controls.Add(this.labelLogLevel);
            this.pageLogging.Controls.Add(this.logDirectoryPathButton);
            this.pageLogging.Controls.Add(this.textLogDirectoryPath);
            this.pageLogging.Controls.Add(this.labelLogDirectory);
            this.pageLogging.Location = new System.Drawing.Point(4, 29);
            this.pageLogging.Name = "pageLogging";
            this.pageLogging.Padding = new System.Windows.Forms.Padding(3);
            this.pageLogging.Size = new System.Drawing.Size(721, 542);
            this.pageLogging.TabIndex = 2;
            this.pageLogging.Text = "Logging";
            this.pageLogging.UseVisualStyleBackColor = true;
            // 
            // checkLoggingEnabled
            // 
            this.checkLoggingEnabled.AutoSize = true;
            this.checkLoggingEnabled.Checked = true;
            this.checkLoggingEnabled.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkLoggingEnabled.Location = new System.Drawing.Point(18, 26);
            this.checkLoggingEnabled.Name = "checkLoggingEnabled";
            this.checkLoggingEnabled.Size = new System.Drawing.Size(155, 24);
            this.checkLoggingEnabled.TabIndex = 22;
            this.checkLoggingEnabled.Text = "Enable Logging?";
            this.checkLoggingEnabled.UseVisualStyleBackColor = true;
            this.checkLoggingEnabled.CheckedChanged += new System.EventHandler(this.CheckLoggingEnabled_CheckedChanged);
            // 
            // comboLogLevel
            // 
            this.comboLogLevel.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboLogLevel.FormattingEnabled = true;
            this.comboLogLevel.Items.AddRange(new object[] {
            "DEBUG",
            "INFO",
            "WARN",
            "ERROR"});
            this.comboLogLevel.Location = new System.Drawing.Point(139, 123);
            this.comboLogLevel.Name = "comboLogLevel";
            this.comboLogLevel.Size = new System.Drawing.Size(121, 28);
            this.comboLogLevel.TabIndex = 21;
            // 
            // labelLogLevel
            // 
            this.labelLogLevel.AutoSize = true;
            this.labelLogLevel.Location = new System.Drawing.Point(18, 126);
            this.labelLogLevel.Name = "labelLogLevel";
            this.labelLogLevel.Size = new System.Drawing.Size(81, 20);
            this.labelLogLevel.TabIndex = 20;
            this.labelLogLevel.Text = "Log Level:";
            // 
            // logDirectoryPathButton
            // 
            this.logDirectoryPathButton.Location = new System.Drawing.Point(592, 69);
            this.logDirectoryPathButton.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.logDirectoryPathButton.Name = "logDirectoryPathButton";
            this.logDirectoryPathButton.Size = new System.Drawing.Size(112, 35);
            this.logDirectoryPathButton.TabIndex = 18;
            this.logDirectoryPathButton.Text = "Browse...";
            this.logDirectoryPathButton.UseVisualStyleBackColor = true;
            this.logDirectoryPathButton.Click += new System.EventHandler(this.LogDirectoryPathButton_Click);
            // 
            // textLogDirectoryPath
            // 
            this.textLogDirectoryPath.Location = new System.Drawing.Point(139, 71);
            this.textLogDirectoryPath.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.textLogDirectoryPath.Name = "textLogDirectoryPath";
            this.textLogDirectoryPath.Size = new System.Drawing.Size(440, 26);
            this.textLogDirectoryPath.TabIndex = 17;
            // 
            // labelLogDirectory
            // 
            this.labelLogDirectory.AutoSize = true;
            this.labelLogDirectory.Location = new System.Drawing.Point(18, 77);
            this.labelLogDirectory.Name = "labelLogDirectory";
            this.labelLogDirectory.Size = new System.Drawing.Size(107, 20);
            this.labelLogDirectory.TabIndex = 16;
            this.labelLogDirectory.Text = "Log Directory:";
            // 
            // DsnEditorForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(9F, 20F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(767, 735);
            this.Controls.Add(this.tabConfiguration);
            this.Controls.Add(this.header);
            this.Controls.Add(this.testButton);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.saveButton);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "DsnEditorForm";
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.Text = "Elasticsearch ODBC DSN Configuration";
            ((System.ComponentModel.ISupportInitialize)(this.header)).EndInit();
            this.groupSSL.ResumeLayout(false);
            this.groupSSL.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownPort)).EndInit();
            this.tabConfiguration.ResumeLayout(false);
            this.pageBasic.ResumeLayout(false);
            this.pageBasic.PerformLayout();
            this.pageSecurity.ResumeLayout(false);
            this.pageSecurity.PerformLayout();
            this.pageLogging.ResumeLayout(false);
            this.pageLogging.PerformLayout();
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
        private System.Windows.Forms.GroupBox groupSSL;
        private System.Windows.Forms.RadioButton radioEnabledFull;
        private System.Windows.Forms.RadioButton radioEnabledHostname;
        private System.Windows.Forms.RadioButton radioEnabledNoHostname;
        private System.Windows.Forms.RadioButton radioEnabledNoValidation;
        private System.Windows.Forms.Button certificatePathButton;
        private System.Windows.Forms.TextBox textCertificatePath;
        private System.Windows.Forms.Label labelCertificatePath;
        private System.Windows.Forms.OpenFileDialog certificateFileDialog;
        private System.Windows.Forms.TabControl tabConfiguration;
        private System.Windows.Forms.TabPage pageBasic;
        private System.Windows.Forms.TabPage pageSecurity;
        private System.Windows.Forms.TextBox textDescription;
        private System.Windows.Forms.Label labelDescription;
        private System.Windows.Forms.TextBox textName;
        private System.Windows.Forms.Label labelName;
        private System.Windows.Forms.TabPage pageLogging;
        private System.Windows.Forms.Label labelLogLevel;
        private System.Windows.Forms.Button logDirectoryPathButton;
        private System.Windows.Forms.TextBox textLogDirectoryPath;
        private System.Windows.Forms.Label labelLogDirectory;
        private System.Windows.Forms.FolderBrowserDialog folderLogDirectoryDialog;
        private System.Windows.Forms.ComboBox comboLogLevel;
        private System.Windows.Forms.CheckBox checkLoggingEnabled;
    }
}

