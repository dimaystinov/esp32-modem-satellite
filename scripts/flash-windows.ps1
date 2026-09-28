$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if (-not [Environment]::Is64BitOperatingSystem) { throw '64-bit Windows is required.' }
    $version = '1.5.1'
    $toolsDir = Join-Path $root '.tools'
    $cli = Join-Path $toolsDir 'arduino-cli.exe'
    if (-not (Test-Path $cli)) {
        New-Item -ItemType Directory -Force $toolsDir | Out-Null
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $asset = "arduino-cli_${version}_Windows_64bit.zip"
        $base = "https://github.com/arduino/arduino-cli/releases/download/v$version"
        $archive = Join-Path $toolsDir $asset
        Write-Host 'Downloading Arduino CLI...'
        Invoke-WebRequest "$base/$asset" -OutFile $archive -UseBasicParsing
        $checks = (Invoke-WebRequest "$base/$version-checksums.txt" -UseBasicParsing).Content
        $line = @($checks -split "`n" | Where-Object { $_.Trim().EndsWith($asset) })
        if ($line.Count -ne 1) { throw 'Missing download checksum.' }
        $expected = ($line[0].Trim() -split '\s+')[0]
        if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne $expected) { throw 'Download checksum mismatch.' }
        Expand-Archive -Path $archive -DestinationPath $toolsDir -Force
        Remove-Item $archive
    }
    function Invoke-Arduino {
        & $cli @args
        if ($LASTEXITCODE -ne 0) { throw "Arduino CLI failed: $($args -join ' ')" }
    }
    $cores = Invoke-Arduino core list --format json | ConvertFrom-Json
    $installed = @($cores.platforms | Where-Object { $_.id -eq 'esp32:esp32' -and $_.installed_version -eq '3.3.11' })
    if ($installed.Count -eq 0) {
        Write-Host 'Installing ESP32 core 3.3.11 (first run may take several minutes)...'
        $index = 'https://espressif.github.io/arduino-esp32/package_esp32_index.json'
        Invoke-Arduino core update-index --additional-urls $index
        Invoke-Arduino core install esp32:esp32@3.3.11 --additional-urls $index
    }
    Invoke-Arduino lib install ArduinoJson@6.21.5 U8g2@2.36.17
    $revision = 'ce3aedef37c74c4c95111cf2a3f112a2dbd7e99e'
    if (-not (Test-Path '.deps/MAVLink/.ready')) {
        New-Item -ItemType Directory -Force '.deps/MAVLink' | Out-Null
        Invoke-WebRequest "https://github.com/mavlink/c_library_v2/archive/$revision.zip" -OutFile '.deps/mavlink.zip' -UseBasicParsing
        Expand-Archive '.deps/mavlink.zip' '.deps' -Force
        Copy-Item ".deps/c_library_v2-$revision/*" '.deps/MAVLink' -Recurse -Force
        Set-Content '.deps/MAVLink/MAVLink.h' "#pragma once`n#include `"common/mavlink.h`"" -Encoding ASCII
        Set-Content '.deps/MAVLink/library.properties' "name=MAVLinkC`nversion=2.0.0`nauthor=MAVLink`nmaintainer=MAVLink`nsentence=Generated MAVLink headers`nparagraph=Pinned official headers`ncategory=Communication`nurl=https://github.com/mavlink/c_library_v2`narchitectures=*" -Encoding ASCII
        New-Item -ItemType File '.deps/MAVLink/.ready' -Force | Out-Null
    }
    $assets = "#pragma once`nnamespace WebAssets {`n"
    $names = [ordered]@{'index.html'='INDEX_HTML'; 'style.css'='STYLE_CSS'; 'app.js'='APP_JS'}
    foreach ($name in $names.Keys) {
        $content = [IO.File]::ReadAllText((Join-Path $root "firmware/modem_c3/data/$name"))
        if ($content.Contains(')WEB"')) { throw 'Reserved raw string delimiter in web asset.' }
        $assets += 'static const char ' + $names[$name] + '[] = R"WEB(' + $content + ')WEB";' + "`n"
    }
    $assets += "}`n"
    [IO.File]::WriteAllText((Join-Path $root 'firmware/modem_c3/src/WebAssets.h'), $assets, (New-Object Text.UTF8Encoding($false)))
    $boards = Invoke-Arduino board list --format json | ConvertFrom-Json
    $ports = @($boards.detected_ports | Where-Object { $_.port.protocol -eq 'serial' -and $_.port.address -match '^COM[0-9]+$' })
    $esp = @($ports | Where-Object { $_.port.properties.vid -eq '0x303A' -and $_.port.properties.pid -eq '0x1001' })
    if ($esp.Count -eq 1) { $port = $esp[0].port.address }
    else {
        if ($ports.Count -eq 0) { throw 'No COM port found. Connect ESP using a data USB cable; try BOOT mode.' }
        $ports | ForEach-Object { Write-Host ($_.port.address + ' ' + $_.port.label) }
        $port = (Read-Host 'Enter the ESP COM port (for example COM5)').Trim().ToUpperInvariant()
        if ($port -notin @($ports | ForEach-Object { $_.port.address })) { throw 'Port is not in the detected list.' }
    }
    Write-Host "Building ESP32-C3 firmware and uploading to $port..."
    $fqbn = 'esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio,FlashSize=4M'
    Invoke-Arduino compile --libraries .deps --fqbn $fqbn --build-path build/cache/c3 --output-dir build/c3 firmware/modem_c3
    Invoke-Arduino upload --fqbn $fqbn --port $port --input-dir build/c3 firmware/modem_c3
    Write-Host 'SUCCESS. Wi-Fi: modem_bridge / 00000000, http://192.168.0.4, first 5 minutes.'
} catch {
    Write-Host "ERROR: $_" -ForegroundColor Red
    exit 1
} finally {
    Pop-Location
}
