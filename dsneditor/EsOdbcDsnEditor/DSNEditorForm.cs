using System;
using System.Data.Odbc;
using System.Windows.Forms;
using System.IO;

// uncomment to have the assembly loading to ask for (various) resources; various solutions: 
// https://stackoverflow.com/questions/4368201/appdomain-currentdomain-assemblyresolve-asking-for-a-appname-resources-assembl
// [assembly: NeutralResourcesLanguageAttribute("en-GB", UltimateResourceFallbackLocation.MainAssembly)]

namespace EsOdbcDsnEditor
{
    /// <summary>
    ///     Delegate for the driver callbacks.
    /// </summary>
    public delegate int DriverCallbackDelegate(string connectionString, ref string errorMessage, uint flags);

    public partial class DsnEditorForm : Form
    {
        private const int ESODBC_DSN_EXISTS_ERROR = -1;

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
            if (isConnecting)
            {
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
            textHostname.Text = Builder.ContainsKey("server") ? Builder["server"].ToString().StripBraces() : string.Empty;
            numericUpDownPort.Text = Builder.ContainsKey("port") ? Builder["port"].ToString().StripBraces() : string.Empty;

            // Security Panel
            textCertificatePath.Text = Builder.ContainsKey("capath") ? Builder["capath"].ToString().StripBraces() : string.Empty;
            radioEnabledNoValidation.Checked = true; // Default setting
            if (Builder.ContainsKey("secure"))
            {
                var result = int.TryParse(Builder["secure"].ToString(), out int val);
                if (result)
                { 
                    switch(val)
                    {
                        case 0: radioButtonDisabled.Checked = true; break;
                        case 1: radioEnabledNoValidation.Checked = true; break;
                        case 2: radioEnabledNoHostname.Checked = true; break;
                        case 3: radioEnabledHostname.Checked = true; break;
                        case 4: radioEnabledFull.Checked = true; break;
                    }
                }
            }

            // Logging Panel
            textLogDirectoryPath.Text = Builder.ContainsKey("tracefile") ? Builder["tracefile"].ToString().StripBraces() : string.Empty;
            comboLogLevel.Text = "DEBUG"; // Default setting
            checkLoggingEnabled.Checked = true; // Default setting
            if (Builder.ContainsKey("tracelevel"))
            {
                switch (Builder["tracelevel"].ToString().ToUpperInvariant())
                {
                    case "DEBUG": comboLogLevel.Text = "DEBUG"; break;
                    case "INFO": comboLogLevel.Text = "INFO"; break;
                    case "WARN": comboLogLevel.Text = "WARN"; break;
                    case "ERROR": comboLogLevel.Text = "ERROR"; break;
                }
            }
            if (Builder.ContainsKey("traceenabled"))
            {
                var result = int.TryParse(Builder["traceenabled"].ToString(), out int val);
                if (result)
                {
                    switch (val)
                    {
                        case 0: checkLoggingEnabled.Checked = false; break;
                        default: checkLoggingEnabled.Checked = true; break;
                    }
                }
            }
            else
            {
                checkLoggingEnabled.Checked = false;
            }

            // Set initial state of action buttons.
            EnableDisableActionButtons();
        }

        /// <summary>
        ///     On save, call the driver's callback. If operation succeeds, close the window.
        ///     On failure, display the error received from the driver and keep editing.
        /// </summary>
        private void SaveButton_Click(object sender, EventArgs e)
        {
            SaveDsn(false);
        }

