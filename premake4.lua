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
		files "kalahai.c"
		
		links { "Ws2_32" }