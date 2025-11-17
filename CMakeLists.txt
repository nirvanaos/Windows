target_compile_options (Core PUBLIC -fms-extensions -fms-compatibility -fms-compatibility-version=19.44.35215)

target_compile_definitions (Core PUBLIC _MSC_FULL_VER=194435215
	_MSC_VER=1944
	_MSVC_LANG=__cplusplus
	_MSC_EXTENSIONS=1
)

if (${NIRVANA_TARGET_PLATFORM} STREQUAL "x64")
	target_compile_definitions (Core PUBLIC _M_AMD64=100 _M_X64=100)
elseif (${NIRVANA_TARGET_PLATFORM} STREQUAL "x86")
	target_compile_definitions (Core PUBLIC _M_IX86=600 _INTEGRAL_MAX_BITS=64)
elseif (${NIRVANA_TARGET_PLATFORM} STREQUAL  "arm")
	target_compile_definitions (Core PUBLIC _M_ARM=7)
elseif (${NIRVANA_TARGET_PLATFORM} STREQUAL  "arm64")
	target_compile_definitions (Core PUBLIC _M_ARM64=1)
endif ()

# Get Windows SDK and Visual Studio environment
execute_process (COMMAND powershell.exe -File "${CMAKE_CURRENT_SOURCE_DIR}/vsdevenv.ps1" ${NIRVANA_TARGET_PLATFORM}
	ENCODING UTF-8
	OUTPUT_VARIABLE vs_environment
)

string (JSON WindowsSdkDir GET ${vs_environment} WindowsSdkDir)
string (JSON WindowsSDKVersion GET ${vs_environment} WindowsSDKVersion)
string (JSON VCToolsInstallDir GET ${vs_environment} VCToolsInstallDir)

set (WIN_SDK_INC_DIR "${WindowsSdkDir}Include/${WindowsSDKVersion}")

target_include_directories (Port SYSTEM AFTER PRIVATE 
	"${VCToolsInstallDir}include"
	${WIN_SDK_INC_DIR}um
	${WIN_SDK_INC_DIR}shared
)

set (WIN_LIB_DIR "${WindowsSdkDir}Lib/${WindowsSDKVersion}um/${NIRVANA_TARGET_PLATFORM}")

target_link_libraries (Port PUBLIC
	${WIN_LIB_DIR}/AdvAPI32.Lib
	${WIN_LIB_DIR}/DbgHelp.Lib
	${WIN_LIB_DIR}/kernel32.Lib
	${WIN_LIB_DIR}/ntdll.lib
	${WIN_LIB_DIR}/OneCore.Lib
	${WIN_LIB_DIR}/User32.Lib
)

add_subdirectory (Source)
add_subdirectory (pe)