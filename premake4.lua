
-- vars
local win_unixenv = false
local cygwin = false
local mingw = false
local clang_libcxx = false
local gcc_compat = false
local platform = "x32"

-- this function returns the first result of "find basepath -name filename", this is needed on some platforms to determine the include path of a library
function find_include(filename, base_path)
	if(os.is("windows") and not win_unixenv) then
		return ""
	end
	
	local proc = io.popen("find "..base_path.." -name \""..filename.."\"", "r")
	local path_names = proc:read("*a")
	proc:close()
	
	if(string.len(path_names) == 0) then
		return ""
	end
	
	local newline = string.find(path_names, "\n")
	if newline == nil then
		return ""
	end
	
	return string.sub(path_names, 0, newline-1)
end


-- actual premake info
solution "a2etools"
	configurations { "Release", "Debug" }

	-- scan args
	local argc = 1
	while(_ARGS[argc] ~= nil) do
		if(_ARGS[argc] == "--env") then
			argc=argc+1
			-- check if we are building with cygwin/mingw
			if(_ARGS[argc] ~= nil and _ARGS[argc] == "cygwin") then
				cygwin = true
				win_unixenv = true
			end
			if(_ARGS[argc] ~= nil and _ARGS[argc] == "mingw") then
				mingw = true
				win_unixenv = true
			end
		end
		if(_ARGS[argc] == "--clang") then
			clang_libcxx = true
		end
		if(_ARGS[argc] == "--gcc") then
			gcc_compat = true
		end
		if(_ARGS[argc] == "--platform") then
			argc=argc+1
			if(_ARGS[argc] ~= nil) then
				platform = _ARGS[argc]
			end
		end
		argc=argc+1
	end

	-- os specifics
	if(not os.is("windows") or win_unixenv) then
		if(not cygwin) then
			includedirs { "/usr/include" }
		else
			includedirs { "/usr/include/w32api", "/usr/include/w32api/GL" }
		end
		includedirs { "/usr/include/freetype2", "/usr/include/libxml2", "/usr/local/include", "/usr/include/a2elight", "/usr/local/include/a2elight" }
		buildoptions { "-Wall -x c++ -std=c++11 -Wno-trigraphs -Wreturn-type -Wunused-variable -funroll-loops" }
		libdirs { "/usr/local/lib" }

		if(clang_libcxx) then
			buildoptions { "-stdlib=libc++ -integrated-as" }
			buildoptions { "-Wno-delete-non-virtual-dtor -Wno-overloaded-virtual -Wunreachable-code -Wdangling-else" }
			linkoptions { "-fvisibility=default" }
			if(not win_unixenv) then
				linkoptions { "-stdlib=libc++" }
			else
				linkoptions { "-lc++.dll" }
			end
		end

		if(gcc_compat) then
			buildoptions { "-Wno-strict-aliasing" }
		end
	end
	
	if(win_unixenv) then
		-- only works with gnu++11 for now ...
		buildoptions { "-std=gnu++11" }
		defines { "WIN_UNIXENV" }
		if(cygwin) then
			defines { "CYGWIN" }
		end
		if(mingw) then
			defines { "__WINDOWS__", "MINGW" }
			includedirs { "/mingw/include" }
			libdirs { "/usr/lib", "/usr/local/lib" }
			buildoptions { "-Wno-unknown-pragmas" }
		end
	end
	
	if(os.is("linux") or os.is("bsd") or win_unixenv) then
		if(not win_unixenv) then
			if(clang_libcxx) then
				-- small linux workaround for now (linking will fail otherwise):
				linkoptions { "-lrt -lc -lstdcxx" }
			end
		end
		includedirs { "/usr/include/SDL2", "/usr/local/include/SDL2" }

		if(gcc_compat) then
			if(not mingw) then
				defines { "_GLIBCXX__PTHREADS" }
			end
			defines { "_GLIBCXX_USE_NANOSLEEP" }
		end
	end
	
	-- prefer system platform
	if(platform == "x64") then
		platforms { "x64", "x32" }
	else
		platforms { "x32", "x64" }
	end

	configuration { "x64" }
		defines { "PLATFORM_X64" }

	configuration { "x32" }
		defines { "PLATFORM_X86" }

project "obj2a2m"
	targetname "obj2a2m"
	kind "ConsoleApp"
	language "C++"
	files { "obj2a2m/**.h", "obj2a2m/**.cpp" }
	targetdir "bin"
	
	-- the same for all
	includedirs { "obj2a2m/" }
	
	-- configs
	configuration "Debug"
		targetname "obj2a2md"
		defines { "DEBUG", "A2E_DEBUG" }
		flags { "Symbols" }
		links { "a2elightd" }
		if(not os.is("windows") or win_unixenv) then
			buildoptions { " -gdwarf-2" }
		end

	configuration "Release"
		targetname "obj2a2m"
		defines { "NDEBUG" }
		flags { "Optimize" }
		links { "a2elight" }
		if(not os.is("windows") or win_unixenv) then
			buildoptions { "-ffast-math -Os" }
		end
