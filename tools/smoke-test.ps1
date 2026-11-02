param(
    [Parameter(Mandatory = $true)][string]$ClientDir,
    [int]$Port = 8080,
    [int]$WaitSeconds = 25
)

$ErrorActionPreference = 'Stop'
$ClientDir = (Resolve-Path $ClientDir).Path

$required = @('MyVPN.exe', 'MyVPN-service.exe', 'platforms\qwindows.dll')
foreach ($item in $required) {
    if (-not (Test-Path (Join-Path $ClientDir $item))) {
        throw "Missing $item in $ClientDir"
    }
}
if (-not (Test-Path (Join-Path $ClientDir 'qml'))) {
    throw "Missing qml directory in $ClientDir"
}

$mockScript = Join-Path $PSScriptRoot 'mock-provider\server.py'
$python = (Get-Command python -ErrorAction Stop).Source
$mock = Start-Process -FilePath $python -ArgumentList @($mockScript, '--port', $Port) -PassThru -WindowStyle Hidden
$app = $null
try {
    Start-Sleep -Seconds 2
    $env:PROVIDER_API_URL = "http://127.0.0.1:$Port"
    $app = Start-Process -FilePath (Join-Path $ClientDir 'MyVPN.exe') -WorkingDirectory $ClientDir -PassThru
    Start-Sleep -Seconds $WaitSeconds
    if ($app.HasExited) {
        throw "MyVPN exited with code $($app.ExitCode)"
    }

    $app.Refresh()
    if ([string]::IsNullOrWhiteSpace($app.MainWindowTitle)) {
        throw 'MyVPN window is not visible'
    }
    "Window: $($app.MainWindowTitle)"

    $key = 'HKCU:\Software\MyVPN.ORG\MyVPN'
    $raw = (Get-ItemProperty -Path $key -Name 'Conf/providerServerMap' -ErrorAction Stop).'Conf/providerServerMap'
    $mapping = $raw | ConvertFrom-Json
    $names = @($mapping.PSObject.Properties.Name)
    if ($names.Count -eq 0) {
        throw 'Provider server map is empty'
    }
    "Imported servers: $($names -join ', ')"
}
finally {
    if ($app -and -not $app.HasExited) {
        Stop-Process -Id $app.Id -Force
    }
    if ($mock -and -not $mock.HasExited) {
        Stop-Process -Id $mock.Id -Force
    }
}
