param(
    [Parameter(Mandatory = $true)]
    [string]$PrivateKeyPath,

    [string]$PublicKeyPath = "resources\license\public_key.json",

    [string]$MachineCode,
    [string]$Licensee,
    [string]$ExpiresAt,
    [string]$Serial = ("VS-" + (Get-Date -Format "yyyyMMddHHmmss")),
    [string[]]$Features = @("standard"),
    [switch]$CreateKeyPair
)

$ErrorActionPreference = "Stop"

function Resolve-OutputPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function New-RsaProvider {
    return New-Object System.Security.Cryptography.RSACryptoServiceProvider 2048
}

function Write-PublicKeyJson {
    param(
        [System.Security.Cryptography.RSAParameters]$Parameters,
        [string]$Path
    )
    $json = [ordered]@{
        algorithm = "RSA-SHA256-PKCS1"
        productId = "VisionSelect"
        modulus = [Convert]::ToBase64String($Parameters.Modulus)
        exponent = [Convert]::ToBase64String($Parameters.Exponent)
    } | ConvertTo-Json
    $targetPath = Resolve-OutputPath $Path
    $targetDir = Split-Path -Parent $targetPath
    if ($targetDir -and -not (Test-Path -LiteralPath $targetDir)) {
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    }
    [System.IO.File]::WriteAllText($targetPath, $json, (New-Object System.Text.UTF8Encoding($false)))
}

if ($CreateKeyPair) {
    $rsa = New-RsaProvider
    $privatePath = Resolve-OutputPath $PrivateKeyPath
    $privateDir = Split-Path -Parent $privatePath
    if ($privateDir -and -not (Test-Path -LiteralPath $privateDir)) {
        New-Item -ItemType Directory -Path $privateDir -Force | Out-Null
    }
    [System.IO.File]::WriteAllText($privatePath, $rsa.ToXmlString($true), (New-Object System.Text.UTF8Encoding($false)))
    Write-PublicKeyJson -Parameters ($rsa.ExportParameters($false)) -Path $PublicKeyPath
    Write-Host "Key pair created."
    Write-Host "Private key: $privatePath"
    Write-Host "Public key:  $(Resolve-OutputPath $PublicKeyPath)"
    if (-not $MachineCode -and -not $Licensee -and -not $ExpiresAt) {
        return
    }
}

$privatePath = Resolve-OutputPath $PrivateKeyPath
if (-not (Test-Path -LiteralPath $privatePath)) {
    throw "Private key not found. Run with -CreateKeyPair once, keep the private key outside source control, then rebuild the app with the generated public key."
}
if (-not $MachineCode -or -not $Licensee -or -not $ExpiresAt) {
    throw "MachineCode, Licensee, and ExpiresAt are required when generating a license key."
}

$expiresDate = [DateTime]::Parse($ExpiresAt).ToString("yyyy-MM-dd")
$rsa = New-RsaProvider
$rsa.FromXmlString([System.IO.File]::ReadAllText($privatePath))

$payload = [ordered]@{
    productId = "VisionSelect"
    licensee = $Licensee
    serial = $Serial
    machineCode = $MachineCode.ToUpperInvariant()
    issuedAt = (Get-Date -Format "yyyy-MM-dd")
    expiresAt = $expiresDate
    features = $Features
}
$payloadJson = $payload | ConvertTo-Json -Compress
$payloadBase64 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($payloadJson))
$signature = $rsa.SignData([System.Text.Encoding]::UTF8.GetBytes($payloadBase64), [System.Security.Cryptography.CryptoConfig]::MapNameToOID("SHA256"))
$licenseKey = "VS1-$payloadBase64-$([Convert]::ToBase64String($signature))"

Write-Output $licenseKey
