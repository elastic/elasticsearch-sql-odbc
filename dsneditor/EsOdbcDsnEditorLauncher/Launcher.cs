using System;
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
            bool onConnect = true;
            string dsn = "driver={IBM DB2 ODBC DRIVER};Database=SampleDB;hostname=SampleServerName;protocol=TCPIP;uid=Admin;pwd=pass!word1;secure=4";
            var form = new EsOdbcDsnEditor.DsnEditorForm(onConnect, dsn, ConnectTest, SaveDsn);
            form.Show();
        }

        private int ConnectTest(string connectionString, ref string errorMessage, uint flags)
        {
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
