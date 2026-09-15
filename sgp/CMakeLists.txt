set(sgpSrc
"${CMAKE_CURRENT_SOURCE_DIR}/Button Sound Control.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Button System.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Compression.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/crash_telemetry.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Cursor Control.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DEBUG.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/crash_report.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DirectDraw Calls.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DirectX Common.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/English.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/FileMan.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/fileio/BfVfsResourceStore.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/fileio/DurableFileOperationsFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/fileio/FileServices.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/fileio/LocalTime.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/fileio/LogStore.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/fileio/PhysicalWritableStore.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/fileio/PlatformPaths.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/fileio/SaveTransaction.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/fileio/StoreRouter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Font.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/himage.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/impTGA.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/input.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/line.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/MemMan.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/mousesystem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/PCX.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/PngLoader.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Random.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/sgp_logger.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/shading.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/soundman.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/STCI.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/stringicmp.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/timer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/video.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject_blitters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vsurface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/WinFont.cpp"
)

if(JA2_PLATFORM_BACKEND STREQUAL "WINDOWS")
	list(APPEND sgpSrc
		"${CMAKE_CURRENT_SOURCE_DIR}/fileio/WindowsDurableFileOperations.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/Sleep.cpp")
elseif(JA2_PLATFORM_BACKEND STREQUAL "LINUX")
	list(APPEND sgpSrc "${CMAKE_CURRENT_SOURCE_DIR}/fileio/PosixDurableFileOperations.cpp")
endif()

set(sgpSrc ${sgpSrc} PARENT_SCOPE)
