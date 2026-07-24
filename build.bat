@echo off
echo ==============================================
echo  Bygger NAM App (Standalone)
echo ==============================================

if exist build rmdir /s /q build
mkdir build
cd build

echo Konfigurerar CMake för Standalone-bygget...
cmake .. -DCMAKE_BUILD_TYPE=Release

echo.
echo Kompilerar...
cmake --build . --config Release --parallel

echo.
echo ==============================================
echo  Bygget är klart!
echo  Du hittar din standalone-app (NamAppClient.exe)
echo  inuti build-mappen (vanligtvis under Release-undermappen).
echo ==============================================
pause
