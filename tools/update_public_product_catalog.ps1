param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path,
    [switch]$SkipNetwork
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$cameraPath = Join-Path $Root "resources\data\cameras.csv"
$lensPath = Join-Path $Root "resources\data\lenses.csv"
$coolensRawPath = Join-Path $Root "resources\data\coolens_lenses_raw.csv"

$cameraHeaders = @(
    "model", "manufacturer", "resolution_x", "resolution_y", "pixel_size_um", "sensor_format",
    "color_mode", "shutter_type", "max_fps", "interface", "bandwidth_mbps",
    "bit_depth", "dynamic_range_db", "lens_mount", "pixel_format", "bandwidth_source", "source_url", "source_date"
)
$lensHeaders = @(
    "model", "manufacturer", "lens_type", "lens_mount", "focal_length_mm", "min_wd_mm",
    "distortion_percent", "image_circle_mm", "megapixel_rating",
    "recommended_min_pixel_um", "pmag", "nominal_wd_mm", "wd_tolerance_mm",
    "max_sensor_diagonal_mm", "telecentricity_deg", "dof_mm",
    "numerical_aperture", "f_number", "coaxial_illumination", "notes", "dof_conditions_confirmed"
)

function New-Row($headers, $values) {
    $row = [ordered]@{}
    foreach ($h in $headers) { $row[$h] = "" }
    foreach ($k in $values.Keys) { $row[$k] = $values[$k] }
    [pscustomobject]$row
}

function Clean-Field($value) {
    if ($null -eq $value) { return "" }
    if ($value -is [string]) {
        return (($value -replace '[\r\n\t]+', ' ') -replace '\s{2,}', ' ').Trim()
    }
    return $value
}

function Clean-Row($row, $headers) {
    $clean = [ordered]@{}
    foreach ($h in $headers) { $clean[$h] = Clean-Field $row.$h }
    [pscustomobject]$clean
}

function Add-ToMap([hashtable]$map, $row) {
    if ($row.model) { $map[(([string]$row.manufacturer).Trim().ToLowerInvariant() + "`n" + ([string]$row.model).Trim().ToLowerInvariant())] = $row }
}

function First-Number([string]$text, [double]$default = 0) {
    if ([string]::IsNullOrWhiteSpace($text)) { return $default }
    $m = [regex]::Match($text, '[0-9]+(?:\.[0-9]+)?')
    if ($m.Success) { return [double]$m.Value }
    return $default
}

function Max-Number([string]$text, [double]$default = 0) {
    if ([string]::IsNullOrWhiteSpace($text)) { return $default }
    $matches = [regex]::Matches($text, '[0-9]+(?:\.[0-9]+)?')
    if ($matches.Count -eq 0) { return $default }
    $max = [double]$matches[0].Value
    foreach ($m in $matches) {
        $value = [double]$m.Value
        if ($value -gt $max) { $max = $value }
    }
    return $max
}

function Max-BitDepth([string]$text, [double]$default = 0) {
    if ([string]::IsNullOrWhiteSpace($text)) { return $default }
    $t = [regex]::Replace($text, '(?i)\b[0-9A-Z_]*422[0-9A-Z_]*\b', ' ')
    $t = [regex]::Replace($t, '(?i)\bfloat\s+[0-9]+(?:\.[0-9]+)?\b', ' ')
    return Max-Number $t $default
}

function Parse-Resolution([string]$text) {
    $m = [regex]::Match($text, '([0-9]{2,})\D+([0-9]{2,})')
    if (!$m.Success) { return @(0, 0) }
    return @([int]$m.Groups[1].Value, [int]$m.Groups[2].Value)
}

function Normalize-Interface([string]$text) {
    $t = $text.ToLowerInvariant()
    if ($t -match 'cxp|coaxpress') { return "CoaXPress" }
    if ($t -match '10\s*g|10\s*gigabit|10000\s*mbit') { return "10GigE" }
    if ($t -match 'usb') { return "USB3" }
    if ($t -match 'camera\s*link|cameralink') { return "CameraLink" }
    if ($t -match 'hcon') { return "HCON" }
    if ($t -match 'gige|ethernet|poe') { return "GigE" }
    return $text
}

function Interface-Bandwidth([string]$text) {
    $t = $text.ToLowerInvariant()
    if ($t -match 'cxp-12') { return 2400 }
    if ($t -match 'cxp|coaxpress') { return 1200 }
    if ($t -match '10\s*g|10\s*gigabit|10000\s*mbit|hcon|qsfp') { return 1000 }
    if ($t -match 'camera\s*link|cameralink') { return 680 }
    if ($t -match 'usb') { return 380 }
    if ($t -match 'gige|ethernet|poe') { return 120 }
    return 0
}

function Normalize-Mount([string]$text) {
    $t = $text.ToUpperInvariant().Replace([char]0x00D7, "X").Replace([char]0xFF38, "X")
    if ($t -match 'M72\s*X\s*P?0\.75') { return "M72 x P0.75" }
    if ($t -match 'M43\s*X\s*P?0\.5') { return "M43 x P0.5" }
    if ($t -match 'M58') { return "M58" }
    if ($t -match 'M42') { return "M42" }
    if ($t -match 'F-MOUNT|F MOUNT') { return "F" }
    if ($t -match 'C-MOUNT|C\s*-\s*MOUNT|C MOUNT|^C$|^C口$') { return "C" }
    if ($t -match 'S-MOUNT|SMOUNT') { return "M12" }
    if ($t -match 'M12') { return "M12" }
    return $text
}

function Sensor-DiagonalMm([string]$format) {
    $map = @{
        '1/4"' = 4.5; '1/3"' = 6.0; '1/2.9"' = 6.3; '1/2.7"' = 6.7;
        '1/2.5"' = 7.2; '1/2.3"' = 7.7; '1/2"' = 8.0; '1/1.8"' = 9.0;
        '1/1.7"' = 9.5; '1/1.2"' = 13.3; '2/3"' = 11.0; '1"' = 16.0;
        '1.1"' = 17.6; '1.2"' = 19.3; '4/3"' = 22.0; 'APS-C' = 28.0;
        'APS-H' = 37.0; '35mm' = 44.0
    }
    foreach ($k in $map.Keys) {
        if ($format -like "*$k*") { return $map[$k] }
    }
    $n = First-Number $format 0
    if ($format -match 'mm' -and $n -gt 0) { return $n }
    return 11.0
}

function Html-Decode([string]$text) {
    if ([string]::IsNullOrWhiteSpace($text)) { return "" }
    return [System.Net.WebUtility]::HtmlDecode($text).Trim()
}

