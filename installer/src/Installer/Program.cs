﻿// Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
// or more contributor license agreements. Licensed under the Elastic License;
// you may not use this file except in compliance with the Elastic License.

using System;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using WixSharp;
using WixSharp.Controls;
using InstallerCA;

namespace Installer
{
	partial class Program
	{
		static void Main(string[] args)
		{
			// Get the input files
			var fullVersionString = args[0];
			var driverBuildsDir = args[1];
			var zipFilepath = args[2];

			// Get the input files
			var zipDirectory = System.IO.Path.GetFileNameWithoutExtension(zipFilepath);
			var driverInputFilesPath = System.IO.Path.Combine(driverBuildsDir, zipDirectory);
			var driverFileInfo = GetDriverFileInfo(driverInputFilesPath);
			var driverFilePath = System.IO.Path.Combine(driverInputFilesPath, driverFileInfo.FileName);

			// Is 64bit target?
			// full version format: <release>[-<qualifier>][-SNAPSHOT]-windows-x86[_64], with release: X.Y.Z and qualifier: alphaX/betaX(/gamaX?)
			var is64bit = fullVersionString.EndsWith("x86_64");
			var bitness = is64bit ? "64bit" : "32bit";
			var platform = is64bit ? "x64" : "x86";

			// Remove the platform
			string platformVersionComponent = is64bit ? "-windows-x86_64" : "-windows-x86";
			var releaseString = fullVersionString;
			if (!string.IsNullOrEmpty(releaseString) &&
				releaseString.Contains(platformVersionComponent))
			{
				releaseString = releaseString.Replace(platformVersionComponent, string.Empty);
			}

			// Remove the -SNAPSHOT
			const string snapshotVersionComponent = "-SNAPSHOT";
			var isSnapshot = false;
			if (!string.IsNullOrEmpty(releaseString) &&
				releaseString.Contains(snapshotVersionComponent))
			{
				isSnapshot = true;
				releaseString = releaseString.Replace(snapshotVersionComponent, string.Empty);
			}

			// Is this a pre-release?
			var preRelease = string.Empty;
			if (!string.IsNullOrEmpty(releaseString) &&
				releaseString.Contains("-"))
			{
				var versionSplit = releaseString.Split('-');
				if (versionSplit.Length > 2)
				{
					throw new ArgumentException($"Unexpected version string: {fullVersionString}");
				}

				preRelease = $"-{versionSplit[1]}";
			}

			// Get the documentation link version
			var documentationLink = "https://www.elastic.co/guide/en/elasticsearch/sql-odbc/index.html";
			if (!releaseString.Contains("-"))
			{
				var verArray = releaseString.Split('.').Reverse().SkipWhile(s => s == "0").Reverse().ToArray();
				// Ensure major releases with no minor are linked correctly
				if (verArray.Length == 1)
				{
					verArray = new[] { verArray[0], "0" };
				}
				var documentationVersion = verArray.Join(".");
				documentationLink = $"https://www.elastic.co/guide/en/elasticsearch/sql-odbc/{documentationVersion}/index.html";
			}

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

			var showODBCAdminControlPanel = new ManagedAction(CustomActions.LaunchODBCControlPanel, typeof(CustomActions).Assembly.Location)
			{
				Sequence = Sequence.NotInSequence,
				Return = Return.ignore
			};

			var project = new Project("ODBCDriverInstaller", components)
			{
				Platform = is64bit ? Platform.x64 : Platform.x86,
				InstallScope = InstallScope.perMachine,
				Version = new Version(driverFileInfo.ProductMajorPart, driverFileInfo.ProductMinorPart, driverFileInfo.ProductBuildPart, driverFileInfo.ProductPrivatePart),
				// Ensure 64bit and 32bit have different product codes
				GUID = new Guid(is64bit ? "e87c5d53-fddf-4539-9447-49032ed527bb" : "ef6b65e0-20c3-43e3-a5e3-24e2ee8c84cb"),
				UI = WUI.WixUI_Common,
				BannerImage = "topbanner.bmp",
				BackgroundImage = "leftbanner.bmp",
				Name = $"Elasticsearch ODBC Driver ({bitness})",
				Description = $"{driverFileInfo.FileDescription} {msiVersionString} ({bitness})",
				ControlPanelInfo = new ProductInfo
				{
					ProductIcon = "ODBC.ico",
					Manufacturer = driverFileInfo.CompanyName,
					UrlInfoAbout = documentationLink,
					HelpLink = "https://discuss.elastic.co/tags/c/elasticsearch/sql"
				},
				OutFileName = $"esodbc-{fullVersionString}", // Use full version string
				Properties = new[]
				{
					// Exit dialog checkbox options
					new Property("WIXUI_EXITDIALOGOPTIONALCHECKBOXTEXT", "Launch ODBC Data Source Administrator after installation"),
					new Property("WIXUI_EXITDIALOGOPTIONALCHECKBOX", "1"),
					
					// Is .NET Framework 4.0 installed?
					new PropertyRef("NETFRAMEWORK40FULL"),

					// Perform registry search for VS 2017 redist key
					new Property("VS2017REDISTINSTALLED",
						new RegistrySearch(
								RegistryHive.LocalMachine,
								$@"SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\{platform}",
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
						$"This installer requires the Visual C++ 2017 Redistributable ({platform}). " +
						$"Please install Visual C++ 2017 Redistributable ({platform}) and then run this installer again."
					),
					new LaunchCondition(
						"Installed OR NETFRAMEWORK40FULL",
						"This installer requires at least .NET Framework 4.0 in order to run the configuration editor and run custom install actions. " +
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
				Actions = new WixSharp.Action[]
				{
					showODBCAdminControlPanel
				},
				CustomUI = UIHelper.BuildCustomUI(showODBCAdminControlPanel)
			};

			const string wixLocation = @"..\..\packages\WixSharp.wix.bin\tools\bin";
			if (!System.IO.Directory.Exists(wixLocation))
				throw new Exception($"The directory '{wixLocation}' could not be found");
			Compiler.WixLocation = wixLocation;

			project.WixVariables.Add("WixUILicenseRtf", System.IO.Path.Combine(driverInputFilesPath, "LICENSE.rtf"));
			project.Include(WixExtension.NetFx);
			project.Include(WixExtension.Util);
			project.Include(WixExtension.UI);

			project.BuildMsi();
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
		public static CustomUI BuildCustomUI(ManagedAction showODBCAdminControlPanel)
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

			customUI.On(NativeDialogs.ExitDialog, Buttons.Finish, new ExecuteCustomAction(showODBCAdminControlPanel.Id, "WIXUI_EXITDIALOGOPTIONALCHECKBOX = 1 and NOT Installed"), new CloseDialog()
			{
				Order = 9999,
			});

			return customUI;
		}
	}
}
