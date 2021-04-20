/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

using System;
using System.Data.Odbc;
using System.Windows.Forms;
using System.IO;
using System.Linq;

// uncomment to have the assembly loading to ask for (various) resources; various solutions: 
// https://stackoverflow.com/questions/4368201/appdomain-currentdomain-assemblyresolve-asking-for-a-appname-resources-assembl
// [assembly: NeutralResourcesLanguageAttribute("en-GB", UltimateResourceFallbackLocation.MainAssembly)]

namespace EsOdbcDsnEditor
{
	/// <summary>
	///    Delegate for the driver callbacks.
	/// </summary>
	public delegate int DriverCallbackDelegate(string connectionString, ref string errorMessage, uint flags);

	public partial class DsnEditorForm : Form
	{
		private const int ESODBC_DSN_EXISTS_ERROR = -1;
		private const int ESODBC_DSN_NAME_INVALID_ERROR = -4;

		private DriverCallbackDelegate testConnection;
		private DriverCallbackDelegate saveDsn;

		private readonly bool isConnecting;

		public OdbcConnectionStringBuilder Builder { get; set; } = new OdbcConnectionStringBuilder();

		public DsnEditorForm(
			bool onConnect,
			string dsn,
			DriverCallbackDelegate connectionTest,
			DriverCallbackDelegate dsnSave)
		{
			InitializeComponent();

			// Wire up default button behaviours
			AcceptButton = saveButton;
			CancelButton = cancelButton;

			isConnecting = onConnect;
			testConnection = connectionTest;
			saveDsn = dsnSave;

			// If connecting then disable some user inputs
			if (isConnecting) {
				textName.ReadOnly = textDescription.ReadOnly = true;
				textName.Enabled = textDescription.Enabled = false;
			}

			// If this is a call serving a connect request, name the button "Connect", otherwise it's a DSN editing, so it's going to be a "Save".
			saveButton.Text = onConnect ? "Connect" : "Save";

			// Parse DSN into the builder
			Builder.ConnectionString = dsn;

			// Basic Panel
			textName.Text = Builder.ContainsKey("dsn") ? Builder["dsn"].ToString().StripBraces() : string.Empty;
			textDescription.Text = Builder.ContainsKey("description") ? Builder["description"].ToString().StripBraces() : string.Empty;
			textUsername.Text = Builder.ContainsKey("uid") ? Builder["uid"].ToString().StripBraces() : string.Empty;
			textPassword.Text = Builder.ContainsKey("pwd") ? Builder["pwd"].ToString().StripBraces() : string.Empty;
			textCloudID.Text = Builder.ContainsKey("cloudid") ? Builder["cloudid"].ToString().StripBraces() : string.Empty;
			textHostname.Text = Builder.ContainsKey("server") ? Builder["server"].ToString().StripBraces() : string.Empty;
			numericUpDownPort.Text = Builder.ContainsKey("port") ? Builder["port"].ToString().StripBraces() : string.Empty;

			toolTipName.SetToolTip(textName, "The name the DSN will be referred by.");
			toolTipDescription.SetToolTip(textDescription, "Allows arbitrary text, generally used for short notes about the configured connection.");
			toolTipCloudID.SetToolTip(textCloudID, "The Cloud ID, if connecting to Elastic Cloud. Settings will be automatically configured.");
			toolTipHostname.SetToolTip(textHostname, "IP address or a resolvable DNS name of the Elasticsearch instance that the driver will connect to.");
			toolTipPort.SetToolTip(numericUpDownPort, "The port which the Elasticsearch listens on.");
			toolTipUsername.SetToolTip(textUsername, "If security is enabled, the username configured to access the REST SQL endpoint.");
			toolTipPassword.SetToolTip(textPassword, "If security is enabled, the password configured to access the REST SQL endpoint.");

			// Security Panel
			textCertificatePath.Text = Builder.ContainsKey("capath") ? Builder["capath"].ToString().StripBraces() : string.Empty;
			radioEnabledNoValidation.Checked = true; // Default setting
			if (Builder.ContainsKey("secure")) {
				var result = int.TryParse(Builder["secure"].ToString(), out int val);
				if (result) {
					switch (val) {
						case 0: radioButtonDisabled.Checked = true; break;
						case 1: radioEnabledNoValidation.Checked = true; break;
						case 2: radioEnabledNoHostname.Checked = true; break;
						case 3: radioEnabledHostname.Checked = true; break;
						case 4: radioEnabledFull.Checked = true; break;
					}
				}
			}

			toolTipDisabled.SetToolTip(radioButtonDisabled,
				"The communication between the driver and the Elasticsearch instance is performed over a clear-text connection." + Environment.NewLine
				+ "This setting can expose the access credentials to a 3rd party intercepting the network traffic and is not recommended.");

			toolTipEnabledNoValidation.SetToolTip(radioEnabledNoValidation,
				"The connection encryption is enabled, but the certificate of the server is not validated." + Environment.NewLine
				+ "This setting allows a 3rd party to act with ease as a man-in-the-middle and thus intercept all communications.");

			toolTipEnabledNoHostname.SetToolTip(radioEnabledNoHostname,
				"The connection encryption is enabled and the driver verifies that server's certificate is valid," + Environment.NewLine
				+ "but it does not verify if the certificate is running on the server it was meant for." + Environment.NewLine
				+ "This setting allows a 3rd party that had access to server's certificate to act as a man-in-the-middle" + Environment.NewLine
				+ "and thus intercept all the communications.");

			toolTipEnabledHostname.SetToolTip(radioEnabledHostname,
				"The connection encryption is enabled and the driver verifies that both the certificate is valid," + Environment.NewLine
				+ "as well as that it is being deployed on the server that the certificate was meant for.");

			toolTipEnabledFull.SetToolTip(radioEnabledFull,
				"This setting is equivalent to the previous one, with one additional check against certificate's revocation." + Environment.NewLine
				+ "This offers the strongest security option and is the recommended setting for production deployments.");

			toolTipCertificatePath.SetToolTip(textCertificatePath,
				"In case the server uses a certificate that is not part of the PKI, for example using a self-signed certificate," + Environment.NewLine
				+ "you can configure the path to a X509 certificate file that will be used by the driver to validate server's offered certificate.");

			// Proxy Panel
			string[] noes = {"no", "false", "0"};
			checkProxyEnabled.Checked = !noes.Contains(Builder.ContainsKey("ProxyEnabled") ? Builder["ProxyEnabled"].ToString() : "no");
			comboBoxProxyType.Text = Builder.ContainsKey("ProxyType") ? Builder["ProxyType"].ToString() : "HTTP";
			textProxyHostname.Text = Builder.ContainsKey("ProxyHost") ? Builder["ProxyHost"].ToString().StripBraces() : string.Empty;
			numericUpDownProxyPort.Text = Builder.ContainsKey("ProxyPort") ? Builder["ProxyPort"].ToString() : string.Empty;
			checkProxyAuthEnabled.Checked = !noes.Contains(Builder.ContainsKey("ProxyAuthEnabled") ? Builder["ProxyAuthEnabled"].ToString() : "no");
			textBoxProxyUsername.Text = Builder.ContainsKey("ProxyAuthUID") ? Builder["ProxyAuthUID"].ToString().StripBraces() : string.Empty;
			textBoxProxyPassword.Text = Builder.ContainsKey("ProxyAuthPWD") ? Builder["ProxyAuthPWD"].ToString().StripBraces() : string.Empty;
			toolTipProxyEnabled.SetToolTip(checkProxyEnabled, "This will enable relaying the connection to Elasticsearch over a proxy.");
			toolTipProxyType.SetToolTip(comboBoxProxyType, "The protocol to use when connecting to the proxy.");
			toolTipProxyHostname.SetToolTip(textProxyHostname, "The IP or domain name of the proxy server.");
			toolTipProxyPort.SetToolTip(numericUpDownProxyPort, "The port the proxy is listening on for connections.");
			toolTipProxyAuthEnabled.SetToolTip(checkProxyAuthEnabled, "Enables the authentication of the connection to the proxy.");
			toolTipProxyUsername.SetToolTip(textBoxProxyUsername, "The ID to use when authenticating to the proxy.");
			toolTipProxyPassword.SetToolTip(textBoxProxyPassword, "The password to use when authenticating to the proxy");

			// Logging Panel
			textLogDirectoryPath.Text = Builder.ContainsKey("tracefile") ? Builder["tracefile"].ToString().StripBraces() : string.Empty;
			comboLogLevel.Text = "DEBUG"; // Default setting
			checkLoggingEnabled.Checked = true; // Default setting
			if (Builder.ContainsKey("tracelevel")) {
				switch (Builder["tracelevel"].ToString().ToUpperInvariant()) {
					case "DEBUG": comboLogLevel.Text = "DEBUG"; break;
					case "INFO": comboLogLevel.Text = "INFO"; break;
					case "WARN": comboLogLevel.Text = "WARN"; break;
					case "ERROR": comboLogLevel.Text = "ERROR"; break;
				}
			}
			if (Builder.ContainsKey("traceenabled")) {
				var result = int.TryParse(Builder["traceenabled"].ToString(), out int val);
				if (result) {
					switch (val) {
						case 0: checkLoggingEnabled.Checked = false; break;
						default: checkLoggingEnabled.Checked = true; break;
					}
				}
			}
			else {
				checkLoggingEnabled.Checked = false;
			}

			toolTipLoggingEnabled.SetToolTip(checkLoggingEnabled,
				"Ticking this will enable driver's logging. A logging directory is also mandatory when this option is enabled," + Environment.NewLine
				+ "however the specified logging directory will be saved in the DSN if provided, even if logging is disabled.");

			toolTipLogDirectoryPath.SetToolTip(textLogDirectoryPath, "Specify which directory to write the log files in.");
			toolTipLogLevel.SetToolTip(comboLogLevel, "Configure the verbosity of the logs.");

			// Misc Panel
			numericUpDownTimeout.Text = Builder.ContainsKey("Timeout") ? Builder["Timeout"].ToString().StripBraces() : "0";
			numericUpDownFetchSize.Text = Builder.ContainsKey("MaxFetchSize") ? Builder["MaxFetchSize"].ToString().StripBraces() : "1000";
			numericUpDownBodySize.Text = Builder.ContainsKey("MaxBodySizeMB") ? Builder["MaxBodySizeMB"].ToString().StripBraces() : "100";
			numericUpDownVarcharLimit.Text = Builder.ContainsKey("VarcharLimit") ? Builder["VarcharLimit"].ToString().StripBraces() : "0";
			comboBoxFloatsFormat.Text = Builder.ContainsKey("ScientificFloats") ? Builder["ScientificFloats"].ToString().StripBraces() : "default";
			comboBoxDataEncoding.Text = Builder.ContainsKey("Packing") ? Builder["Packing"].ToString() : "CBOR";
			comboBoxDataCompression.Text = Builder.ContainsKey("Compression") ? Builder["Compression"].ToString() : "auto";

			checkBoxFollowRedirects.Checked = !noes.Contains(Builder.ContainsKey("Follow") ? Builder["Follow"].ToString().StripBraces() : "yes");
			checkBoxApplyTZ.Checked = !noes.Contains(Builder.ContainsKey("ApplyTZ") ? Builder["ApplyTZ"].ToString().StripBraces() : "no");
			checkBoxAutoEscapePVA.Checked = !noes.Contains(Builder.ContainsKey("AutoEscapePVA") ? Builder["AutoEscapePVA"].ToString().StripBraces() : "yes");
			checkBoxEarlyExecution.Checked = !noes.Contains(Builder.ContainsKey("EarlyExecution") ? Builder["EarlyExecution"].ToString().StripBraces() : "yes");
			checkBoxMultiFieldLenient.Checked = !noes.Contains(Builder.ContainsKey("MultiFieldLenient") ? Builder["MultiFieldLenient"].ToString().StripBraces() : "yes");
			checkBoxIndexIncludeFrozen.Checked = !noes.Contains(Builder.ContainsKey("IndexIncludeFrozen") ? Builder["IndexIncludeFrozen"].ToString().StripBraces() : "no");

			toolTipTimeout.SetToolTip(numericUpDownTimeout, "The maximum number of seconds for a request to the server. The value 0 disables the timeout.");
			toolTipFetchSize.SetToolTip(numericUpDownFetchSize, "The maximum number of rows that Elasticsearch SQL server should send back to the driver for one page.");
			toolTipBodySize.SetToolTip(numericUpDownBodySize, "The maximum number of megabytes that the driver will accept for one page.");
			toolTipVarcharLimit.SetToolTip(numericUpDownVarcharLimit, "The maximum number of characters that the string type (TEXT, KEYWORD etc.) columns will exhibit. "
				+ "The value 0 disables the driver limitation.");
			toolTipFloatsFormat.SetToolTip(comboBoxFloatsFormat, "How should the floating point numbers be printed, when these are converted to string by the driver.");
			toolTipDataEncoding.SetToolTip(comboBoxDataEncoding, "How should the data between the server and the driver be encoded as.");
			toolTipDataCompression.SetToolTip(comboBoxDataCompression, "Should the data between the server and the driver be compressed?");
			toolTipFollowRedirects.SetToolTip(checkBoxFollowRedirects, "Should the driver follow HTTP redirects of the requests to the server?");
			toolTipApplyTZ.SetToolTip(checkBoxApplyTZ, "Should the driver use machine's local timezone? The default is UTC.");
			toolTipAutoEscapePVA.SetToolTip(checkBoxAutoEscapePVA, "Should the driver auto-escape the pattern-value arguments?");
			toolTipMultiFieldLenient.SetToolTip(checkBoxMultiFieldLenient, "Should the server return one value out of a multi-value field (instead of rejecting the request)?");
			toolTipEarlyExecution.SetToolTip(checkBoxEarlyExecution, "Should a query be executed already at preparation time? This will only happen if the query lacks parameters.");
			toolTipIndexIncludeFrozen.SetToolTip(checkBoxIndexIncludeFrozen, "Should the server consider the frozen indices when servicing a request?");

			// Set initial state of action buttons.
			EnableDisableActionButtons();
		}

