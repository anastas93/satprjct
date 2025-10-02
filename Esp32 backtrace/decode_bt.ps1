$ErrorActionPreference = 'Stop'
$log = Join-Path $env:TEMP ("decode_bt_{0}_{1}.log" -f $PID,(Get-Date -Format 'yyyyMMdd_HHmmssfff'))
function Log($m){
  $ts = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
  $line = "$ts  $m"
  Write-Host $line
  try { Add-Content -Path $log -Value $line -Encoding UTF8 -ErrorAction Stop } catch { }
}

try {
  Log 'Step=Clipboard.Read'
  $bt = Get-Clipboard
  if (-not $bt) { throw 'Clipboard empty' }

  Log 'Step=Parse.Addresses'
  $addrs = ($bt -replace '\|<-CORRUPTED','' -replace 'Backtrace:\s*','') -split '\s+' |
           ForEach-Object { ($_ -split ':')[0] } |
           Where-Object { $_ -match '^0x[0-9a-fA-F]+$' } |
           Select-Object -Unique
  if (-not $addrs -or $addrs.Count -eq 0) { throw 'No addresses parsed' }
  Log ('Addrs=' + ($addrs -join ' '))

  Log 'Step=Find.ELF'
  $roots = @(
    "$env:LOCALAPPDATA\arduino\sketches",
    "$env:TEMP\arduino\sketches",
    "$env:LOCALAPPDATA\Temp\arduino\sketches",
    "C:\tmp\build"
  ) | Where-Object { Test-Path $_ }
  Log ('Roots=' + ($roots -join '; '))
  $elf = Get-ChildItem $roots -Recurse -Filter *.elf -ErrorAction SilentlyContinue |
         Sort-Object LastWriteTime -Desc | Select-Object -First 1 -ExpandProperty FullName
  if (-not $elf) { throw 'ELF not found' }
  Log ('ELF=' + $elf)

  Log 'Step=Find.Tools'
  $toolsRoot = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools"
  if (-not (Test-Path $toolsRoot)) { throw ('ESP32 tools not found: ' + $toolsRoot) }

  $addr2 = Get-ChildItem $toolsRoot -Recurse -ErrorAction SilentlyContinue |
           Where-Object { $_.Name -like '*addr2line*.exe' } |
           Select-Object -First 1 -ExpandProperty FullName

  if ($addr2) {
    Log ('Run addr2line=' + $addr2)
    & $addr2 -pfia -e $elf @addrs
    Log ('ExitCode.addr2line=' + $LASTEXITCODE)
    exit $LASTEXITCODE
  }

  $gdb = Get-ChildItem $toolsRoot -Recurse -ErrorAction SilentlyContinue |
         Where-Object { $_.Name -like 'xtensa-esp32-elf-gdb*.exe' -or $_.Name -like 'riscv32-esp-elf-gdb*.exe' } |
         Select-Object -First 1 -ExpandProperty FullName
  if (-not $gdb) { throw 'No gdb found' }
  Log ('Run gdb=' + $gdb)

  $scr = Join-Path $env:TEMP 'bt.gdb'
  "file `"$elf`"" | Set-Content -Encoding ASCII $scr
  "set pagination off" | Add-Content -Encoding ASCII $scr
  foreach ($a in $addrs) {
    "echo === $a ===" | Add-Content -Encoding ASCII $scr
    "echo `n"         | Add-Content -Encoding ASCII $scr
    "info line *$a"   | Add-Content -Encoding ASCII $scr
    "info symbol $a"  | Add-Content -Encoding ASCII $scr
  }
  & $gdb -batch -x $scr
  Log ('ExitCode.gdb=' + $LASTEXITCODE)
  exit $LASTEXITCODE
}
catch {
  Log ('ERROR: ' + $_.Exception.Message)
  if ($_.InvocationInfo) { Log ('AT: ' + $_.InvocationInfo.PositionMessage) }
  exit 1
}
