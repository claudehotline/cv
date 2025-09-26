# Test VideoSourceManager API
Write-Host "Testing VideoSourceManager API on http://localhost:8081"

# Test 1: Get all sources
Write-Host "`n1. Getting all video sources..."
try {
    $response = Invoke-RestMethod -Uri "http://localhost:8081/api/sources" -Method Get
    Write-Host "Response: $($response | ConvertTo-Json -Depth 3)"
} catch {
    Write-Host "Error: $($_.Exception.Message)"
}

# Test 2: Get system info
Write-Host "`n2. Getting system info..."
try {
    $response = Invoke-RestMethod -Uri "http://localhost:8081/api/system/info" -Method Get
    Write-Host "Response: $($response | ConvertTo-Json -Depth 3)"
} catch {
    Write-Host "Error: $($_.Exception.Message)"
}

Write-Host "`nAPI test completed."