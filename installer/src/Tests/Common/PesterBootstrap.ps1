<#
.Synopsis
    Bootstraps Pester on a Vagrant box and invokes the Tests in the $PWD.Drive.Root\vagrant directory
.Example
	.\PesterBootstrap.ps1 -Version "6.5.0" -TestDirName "Silent-Install"
.Parameter TestDirName
    The name of the test directory containing the tests
.Parameter Version
	The product version under test
#>
[CmdletBinding()]
Param(
    [Parameter(Mandatory=$true)]
    [string] $TestDirName,

    [Parameter(Mandatory=$true)]
	[ValidatePattern("^((\w*)\:)?((\d+)\.(\d+)\.(\d+)(?:\-([\w\-]+))?)$")]
    [string] $Version
)

# Used in tests
$env:Version = $Version

# Update Security Protocol for HTTPS requests
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Ssl3 -bor [Net.SecurityProtocolType]::Tls -bor `
										      [Net.SecurityProtocolType]::Tls11 -bor [Net.SecurityProtocolType]::Tls12

$currentDir = Split-Path -parent $MyInvocation.MyCommand.Path
cd $currentDir

$drive = $PWD.Drive.Root
$pester = "Pester"
$date = Get-Date -format "yyyy-MM-ddT-HHmmssfff"
$path = "$($drive)out\results-$TestDirName-$date.xml"

if(-not(Get-Module -Name $pester)) { 
	# Load the Pester module into the current session. Install if not available
	Write-Output "import $pester"
	if(Get-Module -Name $pester -ListAvailable) { 
       	Import-Module -Name $pester 
    }  
	else { 
		PowerShellGet\Install-Module $pester -Force
		Import-Module $pester
	}
}

Invoke-Pester -Path "$($drive)vagrant\*" -OutputFile "$path" -OutputFormat "NUnitXml" -ExcludeTag "Proxy" -PassThru | Out-Null