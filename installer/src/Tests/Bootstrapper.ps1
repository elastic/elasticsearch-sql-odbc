<#
.Synopsis
    Bootstraps and runs integration tests for the ODBC Driver Installer
.Description
    Ensures all preconditions are met to run integration tests e.g. vagrant installed, cygpath.exe in PATH, etc.
    Then runs all integration test directories in .\Tests specified by $Tests parameter
.Example
    Execute tests in all directories
    .\Bootstrapper.ps1 -Version 6.5.0
.Example
    Execute only tests in directories that end in "NoItem" on a local vagrant box
    .\Bootstrapper.ps1 "*NoItem" -Version 6.5.0 -VagrantProvider local
.Parameter Tests
    Is a wildcard to describe the test directories in .\Tests which contain tests to run
.Parameter Version
	The version of the product under test
.Parameter VagrantProvider
	The vagrant provider to use:
	- local = Local vagrant box with virtualbox provider
	- azure = Vagrant box created within Azure with azure provider. A box is created for each test scenario
	- quickazure = Vagrant box created within Azure with azure provider. One box is created and all tests are run on it in sequence
.Parameter Gui
	Whether to launch vagrant with a gui. Only valid for local vagrant provider
.Parameter VagrantDestroy
	Whether to destroy the vagrant box on test finish. Only valid for local vagrant provider
#>
[CmdletBinding()]
Param(
    [Parameter(Position=0, Mandatory=$false, ValueFromPipelineByPropertyName=$true)]
    [Alias("T")]
    [string] $Tests="*",

    [Parameter(Mandatory=$true)]
	[ValidatePattern("^((\w*)\:)?((\d+)\.(\d+)\.(\d+)(?:\-([\w\-]+))?)$")]
    [string] $Version,

	[Parameter(Mandatory=$false)]
	[ValidateSet("local", "azure", "quick-azure")] 
	[string] $VagrantProvider="local",

	[switch] $Gui,

	[switch] $VagrantDestroy
)

$currentDir = Split-Path -Parent $MyInvocation.MyCommand.Path
cd $currentDir

. $currentDir\Common\Utils.ps1
. $currentDir\Common\SemVer.ps1

Set-DebugMode

$solutionDir = $(Get-Item $currentDir).Parent.Parent.Parent.FullName
$buildOutDir = Join-Path -Path $solutionDir -ChildPath "installer\build\out"

###############
# Preconditions
###############

$semanticVersion = ConvertTo-SemanticVersion $Version

$installer = Get-Installer -location $buildOutDir -Version $semanticVersion
if ($installer -eq $null) {
    log "No $($semanticVersion.FullVersion) installer found in $buildOutDir. Build the installer by running build.bat in the solution root" -l Error
    Exit 1
}

$testDirs = Get-ChildItem "Tests\$Tests" -Directory
$testCount = $($testDirs | measure).Count
if ($testCount -eq 0) {
	log "No tests found matching pattern $Tests" -l Error
	Exit 0
}

Add-Vagrant

if ($VagrantProvider -eq "local") {
	Test-AtlasToken
	Test-HyperV
} 
else {
	Add-VagrantAzureProvider
	Add-VagrantAzureBox
}

# remove any files from previous run
Remove-Item "$currentDir\Tests\*" -Recurse -Force -Exclude *.ps1

###########
# Run Tests
###########

log "running $testcount test scenario(s)" -l Debug

Invoke-IntegrationTests -CurrentDir $currentDir -TestDirs $testDirs -VagrantProvider $VagrantProvider -Version $Version -Gui:$Gui -VagrantDestroy:$VagrantDestroy

cd $currentDir