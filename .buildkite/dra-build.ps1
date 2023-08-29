New-Item C:/.cert -Type Directory -Force
[IO.File]::WriteAllText("C:/.cert/msi_certificate.p12", $Env:MSI_CERTIFICATE_SECRET)
[IO.File]::WriteAllText("C:/.cert/msi_password.txt", $Env:MSI_PASSWORD)

Select-String -Pattern '^set\(DRV_VERSION (.*)\)$' -Path ".\CMakeLists.txt" | Select-Object -ExpandProperty Matches -First 1 -OutVariable Match
$VersionString=$Match.groups[1].Value

if ($Env:DRA_WORKFLOW -eq "staging") {
	cmd.exe /c 'build.bat setup proper'
	cmd.exe /c 'build.bat setup 64 type:Release package sign:C:/.cert/msi_certificate.p12+C:/.cert/msi_password.txt'
	cmd.exe /c 'build.bat setup 32 type:Release package sign:C:/.cert/msi_certificate.p12+C:/.cert/msi_password.txt'
} else {
	cmd.exe /c 'build.bat setup proper'
	cmd.exe /c 'build.bat setup 64 type:Release package:-SNAPSHOT sign:C:/.cert/msi_certificate.p12+C:/.cert/msi_password.txt'
	cmd.exe /c 'build.bat setup 32 type:Release package:-SNAPSHOT sign:C:/.cert/msi_certificate.p12+C:/.cert/msi_password.txt'
}

buildkite-agent artifact upload 'installer/build/out/*.msi'
