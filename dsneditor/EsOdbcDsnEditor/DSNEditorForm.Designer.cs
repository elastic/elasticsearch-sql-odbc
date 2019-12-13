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
            this.components = new System.ComponentModel.Container();
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
            this.labelCloudID = new System.Windows.Forms.Label();
            this.textDescription = new System.Windows.Forms.TextBox();
            this.textCloudID = new System.Windows.Forms.TextBox();
            this.labelDescription = new System.Windows.Forms.Label();
            this.textName = new System.Windows.Forms.TextBox();
            this.labelName = new System.Windows.Forms.Label();
            this.pageSecurity = new System.Windows.Forms.TabPage();
            this.pageMisc = new System.Windows.Forms.TabPage();
            this.comboBoxDataEncoding = new System.Windows.Forms.ComboBox();
            this.labelDataEncoding = new System.Windows.Forms.Label();
            this.checkBoxAutoEscapePVA = new System.Windows.Forms.CheckBox();
            this.comboBoxFloatsFormat = new System.Windows.Forms.ComboBox();
            this.labelFloatsFormat = new System.Windows.Forms.Label();
            this.checkBoxIndexIncludeFrozen = new System.Windows.Forms.CheckBox();
            this.checkBoxMultiFieldLenient = new System.Windows.Forms.CheckBox();
            this.checkBoxApplyTZ = new System.Windows.Forms.CheckBox();
            this.checkBoxFollowRedirects = new System.Windows.Forms.CheckBox();
            this.labelBodySize = new System.Windows.Forms.Label();
            this.numericUpDownBodySize = new System.Windows.Forms.NumericUpDown();
            this.labelFetchSize = new System.Windows.Forms.Label();
            this.numericUpDownFetchSize = new System.Windows.Forms.NumericUpDown();
            this.labelTimeout = new System.Windows.Forms.Label();
            this.numericUpDownTimeout = new System.Windows.Forms.NumericUpDown();
            this.pageLogging = new System.Windows.Forms.TabPage();
            this.checkLoggingEnabled = new System.Windows.Forms.CheckBox();
            this.comboLogLevel = new System.Windows.Forms.ComboBox();
            this.labelLogLevel = new System.Windows.Forms.Label();
            this.logDirectoryPathButton = new System.Windows.Forms.Button();
            this.textLogDirectoryPath = new System.Windows.Forms.TextBox();
            this.labelLogDirectory = new System.Windows.Forms.Label();
            this.folderLogDirectoryDialog = new System.Windows.Forms.FolderBrowserDialog();
            this.toolTipName = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipDescription = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipCloudID = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipHostname = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipPort = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipUsername = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipPassword = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipDisabled = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipEnabledNoValidation = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipEnabledNoHostname = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipEnabledHostname = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipEnabledFull = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipCertificatePath = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipLogDirectoryPath = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipLoggingEnabled = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipLogLevel = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipTimeout = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipFetchSize = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipBodySize = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipFloatsFormat = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipFollowRedirects = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipApplyTZ = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipAutoEscapePVA = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipMultiFieldLenient = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipIndexIncludeFrozen = new System.Windows.Forms.ToolTip(this.components);
            this.toolTipDataEncoding = new System.Windows.Forms.ToolTip(this.components);
            this.labelDataCompression = new System.Windows.Forms.Label();
            this.comboBoxDataCompression = new System.Windows.Forms.ComboBox();
            this.toolTipDataCompression = new System.Windows.Forms.ToolTip(this.components);
            ((System.ComponentModel.ISupportInitialize)(this.header)).BeginInit();
            this.groupSSL.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownPort)).BeginInit();
            this.tabConfiguration.SuspendLayout();
            this.pageBasic.SuspendLayout();
            this.pageSecurity.SuspendLayout();
            this.pageMisc.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownBodySize)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownFetchSize)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownTimeout)).BeginInit();
            this.pageLogging.SuspendLayout();
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
            this.textCertificatePath.MaxLength = 512;
            this.textCertificatePath.Name = "textCertificatePath";
            this.textCertificatePath.Size = new System.Drawing.Size(392, 22);
            this.textCertificatePath.TabIndex = 14;
            // 
            // labelCertificatePath
            // 
            this.labelCertificatePath.AutoSize = true;
            this.labelCertificatePath.Location = new System.Drawing.Point(16, 212);
            this.labelCertificatePath.Name = "labelCertificatePath";
            this.labelCertificatePath.Size = new System.Drawing.Size(101, 17);
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
            this.groupSSL.Location = new System.Drawing.Point(16, 16);
            this.groupSSL.Name = "groupSSL";
            this.groupSSL.Size = new System.Drawing.Size(608, 176);
            this.groupSSL.TabIndex = 10;
            this.groupSSL.TabStop = false;
            this.groupSSL.Text = "Secure Sockets Layer (SSL):";
            // 
            // radioEnabledFull
            // 
            this.radioEnabledFull.AutoSize = true;
            this.radioEnabledFull.Location = new System.Drawing.Point(16, 139);
            this.radioEnabledFull.Name = "radioEnabledFull";
            this.radioEnabledFull.Size = new System.Drawing.Size(304, 21);
            this.radioEnabledFull.TabIndex = 13;
            this.radioEnabledFull.TabStop = true;
            this.radioEnabledFull.Text = "Enabled. Certificate identity chain validated.";
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
            this.numericUpDownPort.Location = new System.Drawing.Point(112, 241);
            this.numericUpDownPort.Maximum = new decimal(new int[] {
            65535,
            0,
            0,
            0});
            this.numericUpDownPort.Name = "numericUpDownPort";
            this.numericUpDownPort.Size = new System.Drawing.Size(104, 22);
            this.numericUpDownPort.TabIndex = 6;
            this.numericUpDownPort.Value = new decimal(new int[] {
            9200,
            0,
            0,
            0});
            this.numericUpDownPort.ValueChanged += new System.EventHandler(this.NumericUpDownPort_ValueChanged);
            // 
            // textPassword
            // 
            this.textPassword.Location = new System.Drawing.Point(112, 343);
            this.textPassword.Margin = new System.Windows.Forms.Padding(4);
            this.textPassword.MaxLength = 512;
            this.textPassword.Name = "textPassword";
            this.textPassword.PasswordChar = '*';
            this.textPassword.Size = new System.Drawing.Size(229, 22);
            this.textPassword.TabIndex = 8;
            // 
            // labelPassword
            // 
            this.labelPassword.AutoSize = true;
            this.labelPassword.Location = new System.Drawing.Point(16, 346);
            this.labelPassword.Name = "labelPassword";
            this.labelPassword.Size = new System.Drawing.Size(73, 17);
            this.labelPassword.TabIndex = 6;
            this.labelPassword.Text = "Password:";
            // 
            // textUsername
            // 
            this.textUsername.Location = new System.Drawing.Point(112, 303);
            this.textUsername.Margin = new System.Windows.Forms.Padding(4);
            this.textUsername.MaxLength = 512;
            this.textUsername.Name = "textUsername";
            this.textUsername.Size = new System.Drawing.Size(229, 22);
            this.textUsername.TabIndex = 7;
            // 
            // labelUsername
            // 
            this.labelUsername.AutoSize = true;
            this.labelUsername.Location = new System.Drawing.Point(16, 306);
            this.labelUsername.Name = "labelUsername";
            this.labelUsername.Size = new System.Drawing.Size(77, 17);
            this.labelUsername.TabIndex = 4;
            this.labelUsername.Text = "Username:";
            // 
            // textHostname
            // 
            this.textHostname.Location = new System.Drawing.Point(112, 203);
            this.textHostname.Margin = new System.Windows.Forms.Padding(4);
            this.textHostname.MaxLength = 512;
            this.textHostname.Name = "textHostname";
            this.textHostname.Size = new System.Drawing.Size(506, 22);
            this.textHostname.TabIndex = 5;
            this.textHostname.TextChanged += new System.EventHandler(this.TextHostname_TextChanged);
            // 
            // labelPort
            // 
            this.labelPort.AutoSize = true;
            this.labelPort.Location = new System.Drawing.Point(16, 243);
            this.labelPort.Name = "labelPort";
            this.labelPort.Size = new System.Drawing.Size(38, 17);
            this.labelPort.TabIndex = 2;
            this.labelPort.Text = "Port:";
            // 
            // labelHostname
            // 
            this.labelHostname.AutoSize = true;
            this.labelHostname.Location = new System.Drawing.Point(16, 206);
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
            this.tabConfiguration.Controls.Add(this.pageBasic);
            this.tabConfiguration.Controls.Add(this.pageSecurity);
            this.tabConfiguration.Controls.Add(this.pageMisc);
            this.tabConfiguration.Controls.Add(this.pageLogging);
            this.tabConfiguration.Location = new System.Drawing.Point(17, 74);
            this.tabConfiguration.Name = "tabConfiguration";
            this.tabConfiguration.SelectedIndex = 0;
            this.tabConfiguration.Size = new System.Drawing.Size(648, 460);
            this.tabConfiguration.TabIndex = 8;
            // 
            // pageBasic
            // 
            this.pageBasic.Controls.Add(this.labelCloudID);
            this.pageBasic.Controls.Add(this.textDescription);
            this.pageBasic.Controls.Add(this.textCloudID);
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
            this.pageBasic.Location = new System.Drawing.Point(4, 25);
            this.pageBasic.Name = "pageBasic";
            this.pageBasic.Padding = new System.Windows.Forms.Padding(3);
            this.pageBasic.Size = new System.Drawing.Size(640, 431);
            this.pageBasic.TabIndex = 0;
            this.pageBasic.Text = "Basic";
            this.pageBasic.UseVisualStyleBackColor = true;
            // 
            // labelCloudID
            // 
            this.labelCloudID.AutoSize = true;
            this.labelCloudID.Location = new System.Drawing.Point(16, 118);
            this.labelCloudID.Name = "labelCloudID";
            this.labelCloudID.Size = new System.Drawing.Size(65, 17);
            this.labelCloudID.TabIndex = 12;
            this.labelCloudID.Text = "Cloud ID:";
            // 
            // textDescription
            // 
            this.textDescription.Location = new System.Drawing.Point(112, 56);
            this.textDescription.Margin = new System.Windows.Forms.Padding(4);
            this.textDescription.MaxLength = 256;
            this.textDescription.Name = "textDescription";
            this.textDescription.Size = new System.Drawing.Size(506, 22);
            this.textDescription.TabIndex = 3;
            // 
            // textCloudID
            // 
            this.textCloudID.Location = new System.Drawing.Point(112, 115);
            this.textCloudID.Margin = new System.Windows.Forms.Padding(4);
            this.textCloudID.MaxLength = 512;
            this.textCloudID.Multiline = true;
            this.textCloudID.Name = "textCloudID";
            this.textCloudID.Size = new System.Drawing.Size(506, 72);
            this.textCloudID.TabIndex = 4;
            this.textCloudID.TextChanged += new System.EventHandler(this.TextCloudID_TextChanged);
            // 
            // labelDescription
            // 
            this.labelDescription.AutoSize = true;
            this.labelDescription.Location = new System.Drawing.Point(16, 59);
            this.labelDescription.Name = "labelDescription";
            this.labelDescription.Size = new System.Drawing.Size(83, 17);
            this.labelDescription.TabIndex = 10;
            this.labelDescription.Text = "Description:";
            // 
            // textName
            // 
            this.textName.Location = new System.Drawing.Point(112, 18);
            this.textName.Margin = new System.Windows.Forms.Padding(4);
            this.textName.MaxLength = 256;
            this.textName.Name = "textName";
            this.textName.Size = new System.Drawing.Size(506, 22);
            this.textName.TabIndex = 2;
            this.textName.TextChanged += new System.EventHandler(this.TextName_TextChanged);
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
            // pageSecurity
            // 
            this.pageSecurity.Controls.Add(this.certificatePathButton);
            this.pageSecurity.Controls.Add(this.groupSSL);
            this.pageSecurity.Controls.Add(this.textCertificatePath);
            this.pageSecurity.Controls.Add(this.labelCertificatePath);
            this.pageSecurity.Location = new System.Drawing.Point(4, 25);
            this.pageSecurity.Name = "pageSecurity";
            this.pageSecurity.Padding = new System.Windows.Forms.Padding(3);
            this.pageSecurity.Size = new System.Drawing.Size(640, 431);
            this.pageSecurity.TabIndex = 1;
            this.pageSecurity.Text = "Security";
            this.pageSecurity.UseVisualStyleBackColor = true;
            // 
            // pageMisc
            // 
            this.pageMisc.Controls.Add(this.comboBoxDataCompression);
            this.pageMisc.Controls.Add(this.labelDataCompression);
            this.pageMisc.Controls.Add(this.comboBoxDataEncoding);
            this.pageMisc.Controls.Add(this.labelDataEncoding);
            this.pageMisc.Controls.Add(this.checkBoxAutoEscapePVA);
            this.pageMisc.Controls.Add(this.comboBoxFloatsFormat);
            this.pageMisc.Controls.Add(this.labelFloatsFormat);
            this.pageMisc.Controls.Add(this.checkBoxIndexIncludeFrozen);
            this.pageMisc.Controls.Add(this.checkBoxMultiFieldLenient);
            this.pageMisc.Controls.Add(this.checkBoxApplyTZ);
            this.pageMisc.Controls.Add(this.checkBoxFollowRedirects);
            this.pageMisc.Controls.Add(this.labelBodySize);
            this.pageMisc.Controls.Add(this.numericUpDownBodySize);
            this.pageMisc.Controls.Add(this.labelFetchSize);
            this.pageMisc.Controls.Add(this.numericUpDownFetchSize);
            this.pageMisc.Controls.Add(this.labelTimeout);
            this.pageMisc.Controls.Add(this.numericUpDownTimeout);
            this.pageMisc.Location = new System.Drawing.Point(4, 25);
            this.pageMisc.Name = "pageMisc";
            this.pageMisc.Padding = new System.Windows.Forms.Padding(3);
            this.pageMisc.Size = new System.Drawing.Size(640, 431);
            this.pageMisc.TabIndex = 3;
            this.pageMisc.Text = "Misc";
            this.pageMisc.UseVisualStyleBackColor = true;
            // 
            // comboBoxDataEncoding
            // 
            this.comboBoxDataEncoding.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxDataEncoding.FormattingEnabled = true;
            this.comboBoxDataEncoding.Items.AddRange(new object[] {
            "JSON",
            "CBOR"});
            this.comboBoxDataEncoding.Location = new System.Drawing.Point(182, 155);
            this.comboBoxDataEncoding.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.comboBoxDataEncoding.Name = "comboBoxDataEncoding";
            this.comboBoxDataEncoding.Size = new System.Drawing.Size(108, 24);
            this.comboBoxDataEncoding.TabIndex = 15;
            // 
            // labelDataEncoding
            // 
            this.labelDataEncoding.AutoSize = true;
            this.labelDataEncoding.Location = new System.Drawing.Point(24, 158);
            this.labelDataEncoding.Name = "labelDataEncoding";
            this.labelDataEncoding.Size = new System.Drawing.Size(104, 17);
            this.labelDataEncoding.TabIndex = 30;
            this.labelDataEncoding.Text = "Data encoding:";
            // 
            // checkBoxAutoEscapePVA
            // 
            this.checkBoxAutoEscapePVA.AutoSize = true;
            this.checkBoxAutoEscapePVA.Checked = true;
            this.checkBoxAutoEscapePVA.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxAutoEscapePVA.Location = new System.Drawing.Point(339, 86);
            this.checkBoxAutoEscapePVA.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.checkBoxAutoEscapePVA.Name = "checkBoxAutoEscapePVA";
            this.checkBoxAutoEscapePVA.Size = new System.Drawing.Size(156, 21);
            this.checkBoxAutoEscapePVA.TabIndex = 12;
            this.checkBoxAutoEscapePVA.Text = "Auto-escape PVAs?";
            this.checkBoxAutoEscapePVA.UseVisualStyleBackColor = true;
            // 
            // comboBoxFloatsFormat
            // 
            this.comboBoxFloatsFormat.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxFloatsFormat.FormattingEnabled = true;
            this.comboBoxFloatsFormat.Items.AddRange(new object[] {
            "default",
            "scientific",
            "auto"});
            this.comboBoxFloatsFormat.Location = new System.Drawing.Point(182, 119);
            this.comboBoxFloatsFormat.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.comboBoxFloatsFormat.Name = "comboBoxFloatsFormat";
            this.comboBoxFloatsFormat.Size = new System.Drawing.Size(108, 24);
            this.comboBoxFloatsFormat.TabIndex = 13;
            // 
            // labelFloatsFormat
            // 
            this.labelFloatsFormat.AutoSize = true;
            this.labelFloatsFormat.Location = new System.Drawing.Point(24, 122);
            this.labelFloatsFormat.Name = "labelFloatsFormat";
            this.labelFloatsFormat.Size = new System.Drawing.Size(94, 17);
            this.labelFloatsFormat.TabIndex = 27;
            this.labelFloatsFormat.Text = "Floats format:";
            // 
            // checkBoxIndexIncludeFrozen
            // 
            this.checkBoxIndexIncludeFrozen.AutoSize = true;
            this.checkBoxIndexIncludeFrozen.Location = new System.Drawing.Point(339, 154);
            this.checkBoxIndexIncludeFrozen.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.checkBoxIndexIncludeFrozen.Name = "checkBoxIndexIncludeFrozen";
            this.checkBoxIndexIncludeFrozen.Size = new System.Drawing.Size(175, 21);
            this.checkBoxIndexIncludeFrozen.TabIndex = 16;
            this.checkBoxIndexIncludeFrozen.Text = "Include frozen indices?";
            this.checkBoxIndexIncludeFrozen.UseVisualStyleBackColor = true;
            // 
            // checkBoxMultiFieldLenient
            // 
            this.checkBoxMultiFieldLenient.AutoSize = true;
            this.checkBoxMultiFieldLenient.Checked = true;
            this.checkBoxMultiFieldLenient.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxMultiFieldLenient.Location = new System.Drawing.Point(339, 119);
            this.checkBoxMultiFieldLenient.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.checkBoxMultiFieldLenient.Name = "checkBoxMultiFieldLenient";
            this.checkBoxMultiFieldLenient.Size = new System.Drawing.Size(181, 21);
            this.checkBoxMultiFieldLenient.TabIndex = 14;
            this.checkBoxMultiFieldLenient.Text = "Multi value field lenient?";
            this.checkBoxMultiFieldLenient.UseVisualStyleBackColor = true;
            // 
            // checkBoxApplyTZ
            // 
            this.checkBoxApplyTZ.AutoSize = true;
            this.checkBoxApplyTZ.Location = new System.Drawing.Point(339, 53);
            this.checkBoxApplyTZ.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.checkBoxApplyTZ.Name = "checkBoxApplyTZ";
            this.checkBoxApplyTZ.Size = new System.Drawing.Size(157, 21);
            this.checkBoxApplyTZ.TabIndex = 10;
            this.checkBoxApplyTZ.Text = "Use local timezone?";
            this.checkBoxApplyTZ.UseVisualStyleBackColor = true;
            // 
            // checkBoxFollowRedirects
            // 
            this.checkBoxFollowRedirects.AutoSize = true;
            this.checkBoxFollowRedirects.Checked = true;
            this.checkBoxFollowRedirects.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxFollowRedirects.Location = new System.Drawing.Point(339, 20);
            this.checkBoxFollowRedirects.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.checkBoxFollowRedirects.Name = "checkBoxFollowRedirects";
            this.checkBoxFollowRedirects.Size = new System.Drawing.Size(177, 21);
            this.checkBoxFollowRedirects.TabIndex = 8;
            this.checkBoxFollowRedirects.Text = "Follow HTTP redirects?";
            this.checkBoxFollowRedirects.UseVisualStyleBackColor = true;
            // 
            // labelBodySize
            // 
            this.labelBodySize.AutoSize = true;
            this.labelBodySize.Location = new System.Drawing.Point(24, 88);
            this.labelBodySize.Name = "labelBodySize";
            this.labelBodySize.Size = new System.Drawing.Size(150, 17);
            this.labelBodySize.TabIndex = 10;
            this.labelBodySize.Text = "Max page lenght (MB):";
            // 
            // numericUpDownBodySize
            // 
            this.numericUpDownBodySize.Location = new System.Drawing.Point(182, 86);
            this.numericUpDownBodySize.Maximum = new decimal(new int[] {
            -1,
            0,
            0,
            0});
            this.numericUpDownBodySize.Name = "numericUpDownBodySize";
            this.numericUpDownBodySize.Size = new System.Drawing.Size(104, 22);
            this.numericUpDownBodySize.TabIndex = 11;
            this.numericUpDownBodySize.Value = new decimal(new int[] {
            100,
            0,
            0,
            0});
            // 
            // labelFetchSize
            // 
            this.labelFetchSize.AutoSize = true;
            this.labelFetchSize.Location = new System.Drawing.Point(24, 53);
            this.labelFetchSize.Name = "labelFetchSize";
            this.labelFetchSize.Size = new System.Drawing.Size(145, 17);
            this.labelFetchSize.TabIndex = 8;
            this.labelFetchSize.Text = "Max page size (rows):";
            // 
            // numericUpDownFetchSize
            // 
            this.numericUpDownFetchSize.Increment = new decimal(new int[] {
            1000,
            0,
            0,
            0});
            this.numericUpDownFetchSize.Location = new System.Drawing.Point(182, 51);
            this.numericUpDownFetchSize.Maximum = new decimal(new int[] {
            -1,
            0,
            0,
            0});
            this.numericUpDownFetchSize.Name = "numericUpDownFetchSize";
            this.numericUpDownFetchSize.Size = new System.Drawing.Size(104, 22);
            this.numericUpDownFetchSize.TabIndex = 9;
            this.numericUpDownFetchSize.Value = new decimal(new int[] {
            1000,
            0,
            0,
            0});
            // 
            // labelTimeout
            // 
            this.labelTimeout.AutoSize = true;
            this.labelTimeout.Location = new System.Drawing.Point(24, 20);
            this.labelTimeout.Name = "labelTimeout";
            this.labelTimeout.Size = new System.Drawing.Size(136, 17);
            this.labelTimeout.TabIndex = 6;
            this.labelTimeout.Text = "Request timeout (s):";
            // 
            // numericUpDownTimeout
            // 
            this.numericUpDownTimeout.Location = new System.Drawing.Point(182, 18);
            this.numericUpDownTimeout.Maximum = new decimal(new int[] {
            -1,
            0,
            0,
            0});
            this.numericUpDownTimeout.Name = "numericUpDownTimeout";
            this.numericUpDownTimeout.Size = new System.Drawing.Size(104, 22);
            this.numericUpDownTimeout.TabIndex = 7;
            // 
            // pageLogging
            // 
            this.pageLogging.Controls.Add(this.checkLoggingEnabled);
            this.pageLogging.Controls.Add(this.comboLogLevel);
            this.pageLogging.Controls.Add(this.labelLogLevel);
            this.pageLogging.Controls.Add(this.logDirectoryPathButton);
            this.pageLogging.Controls.Add(this.textLogDirectoryPath);
            this.pageLogging.Controls.Add(this.labelLogDirectory);
            this.pageLogging.Location = new System.Drawing.Point(4, 25);
            this.pageLogging.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.pageLogging.Name = "pageLogging";
            this.pageLogging.Padding = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.pageLogging.Size = new System.Drawing.Size(640, 431);
            this.pageLogging.TabIndex = 2;
            this.pageLogging.Text = "Logging";
            this.pageLogging.UseVisualStyleBackColor = true;
            // 
            // checkLoggingEnabled
            // 
            this.checkLoggingEnabled.AutoSize = true;
            this.checkLoggingEnabled.Checked = true;
            this.checkLoggingEnabled.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkLoggingEnabled.Location = new System.Drawing.Point(16, 21);
            this.checkLoggingEnabled.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.checkLoggingEnabled.Name = "checkLoggingEnabled";
            this.checkLoggingEnabled.Size = new System.Drawing.Size(137, 21);
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
            this.comboLogLevel.Location = new System.Drawing.Point(124, 98);
            this.comboLogLevel.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.comboLogLevel.Name = "comboLogLevel";
            this.comboLogLevel.Size = new System.Drawing.Size(108, 24);
            this.comboLogLevel.TabIndex = 25;
            // 
            // labelLogLevel
            // 
            this.labelLogLevel.AutoSize = true;
            this.labelLogLevel.Location = new System.Drawing.Point(16, 101);
            this.labelLogLevel.Name = "labelLogLevel";
            this.labelLogLevel.Size = new System.Drawing.Size(74, 17);
            this.labelLogLevel.TabIndex = 20;
            this.labelLogLevel.Text = "Log Level:";
            // 
            // logDirectoryPathButton
            // 
            this.logDirectoryPathButton.Location = new System.Drawing.Point(526, 55);
            this.logDirectoryPathButton.Margin = new System.Windows.Forms.Padding(4);
            this.logDirectoryPathButton.Name = "logDirectoryPathButton";
            this.logDirectoryPathButton.Size = new System.Drawing.Size(100, 28);
            this.logDirectoryPathButton.TabIndex = 24;
            this.logDirectoryPathButton.Text = "Browse...";
            this.logDirectoryPathButton.UseVisualStyleBackColor = true;
            this.logDirectoryPathButton.Click += new System.EventHandler(this.LogDirectoryPathButton_Click);
            // 
            // textLogDirectoryPath
            // 
            this.textLogDirectoryPath.Location = new System.Drawing.Point(124, 57);
            this.textLogDirectoryPath.Margin = new System.Windows.Forms.Padding(4);
            this.textLogDirectoryPath.MaxLength = 512;
            this.textLogDirectoryPath.Name = "textLogDirectoryPath";
            this.textLogDirectoryPath.Size = new System.Drawing.Size(392, 22);
            this.textLogDirectoryPath.TabIndex = 23;
            // 
            // labelLogDirectory
            // 
            this.labelLogDirectory.AutoSize = true;
            this.labelLogDirectory.Location = new System.Drawing.Point(16, 62);
            this.labelLogDirectory.Name = "labelLogDirectory";
            this.labelLogDirectory.Size = new System.Drawing.Size(97, 17);
            this.labelLogDirectory.TabIndex = 16;
            this.labelLogDirectory.Text = "Log Directory:";
            // 
            // labelDataCompression
            // 
            this.labelDataCompression.AutoSize = true;
            this.labelDataCompression.Location = new System.Drawing.Point(24, 198);
            this.labelDataCompression.Name = "labelDataCompression";
            this.labelDataCompression.Size = new System.Drawing.Size(126, 17);
            this.labelDataCompression.TabIndex = 31;
            this.labelDataCompression.Text = "Data compression:";
            // 
            // comboBoxDataCompression
            // 
            this.comboBoxDataCompression.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxDataCompression.FormattingEnabled = true;
            this.comboBoxDataCompression.Items.AddRange(new object[] {
            "auto",
            "on",
            "off"});
            this.comboBoxDataCompression.Location = new System.Drawing.Point(182, 191);
            this.comboBoxDataCompression.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.comboBoxDataCompression.Name = "comboBoxDataCompression";
            this.comboBoxDataCompression.Size = new System.Drawing.Size(108, 24);
            this.comboBoxDataCompression.TabIndex = 32;
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
            ((System.ComponentModel.ISupportInitialize)(this.header)).EndInit();
            this.groupSSL.ResumeLayout(false);
            this.groupSSL.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownPort)).EndInit();
            this.tabConfiguration.ResumeLayout(false);
            this.pageBasic.ResumeLayout(false);
            this.pageBasic.PerformLayout();
            this.pageSecurity.ResumeLayout(false);
            this.pageSecurity.PerformLayout();
            this.pageMisc.ResumeLayout(false);
            this.pageMisc.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownBodySize)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownFetchSize)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownTimeout)).EndInit();
            this.pageLogging.ResumeLayout(false);
            this.pageLogging.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion
        private System.Windows.Forms.Button saveButton;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.Button testButton;
        private System.Windows.Forms.PictureBox header;
		private System.Windows.Forms.TextBox textCloudID;
		private System.Windows.Forms.Label labelCloudID;
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
		private System.Windows.Forms.ToolTip toolTipName;
		private System.Windows.Forms.ToolTip toolTipDescription;
		private System.Windows.Forms.ToolTip toolTipCloudID;
		private System.Windows.Forms.ToolTip toolTipHostname;
		private System.Windows.Forms.ToolTip toolTipPort;
		private System.Windows.Forms.ToolTip toolTipUsername;
		private System.Windows.Forms.ToolTip toolTipPassword;
		private System.Windows.Forms.ToolTip toolTipDisabled;
		private System.Windows.Forms.ToolTip toolTipEnabledNoValidation;
		private System.Windows.Forms.ToolTip toolTipEnabledNoHostname;
		private System.Windows.Forms.ToolTip toolTipEnabledHostname;
		private System.Windows.Forms.ToolTip toolTipEnabledFull;
		private System.Windows.Forms.ToolTip toolTipCertificatePath;
		private System.Windows.Forms.ToolTip toolTipLogDirectoryPath;
		private System.Windows.Forms.ToolTip toolTipLoggingEnabled;
		private System.Windows.Forms.ToolTip toolTipLogLevel;
		private System.Windows.Forms.TextBox textDescription;
		private System.Windows.Forms.TabPage pageMisc;
		private System.Windows.Forms.Label labelTimeout;
		private System.Windows.Forms.NumericUpDown numericUpDownTimeout;
		private System.Windows.Forms.Label labelFetchSize;
		private System.Windows.Forms.NumericUpDown numericUpDownFetchSize;
		private System.Windows.Forms.Label labelBodySize;
		private System.Windows.Forms.NumericUpDown numericUpDownBodySize;
		private System.Windows.Forms.CheckBox checkBoxIndexIncludeFrozen;
		private System.Windows.Forms.CheckBox checkBoxMultiFieldLenient;
		private System.Windows.Forms.CheckBox checkBoxApplyTZ;
		private System.Windows.Forms.CheckBox checkBoxFollowRedirects;
		private System.Windows.Forms.ComboBox comboBoxFloatsFormat;
		private System.Windows.Forms.Label labelFloatsFormat;
		private System.Windows.Forms.CheckBox checkBoxAutoEscapePVA;
		private System.Windows.Forms.ToolTip toolTipTimeout;
		private System.Windows.Forms.ToolTip toolTipFetchSize;
		private System.Windows.Forms.ToolTip toolTipBodySize;
		private System.Windows.Forms.ToolTip toolTipFloatsFormat;
		private System.Windows.Forms.ToolTip toolTipFollowRedirects;
		private System.Windows.Forms.ToolTip toolTipApplyTZ;
		private System.Windows.Forms.ToolTip toolTipAutoEscapePVA;
		private System.Windows.Forms.ToolTip toolTipMultiFieldLenient;
		private System.Windows.Forms.ToolTip toolTipIndexIncludeFrozen;
		private System.Windows.Forms.ComboBox comboBoxDataEncoding;
		private System.Windows.Forms.Label labelDataEncoding;
		private System.Windows.Forms.ToolTip toolTipDataEncoding;
		private System.Windows.Forms.ComboBox comboBoxDataCompression;
		private System.Windows.Forms.Label labelDataCompression;
		private System.Windows.Forms.ToolTip toolTipDataCompression;
	}
}

