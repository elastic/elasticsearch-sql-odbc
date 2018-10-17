using System;
using System.Linq;
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
			var releaseString = args[1];
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
					throw new ArgumentException("Unexpected version string: " + args[1]);
				}

				preRelease = "-" + versionSplit[1];
			}

            var odbcVersion = FileVersionInfo.GetVersionInfo(System.IO.Path.GetFullPath("driver\\esodbc7u.dll"));
            var VersionString = $"{odbcVersion.ProductVersion}{preRelease}";

            var driverDirectory = "driver\\";
            var driverFilename = System.IO.Path.Combine(driverDirectory, "esodbc7u.dll");

            var files = System.IO.Directory.GetFiles(driverDirectory)
                              .Where(f => f.EndsWith(driverFilename) == false)
                              .Select(f => new File(f))
                              .Concat(new[] { new File(driverFilename, new ODBCDriver("Elasticsearch Driver")) } )
                              .Cast<WixEntity>()
                              .ToArray();

            var installDirectory = $@"%ProgramFiles%\Elastic\ODBCDriver\{VersionString}";
            var components = new Dir(installDirectory, files);

            var project = new Project("ODBCDriverInstaller", components)
            {
                Platform = Platform.x64,
                InstallScope = InstallScope.perMachine,
                Version = new Version(odbcVersion.ProductMajorPart, odbcVersion.ProductMinorPart, odbcVersion.ProductBuildPart, odbcVersion.ProductPrivatePart),
                GUID = new Guid("e87c5d53-fddf-4539-9447-49032ed527bb"),
                UI = WUI.WixUI_InstallDir,
                BannerImage = "topbanner.bmp",
                BackgroundImage = "leftbanner.bmp",
                Name = "Elasticsearch ODBC Driver",
                Description = $"{odbcVersion.FileDescription} ({VersionString})",
                ControlPanelInfo = new ProductInfo
                {
                    ProductIcon = "ODBC.ico",
                    Manufacturer = odbcVersion.CompanyName,
                    UrlInfoAbout = "https://www.elastic.co/products/stack/elasticsearch-sql",
                    HelpLink = "https://discuss.elastic.co/c/elasticsearch"
                },
                OutFileName = $"elasticsearch-odbc-driver-{VersionString}",
                
                // http://wixtoolset.org/documentation/manual/v3/xsd/wix/majorupgrade.html
                MajorUpgrade = new MajorUpgrade
                {
                    AllowDowngrades = false,
                    AllowSameVersionUpgrades = false,
                    Disallow = true,
                    DisallowUpgradeErrorMessage = "An existing version is already installed, please uninstall this version before continuing.",
                    DowngradeErrorMessage = "A more recent version is already installed, please uninstall this version before continuing.",
                }
            };

            project.Attributes.Add("Manufacturer", odbcVersion.CompanyName);
            project.WixVariables.Add("WixUILicenseRtf", "driver\\LICENSE.rtf");

            project.BuildMsi();
        }
    }
}