param(
    [Parameter(Mandatory=$true)][string]$FontPath,
    [Parameter(Mandatory=$true)][string]$OutPath
)

Add-Type -AssemblyName PresentationCore | Out-Null

if (-not (Test-Path $FontPath)) {
    Write-Error "Font not found: $FontPath"
    exit 1
}

$uri = New-Object System.Uri($FontPath)
$glyphTypeface = New-Object System.Windows.Media.GlyphTypeface($uri)
$charToGlyph = $glyphTypeface.CharacterToGlyphMap

# Define target scripts (top/common scripts)
$scripts = @(
    @{ Name='Latin'; Ranges=@(@(0x0020,0x007E), @(0x00A0,0x00FF), @(0x0100,0x017F), @(0x0180,0x024F), @(0x1E00,0x1EFF), @(0x0300,0x036F)) },
    @{ Name='Greek'; Ranges=@(@(0x0370,0x03FF)) },
    @{ Name='Cyrillic'; Ranges=@(@(0x0400,0x04FF)) },
    @{ Name='Arabic'; Ranges=@(@(0x0600,0x06FF), @(0x0750,0x077F), @(0x08A0,0x08FF), @(0xFB50,0xFDFF), @(0xFE70,0xFEFF)) },
    @{ Name='Devanagari'; Ranges=@(@(0x0900,0x097F)) },
    @{ Name='Bengali'; Ranges=@(@(0x0980,0x09FF)) },
    @{ Name='Hebrew'; Ranges=@(@(0x0590,0x05FF)) },
    @{ Name='Thai'; Ranges=@(@(0x0E00,0x0E7F)) },
    @{ Name='Hiragana'; Ranges=@(@(0x3040,0x309F)) },
    @{ Name='Katakana'; Ranges=@(@(0x30A0,0x30FF)) },
    @{ Name='Punctuation'; Ranges=@(@(0x2000,0x206F), @(0x20A0,0x20CF)) }
)

$allGlyphs = New-Object System.Collections.Generic.HashSet[int]
$report = @()

foreach ($s in $scripts) {
    $count = 0
    foreach ($pair in $s.Ranges) {
        $start = $pair[0]
        $end = $pair[1]
        for ($cp = $start; $cp -le $end; $cp++) {
            if ($charToGlyph.ContainsKey($cp)) {
                $gid = [int]$charToGlyph[$cp]
                if ($allGlyphs.Add($gid)) { }
                $count++
            }
        }
    }
    $report += [PSCustomObject]@{ Script=$s.Name; Covered=$count }
}

# Write glyphset (glyph indices, one per line)
New-Item -ItemType Directory -Force -Path (Split-Path $OutPath) | Out-Null
[IO.File]::WriteAllLines($OutPath, ($allGlyphs | Sort-Object | ForEach-Object { $_.ToString() }))

# Write report next to glyphset
$reportPath = [IO.Path]::ChangeExtension($OutPath, '.report.txt')
$lines = @("Script coverage for $FontPath") + ($report | Sort-Object Script | ForEach-Object { "{0,-12} : {1}" -f $_.Script, $_.Covered })
[IO.File]::WriteAllLines($reportPath, $lines)

Write-Host "Glyphs written:" $allGlyphs.Count
foreach ($r in $report) { Write-Host ($r.Script + ': ' + $r.Covered) }