		/// <summary>
		///     On save, call the driver's callback. If operation succeeds, close the window.
		///     On failure, display the error received from the driver and keep editing.
		/// </summary>
		private void SaveButton_Click(object sender, EventArgs e) => SaveDsn(false);

		private void SaveDsn(bool forceOverwrite)
		{
			var errorMessage = string.Empty;

			var dsnResult = RebuildAndValidateDsn();
			if (!dsnResult) return;

			var dsn = Builder.ToString();
			var flag = forceOverwrite ? 1u : 0;

			var result = saveDsn(dsn, ref errorMessage, flag);
			if (result >= 0 || (forceOverwrite && result == ESODBC_DSN_EXISTS_ERROR)) {
				Close();
				return;
			}

			// Specific handling for prompting the user if result is an overwrite action
			if (forceOverwrite == false
				&& result == ESODBC_DSN_EXISTS_ERROR) {
				var dialogResult = MessageBox.Show("The DSN already exists, are you sure you wish to overwrite it?", "Overwrite", MessageBoxButtons.YesNo);
				if (dialogResult == DialogResult.Yes) {
					SaveDsn(true);
				}

				return;
			}

			if (errorMessage.Length <= 0) {
				errorMessage = (result == ESODBC_DSN_NAME_INVALID_ERROR)
					? "Invalid DSN name"
					: "Saving the DSN failed";
			}

			MessageBox.Show(errorMessage, "Operation failure", MessageBoxButtons.OK, MessageBoxIcon.Error);
		}

