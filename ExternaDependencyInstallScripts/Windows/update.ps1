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