param()

$ErrorActionPreference = "Stop"

Write-Host "Manual alpha typing checklist:"
Write-Host "1. Start sidecar: .\build\Start-Sidecar.ps1"
Write-Host "2. Register TIP: .\build\Run-DevIME.ps1 -StartSidecar"
Write-Host "3. Enable EstraIME Alpha in Windows input settings"
Write-Host "4. Verify in Notepad:"
Write-Host "   - nihao -> first candidate should be the standard greeting"
Write-Host "   - Backspace updates composition"
Write-Host "   - Space commits current candidate"
Write-Host "   - 1..9 selects candidate by number"
Write-Host "   - PageUp/PageDown switches candidate page"
Write-Host "   - Up/Down moves selection inside current page"
Write-Host "   - Ctrl+Space toggles Chinese/English mode"
Write-Host "   - , . ; : ? ! map to Chinese punctuation"
Write-Host "5. Repeat in Edge/Chrome textarea and VS Code comments"
