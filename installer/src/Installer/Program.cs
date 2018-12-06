// Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
// or more contributor license agreements. Licensed under the Elastic License;
// you may not use this file except in compliance with the Elastic License.

using System;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using WixSharp;
using WixSharp.Controls;
using System.Xml.Linq;

namespace ODBCInstaller
{
    partial class Program
    {
        static void Main(string[] args)
		{
			// Is 64bit target?
			var is64bit = IntPtr.Size == 8;
			var bitness = is64bit ? "64bit" : "32bit";

			// Get the input files
			var fullVersionString = args[0];
			var driverBuildsDir = args[1];
			var zipFilepath = args[2];

			// Get the input files
			var zipDirectory = System.IO.Path.GetFileNameWithoutExtension(zipFilepath);
			var driverInputFilesPath = System.IO.Path.Combine(driverBuildsDir, zipDirectory);
			var driverFileInfo = GetDriverFileInfo(driverInputFilesPath);
			var driverFilePath = System.IO.Path.Combine(driverInputFilesPath, driverFileInfo.FileName);

			// Remove the platform
			string platformVersionComponent = is64bit
										? "-windows-x86_64"
										: "-windows-x86";
			var releaseString = fullVersionString;
			if (string.IsNullOrEmpty(releaseString) == false &&
				releaseString.Contains(platformVersionComponent))
			{
				releaseString = releaseString.Replace(platformVersionComponent, string.Empty);
			}

			// Remove the -SNAPSHOT
			const string snapshotVersionComponent = "-SNAPSHOT";
			var isSnapshot = false;
			if (string.IsNullOrEmpty(releaseString) == false &&
				releaseString.Contains(snapshotVersionComponent))
			{
				isSnapshot = true;
				releaseString = releaseString.Replace(snapshotVersionComponent, string.Empty);
			}

			// Is this a pre-release?
			var preRelease = string.Empty;
			if (releaseString.Contains("-"))
			{
				var versionSplit = releaseString.Split('-');
				if (versionSplit.Length > 2)
				{
					throw new ArgumentException("Unexpected version string: " + fullVersionString);
				}

				preRelease = "-" + versionSplit[1];
			}

			// Get the documentation link version
			var documentationLink = "https://www.elastic.co/guide/en/elasticsearch/sql-odbc/index.html";
			if (!releaseString.Contains("-"))
			{
				var documentationVersion = releaseString.Split('.').Reverse().SkipWhile(s => s == "0").Reverse().ToArray().Join(".");
				documentationLink = $"https://www.elastic.co/guide/en/elasticsearch/sql-odbc/{documentationVersion}/index.html";
			}

			Console.WriteLine($"Setting documentation link to : {documentationLink}");

			// Append any prerelease flags onto the version string
			var msiVersionString = $"{driverFileInfo.ProductVersion}{preRelease}";

            var files = System.IO.Directory.GetFiles(driverInputFilesPath)
                              .Where(f => f.EndsWith(driverFilePath) == false)
                              .Select(f => new File(f))
                              .Concat(new[] { new File(driverFilePath, new ODBCDriver("Elasticsearch Driver")) })
                              .Cast<WixEntity>()
                              .ToArray();

            var installDirectory = $@"%ProgramFiles%\Elastic\ODBCDriver\{msiVersionString}";
            var components = new Dir(installDirectory, files);

            var project = new Project("ODBCDriverInstaller", components)
            {
                Platform = is64bit
								? Platform.x64
								: Platform.x86,
                InstallScope = InstallScope.perMachine,
                Version = new Version(driverFileInfo.ProductMajorPart, driverFileInfo.ProductMinorPart, driverFileInfo.ProductBuildPart, driverFileInfo.ProductPrivatePart),
                GUID = new Guid(is64bit
									? "e87c5d53-fddf-4539-9447-49032ed527bb"
									: "ef6b65e0-20c3-43e3-a5e3-24e2ee8c84cb"),
				UI = WUI.WixUI_InstallDir,
                BannerImage = "topbanner.bmp",
                BackgroundImage = "leftbanner.bmp",
                Name = "Elasticsearch ODBC Driver",
                Description = $"{driverFileInfo.FileDescription} ({msiVersionString}) {bitness}",
                ControlPanelInfo = new ProductInfo
                {
                    ProductIcon = "ODBC.ico",
                    Manufacturer = driverFileInfo.CompanyName,
                    UrlInfoAbout = "https://www.elastic.co/products/stack/elasticsearch-sql",
                    HelpLink = "https://discuss.elastic.co/c/elasticsearch"
                },
                OutFileName = "esodbc-" + fullVersionString, // Use full version string
				Properties = new[]
				{
					// Exit dialog checkbox options
					new Property("WIXUI_EXITDIALOGOPTIONALCHECKBOXTEXT", "Launch ODBC Data Source Administrator after installation"),
					new Property("WIXUI_EXITDIALOGOPTIONALCHECKBOX", "1"),
					new Property("WixShellExecTarget", "odbcad32.exe"),

					// Is .NET Framework 4.0 installed?
					new PropertyRef("NETFRAMEWORK40FULL"),

					// Perform registry search for VS 2017 redist key
					new Property("VS2017REDISTINSTALLED",
						new RegistrySearch(
								RegistryHive.LocalMachine,
								is64bit
									? @"SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64"
									: @"SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86",
								"Installed",
								RegistrySearchType.raw)
						{
							Win64 = is64bit
						})
				},
				LaunchConditions = new List<LaunchCondition>
				{
					/*
						Windows 10:				VersionNT64 = 1000 AND MsiNTProductType = 1
						Windows Server 2016:	VersionNT64 = 1000 AND MsiNTProductType <> 1 
					*/
					new LaunchCondition(
						"Installed OR (NOT ((VersionNT64 = 1000 AND MsiNTProductType = 1) OR (VersionNT64 = 1000 AND MsiNTProductType <> 1)))",
						"This installer requires at least Windows 10 or Windows Server 2016."
					),
					new LaunchCondition(
						"Installed OR VS2017REDISTINSTALLED",
						"This installer requires the Visual C++ 2017 Redistributable. " +
						"Please install Visual C++ 2017 Redistributable and then run this installer again."
					),
					new LaunchCondition(
						"Installed OR NETFRAMEWORK40FULL",
						"This installer requires at least .NET Framework 4.0 in order to run the configuration editor. " +
						"Please install .NET Framework 4.0 and then run this installer again."
					)
				},

				// http://wixtoolset.org/documentation/manual/v3/xsd/wix/majorupgrade.html
				MajorUpgrade = new MajorUpgrade
				{
					AllowDowngrades = false,
					AllowSameVersionUpgrades = false,
					Disallow = true,
					DisallowUpgradeErrorMessage = "An existing version is already installed, please uninstall before continuing.",
					DowngradeErrorMessage = "A more recent version is already installed, please uninstall before continuing.",
				},
				CustomUI = UIHelper.BuildCustomUI(finishActionName),
			};

			const string wixLocation = @"..\..\packages\WixSharp.wix.bin\tools\bin";
			if (!System.IO.Directory.Exists(wixLocation))
				throw new Exception($"The directory '{wixLocation}' could not be found");
			Compiler.WixLocation = wixLocation;

			project.WixVariables.Add("WixUILicenseRtf", System.IO.Path.Combine(driverInputFilesPath, "LICENSE.rtf"));
			project.Include(WixExtension.NetFx);
			project.Include(WixExtension.Util);
			project.Include(WixExtension.UI);
			project.WixSourceGenerated += document => Project_WixSourceGenerated(finishActionName, document);

			project.BuildMsi();
		}

