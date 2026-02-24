$ErrorActionPreference = "Stop"

$dx11Path = "docs/reports/dx11-dx12-decision.md"
$coopPath = "docs/reports/gpu-coop-decision.md"
$e2ePath = "docs/reports/exedit2-e2e-status.md"
$outPath = "docs/reports/remake-remaining-tasks.md"

if (-not (Test-Path "docs/reports")) {
    New-Item -ItemType Directory -Path "docs/reports" | Out-Null
}

$dx11Gate = "UNKNOWN"
$dx11Next = "N/A"
$coopGate = "UNKNOWN"
$coopNext = "N/A"
$e2eGate = "UNKNOWN"
$e2eRecords = "N/A"
$e2eReason = "N/A"

if (Test-Path $dx11Path) {
    foreach ($line in Get-Content $dx11Path) {
        if ($line -match "^- adoption_gate: (.+)$") {
            $dx11Gate = $Matches[1].Trim()
        }
        if ($line -match "^- next_dx12_reevaluation: (.+)$") {
            $dx11Next = $Matches[1].Trim()
        }
    }
}

if (Test-Path $coopPath) {
    foreach ($line in Get-Content $coopPath) {
        if ($line -match "^- adoption_gate: (.+)$") {
            $coopGate = $Matches[1].Trim()
        }
        if ($line -match "^- next_gpu_coop_reevaluation: (.+)$") {
            $coopNext = $Matches[1].Trim()
        }
    }
}

if (Test-Path $e2ePath) {
    foreach ($line in Get-Content $e2ePath) {
        if ($line -match "^- gate: (.+)$") {
            $e2eGate = $Matches[1].Trim()
        }
        if ($line -match "^- records: (.+)$") {
            $e2eRecords = $Matches[1].Trim()
        }
        if ($line -match "^- gate_reason: (.+)$") {
            $e2eReason = $Matches[1].Trim()
        }
    }
}

$remaining = @()
if ($e2eGate -ne "PASS") {
    $remaining += "ExEdit2 E2E record is missing. Add host validation results via scripts/append_exedit2_e2e_result.cmd."
}
if ($dx11Gate -ne "PASS") {
    $remaining += "DX11/DX12 adoption gate is FAIL. Continue benchmark/quality monitoring and keep DX11 default until criteria are satisfied."
}
if ($coopGate -ne "PASS") {
    $remaining += "GPU coop adoption gate is FAIL. Improve ratio and async/single metrics to satisfy thresholds."
}
$remaining += "Continue Task 6: optimize GPU side implementations for Fast/Temporal variants."
$remaining += "Keep accumulating ExEdit2 host E2E evidence for release decision."

$today = Get-Date -Format "yyyy-MM-dd"
$lines = @()
$lines += "# ExEdit2 Remake Remaining Tasks"
$lines += ""
$lines += "- generated_on: $today"
$lines += "- dx11_dx12_adoption_gate: $dx11Gate"
$lines += "- dx11_dx12_next_reevaluation: $dx11Next"
$lines += "- gpu_coop_adoption_gate: $coopGate"
$lines += "- gpu_coop_next_reevaluation: $coopNext"
$lines += "- exedit2_e2e_gate: $e2eGate"
$lines += "- exedit2_e2e_records: $e2eRecords"
$lines += "- exedit2_e2e_reason: $e2eReason"
$lines += ""
$lines += "## Remaining Tasks"

$index = 1
foreach ($item in $remaining) {
    $lines += "$index. $item"
    $index++
}

$lines | Set-Content -Encoding utf8 $outPath
Write-Host "[generate_remake_remaining_tasks_report] generated $outPath"
