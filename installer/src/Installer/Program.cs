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
			// Get the input files
			var fullVersionString = args[0];
			var driverBuildsDir = args[1];
			var zipFilepath = args[2];
			var zipDirectory = System.IO.Path.GetFileNameWithoutExtension(zipFilepath);
			var driverInputFilesPath = System.IO.Path.Combine(driverBuildsDir, zipDirectory);
			var driverFileInfo = GetDriverFileInfo(driverInputFilesPath);
			var driverFilePath = System.IO.Path.Combine(driverInputFilesPath, driverFileInfo.FileName);

			// Remove the platform suffix
			const string platformSuffix = "-windows-x86_64";
			var releaseString = fullVersionString;
			if (string.IsNullOrEmpty(releaseString) == false &&
				releaseString.EndsWith(platformSuffix))
			{
				releaseString = releaseString.Replace(platformSuffix, string.Empty);
			}

			// Remove the -SNAPSHOT suffix
			const string snapshotSuffix = "-SNAPSHOT";
			var isSnapshot = false;
			if (string.IsNullOrEmpty(releaseString) == false &&
				releaseString.EndsWith(snapshotSuffix))
			{
				isSnapshot = true;
				releaseString = releaseString.Replace(snapshotSuffix, string.Empty);
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
                Platform = Platform.x64,
                InstallScope = InstallScope.perMachine,
                Version = new Version(driverFileInfo.ProductMajorPart, driverFileInfo.ProductMinorPart, driverFileInfo.ProductBuildPart, driverFileInfo.ProductPrivatePart),
                GUID = new Guid("e87c5d53-fddf-4539-9447-49032ed527bb"),
                UI = WUI.WixUI_InstallDir,
                BannerImage = "topbanner.bmp",
                BackgroundImage = "leftbanner.bmp",
                Name = "Elasticsearch ODBC Driver",
                Description = $"{driverFileInfo.FileDescription} ({msiVersionString})",
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
						new RegistrySearch(RegistryHive.LocalMachine, @"SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64", "Installed", RegistrySearchType.raw){
							Win64 = true
						})
				},
				LaunchConditions = new List<LaunchCondition>
				{
					/*
						Windows 10:				VersionNT64 = 1000 AND MsiNTProductType = 1
						Windows Server 2016:	VersionNT64 = 1000 AND MsiNTProductType <> 1 
					*/
					new LaunchCondition(
						"NOT ((VersionNT64 = 1000 AND MsiNTProductType = 1) OR (VersionNT64 = 1000 AND MsiNTProductType <> 1))",
						"This installer requires at least Windows 10 or Windows Server 2016."
					),
					new LaunchCondition(
						"VS2017REDISTINSTALLED",
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