# readserial.ps1 — capture a board's boot banner over native USB-Serial-JTAG.
#
#   .\tools\readserial.ps1 COM9
#
# Why this exists.  On native USB, Arduino's Serial is HWCDCSerial, which
# discards output when no host is attached.  After a reset the USB device drops
# off the bus and re-enumerates, and that takes longer than the delay(400) in
# setup() — so a monitor opened right after upload reliably misses the banner.
# Sketches that only print on an event (voice_rx, step1_ping_rx) then look
# completely dead when they are in fact healthy.
#
# This resets the board FIRST and only then opens the port, so setup() runs
# with a host already waiting.
#
# Note: Windows serial access is exclusive.  Close any PlatformIO monitor on
# the port first or this fails with "Access to the path 'COMx' is denied".

param(
    [string] $PortName = 'COM9',
    [int]    $Seconds  = 14
)

function Wait-Port($name, $timeoutSec) {
    $end = (Get-Date).AddSeconds($timeoutSec)
    while ((Get-Date) -lt $end) {
        if ([System.IO.Ports.SerialPort]::GetPortNames() -contains $name) { return $true }
        Start-Sleep -Milliseconds 200
    }
    return $false
}

# --- pulse EN to restart the sketch from the top -------------------------
# RTS -> EN (reset), DTR -> IO0 (boot mode).  DTR must stay deasserted or the
# chip comes up in the ROM download stub instead of running the firmware.
try {
    $p = New-Object System.IO.Ports.SerialPort $PortName, 115200, 'None', 8, 'One'
    $p.DtrEnable = $false
    $p.Open()
    $p.RtsEnable = $true
    Start-Sleep -Milliseconds 120
    $p.RtsEnable = $false
    $p.Close()
} catch {
    "note: reset pulse failed ($($_.Exception.Message)) - reading whatever is already streaming"
}

# Resetting drops the USB device, so wait for it to enumerate again.
Start-Sleep -Milliseconds 300
if (-not (Wait-Port $PortName 12)) { "ERROR: $PortName never came back after reset"; exit 1 }

$sp = New-Object System.IO.Ports.SerialPort $PortName, 115200, 'None', 8, 'One'
$sp.DtrEnable = $true      # HWCDC only emits once it sees a host attached
$sp.RtsEnable = $false
$sp.ReadTimeout = 500
$sp.Open()

$deadline = (Get-Date).AddSeconds($Seconds)
$sb = New-Object System.Text.StringBuilder
while ((Get-Date) -lt $deadline) {
    try   { [void]$sb.Append($sp.ReadExisting()) } catch { }
    Start-Sleep -Milliseconds 150
}
$sp.Close()

$text = $sb.ToString()
if ([string]::IsNullOrWhiteSpace($text)) { "(no serial data received)" } else { $text.TrimEnd() }
