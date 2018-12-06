using System;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using WixSharp;

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

			var preRelease = string.Empty;
			// Is this a pre-release?
			if (releaseString.Contains("-"))
			{
				var versionSplit = releaseString.Split('-');
				if (versionSplit.Length > 2)
				{
					throw new ArgumentException("Unexpected version string: " + fullVersionString);
				}

				preRelease = "-" + versionSplit[1];
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
					new PropertyRef("NETFRAMEWORK40FULL"),

					// Perform registry search for redist key
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
                }
            };

			const string wixLocation = @"..\..\packages\WixSharp.wix.bin\tools\bin";
			if (!System.IO.Directory.Exists(wixLocation))
				throw new Exception($"The directory '{wixLocation}' could not be found");
			//Compiler.LightOptions = "-sw1076 -sw1079 -sval";
			Compiler.WixLocation = wixLocation;

            project.WixVariables.Add("WixUILicenseRtf", System.IO.Path.Combine(driverInputFilesPath, "LICENSE.rtf"));
			project.Include(WixExtension.NetFx);
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
}