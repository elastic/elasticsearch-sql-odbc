$env:MSI_CERTIFICATE_SECRET = vault read -field=cert secret/ci/elastic-elasticsearch-sql-odbc/msi
$env:MSI_PASSWORD = vault read -field=password secret/ci/elastic-elasticsearch-sql-odbc/msi
