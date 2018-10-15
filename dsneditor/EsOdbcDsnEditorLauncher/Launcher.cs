using System;
using System.Threading;
using System.Windows.Forms;

namespace EsOdbcDsnEditorLauncher
{
    public partial class Launcher : Form
    {
        public Launcher()
        {
            InitializeComponent();
        }

        private void LaunchButton_Click(object sender, EventArgs e)
        {
            bool onConnect = false;
            string dsn = "driver={Elasticsearch Driver};Database=localhost;hostname=localhost;uid=elastic;pwd=pass!word1;secure=4";
            var form = new EsOdbcDsnEditor.DsnEditorForm(onConnect, dsn, ConnectTest, SaveDsn);
            form.Show();
        }

        private int ConnectTest(string connectionString, ref string errorMessage, uint flags)
        {
            Thread.Sleep(5000); // Simulate a slow connection test
            textLog.Text += "CONNECT. Connection String:" + connectionString + Environment.NewLine;
            return 0;
        }

        private int SaveDsn(string connectionString, ref string errorMessage, uint flags)
        {
            textLog.Text += "SAVE. Connection String:" + connectionString + Environment.NewLine;
            //errorMessage = "ESODBC_DSN_EXISTS_ERROR";
            //return -1;
            return 0;
        }
    }
}
