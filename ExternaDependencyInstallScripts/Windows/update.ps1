param(
    [string]$RootDir = ".",
    [string]$BuildType = "Debug",
    [string]$ToolchainFile = ""
)

# Get all subdirectories
$folders = @(
    "Catch2",
	"cpptrace",
    "libassert",
    "libsodium-cmake",
    "spdlog",
    "tracy",
    "tomlplusplus",
    "unordered_dense"
)

$buildPath = "build-ninja-$($BuildType.ToLower())"

$currentPath = Get-Location

# Normalise and set the working directory
$RootDir = Resolve-Path -Path $RootDir
Set-Location -Path $RootDir

foreach ($folder in $folders) {
	$buildDir = Join-Path -Path $folder -ChildPath $buildPath
    if (Test-Path -Path $buildDir) {
		Remove-Item $buildDir -Recurse -Force
	}
}

Write-Host "`n=== Processing tracy ===" -ForegroundColor Cyan
Set-Location -Path "tracy"
git pull

cmake `
    -G "Ninja" `
    -B "$buildPath" `
    "-DCMAKE_CXX_STANDARD=23" `
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON" `
    "-DCMAKE_CXX_EXTENSIONS=OFF" `
    "-DCMAKE_CXX_SCAN_FOR_MODULES=ON" `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"
# Build and install
cmake --build "$buildPath"
cmake --install "$buildPath"

Set-Location -Path $RootDir

Write-Host "`n=== Processing libsodium ===" -ForegroundColor Cyan
Set-Location -Path "libsodium-cmake"
git submodule update --init --recursive

cmake `
    -G "Ninja" `
    -B "$buildPath" `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"

cmake --build "$buildPath"
cmake --install "$buildPath"

Set-Location -Path $RootDir

Write-Host "`n=== Processing cpptrace ===" -ForegroundColor Cyan
Set-Location -Path "cpptrace"
git pull
cmake `
    -G "Ninja" `
    -B "$buildPath" `
    "-DCMAKE_CXX_STANDARD=23" `
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON" `
    "-DCMAKE_CXX_EXTENSIONS=OFF" `
    "-DCMAKE_CXX_SCAN_FOR_MODULES=ON" `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"`
    "-DCPPTRACE_POSITION_INDEPENDENT_CODE=OFF"`
    "-DCPPTRACE_STD_FORMAT=ON"
    "-DBUILD_SHARED_LIBS=OFF"

cmake --build "$buildPath"
cmake --install "$buildPath"

Set-Location -Path $RootDir

Write-Host "`n=== Processing tomlplusplus ===" -ForegroundColor Cyan
Set-Location -Path "tomlplusplus"
git pull

cmake `
    -G "Ninja" `
    -B "$buildPath" `
    "-DCMAKE_CXX_STANDARD=23" `
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON" `
    "-DCMAKE_CXX_EXTENSIONS=OFF" `
    "-DCMAKE_CXX_SCAN_FOR_MODULES=ON" `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile" `
    "-DTOMLPLUSPLUS_BUILD_MODULES=ON"

cmake --build "$buildPath"
cmake --install "$buildPath"

Set-Location -Path $RootDir

Write-Host "`n=== Processing spdlog ===" -ForegroundColor Cyan
Set-Location -Path "spdlog"
git pull

cmake `
    -G "Ninja" `
    -B "$buildPath" `
    "-DCMAKE_CXX_STANDARD=23" `
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON" `
    "-DCMAKE_CXX_EXTENSIONS=OFF" `
    "-DCMAKE_CXX_SCAN_FOR_MODULES=ON" `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"

cmake --build "$buildPath"
cmake --install "$buildPath"

Set-Location -Path $RootDir

Write-Host "`n=== Processing Catch2 ===" -ForegroundColor Cyan
Set-Location -Path "Catch2"
git pull

cmake `
    -G "Ninja" `
    -B "$buildPath" `
    "-DCMAKE_CXX_STANDARD=23" `
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON" `
    "-DCMAKE_CXX_EXTENSIONS=OFF" `
    "-DCMAKE_CXX_SCAN_FOR_MODULES=ON" `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"

cmake --build "$buildPath"
cmake --install "$buildPath"

Set-Location -Path $RootDir

Write-Host "`n=== Processing unordered_dense ===" -ForegroundColor Cyan
Set-Location -Path "unordered_dense"
git pull

cmake `
    -G "Ninja" `
    -B "$buildPath" `
    "-DCMAKE_CXX_STANDARD=23" `
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON" `
    "-DCMAKE_CXX_EXTENSIONS=OFF" `
    "-DCMAKE_CXX_SCAN_FOR_MODULES=ON" `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile" `
    "-DANKERL_ENABLE_MODULES=TRUE"

cmake --build "$buildPath"
cmake --install "$buildPath"

Set-Location -Path $RootDir

Write-Host "`n=== Processing libassert ===" -ForegroundColor Cyan
Set-Location -Path "libassert"
git pull

cmake `
    -G "Ninja" `
    -B "$buildPath" `
    "-DCMAKE_CXX_STANDARD=23" `
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON" `
    "-DCMAKE_CXX_EXTENSIONS=OFF" `
    "-DCMAKE_CXX_SCAN_FOR_MODULES=ON" `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile" `
    "-DLIBASSERT_USE_EXTERNAL_CPPTRACE=TRUE" `
    "-DLIBASSERT_STATIC_DEFINE=TRUE"

cmake --build "$buildPath"
cmake --install "$buildPath"

# Return to orginal calling location
Set-Location -Path $currentPath