        private void SaveDsn(bool forceOverwrite)
        {
            var errorMessage = string.Empty;

            var dsnResult = RebuildAndValidateDsn();
            if (!dsnResult) return;

            var dsn = Builder.ToString();
            var flag = forceOverwrite ? 1u : 0;

            int result = saveDsn(dsn, ref errorMessage, flag);
            if (result >= 0 || (forceOverwrite
                                && result == ESODBC_DSN_EXISTS_ERROR))
            {
                Close();
                return;
            }

            // Specific handling for prompting the user if result is an overwrite action
            if (forceOverwrite == false
                && result == ESODBC_DSN_EXISTS_ERROR)
            {
                var dialogResult = MessageBox.Show("The DSN already exists, are you sure you wish to overwrite it?", "Overwrite", MessageBoxButtons.YesNo);
                if (dialogResult == DialogResult.Yes)
                {
                    SaveDsn(true);
                }

                return;
            }

            if (errorMessage.Length <= 0)
            {
                errorMessage = "Saving the DSN failed";
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
            int result = testConnection(dsn, ref errorMessage, 0);
            Cursor = Cursors.Arrow;

            if (result >= 0)
            {
                MessageBox.Show("Connection Success", "Connection Test", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            else
            {
                var message = "Connection Failed";
                if (0 < errorMessage.Length)
                {
                    message += ": " + errorMessage;
                }
                MessageBox.Show(message, "Connection Test", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private bool RebuildAndValidateDsn()
        {
            // Basic Panel
            Builder["dsn"] = textName.Text;
            Builder["description"] = textDescription.Text;
            Builder["uid"] = textUsername.Text;
            Builder["pwd"] = textPassword.Text;
            Builder["server"] = textHostname.Text;
            Builder["port"] = numericUpDownPort.Text;

            // Security Panel
            Builder["capath"] = textCertificatePath.Text;
            if (radioButtonDisabled.Checked) Builder["secure"] = 0;
            if (radioEnabledNoValidation.Checked) Builder["secure"] = 1;
            if (radioEnabledNoHostname.Checked) Builder["secure"] = 2;
            if (radioEnabledHostname.Checked) Builder["secure"] = 3;
            if (radioEnabledFull.Checked) Builder["secure"] = 4;

            // Logging Panel
            Builder["tracefile"] = textLogDirectoryPath.Text;
            Builder["tracelevel"] = comboLogLevel.Text;
            Builder["traceenabled"] = checkLoggingEnabled.Checked ? "1" : "0";

            // Validations
            var certificateFileOK = true;
            var logDirectoryOK = true;

            if (!string.IsNullOrEmpty(textCertificatePath.Text))
            {
                certificateFileOK = ValidateCertificateFile(textCertificatePath.Text);
            }

            if (!string.IsNullOrEmpty(textLogDirectoryPath.Text))
            {
                logDirectoryOK = ValidateLogFolderPath(textLogDirectoryPath.Text);
            }

            return certificateFileOK && logDirectoryOK;
        }
        
        private void LogDirectoryPathButton_Click(object sender, EventArgs e)
        {
            var result = folderLogDirectoryDialog.ShowDialog();
            if (result == DialogResult.OK)
            {
                string path = folderLogDirectoryDialog.SelectedPath;
                if (ValidateLogFolderPath(path))
                {
                    textLogDirectoryPath.Text = path;
                }
            }
        }

        private bool ValidateLogFolderPath(string path)
        {
            if (string.IsNullOrEmpty(path) || Directory.Exists(path))
            {
                return true;
            }

            MessageBox.Show("Log directory invalid", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }

        private void CertificatePathButton_Click(object sender, EventArgs e)
        {
            var result = certificateFileDialog.ShowDialog();
            if (result == DialogResult.OK)
            {
                string file = certificateFileDialog.FileName;
                if (ValidateCertificateFile(file))
                {
                    textCertificatePath.Text = file;
                }
            }
        }

        private bool ValidateCertificateFile(string file)
        {
            if (string.IsNullOrEmpty(file)
                || (File.Exists(file) && File.ReadAllBytes(file).Length > 0))
            {
                return true;
            }

            MessageBox.Show("Certificate file invalid", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }

        private void TextName_TextChanged(object sender, EventArgs e)
        {
            EnableDisableActionButtons();
        }

        private void TextHostname_TextChanged(object sender, EventArgs e)
        {
            EnableDisableActionButtons();
        }

        private void NumericUpDownPort_ValueChanged(object sender, EventArgs e)
        {
            EnableDisableActionButtons();
        }

        private void EnableDisableActionButtons()
        {
            saveButton.Enabled = string.IsNullOrEmpty(textName.Text) == false
                                 && string.IsNullOrEmpty(textHostname.Text) == false;
            testButton.Enabled = string.IsNullOrEmpty(textHostname.Text) == false;
        }
        
        private void CheckLoggingEnabled_CheckedChanged(object sender, EventArgs e)
        {
            EnableDisableLoggingControls();
        }

        private void EnableDisableLoggingControls()
        {
            textLogDirectoryPath.Enabled = checkLoggingEnabled.Checked;
            comboLogLevel.Enabled = checkLoggingEnabled.Checked;
            logDirectoryPathButton.Enabled = checkLoggingEnabled.Checked;
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
            get
            {
                const int WS_SYSMENU = 0x80000;
                CreateParams cp = base.CreateParams;
                cp.Style &= ~WS_SYSMENU;
                return cp;
            }
        }
    }
}