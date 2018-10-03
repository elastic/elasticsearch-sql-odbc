// break at 120 columns

using System;
using System.Resources;
using System.Windows.Forms;

// uncomment to have the assembley loading to ask for (various) resources; various solutions: 
// https://stackoverflow.com/questions/4368201/appdomain-currentdomain-assemblyresolve-asking-for-a-appname-resources-assembl
// [assembly: NeutralResourcesLanguageAttribute("en-GB", UltimateResourceFallbackLocation.MainAssembly)]

namespace EsOdbcDsnEditor
{
    /*
     * Delegate for the driver callbacks.
     */
    public delegate int DriverCallbackDelegate(string connectionString, ref string errorMessage, uint flags);

    public partial class DsnEditorForm : Form
    {
        private String dsn; // the connection string (also called DSN, in this project)
        private DriverCallbackDelegate delegConnectionTest;
        private DriverCallbackDelegate delegSaveDsn;

        public string Dsn { get => dsn; set => dsn = value; }
        

        public DsnEditorForm(bool onConnect, String dsn,
            DriverCallbackDelegate delegConnectionTest, DriverCallbackDelegate delegSaveDsn)
        {
            InitializeComponent();
            AcceptButton = saveButton;
            CancelButton = cancelButton;
            // TODO: how to map the X (closing) button (ALT-F4 & co.) to the CancelButton?
                        
            /*
             * If this is a call serving a connect request, call the button "Connect".
             * Otherwise it's a DSN editing, so it's going to be a "Save".
             */
            saveButton.Text = onConnect ? "Connect" : "Save";

            Dsn = dsn;
            
            this.delegConnectionTest = delegConnectionTest;
            this.delegSaveDsn = delegSaveDsn;            
        }

        private void DsnEditorForm_Load(object sender, EventArgs e)
        {
            connStringBox.Text = Dsn;
            connStringBox_TextChanged(null, null);
        }

        //private void label1_Click(object sender, EventArgs e) {}

        /*
         * On save, call the driver's callback. If operation succeeds, close the window.
         * On failure, display the error received from the driver and keep editing.
         */
        private void saveButton_Click(object sender, EventArgs e)
        {
            String errorMessage = ""; // must be init'ed
            Dsn = connStringBox.Text;

            int ret = delegSaveDsn(Dsn, ref errorMessage, 0);

            if (0 <= ret)
            {
                Close();
                return;
            }
            // saving failed
            if (errorMessage.Length <= 0)
            {
                errorMessage = "Saving the DSN failed";
            }
            MessageBox.Show(errorMessage, "Operation failure", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            // Empty DSN signals the user canceling the editing.
            Dsn = "";
            Close();
        }

        /*
         * With the Test button the user checks if the input data leads to a connection.
         * The function calls the driver callback and displays the result of the operation.
         */
        private void testButton_Click(object sender, EventArgs e)
        {
            String errorMessage = ""; // must be init'ed
            Dsn = connStringBox.Text;

            int ret = delegConnectionTest(Dsn, ref errorMessage, 0);

            if (0 <= ret)
            {
                MessageBox.Show("Connection success.", "Connection Test", MessageBoxButtons.OK);
            }
            else
            {
                String message = "Connection failed";
                if (0 < errorMessage.Length)
                {
                    message += ": " + errorMessage;
                }
                message += ".";
                MessageBox.Show(message, "Connection Test", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            
        }

        private void connStringBox_TextChanged(object sender, EventArgs e)
        {
            /*
             * only enable the Save and Test buttons if there's some input to work with.
             */
            testButton.Enabled = 0 < connStringBox.Text.Length ? true : false;
            saveButton.Enabled = 0 < connStringBox.Text.Length ? true : false;
        }

        // TODO: register this in a handler.
        private void DSNEditorForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            Dsn = "";
        }
    }

    public static class DsnEditorFactory
    {
        public static int DsnEditor(bool onConnect, String dsnIn,
            DriverCallbackDelegate delegConnectionTest, DriverCallbackDelegate delegSaveDsn)
        {
            Application.EnableVisualStyles();
            // this would trigger errors on subsequent factory invocations
            //Application.SetCompatibleTextRenderingDefault(false);
     
            DsnEditorForm form = new DsnEditorForm(onConnect, dsnIn, delegConnectionTest, delegSaveDsn);
            Application.Run(form);
            return form.Dsn.Length;
        }
    }
}
