using System.Windows.Forms;
using System;

// uncomment to have the assembly loading to ask for (various) resources; various solutions: 
// https://stackoverflow.com/questions/4368201/appdomain-currentdomain-assemblyresolve-asking-for-a-appname-resources-assembl
// [assembly: NeutralResourcesLanguageAttribute("en-GB", UltimateResourceFallbackLocation.MainAssembly)]

namespace EsOdbcDsnEditor
{
    public static class DsnEditorFactory
    {
        public static int DsnEditor(
            bool onConnect,
            string dsnIn,
            DriverCallbackDelegate delegConnectionTest,
            DriverCallbackDelegate delegSaveDsn)
        {
            Application.EnableVisualStyles();
            DsnEditorForm form = new DsnEditorForm(onConnect, dsnIn, delegConnectionTest, delegSaveDsn);
            Application.Run(form);
            var dsn = form.Builder.ToString();
            return dsn.Length;
        }
    }
}