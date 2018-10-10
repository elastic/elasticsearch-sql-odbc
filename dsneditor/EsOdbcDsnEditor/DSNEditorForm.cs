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
            this.isConnecting = onConnect;

            InitializeComponent();

            AcceptButton = saveButton;
            CancelButton = cancelButton;
            testConnection = connectionTest;
            saveDsn = dsnSave;

            // If connecting then disable some user inputs
            if (isConnecting)
            {
                textName.ReadOnly = true;
                textName.Enabled = false;

                textDescription.ReadOnly = true;
                textDescription.Enabled = false;
            }

            // If this is a call serving a connect request, call the button "Connect".
            // Otherwise it's a DSN editing, so it's going to be a "Save".
            saveButton.Text = onConnect ? "Connect" : "Save";

            // Parse DSN
            Builder.ConnectionString = dsn;

            // Basic Panel
            textName.Text = Builder.ContainsKey("dsn") ? StripBraces(Builder["dsn"].ToString()) : string.Empty;
            textDescription.Text = Builder.ContainsKey("description") ? StripBraces(Builder["description"].ToString()) : string.Empty;
            textUsername.Text = Builder.ContainsKey("uid") ? StripBraces(Builder["uid"].ToString()) : string.Empty;
            textPassword.Text = Builder.ContainsKey("pwd") ? StripBraces(Builder["pwd"].ToString()) : string.Empty;
            textHostname.Text = Builder.ContainsKey("server") ? StripBraces(Builder["server"].ToString()) : string.Empty;
            numericUpDownPort.Text = Builder.ContainsKey("port") ? StripBraces(Builder["port"].ToString()) : string.Empty;

            // Security Panel
            radioEnabledNoValidation.Checked = true; // Default setting
            textCertificatePath.Text = Builder.ContainsKey("capath") ? StripBraces(Builder["capath"].ToString()) : string.Empty;

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
        }

        /// <summary>
        ///     On save, call the driver's callback. If operation succeeds, close the window.
        ///     On failure, display the error received from the driver and keep editing.
        /// </summary>
        private void SaveButton_Click(object sender, EventArgs e)
        {
            SaveDsn(false);
        }

        private void SaveDsn(bool allowOverwrites)
        {
            var errorMessage = string.Empty;

            var dsnResult = RebuildAndValidateDsn();
            if (!dsnResult) return;

            var dsn = Builder.ToString();
            var flag = allowOverwrites ? 1u : 0;

            int result = saveDsn(dsn, ref errorMessage, flag);
            if (result >= 0 || (allowOverwrites
                                && result == ESODBC_DSN_EXISTS_ERROR))
            {
                Close();
                return;
            }

            // Specific handling for prompting the user if overwriting
            if (allowOverwrites == false
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

            int result = testConnection(dsn, ref errorMessage, 0);

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

        private void DsnEditorForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            Builder.Clear();
        }

        private void CancelButton_Click(object sender, EventArgs e)
        {
            Builder.Clear();
            Close();
        }

        private void CertificatePathButton_Click(object sender, EventArgs e)
        {
            var result = certificateFileDialog.ShowDialog();
            if (result == DialogResult.OK)
            {
                string file = certificateFileDialog.FileName;
                ValidateCertificateFile(file);
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
            if (radioButtonDisabled.Checked) Builder["secure"] = 0;
            if (radioEnabledNoValidation.Checked) Builder["secure"] = 1;
            if (radioEnabledNoHostname.Checked) Builder["secure"] = 2;
            if (radioEnabledHostname.Checked) Builder["secure"] = 3;
            if (radioEnabledFull.Checked) Builder["secure"] = 4;

            Builder["capath"] = textCertificatePath.Text;

            if (!string.IsNullOrEmpty(textCertificatePath.Text))
            {
                return ValidateCertificateFile(textCertificatePath.Text);
            }

            return true;
        }

        private bool ValidateCertificateFile(string file)
        {
            // Simple validation
            if (File.Exists(file) && File.ReadAllBytes(file).Length > 0)
            {
                return true;
            }

            MessageBox.Show("Certificate file invalid", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }

        private static string StripBraces(string input)
        {
            if (input.StartsWith("{") && input.EndsWith("}"))
            {
                return input.Substring(1, input.Length - 2);
            }
            return input;
        }

        private void textName_TextChanged(object sender, EventArgs e)
        {

        }

        private void textHostname_TextChanged(object sender, EventArgs e)
        {

        }

        private void numericUpDownPort_ValueChanged(object sender, EventArgs e)
        {

        }
    }
}