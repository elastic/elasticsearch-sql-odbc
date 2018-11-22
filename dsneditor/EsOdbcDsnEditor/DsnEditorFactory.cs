/*
 * Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
 * or more contributor license agreements. Licensed under the Elastic License;
 * you may not use this file except in compliance with the Elastic License.
 */

using System.Windows.Forms;

// uncomment to have the assembly loading to ask for (various) resources; various solutions: 
// https://stackoverflow.com/questions/4368201/appdomain-currentdomain-assemblyresolve-asking-for-a-appname-resources-assembl
// [assembly: NeutralResourcesLanguageAttribute("en-GB", UltimateResourceFallbackLocation.MainAssembly)]

namespace EsOdbcDsnEditor {
	public static class DsnEditorFactory {
		public static int DsnEditor(
			bool onConnect,
			string dsnIn,
			DriverCallbackDelegate delegConnectionTest,
			DriverCallbackDelegate delegSaveDsn) {
			Application.EnableVisualStyles();
			var form = new DsnEditorForm(onConnect, dsnIn, delegConnectionTest, delegSaveDsn);
			// instead of Application.Run(form): prevent STA apps from launching a new editor within the same thread
			form.ShowDialog();
			var dsn = form.Builder.ToString();
			return dsn.Length;
		}
	}
}
