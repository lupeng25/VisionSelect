param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path,
    [switch]$CheckMojibakeHeuristic
)

$ErrorActionPreference = "Stop"

$textExtensions = @(
    ".bat", ".cpp", ".csv", ".editorconfig", ".gitattributes", ".gitignore",
    ".h", ".json", ".md", ".pri", ".pro", ".ps1", ".qrc", ".qss", ".txt", ".ui"
)

$textNames = @(
    "AGENTS.md", "README.md", "VisionSelect.pro"
)

function Test-IsTextPath([string]$Path) {
    $fileName = [System.IO.Path]::GetFileName($Path)
    if ($textNames -contains $fileName) { return $true }

    $extension = [System.IO.Path]::GetExtension($Path).ToLowerInvariant()
    return $textExtensions -contains $extension
}

function Get-MojibakeTerms {
    @(
        [char]0x951B, # common mojibake: Chinese comma/semicolon lead
        [char]0x93C3,
        [char]0x9435,
        [char]0x95C0,
        [char]0x934F,
        [char]0x6434,
        [char]0x5BE5,
        [char]0x7C7B,
        [char]0x5F7F,
        [char]0x53AA,
        [char]0x539C,
        [char]0x507D,
        [char]0x5EBE,
        [char]0x657E,
        [char]0x702D,
        [char]0x7447
    ) | ForEach-Object { $_.ToString() }
}

Push-Location $Root
try {
    $files = git ls-files --cached --others --exclude-standard
    if ($LASTEXITCODE -ne 0) {
        throw "git ls-files failed."
    }
}
finally {
    Pop-Location
}

$utf8Strict = New-Object System.Text.UTF8Encoding -ArgumentList $false, $true
$failures = New-Object System.Collections.Generic.List[string]
$checked = 0
$mojibakeTerms = Get-MojibakeTerms

foreach ($relativePath in $files) {
    if (-not (Test-IsTextPath $relativePath)) { continue }

    $checked++
    $fullPath = Join-Path $Root ($relativePath -replace '/', '\')
    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($fullPath)
    if ($bytes.Length -eq 0) { continue }

    if ([Array]::IndexOf($bytes, [byte]0) -ge 0) {
        $failures.Add("$relativePath contains NUL bytes; expected a text file.")
        continue
    }

    try {
        $text = $utf8Strict.GetString($bytes)
    }
    catch {
        $failures.Add("$relativePath is not valid UTF-8.")
        continue
    }

    if ($text.Contains([char]0xFFFD)) {
        $failures.Add("$relativePath contains Unicode replacement characters.")
    }

    if ($CheckMojibakeHeuristic) {
        foreach ($term in $mojibakeTerms) {
            if ($text.Contains($term)) {
                $failures.Add("$relativePath contains suspicious mojibake text near '$term'.")
                break
            }
        }

        if ($text -match "(\u00C3.|\u00C2.|\u00E2[\u20AC\u2122\u0153\u20AC\u201C])") {
            $failures.Add("$relativePath contains suspicious Latin mojibake text.")
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Error ("Text encoding check failed:`n" + ($failures -join "`n"))
    exit 1
}

Write-Host "Checked $checked project text files; all are valid UTF-8."
