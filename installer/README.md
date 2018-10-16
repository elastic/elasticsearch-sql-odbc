# odbc-installer
Elasticsearch ODBC MSI installer

## Minimum requirements

- Windows 10 64-bit (remain within Microsoft mainstream support lifecycle)
- Excel 2016 (latest)
- VC++2017 redistributables

## Test using PowerShell

1. Create the User DSN
```
Add-OdbcDsn -Name "ED_PS" -DriverName "Elasticsearch Driver" -DsnType "User" -SetPropertyValue @("Server=localhost", "Secure=0")
```

2. Allow PowerShell to run scripts
```
Set-ExecutionPolicy -Scope process -ExecutionPolicy unrestricted
```

3. Run script `./select.ps1` 
Requires local elasticsearch with index'd data.
Substitute query string.
```
# save as select.ps1
$connectstring = "DSN=ED_PS;"
$sql = "SELECT * FROM twitter"

$conn = New-Object System.Data.Odbc.OdbcConnection($connectstring)
$conn.open()
$cmd = New-Object system.Data.Odbc.OdbcCommand($sql,$conn)
$da = New-Object system.Data.Odbc.OdbcDataAdapter($cmd)
$dt = New-Object system.Data.datatable
$null = $da.fill($dt)
$conn.close()
$dt
```
