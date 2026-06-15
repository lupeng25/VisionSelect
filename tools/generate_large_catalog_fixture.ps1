param(
    [string]$OutputDir = "large_catalog_fixture",
    [int]$CameraCount = 20000,
    [int]$LensCount = 30000,
    [int]$LightCount = 50000
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

function New-Utf8Writer($path) {
    return [System.IO.StreamWriter]::new($path, $false, [System.Text.UTF8Encoding]::new($false))
}

$cameraPath = Join-Path $OutputDir "cameras.csv"
$writer = New-Utf8Writer $cameraPath
try {
    $writer.WriteLine('"model","manufacturer","resolution_x","resolution_y","pixel_size_um","sensor_format","color_mode","shutter_type","max_fps","interface","bandwidth_mbps","bit_depth","dynamic_range_db","lens_mount"')
    for ($i = 1; $i -le $CameraCount; $i++) {
        $resolutionX = 1280 + (($i % 8) * 512)
        $resolutionY = 1024 + (($i % 6) * 384)
        $pixel = @(2.4, 3.45, 4.8, 5.86)[$i % 4]
        $fps = 15 + ($i % 180)
        $interface = @("GigE", "USB3", "CoaXPress", "10GigE")[$i % 4]
        $mount = @("C", "M42", "F")[$i % 3]
        $writer.WriteLine(('"{0:D6}","LoadCam","{1}","{2}","{3}","{4} mm x {5} mm","{6}","Global","{7}","{8}","{9}","12","60","{10}"' -f $i, $resolutionX, $resolutionY, $pixel, [math]::Round($resolutionX * $pixel / 1000, 2), [math]::Round($resolutionY * $pixel / 1000, 2), (@("Mono", "Color")[$i % 2]), $fps, $interface, (120 + ($i % 6) * 200), $mount))
    }
}
finally {
    $writer.Dispose()
}

$lensPath = Join-Path $OutputDir "lenses.csv"
$writer = New-Utf8Writer $lensPath
try {
    $writer.WriteLine('"model","manufacturer","lens_type","lens_mount","focal_length_mm","min_wd_mm","distortion_percent","image_circle_mm","megapixel_rating","recommended_min_pixel_um","pmag","nominal_wd_mm","wd_tolerance_mm","max_sensor_diagonal_mm","telecentricity_deg","dof_mm","numerical_aperture","f_number","coaxial_illumination","notes"')
    for ($i = 1; $i -le $LensCount; $i++) {
        $isTelecentric = ($i % 3) -ne 0
        $type = if ($isTelecentric) { @("ObjectTelecentric", "BiTelecentric")[$i % 2] } else { "FixedFocal" }
        $focal = if ($isTelecentric) { 0 } else { @(8, 12, 16, 25, 35, 50)[$i % 6] }
        $pmag = if ($isTelecentric) { @(0.05, 0.1, 0.2, 0.3, 0.5, 1.0)[$i % 6] } else { 0 }
        $mount = @("C", "M42", "F")[$i % 3]
        $imageCircle = @(11, 16, 20, 30, 43)[$i % 5]
        $writer.WriteLine(('"{0:D6}","LoadLens","{1}","{2}","{3}","80","0.05","{4}","12","3.45","{5}","{6}","5","{4}","0.08","8","0","2.8","false","large fixture lens"' -f $i, $type, $mount, $focal, $imageCircle, $pmag, (80 + ($i % 8) * 20)))
    }
}
finally {
    $writer.Dispose()
}

$lightPath = Join-Path $OutputDir "lights.csv"
$writer = New-Utf8Writer $lightPath
try {
    $writer.WriteLine('"model","manufacturer","light_type","color","wavelength_nm","mode","active_width_mm","active_height_mm","best_for"')
    for ($i = 1; $i -le $LightCount; $i++) {
        $type = @("Backlight", "Ring", "Bar", "Coaxial", "Dome", "TelecentricBacklight")[$i % 6]
        $color = @("White", "Red", "Green", "Blue", "IR")[$i % 5]
        $wave = @(0, 630, 520, 470, 850)[$i % 5]
        $mode = @("Continuous", "Strobe")[$i % 2]
        $width = 20 + ($i % 80) * 5
        $height = 20 + ($i % 60) * 5
        $writer.WriteLine(('"{0:D6}","LoadLight","{1}","{2}","{3}","{4}","{5}","{6}","large fixture light"' -f $i, $type, $color, $wave, $mode, $width, $height))
    }
}
finally {
    $writer.Dispose()
}

Write-Host "Wrote fixture CSV files to $OutputDir"