		/// <summary>
		///     With the Test button the user checks if the input data leads to a connection.
		///     The function calls the driver callback and displays the result of the operation.
		/// </summary>
		private void TestConnectionButton_Click(object sender, EventArgs e)
		{
			var errorMessage = string.Empty;

			var dsnResult = RebuildAndValidateDsn();
			if (!dsnResult) return;

			var dsn = Builder.ToString();

			// Wrap slow operation in a wait cursor
			Cursor = Cursors.WaitCursor;
			var result = testConnection(dsn, ref errorMessage, 0);
			Cursor = Cursors.Arrow;

			if (result >= 0) {
				MessageBox.Show("Connection Success", "Connection Test", MessageBoxButtons.OK, MessageBoxIcon.Information);
			}
			else {
				var message = "Connection Failed";
				if (0 < errorMessage.Length) {
					message += $": {errorMessage}";
				}
				MessageBox.Show(message, "Connection Test", MessageBoxButtons.OK, MessageBoxIcon.Error);
			}
		}

		private bool RebuildAndValidateDsn()
		{
			// Basic Panel
			Builder["dsn"] = textName.Text;
			Builder["description"] = textDescription.Text;
			// https://www.elastic.co/guide/en/elasticsearch/reference/current/security-api-put-user.html#security-api-put-user-path-params :
			// "Leading or trailing whitespace is not allowed." -> it is more likely that a user will insert a
			// (trailing) white space by mistake than intentionally trying to set it.
			Builder["uid"] = textUsername.Text.Trim();
			Builder["pwd"] = textPassword.Text;
			Builder["cloudid"] = "{" + textCloudID.Text.StripBraces().Trim() + "}";
			Builder["server"] = textHostname.Text.Trim();
			Builder["port"] = numericUpDownPort.Text;

			// Security Panel
			Builder["capath"] = textCertificatePath.Text;
			if (radioButtonDisabled.Checked) Builder["secure"] = 0;
			if (radioEnabledNoValidation.Checked) Builder["secure"] = 1;
			if (radioEnabledNoHostname.Checked) Builder["secure"] = 2;
			if (radioEnabledHostname.Checked) Builder["secure"] = 3;
			if (radioEnabledFull.Checked) Builder["secure"] = 4;

			// Proxy Panel
			Builder["ProxyEnabled"] = checkProxyEnabled.Checked ? "true" : "false";
			Builder["ProxyType"] = comboBoxProxyType.Text;
			Builder["ProxyHost"] = textProxyHostname.Text;
			Builder["ProxyPort"] = numericUpDownProxyPort.Text;
			Builder["ProxyAuthEnabled"] = checkProxyAuthEnabled.Checked ? "true" : "false";
			Builder["ProxyAuthUID"] = textBoxProxyUsername.Text;
			Builder["ProxyAuthPWD"] = textBoxProxyPassword.Text;

			// Logging Panel
			Builder["tracefile"] = textLogDirectoryPath.Text;
			Builder["tracelevel"] = comboLogLevel.Text;
			Builder["traceenabled"] = checkLoggingEnabled.Checked ? "1" : "0";

			// Misc Panel
			Builder["Timeout"] = numericUpDownTimeout.Text;
			Builder["MaxFetchSize"] = numericUpDownFetchSize.Text;
			Builder["MaxBodySizeMB"] = numericUpDownBodySize.Text;
			Builder["VarcharLimit"] = numericUpDownVarcharLimit.Text;
			Builder["ScientificFloats"] = comboBoxFloatsFormat.Text;
			Builder["Packing"] = comboBoxDataEncoding.Text;
			Builder["Compression"] = comboBoxDataCompression.Text;
			Builder["Follow"] = checkBoxFollowRedirects.Checked ? "true" : "false";
			Builder["ApplyTZ"] = checkBoxApplyTZ.Checked ? "true" : "false";
			Builder["AutoEscapePVA"] = checkBoxAutoEscapePVA.Checked ? "true" : "false";
			Builder["EarlyExecution"] = checkBoxEarlyExecution.Checked ? "true" : "false";
			Builder["MultiFieldLenient"] = checkBoxMultiFieldLenient.Checked ? "true" : "false";
			Builder["IndexIncludeFrozen"] = checkBoxIndexIncludeFrozen.Checked ? "true" : "false";

			// Validations
			var keynameOK = true;
			var certificateFileOK = true;
			var logDirectoryOK = true;

			if (!string.IsNullOrEmpty(textName.Text)) {
				keynameOK = ValidateKeyName(textName.Text);
			}

			if (!string.IsNullOrEmpty(textCertificatePath.Text)) {
				certificateFileOK = ValidateCertificateFile(textCertificatePath.Text);
			}

			if (!string.IsNullOrEmpty(textLogDirectoryPath.Text)) {
				logDirectoryOK = ValidateLogFolderPath(textLogDirectoryPath.Text);
			}

			return keynameOK && certificateFileOK && logDirectoryOK;
		}

