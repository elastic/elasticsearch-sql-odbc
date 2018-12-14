using Microsoft.Deployment.WindowsInstaller;
using System;
using System.Diagnostics;

namespace InstallerCA
{
	// Implemented as a CustomAction as using the WixShellExecTarget only ever launches a 32 bit process,
	// then attempting to access any 64 bit programs will succumb to windows path rewriting and resolve to
	// 32 bit equivalent program.
	public class CustomActions
	{
		[CustomAction]
		public static ActionResult LaunchODBCControlPanel(Session session)
		{
			var windir = Environment.GetFolderPath(Environment.SpecialFolder.Windows);
			var odbcad32 = System.IO.Path.Combine(windir, "system32", "odbcad32.exe");
			session.Log($"Launching: {odbcad32}");
			using (var process = new Process())
			{
				process.StartInfo.FileName = odbcad32;
				process.Start();
				process.WaitForExit();
			}
			return ActionResult.Success;
		}
	}
}
