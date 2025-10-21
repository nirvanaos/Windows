# Get Windows SDK and Visual Studio environment
execute_process (COMMAND powershell.exe -File "${CMAKE_CURRENT_SOURCE_DIR}/vsdevenv.ps1"
	ENCODING UTF-8
	OUTPUT_VARIABLE vs_environment
)

string (JSON WIN_SDK_INC_DIR GET ${vs_environment} WIN_SDK_INC_DIR)
string (JSON MSVC_INC_DIR GET ${vs_environment} MSVC_INC_DIR)

add_library (Port STATIC)
target_link_libraries (Port PUBLIC Core)

target_include_directories (Port SYSTEM AFTER PRIVATE 
	${MSVC_INC_DIR}
	${WIN_SDK_INC_DIR}um
	${WIN_SDK_INC_DIR}shared
)

target_compile_options (Port PRIVATE -fms-extensions -fms-compatibility -fms-compatibility-version=19.44.35215)
target_compile_definitions (Port PRIVATE NIRVANA_HOST_PLATFORM=${NIRVANA_HOST_PLATFORM})

target_compile_definitions (Port PRIVATE _MSC_FULL_VER=194435215
	_MSC_VER=1944
	_MSVC_LANG=__cplusplus
	_MSC_EXTENSIONS=1
)

if (${NIRVANA_TARGET_PLATFORM} STREQUAL "x64")
  target_compile_definitions (Port PRIVATE _M_AMD64=100 _M_X64=100)
elseif (${NIRVANA_TARGET_PLATFORM} STREQUAL "x86")
  target_compile_definitions (Port PRIVATE _M_IX86=600 _INTEGRAL_MAX_BITS=64)
elseif (${NIRVANA_TARGET_PLATFORM} STREQUAL  "arm")
  target_compile_definitions (Port PRIVATE _M_ARM=7)
elseif (${NIRVANA_TARGET_PLATFORM} STREQUAL  "arm64")
  target_compile_definitions (Port PRIVATE _M_ARM64=1)
endif ()

add_subdirectory (Source)

