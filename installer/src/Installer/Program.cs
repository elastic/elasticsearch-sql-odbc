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
			// Remove the platform suffix
			const string platformSuffix = "-windows-x86_64";
			var releaseString = args[0];
			if (string.IsNullOrEmpty(releaseString) == false &&
				releaseString.EndsWith(platformSuffix))
			{
				releaseString = releaseString.Replace(platformSuffix, string.Empty);
			}

			var preRelease = string.Empty;
			// Is this a pre-release?
			if (releaseString.Contains("-"))
			{
				var versionSplit = releaseString.Split('-');
				if (versionSplit.Length > 2)
				{
					throw new ArgumentException("Unexpected version string: " + args[0]);
				}

				preRelease = "-" + versionSplit[1];
			}

			// Get the input files
			var zipFilepath = args[1];
			var zipContentsDirectory = new System.IO.DirectoryInfo(zipFilepath.Replace(".zip", string.Empty)).FullName;
			var driverFileInfo = GetDriverFileInfo(zipContentsDirectory);
            var driverFilePath = System.IO.Path.Combine(zipContentsDirectory, driverFileInfo.FileName);

			// Append any prerelease flags onto the version string
			var msiVersionString = $"{driverFileInfo.ProductVersion}{preRelease}";
			var msiFileName = new System.IO.FileInfo(zipFilepath).Name.Replace(".zip", string.Empty);

            var files = System.IO.Directory.GetFiles(zipContentsDirectory)
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
                OutFileName = msiFileName,

				Properties = new[]
				{
					new PropertyRef("NETFRAMEWORK40FULL"),
				},
				LaunchConditions = new List<LaunchCondition>
				{
					new LaunchCondition(
						"Installed OR NETFRAMEWORK40FULL",
						"This installer requires at least .NET Framework 4.0 in order to run the configuration editor. " +
						"Please install .NET Framework 4.0 then run this installer again."
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

            project.Attributes.Add("Manufacturer", driverFileInfo.CompanyName);
            project.WixVariables.Add("WixUILicenseRtf", System.IO.Path.Combine(zipContentsDirectory, "LICENSE.rtf"));
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