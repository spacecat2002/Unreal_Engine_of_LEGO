@echo off

REM Change CONFIG to Debug to get debuggable shader conductor
set CONFIG=RelWithDebInfo
REM set CONFIG=Debug



if exist ShaderConductor\lib\Win64 goto Continue
echo 
echo ************************************
echo *** Creating ShaderConductor\lib\Win64...
mkdir ShaderConductor\lib
mkdir ShaderConductor\lib\Win64

:Continue
set ENGINE_THIRD_PARTY_BIN=..\..\Binaries\ThirdParty\ShaderConductor\Win64
set ENGINE_THIRD_PARTY_SOURCE=..\..\Source\ThirdParty\ShaderConductor


echo 
echo ************************************
echo *** Checking out files...
pushd ..\%ENGINE_THIRD_PARTY_BIN%
	p4 edit ..\%THIRD_PARTY_CHANGELIST% ./...
popd

pushd ShaderConductor\lib\Win64
	p4 edit ..\%THIRD_PARTY_CHANGELIST% ./...
popd

mkdir ..\..\..\Intermediate\ShaderConductor
pushd ..\..\..\Intermediate\ShaderConductor
	echo 
	echo ************************************
	echo *** CMake
	cmake -G "Visual Studio 15" -T host=x64 -A x64 %ENGINE_THIRD_PARTY_SOURCE%\ShaderConductor

	echo 
	echo ************************************
	echo *** MSBuild
	"%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\MSBuild\15.0\Bin\MSbuild.exe" ALL_BUILD.vcxproj /nologo /v:m /p:Platform=x64;Configuration="%CONFIG%"
	
	echo 
	echo ************************************
	echo *** Copying to final destination
	REM Currently renamed to dxcompiler_sc so we know it came from ShaderConductor (as opposed to the Win SDK)
 	xcopy Bin\%CONFIG%\dxcompiler.dll       %ENGINE_THIRD_PARTY_BIN%\dxcompiler_sc.dll  /F /Y
 	xcopy Bin\%CONFIG%\ShaderConductor.dll  %ENGINE_THIRD_PARTY_BIN%\ShaderConductor.dll  /F /Y
 	xcopy Bin\%CONFIG%\ShaderConductor.pdb  %ENGINE_THIRD_PARTY_BIN%\ShaderConductor.pdb  /F /Y
	xcopy Lib\%CONFIG%\ShaderConductor.lib  %ENGINE_THIRD_PARTY_SOURCE%\ShaderConductor\lib\Win64 /F /Y
popd

:Done
