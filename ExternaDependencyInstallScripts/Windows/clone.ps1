param(
	[string]$RootDir = "."
)

# List of git repository URLs
$RepoUrls = @(
    "https://github.com/catchorg/Catch2.git",
	"https://github.com/jeremy-rifkin/cpptrace.git",
    "https://github.com/jeremy-rifkin/libassert.git",
    "https://github.com/shelltitan/libsodium-cmake.git",
    "https://github.com/gabime/spdlog.git",
    "https://github.com/wolfpld/tracy.git",
    "https://github.com/marzer/tomlplusplus.git",
    "https://github.com/martinus/unordered_dense.git"
)

# Normalise and set the working directory
$RootDir = Resolve-Path -Path $RootDir
Set-Location -Path $RootDir

foreach ($url in $RepoUrls) {
    # Extract repo name from the URL (e.g., 'Catch2' from '.../Catch2.git')
    $repoName = ($url -split '/' | Select-Object -Last 1) -replace '\.git$', ''

    if (Test-Path $repoName) {
        Write-Host "Directory '$repoName' already exists. Skipping..." -ForegroundColor Yellow
        continue
    }

    Write-Host "Cloning $url into folder '$repoName'..." -ForegroundColor Cyan
    git clone $url $repoName
}