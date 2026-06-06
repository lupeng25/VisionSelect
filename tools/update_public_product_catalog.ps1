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
    "bit_depth", "dynamic_range_db", "lens_mount"
)
$lensHeaders = @(
    "model", "manufacturer", "lens_type", "lens_mount", "focal_length_mm", "min_wd_mm",
    "distortion_percent", "image_circle_mm", "megapixel_rating",
    "recommended_min_pixel_um", "pmag", "nominal_wd_mm", "wd_tolerance_mm",
    "max_sensor_diagonal_mm", "telecentricity_deg", "dof_mm",
    "numerical_aperture", "f_number", "coaxial_illumination", "notes"
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
    if ($row.model) { $map[$row.model] = $row }
}

function First-Number([string]$text, [double]$default = 0) {
    if ([string]::IsNullOrWhiteSpace($text)) { return $default }
    $m = [regex]::Match($text, '[0-9]+(?:\.[0-9]+)?')
    if ($m.Success) { return [double]$m.Value }
    return $default
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
    return 120
}

function Normalize-Mount([string]$text) {
    $t = $text.ToUpperInvariant()
    if ($t -match 'M58') { return "M58" }
    if ($t -match 'M42') { return "M42" }
    if ($t -match 'F-MOUNT|F MOUNT') { return "F" }
    if ($t -match 'C-MOUNT|C MOUNT|^C$') { return "C" }
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

function Sensor-DiagonalFromSpec([string]$text) {
    $t = Html-Decode $text
    $m = [regex]::Match($t, 'Full\(([0-9]+(?:\.[0-9]+)?)\)')
    if ($m.Success) { return [double]$m.Groups[1].Value }
    $m = [regex]::Match($t, '^\s*([0-9]+(?:\.[0-9]+)?)')
    if ($m.Success) { return [double]$m.Groups[1].Value }
    return Sensor-DiagonalMm $t
}

function Parse-WdPair([string]$text) {
    $t = Html-Decode $text
    $nominal = First-Number $t 0
    $tol = 0
    $m = [regex]::Match($t, '±\s*([0-9]+(?:\.[0-9]+)?)')
    if ($m.Success) { $tol = [double]$m.Groups[1].Value }
    return @($nominal, $tol)
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
    return $t -match '(?i)^\s*(是|有|yes|true|1)\s*$'
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
            if ($c.attr_name) { $m[$c.attr_name] = [string]$c.attr_value }
        }
    }
    return $m
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
        shutter_type = $shutter
        max_fps = $fps
        interface = $ifaceNorm
        bandwidth_mbps = (Interface-Bandwidth $iface)
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
        dof_mm = (First-Number $dof 0)
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
            Add-Camera $cameraMap $p["Product Model"] $r[0] $r[1] $pixel $sensor $color $shutter $fps $iface (First-Number $p["Bit depth"] 12) (First-Number $p["Dynamic range"] 0) $p["Lens mount"] "Hikrobot"
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
            Add-FixedLens $lensMap $p["Product Model"] (First-Number $p["Focal length"] 0) (First-Number $p["F-number"] 0) (First-Number $p["Image size"] 0) (First-Number $p["Type"] 0) $minWd (First-Number $p["Distortion"] 0) $p["Mount"] "Hikrobot FA lens from official product API" "Hikrobot"
        } catch {
            Write-Warning "Skipped Hikrobot lens id=$($rec.id): $($_.Exception.Message)"
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
            Add-Camera $cameraMap $detail.data.model $r[0] $r[1] $pixel $sensor $color $p["Shutter"] $fps $p["Port"] (First-Number $p["Bit Depth"] 12) (First-Number $p["Dynamic Range"] 0) $p["Lens Mount"] "iRAYPLE"
        } catch {
            Write-Warning "Skipped iRAYPLE camera id=$($rec.id): $($_.Exception.Message)"
        }
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
                $coolensRawRows += [pscustomobject]@{
                    source_page = $page.id
                    series = $page.series
                    model = (Html-Decode $rec.pl_title)
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
                if ($page.kind -eq "dtcm") {
                    Add-CoolensTeleLens $lensMap $rec $page.type $page.series $rec.text0 $rec.text2 $rec.text3 $rec.text4 $rec.text5 "" "" "" ""
                } elseif ($page.kind -eq "dtcmSmall") {
                    Add-CoolensTeleLens $lensMap $rec $page.type $page.series $rec.text0 $rec.text2 $rec.text3 "" $rec.text4 "" "" "" ""
                } elseif ($page.kind -eq "tele") {
                    Add-CoolensTeleLens $lensMap $rec $page.type $page.series $rec.text0 $rec.text1 $rec.text2 $rec.text3 "" $rec.text4 "" "" ""
                } elseif ($page.kind -eq "ltcm") {
                    Add-CoolensTeleLens $lensMap $rec $page.type $page.series $rec.text3 $rec.text1 $rec.text2 $rec.text4 "" "" $rec.text7 $rec.text8 $rec.text6
                } elseif ($page.kind -eq "mfa") {
                    Add-CoolensFixedLens $lensMap $rec $page.series
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