		private static void Project_WixSourceGenerated(string finishActionName, XDocument document)
		{
			var documentRoot = document.Root;
			var ns = documentRoot.Name.Namespace;
			var product = documentRoot.Descendants(ns + "Product").Single();

			// executes what's defined in WixShellExecTarget Property.
			// WixSharp does not have an element for WixShellExec custom action
			product.Add(new XElement(ns + "CustomAction",
				new XAttribute("Id", finishActionName),
				new XAttribute("BinaryKey", "WixCA"),
				new XAttribute("DllEntry", "WixShellExec"),
				new XAttribute("Impersonate", "yes")
			));
		}

		private static FileVersionInfo GetDriverFileInfo(string zipContentsDirectory)
		{
			return System.IO.Directory.GetFiles(zipContentsDirectory)
						 .Where(f => f.EndsWith(".dll"))
						 .Select(f => FileVersionInfo.GetVersionInfo(f))
						 .Single(f => f.FileDescription == "ODBC Unicode driver for Elasticsearch");
		}
	}

	public class UIHelper
	{
		public static CustomUI BuildCustomUI(string finishActionName)
		{
			var customUI = new CustomUI();

			customUI.On(NativeDialogs.WelcomeDlg, Buttons.Next, new ShowDialog(NativeDialogs.LicenseAgreementDlg));

			customUI.On(NativeDialogs.LicenseAgreementDlg, Buttons.Back, new ShowDialog(NativeDialogs.WelcomeDlg));
			customUI.On(NativeDialogs.LicenseAgreementDlg, Buttons.Next, new ShowDialog(NativeDialogs.InstallDirDlg));

			customUI.On(NativeDialogs.InstallDirDlg, Buttons.Back, new ShowDialog(NativeDialogs.LicenseAgreementDlg));
			customUI.On(NativeDialogs.InstallDirDlg, Buttons.Next, new SetTargetPath(),
															 new ShowDialog(NativeDialogs.VerifyReadyDlg));

			customUI.On(NativeDialogs.InstallDirDlg, Buttons.ChangeFolder,
															 new SetProperty("_BrowseProperty", "[WIXUI_INSTALLDIR]"),
															 new ShowDialog(CommonDialogs.BrowseDlg));

			customUI.On(NativeDialogs.VerifyReadyDlg, Buttons.Back, new ShowDialog(NativeDialogs.InstallDirDlg, Condition.NOT_Installed),
															  new ShowDialog(NativeDialogs.MaintenanceTypeDlg, Condition.Installed));

			customUI.On(NativeDialogs.MaintenanceWelcomeDlg, Buttons.Next, new ShowDialog(NativeDialogs.MaintenanceTypeDlg));

			customUI.On(NativeDialogs.MaintenanceTypeDlg, Buttons.Back, new ShowDialog(NativeDialogs.MaintenanceWelcomeDlg));
			customUI.On(NativeDialogs.MaintenanceTypeDlg, Buttons.Repair, new ShowDialog(NativeDialogs.VerifyReadyDlg));
			customUI.On(NativeDialogs.MaintenanceTypeDlg, Buttons.Remove, new ShowDialog(NativeDialogs.VerifyReadyDlg));

			customUI.On(NativeDialogs.ExitDialog, Buttons.Finish, new ExecuteCustomAction(finishActionName, "WIXUI_EXITDIALOGOPTIONALCHECKBOX = 1 and NOT Installed"), new CloseDialog()
			{
				Order = 9999,
			});

			return customUI;
		}
	}
}
