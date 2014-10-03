solution "kalahai"
	configurations { "Debug", "Release" }
	platforms { "x32", "x64" }
	
	location "build"
	targetdir "build"
	debugdir "build"
	
	configuration "Debug"
		defines { "DEBUG" }
		flags { "Symbols" }
	configuration "Release"
		defines "NDEBUG"
		flags "Optimize"
	configuration {}
	
	project "kalahai"
		kind "ConsoleApp"
		language "C"
		files { "kalahai.h", "kalahai.c", "kalahai_main.c" }
		
		links { "Ws2_32" }
	project "kalahai_tests"
		kind "ConsoleApp"
		language "C"
		files { "kalahai.h", "kalahai.c", "kalahai_test_main.c" }
		
		links { "Ws2_32" }