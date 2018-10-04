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
        private DriverCallbackDelegate testConnection;
        private DriverCallbackDelegate saveDsn;

        public OdbcConnectionStringBuilder Builder { get; set; } = new OdbcConnectionStringBuilder();

        public DsnEditorForm(
            bool onConnect,
            string dsn,
            DriverCallbackDelegate connectionTest,
            DriverCallbackDelegate dsnSave)
        {
            InitializeComponent();

            AcceptButton = saveButton;
            CancelButton = cancelButton;

			// Save the delegates
            testConnection = connectionTest;
            saveDsn = dsnSave;

            // If this is a call serving a connect request, call the button "Connect".
            // Otherwise it's a DSN editing, so it's going to be a "Save".
            saveButton.Text = onConnect ? "Connect" : "Save";

			// Parse the connection string using the builder
            Builder.ConnectionString = dsn;

			// Basic Panel
            textName.Text = Builder.ContainsKey("dsn") ? Builder["dsn"].ToString() : string.Empty;
            textDescription.Text = Builder.ContainsKey("description") ? Builder["description"].ToString() : string.Empty;
            textUsername.Text = Builder.ContainsKey("uid") ? Builder["uid"].ToString() : string.Empty;
            textPassword.Text = Builder.ContainsKey("pwd") ? Builder["pwd"].ToString() : string.Empty;
            textHostname.Text = Builder.ContainsKey("server") ? Builder["server"].ToString() : "localhost";
            numericUpDownPort.Text = Builder.ContainsKey("port") ? Builder["port"].ToString() : "9200";
            
			// Security Panel
            if (Builder.ContainsKey("secure"))
            {
                var val = Convert.ToInt32(Builder["secure"]);
                switch(val)
                {
                    case 0: radioButtonDisabled.Checked = true; break;
                    case 1: radioEnabledNoValidation.Checked = true; break;
                    case 2: radioEnabledNoHostname.Checked = true; break;
                    case 3: radioEnabledHostname.Checked = true; break;
                    case 4: radioEnabledFull.Checked = true; break;
                }
            }
            else
            {
                radioEnabledNoValidation.Checked = true;
            }

            textCertificatePath.Text = Builder.ContainsKey("capath") ? Builder["capath"].ToString() : string.Empty;
        }

        /// <summary>
        ///     On save, call the driver's callback. If operation succeeds, close the window.
        ///     On failure, display the error received from the driver and keep editing.
        /// </summary>
        private void SaveButton_Click(object sender, EventArgs e)
        {
            var errorMessage = string.Empty;

            var dsnResult = RebuildAndValidateDsn();
            if (!dsnResult) return;

            var dsn = Builder.ToString();

            int result = saveDsn(dsn, ref errorMessage, 0);
            if (result >= 0)
            {
                Close();
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
            if (!string.IsNullOrEmpty(textName.Text)) Builder["dsn"] = textName.Text;
            if (!string.IsNullOrEmpty(textDescription.Text)) Builder["description"] = textDescription.Text;
            if (!string.IsNullOrEmpty(textUsername.Text)) Builder["uid"] = textUsername.Text;
            if (!string.IsNullOrEmpty(textPassword.Text)) Builder["pwd"] = textPassword.Text;
            if (!string.IsNullOrEmpty(textHostname.Text)) Builder["server"] = textHostname.Text;
            if (!string.IsNullOrEmpty(numericUpDownPort.Text)) Builder["port"] = numericUpDownPort.Text;

			// Security Panel
            if (radioButtonDisabled.Checked) Builder["secure"] = 0;
            if (radioEnabledNoValidation.Checked) Builder["secure"] = 1;
            if (radioEnabledNoHostname.Checked) Builder["secure"] = 2;
            if (radioEnabledHostname.Checked) Builder["secure"] = 3;
            if (radioEnabledFull.Checked) Builder["secure"] = 4;

            if (!string.IsNullOrEmpty(textCertificatePath.Text))
            {
                Builder["capath"] = textCertificatePath.Text;
                return ValidateCertificateFile(textCertificatePath.Text);
            }

            return true;
        }

        private bool ValidateCertificateFile(string file)
        {
            if (File.Exists(file) && File.ReadAllBytes(file).Length > 0)
            {
                return true;
            }

            MessageBox.Show("Certificate file invalid", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }
    }
}