function Sensor-DiagonalFromFormat([string]$text) {
    $t = (Html-Decode $text).Replace([char]0x2033, '"').Replace("''", '"').Replace(" ", "")
    $t = $t.Trim('"')
    $formats = @(
        @('1/4', 4.5), @('1/3', 6.0), @('1/2.9', 6.3), @('1/2.7', 6.7),
        @('1/2.5', 7.2), @('1/2.3', 7.7), @('1/2', 8.0), @('1/1.8', 9.0),
        @('1/1.7', 9.5), @('2/3', 11.0), @('1/1.2', 13.3), @('1', 16.0),
        @('1.1', 17.6), @('1.2', 19.3), @('4/3', 22.0), @('APS-C', 28.0),
        @('APS-H', 37.0), @('35mm', 44.0)
    )
    foreach ($entry in $formats) {
        if ($t -eq $entry[0] -or $t -like "$($entry[0])`"*") { return [double]$entry[1] }
    }
    return 0
}

function Sensor-DiagonalFromSpec([string]$text) {
    $t = Html-Decode $text
    $m = [regex]::Match($t, 'Full\(([0-9]+(?:\.[0-9]+)?)\)')
    if ($m.Success) { return [double]$m.Groups[1].Value }
    $m = [regex]::Match($t, '[ØΦφФ]?\s*([0-9]+(?:\.[0-9]+)?)\s*mm', 'IgnoreCase')
    if ($m.Success) { return [double]$m.Groups[1].Value }
    $formatDiagonal = Sensor-DiagonalFromFormat $t
    if ($formatDiagonal -gt 0) { return $formatDiagonal }
    $m = [regex]::Match($t, '^\s*([0-9]+(?:\.[0-9]+)?)')
    if ($m.Success) { return [double]$m.Groups[1].Value }
    return Sensor-DiagonalMm $t
}

function Parse-WdPair([string]$text) {
    $t = Html-Decode $text
    $nominal = First-Number $t 0
    $tol = 0
    $m = [regex]::Match($t, "$([char]0x00b1)\s*([0-9]+(?:\.[0-9]+)?)")
    if (!$m.Success) {
        $m = [regex]::Match($t, "$([char]0x5364)\s*([0-9]+(?:\.[0-9]+)?)")
    }
    if ($m.Success) { $tol = [double]$m.Groups[1].Value }
    return @($nominal, $tol)
}

function Parse-MinWorkingDistanceMm([string]$text) {
    $t = Html-Decode $text
    $minWd = First-Number $t 0
    if ($minWd -gt 0 -and $t -match '(?i)[0-9]\s*m\b' -and $t -notmatch '(?i)mm' -and $minWd -lt 20) {
        $minWd *= 1000
    }
    return $minWd
}

function Parse-DistortionPercent([string]$text) {
    $t = Html-Decode $text
    $value = First-Number $t 0
    if ($value -gt 0 -and $value -lt 0.1 -and $t -notmatch '%') {
        return $value * 100.0
    }
    return $value
}

function Megapixels-FromText([string]$text, [bool]$allowBareM = $false) {
    $pattern = if ($allowBareM) {
        '(?i)([0-9]+(?:\.[0-9]+)?)\s*(?:MP|M\b)'
    } else {
        '(?i)([0-9]+(?:\.[0-9]+)?)\s*MP'
    }
    $m = [regex]::Match((Html-Decode $text), $pattern)
    if ($m.Success) { return [double]$m.Groups[1].Value }
    return 0
}

function Format-Number([double]$value) {
    if ([math]::Abs($value - [math]::Round($value)) -lt 0.0000001) {
        return [string][int][math]::Round($value)
    }
    return ("{0:0.######}" -f $value).TrimEnd("0").TrimEnd(".")
}

function Parse-DofMm([string]$text) {
    $t = Html-Decode $text
    $dof = First-Number $t 0
    if ($dof -le 0) { return 0 }
    return $dof
}

function Strip-Html([string]$text) {
    if ([string]::IsNullOrWhiteSpace($text)) { return "" }
    return Clean-Field (Html-Decode ($text -replace '<[^>]+>', ''))
}

function Get-CoolensProductFields([string]$productId, [string]$listId) {
    $fields = @{}
    if ([string]::IsNullOrWhiteSpace($productId) -or [string]::IsNullOrWhiteSpace($listId)) {
        return $fields
    }

    $url = "https://coolens.cn/productview/$productId.html?plid=$listId"
    $response = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 20
    $html = $response.Content
    $matches = [regex]::Matches(
        $html,
        '<div class="dd">\s*<div class="title ellipsis">\s*<div class="ww">(.*?)</div>\s*</div>\s*<div class="data ellipsis">(.*?)</div>',
        'Singleline')
    foreach ($match in $matches) {
        $label = Strip-Html $match.Groups[1].Value
        $value = Strip-Html $match.Groups[2].Value
        $upper = $label.ToUpperInvariant()
        if ($upper.Contains("WD")) {
            $fields["wd"] = $value
        } elseif ($upper.Contains("DOF")) {
            $fields["dof"] = $value
        } elseif ($upper.Contains("F/#")) {
            $fields["fno"] = $value
        } elseif ($label.Contains("$([char]0x7126)$([char]0x8ddd)")) {
            $fields["focal"] = $value
        } elseif ($label.Contains("$([char]0x63a5)$([char]0x53e3)")) {
            $fields["mount"] = $value
        } elseif ($label.Contains("$([char]0x652f)$([char]0x6301)") -and ($upper.Contains("CCD") -or $label.Contains("$([char]0x5c3a)$([char]0x5bf8)"))) {
            $fields["sensor"] = $value
        } elseif ($label.Contains("$([char]0x5206)$([char]0x8fa8)$([char]0x7387)")) {
            $fields["objectResolution"] = $value
        } elseif ($label.Contains("%") -and ($upper.Contains("MAX") -or $label.Contains("$([char]0x7578)$([char]0x53d8)"))) {
            $fields["distortion"] = $value
        } elseif (($upper.Contains("MAX") -or $label.Contains("$([char]0x8fdc)$([char]0x5fc3)$([char]0x5ea6)")) -and !$label.Contains("%")) {
            $fields["telecentricity"] = $value
        }
    }
    return $fields
}

function Update-CoolensLensDetail([hashtable]$map, [string]$model, [hashtable]$detail) {
    if ([string]::IsNullOrWhiteSpace($model)) { return }
    $productKey = 'coolens' + "`n" + $model.Trim().ToLowerInvariant()
    if (!$map.ContainsKey($productKey) -or !$detail -or $detail.Count -eq 0) {
        return
    }

    $row = $map[$productKey]
    if ($row.manufacturer -ne "COOLENS" -or ($row.lens_type -ne "ObjectTelecentric" -and $row.lens_type -ne "BiTelecentric")) {
        return
    }

    if ($detail.ContainsKey("wd")) {
        $wdPair = Parse-WdPair $detail["wd"]
        if ($wdPair[0] -gt 0) {
            $row.nominal_wd_mm = Format-Number $wdPair[0]
        }
        if ($wdPair[1] -gt 0 -and (First-Number ([string]$row.wd_tolerance_mm) 0) -le 0) {
            $row.wd_tolerance_mm = Format-Number $wdPair[1]
        }
    }
    if ($detail.ContainsKey("dof")) {
        $dof = Parse-DofMm $detail["dof"]
        $currentDof = First-Number ([string]$row.dof_mm) 0
        $looksLikeHalfDof = $false
        if ($dof -gt 0 -and ($currentDof -le 0 -or $looksLikeHalfDof -or [math]::Abs($currentDof - $dof) -gt 0.0000001)) {
            $row.dof_mm = Format-Number $dof
        }
    }
    if ($detail.ContainsKey("fno")) {
        $fno = First-Number $detail["fno"] 0
        if ($fno -gt 0) {
            $row.f_number = Format-Number $fno
        }
    }
    if ($detail.ContainsKey("distortion")) {
        $distortion = First-Number $detail["distortion"] 0
        if ($distortion -gt 0) {
            $row.distortion_percent = Format-Number $distortion
        }
    }
    if ($detail.ContainsKey("telecentricity")) {
        $telecentricity = First-Number $detail["telecentricity"] 0
        if ($telecentricity -gt 0) {
            $row.telecentricity_deg = Format-Number $telecentricity
        }
    }
}

function Infer-Mount([string]$model, [string]$mountText) {
    $mount = Normalize-Mount (Html-Decode $mountText)
    if (![string]::IsNullOrWhiteSpace($mount)) { return $mount }
    $m = $model.ToUpperInvariant()
    if ($m -match 'M58') { return "M58" }
    if ($m -match 'M42') { return "M42" }
    if ($m -match 'F[-_]?MOUNT') { return "F" }
    return ""
}

function Is-Yes([string]$text) {
    $t = Html-Decode $text
    return $t -match '(?i)^\s*(yes|true|1)\s*$' -or
        $t.Contains("$([char]0x662f)") -or
        $t.Contains("$([char]0x6709)")
}

function Invoke-Utf8Json([string]$uri) {
    $response = Invoke-WebRequest -Uri $uri -UseBasicParsing -TimeoutSec 60
    if ($response.RawContentStream.CanSeek) { $response.RawContentStream.Position = 0 }
    $ms = New-Object IO.MemoryStream
    $response.RawContentStream.CopyTo($ms)
    $text = [Text.Encoding]::UTF8.GetString($ms.ToArray())
    return $text | ConvertFrom-Json
}

function Param-Map($groups) {
    $m = @{}
    foreach ($g in $groups) {
        foreach ($c in $g.child) {
            if ($c.attr_name) { $m[(Clean-Field $c.attr_name)] = Clean-Field ([string]$c.attr_value) }
        }
    }
    return $m
}

function Param-Value([hashtable]$map, [string[]]$keys) {
    foreach ($key in $keys) {
        if ($map.ContainsKey($key) -and ![string]::IsNullOrWhiteSpace($map[$key])) {
            return $map[$key]
        }
    }
    return ""
}

function Normalize-ColorMode([string]$model, [string]$color, [string]$typeText, [string]$pixelFormat) {
    $joined = "$typeText $pixelFormat"
    if ($joined -match '(?i)\bcolor\b|bayer') { return "Color" }
    if ($joined -match '(?i)\bmono\b') { return "Mono" }

    $c = Clean-Field $color
    if ($c -match '(?i)color|colour') { return "Color" }
    if ($c -match '(?i)mono|monochrome') { return "Mono" }

    $m = $model.ToUpperInvariant()
    if ($m -match '(GC|UC|YC|VC|NC|CC|TC)(?:[-/\s(]|$)' -or $m -match 'Y1C') { return "Color" }
    if ($m -match '(GM|UM|YM|VM|NM|XM|TM|CM|YN)(?:[-/\s(]|$)' -or $m -match 'Y1M') { return "Mono" }
    return $c
}

function Add-Camera([hashtable]$map, [string]$model, [int]$rx, [int]$ry, [double]$pixel,
                    [string]$sensor, [string]$color, [string]$shutter, [double]$fps,
                    [string]$iface, [double]$bitDepth, [double]$dynamicRange, [string]$mount,
                    [string]$manufacturer = "") {
    if ($rx -le 0 -or $ry -le 0 -or $pixel -le 0) { return }
    $ifaceNorm = Normalize-Interface $iface
    Add-ToMap $map (New-Row $cameraHeaders @{
        model = $model
        manufacturer = $manufacturer
        resolution_x = $rx
        resolution_y = $ry
        pixel_size_um = $pixel
        sensor_format = $sensor
        color_mode = (Normalize-ColorMode $model $color "" "")
        pixel_format = $(if ($color -match '^(Mono(8|10|12|14|16)(p|Packed)?|Bayer(RG|GB|GR|BG)(8|10|12|16)(p|Packed)?|RGB8|BGR8)$') { $color } else { '' })
        shutter_type = $shutter
        max_fps = $fps
        interface = $ifaceNorm
        bandwidth_mbps = (Interface-Bandwidth $iface)
        bandwidth_source = 'estimated'
        bit_depth = $bitDepth
        dynamic_range_db = $dynamicRange
        lens_mount = (Normalize-Mount $mount)
    })
}

function Add-CstCameraVariants([hashtable]$map, [string]$modelPattern, [string]$res, [double]$fps,
                               [string]$sensor, [double]$pixel, [string]$shutter,
                               [string]$colorFlag, [string]$iface) {
    $r = Parse-Resolution $res
    if ($colorFlag -eq "M/C") {
        $mono = $modelPattern -replace '/C$', ''
        $color = $mono
        if ($color -match 'GM$') { $color = $color -replace 'GM$', 'GC' }
        elseif ($color -match 'UM$') { $color = $color -replace 'UM$', 'UC' }
        elseif ($color -match 'CM$') { $color = $color -replace 'CM$', 'CC' }
        Add-Camera $map $mono $r[0] $r[1] $pixel $sensor "Mono" $shutter $fps $iface 12 0 "C" "CST"
        Add-Camera $map $color $r[0] $r[1] $pixel $sensor "Color" $shutter $fps $iface 12 0 "C" "CST"
    } else {
        $model = $modelPattern -replace '/C$', ''
        Add-Camera $map $model $r[0] $r[1] $pixel $sensor "Mono" $shutter $fps $iface 12 0 "C" "CST"
    }
}

function Add-FixedLens([hashtable]$map, [string]$model, [double]$focal, [double]$fno,
                       [double]$imageCircle, [double]$mp, [double]$minWdMm,
                       [double]$distortion, [string]$mount, [string]$notes,
                       [string]$manufacturer = "") {
    Add-ToMap $map (New-Row $lensHeaders @{
        model = $model
        manufacturer = $manufacturer
        lens_type = "FixedFocal"
        lens_mount = (Normalize-Mount $mount)
        focal_length_mm = $focal
        min_wd_mm = $minWdMm
        distortion_percent = $distortion
        image_circle_mm = $imageCircle
        megapixel_rating = $mp
        recommended_min_pixel_um = 3.45
        pmag = 0
        nominal_wd_mm = 0
        wd_tolerance_mm = 0
        max_sensor_diagonal_mm = 0
        telecentricity_deg = 0
        dof_mm = 0
        numerical_aperture = 0
        f_number = $fno
        coaxial_illumination = "false"
        notes = $notes
    })
}

function Add-TeleLens([hashtable]$map, [string]$model, [string]$type, [double]$pmag,
                      [double]$imageCircle, [double]$fno, [double]$resolutionUm,
                      [double]$wd, [double]$dof, [double]$distortion,
                      [double]$teleDeg, [string]$notes,
                      [string]$manufacturer = "", [string]$coaxial = "") {
    $supportsCoax = if ([string]::IsNullOrWhiteSpace($coaxial)) {
        ($model -match 'D$|DH$').ToString().ToLowerInvariant()
    } else {
        $coaxial.ToLowerInvariant()
    }
    Add-ToMap $map (New-Row $lensHeaders @{
        model = $model
        manufacturer = $manufacturer
        lens_type = $type
        lens_mount = "C"
        focal_length_mm = 0
        min_wd_mm = 0
        distortion_percent = $distortion
        image_circle_mm = $imageCircle
        megapixel_rating = 12
        recommended_min_pixel_um = $resolutionUm
        pmag = $pmag
        nominal_wd_mm = $wd
        wd_tolerance_mm = ([math]::Round($wd * 0.03, 2))
        max_sensor_diagonal_mm = $imageCircle
        telecentricity_deg = $teleDeg
        dof_mm = $dof
        numerical_aperture = 0
        f_number = $fno
        coaxial_illumination = $supportsCoax
        notes = $notes
    })
}

function Add-CoolensTeleLens([hashtable]$map, $row, [string]$type, [string]$series,
                             [string]$sensor, [string]$pmag, [string]$wd,
                             [string]$fno, [string]$mount, [string]$coax,
                             [string]$distortion, [string]$telecentricity, [string]$dof) {
    $model = Html-Decode $row.pl_title
    $imageCircle = Sensor-DiagonalFromSpec $sensor
    $mag = First-Number $pmag 0
    if ([string]::IsNullOrWhiteSpace($model) -or $imageCircle -le 0 -or $mag -le 0) { return }

    $wdPair = Parse-WdPair $wd
    Add-ToMap $map (New-Row $lensHeaders @{
        model = $model
        manufacturer = "COOLENS"
        lens_type = $type
        lens_mount = (Infer-Mount $model $mount)
        focal_length_mm = 0
        min_wd_mm = 0
        distortion_percent = (First-Number $distortion 0)
        image_circle_mm = $imageCircle
        megapixel_rating = 0
        recommended_min_pixel_um = 0
        pmag = $mag
        nominal_wd_mm = $wdPair[0]
        wd_tolerance_mm = $wdPair[1]
        max_sensor_diagonal_mm = $imageCircle
        telecentricity_deg = (First-Number $telecentricity 0)
        dof_mm = (Parse-DofMm $dof)
        numerical_aperture = 0
        f_number = (First-Number $fno 0)
        coaxial_illumination = (Is-Yes $coax).ToString().ToLowerInvariant()
        notes = "COOLENS $series official product list"
    })
}

function Add-CoolensFixedLens([hashtable]$map, $row, [string]$series) {
    $model = Html-Decode $row.pl_title
    $focal = First-Number $row.text0 0
    $imageCircle = First-Number $row.text6 0
    if ($imageCircle -le 0) { $imageCircle = Sensor-DiagonalFromSpec $row.text2 }
    $minWd = First-Number $row.text4 0
    if ($minWd -gt 0 -and $minWd -lt 20) { $minWd = $minWd * 1000 }
    if ([string]::IsNullOrWhiteSpace($model) -or $focal -le 0 -or $imageCircle -le 0) { return }

    Add-FixedLens $map $model $focal (First-Number $row.text1 0) $imageCircle 0 $minWd (First-Number $row.text3 0) $row.text5 "COOLENS $series official product list" "COOLENS"
}

function Add-CoolensWwtFixedLens([hashtable]$map, $row, [hashtable]$detail, [string]$series) {
    $model = Html-Decode $row.pl_title
    if ([string]::IsNullOrWhiteSpace($model) -or !$detail -or !$detail.ContainsKey("focal")) { return }

    $sensor = if ($detail.ContainsKey("sensor")) { $detail["sensor"] } else { $row.text0 }
    $imageCircle = Sensor-DiagonalFromSpec $sensor
    $focal = First-Number $detail["focal"] 0
    if ($imageCircle -le 0 -or $focal -le 0) { return }

    $wdText = if ($detail.ContainsKey("wd")) { $detail["wd"] } else { $row.text1 }
    $minWd = (Parse-WdPair $wdText)[0]
    $fno = if ($detail.ContainsKey("fno")) { First-Number $detail["fno"] 0 } else { First-Number $row.text2 0 }
    $distortion = if ($detail.ContainsKey("distortion")) { First-Number $detail["distortion"] 0 } else { 0 }
    $mount = if ($detail.ContainsKey("mount")) { $detail["mount"] } else { "C" }

    Add-FixedLens $map $model $focal $fno $imageCircle 0 $minWd $distortion $mount "COOLENS $series official product detail" "COOLENS"
}

function Add-IraypleFixedLens([hashtable]$map, $detail) {
    $model = Clean-Field $detail.data.model
    $p = Param-Map $detail.data.parameter
    $focal = First-Number (Param-Value $p @("Focal Length", "镜头焦距")) 0
    $sensor = Param-Value $p @("Image Sensor Size", "lmage Sensor Size", "像面尺寸")
    $imageCircle = Sensor-DiagonalFromSpec $sensor
    $mount = Param-Value $p @("Camera Mount", "安装接口")
    $focusRange = Param-Value $p @("Focus Range", "对焦范围")
    $series = Param-Value $p @("Series", "系列")
    $note = "iRAYPLE official product API; $series"
    if ([string]::IsNullOrWhiteSpace($mount) -and $series -eq "MH-SP Series" -and $model -match '^MH\d+SP$') {
        $mount = "C Mount"
        $note = "$note; camera mount inferred from other MH-SP official records"
    }
    $mp = Megapixels-FromText "$series $model"
    $distortionText = Param-Value $p @("Optical Distortion", "光学畸变", "TV Distortion", "TV畸变")
    if ([string]::IsNullOrWhiteSpace($model) -or $focal -le 0 -or $imageCircle -le 0) { return }

    Add-FixedLens $map $model $focal `
        (First-Number (Param-Value $p @("Aperture", "光圈孔径")) 0) `
        $imageCircle $mp (Parse-MinWorkingDistanceMm $focusRange) `
        (Parse-DistortionPercent $distortionText) $mount `
        $note "iRAYPLE"
}

function Get-DahengLensSpec([string]$url) {
    $html = (Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 30).Content
    $spec = @{}
    $matches = [regex]::Matches($html, '<tr>\s*<td class="td1">\s*(.*?)\s*</td>\s*<td>\s*(.*?)\s*</td>\s*</tr>', 'Singleline')
    foreach ($match in $matches) {
        $key = Strip-Html $match.Groups[1].Value
        $value = Strip-Html $match.Groups[2].Value
        if (![string]::IsNullOrWhiteSpace($key)) { $spec[$key] = $value }
    }
    return $spec
}

function Add-DahengFixedLens([hashtable]$map, [hashtable]$spec, [string]$sourceUrl) {
    $model = Clean-Field $spec["Model"]
    $focal = First-Number $spec["Focal Length(mm)"] 0
    $imageCircle = Sensor-DiagonalFromFormat $spec["Format(inch)"]
    $mp = Megapixels-FromText $spec["Resolution"] $true
    $minWd = First-Number $spec["Min. Working Distance(mm)"] 0
    if ($minWd -le 0) { $minWd = Parse-MinWorkingDistanceMm $spec["Working Distance(mm)"] }
    if ([string]::IsNullOrWhiteSpace($model) -or $focal -le 0 -or $imageCircle -le 0) { return }

    Add-FixedLens $map $model $focal (First-Number $spec["Aperture"] 0) $imageCircle $mp $minWd `
        (First-Number $spec["Distortion(%)"] 0) $spec["Mount"] `
        "Daheng Imaging official lens specifications; $sourceUrl" "Daheng Imaging"
}

$cameraMap = @{}
foreach ($row in (Import-Csv $cameraPath)) { Add-ToMap $cameraMap $row }
$lensMap = @{}
foreach ($row in (Import-Csv $lensPath)) { Add-ToMap $lensMap $row }

# CST official structured tables:
# https://www.cstmv.com/show/1641.html and https://www.cstmv.com/show/1651.html
$cstCameras = @(
    @("CST-CG004-300GM/C","720x540",300,"1/2.9""",6.9,"Global","M/C","GigE,POE"),
    @("CST-CG013-80GM/C","1280x1024",80,"1/2""",4.8,"Global","M/C","GigE,POE"),
    @("CST-CG013-87GM/C","1280x1024",87,"1/2.7""",4.0,"Global","M/C","GigE,POE"),
    @("CST-CG016-77GM/C","1440x1080",77,"1/2.9""",3.45,"Global","M/C","GigE,POE"),
    @("CST-CG020-60GM/C","1624x1240",60,"1/1.7""",4.5,"Global","M/C","GigE,POE"),
    @("CST-CG023-39GM/C","1920x1200",39,"1/1.2""",5.86,"Global","M/C","GigE,POE"),
    @("CST-CG030-36GM/C","2048x1536",36,"1/1.8""",3.45,"Global","M/C","GigE,POE"),
    @("CST-CG050-20GM/C","2448x2048",20,"2/3""",3.45,"Global","M/C","GigE,POE"),
    @("CST-CG050-75UM/C","2448x2048",75,"2/3""",3.45,"Global","M/C","USB3.0"),
    @("CST-CG053-21GM/C","2592x2048",21,"2/3""",3.2,"Global","M/C","GigE,POE"),
    @("CST-CR050-23GM/C","2592x1944",23,"1/2.5""",2.2,"Rolling","M/C","GigE"),
    @("CST-CR060-18GM/C","3072x2048",18,"1/1.8""",2.4,"Rolling","M/C","GigE,POE"),
    @("CST-CR100-10GM/C","3856x2764",10,"1/2.3""",1.67,"Rolling","M","GigE,POE"),
    @("CST-CR120-9GM/C","4000x3000",9,"1/1.7""",1.85,"Rolling","M/C","GigE,POE"),
    @("CST-CR200-6GM/C","5472x3648",5.8,"1""",2.4,"Rolling","M/C","GigE,POE"),
    @("CST-CG123-9GM/C","4096x3000",9,"1.1""",3.45,"Global","M/C","GigE,POE"),
    @("CST-CG250-4GM/C","5120x5120",4,"1.1""",2.5,"Global","M/C","GigE,POE"),
    @("CST-CG300-3GM/C","6464x4852",3.6,"22.3x16.7mm",3.45,"Global","M/C","GigE"),
    @("CST-CG654-1GM/C","9344x7000",1.7,"29.9x22.4mm",3.2,"Global","M/C","GigE"),
    @("CST-CG123-30UM/C","4096x3000",23,"1.1""",3.45,"Global","M/C","USB 3.0"),
    @("CST-CG250-14UM/C","5120x5120",14,"1.1""",2.5,"Global","M/C","USB 3.0"),
    @("CST-CG123-20CM/C","4096x3000",20,"1.1""",3.45,"Global","M/C","CameraLink"),
    @("CST-CG250-30CM","5120x5120",30,"1.1""",2.5,"Global","M","CameraLink")
)
foreach ($c in $cstCameras) {
    Add-CstCameraVariants $cameraMap $c[0] $c[1] $c[2] $c[3] $c[4] $c[5] $c[6] $c[7]
}

# CST telecentric lenses, official tables:
# https://www.cstmv.com/show/1650.html and https://www.cstmv.com/show/1654.html
$cstObjectTele = @(
    @("CST-TL11005D",0.5,11,9.3,12,110,6.4,0.05,0.1), @("CST-TL11005",0.5,11,9.3,12,110,6.4,0.05,0.1),
    @("CST-TL11010D",1.0,11,11,7.4,110,2.2,0.0,0.1), @("CST-TL11010",1.0,11,11,7.4,110,2.2,0.0,0.1),
    @("CST-TL11020D",2.0,11,13.6,4.5,110,0.5,0.05,0.1), @("CST-TL11020",2.0,11,13.6,4.5,110,0.5,0.05,0.1),
    @("CST-TL11040D",4.0,11,22,3.7,110,0.24,0.05,0.1), @("CST-TL11040",4.0,11,22,3.7,110,0.24,0.05,0.1),
    @("CST-TL11060D",6.0,11,35,4,110,0.18,0.02,0.1), @("CST-TL11060",6.0,11,35,4,110,0.18,0.02,0.1),
    @("CST-TL6508D",0.8,11,10,8.25,65,1.7,0.09,0.1), @("CST-TL6508",0.8,11,10,8.25,65,1.7,0.09,0.1),
    @("CST-TL6510D",1.0,11,11.1,7.33,65,1.2,0.04,0.1), @("CST-TL6510",1.0,11,11.1,7.33,65,1.2,0.04,0.1),
    @("CST-TL6515D",1.5,11,11.9,5.2,65,0.51,0.099,0.15), @("CST-TL6515",1.5,11,11.9,5.2,65,0.51,0.099,0.15),
    @("CST-TL6520D",2.14,11,14.4,4.5,65,0.34,0.039,0.11), @("CST-TL6520",2.14,11,14.4,4.5,65,0.34,0.039,0.11),
    @("CST-TL6530D",3.0,11,15.7,3.5,65,0.18,0.038,0.15), @("CST-TL6530",3.0,11,15.7,3.5,65,0.18,0.038,0.15),
    @("CST-TL6540D",4.0,11,17.7,3,65,0.1,0.064,0.11), @("CST-TL6540",4.0,11,17.7,3,65,0.1,0.064,0.11),
    @("CST-TL6560D",6.0,11,26.8,3,65,0.07,0.03,0.15), @("CST-TL6560",6.0,11,26.8,3,65,0.07,0.03,0.15),
    @("CST-TL4175-1M",0.4,17.6,6.8,11.3,175,4.4,0.122,0.1)
)
foreach ($l in $cstObjectTele) {
    Add-TeleLens $lensMap $l[0] "ObjectTelecentric" $l[1] $l[2] $l[3] $l[4] $l[5] $l[6] $l[7] $l[8] "CST standard object-side telecentric lens" "CST"
}
Add-TeleLens $lensMap "CST-TL25D90-H" "ObjectTelecentric" 1.25 17.6 8.4 4.5 90 0.6 0.059 0.1 "CST object-side telecentric lens from official standard telecentric table" "CST" "true"
$cstBiTele = @(
    @("CST-TL6505DH",0.5,11,6,8,65,2.54,0.001,0.25), @("CST-TL6505H",0.5,11,6,8,65,2.54,0.001,0.25),
    @("CST-TL17805DH",0.5,11,7,9.2,178,2.9,0.001,0.11), @("CST-TL17805H",0.5,11,7,9.2,178,2.9,0.001,0.11),
    @("CST-TL11004DH",0.4,11,8,13.2,110,5.2,0.011,0.25), @("CST-TL11004H",0.4,11,8,13.2,110,5.2,0.020,0.25),
    @("CST-TL184036H",0.367,22,5.9,18,184.4,6.2,0.010,0.08),
    @("CST-TL6503DH",0.3,11,7.5,16.5,65,8.8,0.050,0.07), @("CST-TL6503H",0.3,11,7.5,16.5,65,8.8,0.043,0.044),
    @("CST-TL11003DH",0.3,11,7.5,16.4,110,8.2,0.025,0.1), @("CST-TL11003H",0.3,11,7.5,16.4,110,8.7,0.065,0.1),
    @("CST-TL150024H",0.24,11,8,22,150,14.6,0.030,0.031), @("CST-TL167022H",0.22,11,6.4,18,167,8.4,0.030,0.008),
    @("CST-TL150022H",0.22,11,8,24.1,150,17.4,0.030,0.032), @("CST-TL15002H",0.2,11,8,26.4,150,21,0.030,0.039),
    @("CST-TL178018H",0.18,11,8,30,178,26,0.055,0.031), @("CST-TL167018H",0.188,11,6.4,10.6,167,8.4,0.014,0.009),
    @("CST-TL178016H",0.16,11,8,33,178,33,0.060,0.029), @("CST-TL178014H",0.14,11,8,37.9,178,43.2,0.060,0.031),
    @("CST-TL250012H",0.12,11,8,43.7,250,58,0.040,0.036), @("CST-TL25001H",0.1,11,8,52.8,250,85,0.050,0.036)
)
foreach ($l in $cstBiTele) {
    Add-TeleLens $lensMap $l[0] "BiTelecentric" $l[1] $l[2] $l[3] $l[4] $l[5] $l[6] $l[7] $l[8] "CST high-resolution bi-telecentric lens" "CST"
}
Add-FixedLens $lensMap "CST-A7528-5M" 75 2.8 9 5 500 0.05 "C" "CST 1/1.8 5MP FA lens" "CST"
Add-FixedLens $lensMap "CST-YB1220-12M" 12 2.0 11 12 150 0.12 "C" "CST 2/3 12MP YB FA lens" "CST"
Add-FixedLens $lensMap "CST-YB2520-12M" 25 2.0 11 12 200 0.12 "C" "CST 2/3 12MP YB FA lens" "CST"

if (!$SkipNetwork) {
    Write-Host "Fetching Hikrobot area scan cameras..."
    $hikPageSize = 500
    $hikCamList = (Invoke-WebRequest -Uri "https://www.hikrobotics.com/en/Api/Foreground/Vision/VisionProductContent?firstModuleId=78&secondaryModuleId=145&page=1&size=$hikPageSize&showEol=false" -UseBasicParsing -TimeoutSec 60).Content | ConvertFrom-Json
    $hikRecords = @()
    $hikRecords += $hikCamList.data.VisionProductContent.records
    $hikTotal = [int]$hikCamList.data.VisionProductContent.total
    $hikPages = [math]::Ceiling($hikTotal / $hikPageSize)
    for ($page = 2; $page -le $hikPages; $page++) {
        $pageData = (Invoke-WebRequest -Uri "https://www.hikrobotics.com/en/Api/Foreground/Vision/VisionProductContent?firstModuleId=78&secondaryModuleId=145&page=$page&size=$hikPageSize&showEol=false" -UseBasicParsing -TimeoutSec 60).Content | ConvertFrom-Json
        $hikRecords += $pageData.data.VisionProductContent.records
    }
    $i = 0
    foreach ($rec in $hikRecords) {
        $i++
        if ($i % 50 -eq 0) { Write-Host "  Hikrobot camera details $i / $($hikRecords.Count)" }
        try {
            $cfg = (Invoke-WebRequest -Uri "https://www.hikrobotics.com/en/Api/Foreground/Vision/VisionProductConfig?id=$($rec.id)" -UseBasicParsing -TimeoutSec 20).Content | ConvertFrom-Json
            $p = @{}
            foreach ($item in $cfg.data) { $p[$item.name] = [string]$item.value }
            $r = Parse-Resolution $p["Resolution"]
            $pixel = First-Number $p["Pixel size"] 0
            if ($pixel -eq 0) { $pixel = First-Number $p["Pixel Size"] 0 }
            $fps = First-Number $p["Max. frame rate"] 0
            if ($fps -eq 0) { $fps = First-Number $p["Frame Rate"] 0 }
            $sensor = $p["Sensor size"]
            if (!$sensor) { $sensor = $p["Image Sensor"] }
            $sensorType = $p["Sensor type"]
            $shutter = if ($sensorType -match '(?i)rolling') { "Rolling" } elseif ($sensorType -match '(?i)global') { "Global" } else { $p["Shutter"] }
            $color = $p["Mono/color"]
            if (!$color) { $color = $p["Mono/Color"] }
            $color = Normalize-ColorMode $p["Product Model"] $color $p["Type"] $p["Pixel format"]
            $iface = $p["Data interface"]
            if (!$iface) { $iface = $p["Port"] }
            $bitDepthText = Param-Value $p @("Bit depth", "Bit Depth")
            if (!$bitDepthText) { $bitDepthText = $p["Pixel format"] }
            Add-Camera $cameraMap $p["Product Model"] $r[0] $r[1] $pixel $sensor $color $shutter $fps $iface (Max-BitDepth $bitDepthText 12) (First-Number $p["Dynamic range"] 0) $p["Lens mount"] "Hikrobot"
        } catch {
            Write-Warning "Skipped Hikrobot camera id=$($rec.id): $($_.Exception.Message)"
        }
    }

    Write-Host "Fetching Hikrobot FA lenses..."
    $hikLensList = (Invoke-WebRequest -Uri "https://www.hikrobotics.com/en/Api/Foreground/Vision/VisionProductContent?firstModuleId=40&secondaryModuleId=49&page=1&size=500&showEol=false" -UseBasicParsing -TimeoutSec 60).Content | ConvertFrom-Json
    foreach ($rec in $hikLensList.data.VisionProductContent.records) {
        try {
            $cfg = (Invoke-WebRequest -Uri "https://www.hikrobotics.com/en/Api/Foreground/Vision/VisionProductConfig?id=$($rec.id)" -UseBasicParsing -TimeoutSec 20).Content | ConvertFrom-Json
            $p = @{}
            foreach ($item in $cfg.data) { $p[$item.name] = [string]$item.value }
            $minWd = First-Number $p["Minimum object distance"] 0
            if ($p["Minimum object distance"] -match '\bm\b' -and $minWd -lt 20) { $minWd = $minWd * 1000 }
            Add-FixedLens $lensMap $p["Product Model"] (First-Number $p["Focal length"] 0) (First-Number $p["F-number"] 0) (First-Number $p["Image size"] 0) (Megapixels-FromText $p["Type"] $true) $minWd (First-Number $p["Distortion"] 0) $p["Mount"] "Hikrobot FA lens from official product API" "Hikrobot"
        } catch {
            Write-Warning "Skipped Hikrobot lens id=$($rec.id): $($_.Exception.Message)"
        }
    }

    Write-Host "Fetching Hikrobot M12 lenses..."
    $hikM12LensList = (Invoke-WebRequest -Uri "https://www.hikrobotics.com/en/Api/Foreground/Vision/VisionProductContent?firstModuleId=40&secondaryModuleId=180&page=1&size=100&showEol=false" -UseBasicParsing -TimeoutSec 60).Content | ConvertFrom-Json
    foreach ($rec in $hikM12LensList.data.VisionProductContent.records) {
        try {
            $cfg = (Invoke-WebRequest -Uri "https://www.hikrobotics.com/en/Api/Foreground/Vision/VisionProductConfig?id=$($rec.id)" -UseBasicParsing -TimeoutSec 20).Content | ConvertFrom-Json
            $p = @{}
            foreach ($item in $cfg.data) { $p[$item.name] = [string]$item.value }
            $minWd = First-Number $p["Working Distance Range"] 0
            Add-FixedLens $lensMap $p["Product Model"] (First-Number $p["Focal length"] 0) (First-Number $p["F-Number"] 0) (First-Number $p["Image Size"] 0) (Megapixels-FromText $p["Type"] $true) $minWd (First-Number $p["TV Distortion"] 0) $p["Mount"] "Hikrobot M12 lens from official product API" "Hikrobot"
        } catch {
            Write-Warning "Skipped Hikrobot M12 lens id=$($rec.id): $($_.Exception.Message)"
        }
    }

    Write-Host "Fetching iRAYPLE area scan cameras..."
    $first = (Invoke-WebRequest -Uri "https://www.irayple.com/api/en/vision/productListNew?id=106&page=1&type=vision" -UseBasicParsing -TimeoutSec 60).Content | ConvertFrom-Json
    $allHuaray = @()
    $allHuaray += $first.data.list
    for ($page = 2; $page -le [int]$first.data.totalPage; $page++) {
        $pageData = (Invoke-WebRequest -Uri "https://www.irayple.com/api/en/vision/productListNew?id=106&page=$page&type=vision" -UseBasicParsing -TimeoutSec 60).Content | ConvertFrom-Json
        $allHuaray += $pageData.data.list
    }
    $i = 0
    foreach ($rec in $allHuaray) {
        $i++
        if ($i % 50 -eq 0) { Write-Host "  iRAYPLE camera details $i / $($allHuaray.Count)" }
        try {
            $detail = (Invoke-WebRequest -Uri "https://www.irayple.com/api/en/product/productDetails?id=$($rec.id)" -UseBasicParsing -TimeoutSec 20).Content | ConvertFrom-Json
            $p = Param-Map $detail.data.parameter
            $r = Parse-Resolution $p["Resolution"]
            $pixel = First-Number $p["Pixel Size"] 0
            $fps = First-Number $p["Frame Rate"] 0
            $sensor = $p["Image Sensor"]
            $color = Normalize-ColorMode $detail.data.model $p["Mono/Color"] "" $p["Image Format"]
            Add-Camera $cameraMap $detail.data.model $r[0] $r[1] $pixel $sensor $color $p["Shutter"] $fps $p["Port"] (Max-BitDepth $p["Bit Depth"] 12) (First-Number $p["Dynamic Range"] 0) $p["Lens Mount"] "iRAYPLE"
        } catch {
            Write-Warning "Skipped iRAYPLE camera id=$($rec.id): $($_.Exception.Message)"
        }
    }

    Write-Host "Fetching iRAYPLE lenses..."
    $iraypleLensCategoryIds = @(121, 187)
    foreach ($categoryId in $iraypleLensCategoryIds) {
        try {
            $firstLensPage = Invoke-Utf8Json "https://www.irayple.com/api/en/vision/productListNew?id=$categoryId&page=1&type=vision"
            $lensRecords = @()
            $lensRecords += $firstLensPage.data.list
            for ($page = 2; $page -le [int]$firstLensPage.data.totalPage; $page++) {
                $pageData = Invoke-Utf8Json "https://www.irayple.com/api/en/vision/productListNew?id=$categoryId&page=$page&type=vision"
                $lensRecords += $pageData.data.list
            }
            Write-Host "  iRAYPLE category ${categoryId}: $($lensRecords.Count) rows"
            foreach ($rec in $lensRecords) {
                try {
                    $detail = Invoke-Utf8Json "https://www.irayple.com/api/en/product/productDetails?id=$($rec.id)"
                    Add-IraypleFixedLens $lensMap $detail
                } catch {
                    Write-Warning "Skipped iRAYPLE lens id=$($rec.id): $($_.Exception.Message)"
                }
            }
        } catch {
            Write-Warning "Skipped iRAYPLE lens category ${categoryId}: $($_.Exception.Message)"
        }
    }

    Write-Host "Fetching Daheng Imaging fixed-focus lenses..."
    try {
        $dahengIndex = (Invoke-WebRequest -Uri "https://en.daheng-imaging.com/index.php?a=xxlists&c=index&catid=449&m=content" -UseBasicParsing -TimeoutSec 30).Content
        $dahengLinks = [regex]::Matches($dahengIndex, 'show-[0-9]+-[0-9]+-1\.html') |
            ForEach-Object { "https://en.daheng-imaging.com/$($_.Value)" } |
            Sort-Object -Unique
        Write-Host "  Daheng Imaging lens pages: $($dahengLinks.Count)"
        foreach ($link in $dahengLinks) {
            try {
                Add-DahengFixedLens $lensMap (Get-DahengLensSpec $link) $link
            } catch {
                Write-Warning "Skipped Daheng Imaging lens page ${link}: $($_.Exception.Message)"
            }
        }
    } catch {
        Write-Warning "Skipped Daheng Imaging lens catalog: $($_.Exception.Message)"
    }

    Write-Host "Fetching COOLENS lens products..."
    $coolensRawRows = @()
    $coolensPages = @(
        @{ id = "003000000"; series = "DTCA large-format line-scan lens"; kind = "raw"; type = "" },
        @{ id = "003000003"; series = "DTCA high-performance industrial lens"; kind = "raw"; type = "" },
        @{ id = "003000002"; series = "DTCM large-format bi-telecentric"; kind = "dtcm"; type = "BiTelecentric" },
        @{ id = "003000001"; series = "DTCM small-format bi-telecentric"; kind = "dtcmSmall"; type = "BiTelecentric" },
        @{ id = "003000004"; series = "WWK large-format telecentric"; kind = "tele"; type = "ObjectTelecentric" },
        @{ id = "003001004"; series = "WWH 5MP telecentric"; kind = "tele"; type = "ObjectTelecentric" },
        @{ id = "003001005"; series = "WWL standard telecentric"; kind = "tele"; type = "ObjectTelecentric" },
        @{ id = "003006005"; series = "LTCM flat line-scan telecentric"; kind = "ltcm"; type = "BiTelecentric" },
        @{ id = "003001001"; series = "MFA FA fixed focal"; kind = "mfa"; type = "FixedFocal" },
        @{ id = "003001006"; series = "WWT industrial lens"; kind = "raw"; type = "" },
        @{ id = "003006003"; series = "DTCZ zoom telecentric"; kind = "raw"; type = "" }
    )
    foreach ($page in $coolensPages) {
        try {
            $url = "https://www.coolens.cn/product/$($page.id).html?act=getlist&xid=0&page=1&limit=3000"
            $list = Invoke-Utf8Json $url
            Write-Host "  COOLENS $($page.series): $($list.data.Count) rows"
            foreach ($rec in $list.data) {
                $model = Html-Decode $rec.pl_title
                $detail = @{}
                try {
                    $detail = Get-CoolensProductFields $rec.pl_proid $rec.pl_id
                    Update-CoolensLensDetail $lensMap $model $detail
                } catch {
                    Write-Warning "Skipped COOLENS detail $model id=$($rec.pl_id): $($_.Exception.Message)"
                }
                $coolensRawRows += [pscustomobject]@{
                    source_page = $page.id
                    series = $page.series
                    model = $model
                    pl_id = $rec.pl_id
                    pl_proid = $rec.pl_proid
                    text0 = (Html-Decode $rec.text0)
                    text1 = (Html-Decode $rec.text1)
                    text2 = (Html-Decode $rec.text2)
                    text3 = (Html-Decode $rec.text3)
                    text4 = (Html-Decode $rec.text4)
                    text5 = (Html-Decode $rec.text5)
                    text6 = (Html-Decode $rec.text6)
                    text7 = (Html-Decode $rec.text7)
                    text8 = (Html-Decode $rec.text8)
                    text9 = (Html-Decode $rec.text9)
                    source_url = "https://www.coolens.cn/product/$($page.id).html"
                }
                $wdText = if ($detail.ContainsKey("wd")) { $detail["wd"] } else { $rec.text3 }
                $dofText = if ($detail.ContainsKey("dof")) { $detail["dof"] } else { "" }
                $distortionText = if ($detail.ContainsKey("distortion")) { $detail["distortion"] } else { "" }
                $telecentricityText = if ($detail.ContainsKey("telecentricity")) { $detail["telecentricity"] } else { "" }
                if ($page.kind -eq "dtcm") {
                    $fnoText = if ($detail.ContainsKey("fno")) { $detail["fno"] } else { $rec.text4 }
                    Add-CoolensTeleLens $lensMap $rec $page.type $page.series $rec.text0 $rec.text2 $wdText $fnoText $rec.text5 "" $distortionText $telecentricityText $dofText
                } elseif ($page.kind -eq "dtcmSmall") {
                    $fnoText = if ($detail.ContainsKey("fno")) { $detail["fno"] } else { "" }
                    Add-CoolensTeleLens $lensMap $rec $page.type $page.series $rec.text0 $rec.text2 $wdText $fnoText $rec.text4 "" $distortionText $telecentricityText $dofText
                } elseif ($page.kind -eq "tele") {
                    $teleWdText = if ($detail.ContainsKey("wd")) { $detail["wd"] } else { $rec.text2 }
                    $fnoText = if ($detail.ContainsKey("fno")) { $detail["fno"] } else { $rec.text3 }
                    Add-CoolensTeleLens $lensMap $rec $page.type $page.series $rec.text0 $rec.text1 $teleWdText $fnoText "" $rec.text4 $distortionText $telecentricityText $dofText
                } elseif ($page.kind -eq "ltcm") {
                    $fnoText = if ($detail.ContainsKey("fno")) { $detail["fno"] } else { $rec.text4 }
                    $ltcmDofText = if ($detail.ContainsKey("dof")) { $detail["dof"] } else { $rec.text6 }
                    $ltcmDistortionText = if ($detail.ContainsKey("distortion")) { $detail["distortion"] } else { $rec.text7 }
                    $ltcmTelecentricityText = if ($detail.ContainsKey("telecentricity")) { $detail["telecentricity"] } else { $rec.text8 }
                    Add-CoolensTeleLens $lensMap $rec $page.type $page.series $rec.text3 $rec.text1 $wdText $fnoText "" "" $ltcmDistortionText $ltcmTelecentricityText $ltcmDofText
                } elseif ($page.kind -eq "mfa") {
                    Add-CoolensFixedLens $lensMap $rec $page.series
                } elseif ($page.id -eq "003001006") {
                    Add-CoolensWwtFixedLens $lensMap $rec $detail $page.series
                }
            }
        } catch {
            Write-Warning "Skipped COOLENS page $($page.id): $($_.Exception.Message)"
        }
    }
    if ($coolensRawRows.Count -gt 0) {
        $coolensRawRows | Export-Csv $coolensRawPath -NoTypeInformation -Encoding UTF8
        Write-Host "Wrote $($coolensRawRows.Count) raw COOLENS lens rows to $coolensRawPath"
    }
}

$cameraRows = $cameraMap.Values | ForEach-Object { Clean-Row $_ $cameraHeaders } | Sort-Object model
$lensRows = $lensMap.Values | ForEach-Object { Clean-Row $_ $lensHeaders } | Sort-Object model

$cameraRows | Select-Object $cameraHeaders | Export-Csv $cameraPath -NoTypeInformation -Encoding UTF8
$lensRows | Select-Object $lensHeaders | Export-Csv $lensPath -NoTypeInformation -Encoding UTF8

Write-Host "Wrote $($cameraRows.Count) cameras to $cameraPath"
Write-Host "Wrote $($lensRows.Count) lenses to $lensPath"