		private void LogDirectoryPathButton_Click(object sender, EventArgs e)
		{
			var result = folderLogDirectoryDialog.ShowDialog();
			if (result == DialogResult.OK) {
				var path = folderLogDirectoryDialog.SelectedPath;
				if (ValidateLogFolderPath(path)) {
					textLogDirectoryPath.Text = path;
				}
			}
		}

		private bool ValidateLogFolderPath(string path)
		{
			if (!checkLoggingEnabled.Checked || string.IsNullOrEmpty(path) || Directory.Exists(path)) {
				return true;
			}

			MessageBox.Show("Log directory invalid, path does not exist", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
			return false;
		}

		private void CertificatePathButton_Click(object sender, EventArgs e)
		{
			var result = certificateFileDialog.ShowDialog();
			if (result == DialogResult.OK) {
				var file = certificateFileDialog.FileName;
				if (ValidateCertificateFile(file)) {
					textCertificatePath.Text = file;
				}
			}
		}

		private bool ValidateKeyName(string keyname)
		{
			if (keyname.Length > 255) {
				MessageBox.Show("Name must be less than 255 characters", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
				return false;
			}

			if (keyname.Contains("\\")) {
				MessageBox.Show("Name cannot contain backslash \\ characters", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
				return false;
			}

			return true;
		}

		private bool ValidateCertificateFile(string file)
		{
			if (string.IsNullOrEmpty(file)) {
				return true;
			}

			var info = new FileInfo(file);
			if (info.Exists && info.Length > 0) {
				return true;
			}

			MessageBox.Show("Certificate file invalid", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
			return false;
		}

		private void TextName_TextChanged(object sender, EventArgs e) => EnableDisableActionButtons();

		private void TextHostname_TextChanged(object sender, EventArgs e) => EnableDisableActionButtons();

		private void TextCloudID_TextChanged(object sender, EventArgs e) => EnableDisableActionButtons();

		private void NumericUpDownPort_ValueChanged(object sender, EventArgs e) => EnableDisableActionButtons();

		private void CheckLoggingEnabled_CheckedChanged(object sender, EventArgs e) => EnableDisableLoggingControls();

		private void CheckProxyEnabled_CheckedChanged(object sender, EventArgs e) => EnableDisableProxyControls();

		private void CheckProxyAuthEnabled_CheckedChanged(object sender, EventArgs e) => EnableDisableProxyAuthControls();

		private void ComboBoxProxyType_SelectedIndexChanged(object sender, EventArgs e) => UpdateDefaultPort();

		private void EnableDisableActionButtons()
		{
			if (string.IsNullOrEmpty(textCloudID.Text) == false) {
				radioButtonDisabled.Checked = false;
				radioEnabledNoValidation.Checked = false;
				radioEnabledNoHostname.Checked = false;
				radioEnabledHostname.Checked = false;
				radioEnabledFull.Checked = false;
				groupSSL.Enabled = false;
				Builder.Remove("secure");

				textHostname.ResetText();
				Builder.Remove("server");
				numericUpDownPort.ResetText();
				Builder.Remove("port");
			}
			else {
				groupSSL.Enabled = true;
			}

			textCloudID.Enabled = string.IsNullOrEmpty(textHostname.Text);
			textHostname.Enabled = numericUpDownPort.Enabled
				= textCertificatePath.Enabled = certificatePathButton.Enabled
				= string.IsNullOrEmpty(textCloudID.Text);

			if (isConnecting) {
				// If connecting, enable the button if we have a hostname.
				// This can be triggered by app connecting or FileDSN verifying the connection.
				saveButton.Enabled = string.IsNullOrEmpty(textHostname.Text) == false || string.IsNullOrEmpty(textCloudID.Text) == false;
			} else {
				// If editing a (User/System) DSN, enable the buton if both DSN name and hostname are available.
				saveButton.Enabled = string.IsNullOrEmpty(textName.Text) == false
									 && (string.IsNullOrEmpty(textHostname.Text) == false || string.IsNullOrEmpty(textCloudID.Text) == false);
			}
			testButton.Enabled = string.IsNullOrEmpty(textHostname.Text) == false || string.IsNullOrEmpty(textCloudID.Text) == false;
		}

		private void EnableDisableLoggingControls()
		{
			textLogDirectoryPath.Enabled = checkLoggingEnabled.Checked;
			comboLogLevel.Enabled = checkLoggingEnabled.Checked;
			logDirectoryPathButton.Enabled = checkLoggingEnabled.Checked;
		}

		private void EnableDisableProxyControls()
		{
			comboBoxProxyType.Enabled = checkProxyEnabled.Checked;
			textProxyHostname.Enabled = checkProxyEnabled.Checked;
			numericUpDownProxyPort.Enabled = checkProxyEnabled.Checked;
			checkProxyAuthEnabled.Enabled = checkProxyEnabled.Checked;
			textBoxProxyUsername.Enabled = checkProxyAuthEnabled.Checked && checkProxyEnabled.Checked;
			textBoxProxyPassword.Enabled = checkProxyAuthEnabled.Checked && checkProxyEnabled.Checked;
		}

		private void EnableDisableProxyAuthControls()
		{
			textBoxProxyUsername.Enabled = checkProxyAuthEnabled.Checked;
			textBoxProxyPassword.Enabled = checkProxyAuthEnabled.Checked;
		}

		private void UpdateDefaultPort()
		{
			switch(comboBoxProxyType.Text.ToUpperInvariant())
			{
				case "HTTP": numericUpDownProxyPort.Text = "8080"; break;
				// TODO: https://github.com/jeroen/curl/issues/186 : "Schannel backend doesn't support HTTPS proxy"
				case "HTTPS": numericUpDownProxyPort.Text = "443"; break;
				case "SOCKS4":
				case "SOCKS4A":
				case "SOCKS5":
				case "SOCKS5H": numericUpDownProxyPort.Text = "1080"; break;
			}
		}

		private void CancelButton_Click(object sender, EventArgs e)
		{
			// Clear the builder so that the resulting int returned to the caller is 0.
			Builder.Clear();
			Close();
		}

		// Remove the [X] close button in top right to force user to use the cancel button.
		protected override CreateParams CreateParams
		{
			get {
				const int WS_SYSMENU = 0x80000;
				var cp = base.CreateParams;
				cp.Style &= ~WS_SYSMENU;
				return cp;
			}
		}
	}
}
