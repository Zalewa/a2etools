
-- vars
local win_unixenv = false
local cygwin = false
local mingw = false
local clang_libcxx = false
local gcc_compat = false
local cuda = false
local platform = "x32"
local system_includes = ""

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

function add_include(path)
	system_includes = system_includes.." -isystem "..path
end


-- actual premake info
solution "a2etools"
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
		if(_ARGS[argc] == "--cuda") then
			cuda = true
		end
		argc=argc+1
	end
	
	configurations { "Release", "Debug" }

	configuration "Debug"
		links { "a2elightd" }
	
	configuration "Release"
		links { "a2elight" }

	-- os specifics
	if(not os.is("windows") or win_unixenv) then
		if(not cygwin) then
			add_include("/usr/include")
		else
			add_include("/usr/include/w32api")
			add_include("/usr/include/w32api/GL")
		end
		add_include("/usr/local/include")
		add_include("/usr/include/libxml2")
		add_include("/usr/include/a2elight")
		add_include("/usr/local/include/a2elight")
		add_include("/usr/include/freetype2")
		add_include("/usr/local/include/freetype2")
		
		buildoptions { "-Wall -x c++ -std=c++11 -Wno-trigraphs -Wreturn-type -Wunused-variable -funroll-loops" }
		libdirs { "/usr/local/lib" }

		if(clang_libcxx) then
			buildoptions { "-stdlib=libc++ -integrated-as" }
			buildoptions { "-Weverything" }
			buildoptions { "-Wno-unknown-warning-option" }
			buildoptions { "-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-header-hygiene -Wno-gnu -Wno-float-equal" }
			buildoptions { "-Wno-documentation -Wno-system-headers -Wno-global-constructors -Wno-padded -Wno-packed" }
			buildoptions { "-Wno-switch-enum -Wno-sign-conversion -Wno-conversion -Wno-exit-time-destructors" }
			buildoptions { "-Wno-unused-member-function -Wno-four-char-constants" }
			linkoptions { "-fvisibility=default" }
			if(not win_unixenv) then
				defines { "A2E_EXPORT=1" }
				linkoptions { "-stdlib=libc++" }
			else
				-- must be done before the next link options
				linkoptions { "`sdl2-config --libs`" }

				-- link against everything that libc++ links to, now that there are no default libs
				linkoptions { "-lsupc++ -lpthread -lmingw32 -lgcc_s -lgcc -lmoldname -lmingwex -lmsvcr100 -ladvapi32 -lshell32 -luser32 -lkernel32 -lmingw32 -lgcc_s -lgcc -lmoldname -lmingwex -lmsvcrt" }
				linkoptions { "-nodefaultlibs -stdlib=libc++ -lc++.dll" }
				add_include("/usr/include/c++/v1")
			end
		end

		if(gcc_compat) then
			buildoptions { "-Wno-strict-aliasing -Wno-multichar" }
		end

		if(cuda) then
			add_include("/usr/local/cuda/include")
			add_include("/usr/local/cuda-5.0/include")
			defines { "A2E_CUDA_CL=1" }
			if(platform == "x64") then
				libdirs { "/opt/cuda-toolkit/lib64", "/usr/local/cuda/lib64", "/usr/local/cuda-5.0/lib64" }
			else
				libdirs { "/opt/cuda-toolkit/lib", "/usr/local/cuda/lib", "/usr/local/cuda-5.0/lib" }
			end
			links { "cuda", "cudart" }
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
			add_include("/mingw/include")
			libdirs { "/usr/lib", "/usr/local/lib" }
			buildoptions { "-Wno-unknown-pragmas" }
		end
	end
	
	if(os.is("linux") or os.is("bsd") or win_unixenv) then
		add_include("/usr/include/SDL2")
		add_include("/usr/local/include/SDL2")
		-- set system includes
		buildoptions { system_includes }
		
		if(not win_unixenv) then
			if(clang_libcxx) then
				-- small linux workaround for now (linking will fail otherwise):
				linkoptions { "-lrt -lc -lstdcxx" }
			end
		elseif(mingw and not clang_libcxx) then
			linkoptions { "`sdl2-config --libs | sed -E 's/(-mwindows)//g'`" }
		end

		if(gcc_compat) then
			if(not mingw) then
				defines { "_GLIBCXX__PTHREADS" }
			end
			defines { "_GLIBCXX_USE_NANOSLEEP", "_GLIBCXX_USE_SCHED_YIELD" }
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
		if(not os.is("windows") or win_unixenv) then
			buildoptions { " -gdwarf-2" }
		end

	configuration "Release"
		targetname "obj2a2m"
		defines { "NDEBUG" }
		flags { "Optimize" }
		if(not os.is("windows") or win_unixenv) then
			buildoptions { "-ffast-math -Os" }
		end

project "kernelcacher"
	targetname "kernelcacher"
	kind "ConsoleApp"
	language "C++"
	files { "kernelcacher/**.h", "kernelcacher/**.cpp" }
	targetdir "bin"
	
	-- the same for all
	includedirs { "kernelcacher/" }
	
	-- configs
	configuration "Debug"
		targetname "kernelcacherd"
		defines { "DEBUG", "A2E_DEBUG", "A2E_CUDA_CL" }
		flags { "Symbols" }
		links { "z", "SDL2" }
		if(not os.is("windows") or win_unixenv) then
			buildoptions { " -gdwarf-2" }
		end

	configuration "Release"
		targetname "kernelcacher"
		defines { "NDEBUG", "A2E_CUDA_CL" }
		flags { "Optimize" }
		links { "z", "SDL2" }
		if(not os.is("windows") or win_unixenv) then
			buildoptions { "-ffast-math -Os" }
		end