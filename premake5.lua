newoption {
    trigger     = "with-version",
    value       = "STRING",
    description = "Current version",
}

workspace "SniperGhostWarrior.FusionFix"
   configurations { "Release", "Debug" }
   architecture "x86"
   location "build"
   cppdialect "C++latest"
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"
   targetextension ".asi"
   buildoptions { "/dxifcInlineFunctions- /Zc:__cplusplus /utf-8" }
   staticruntime "On"
   characterset ("Unicode")
   multiprocessorcompile ("On")
   startproject "SniperGhostWarrior.FusionFix"

   defines { "rsc_CompanyName=\"SniperGhostWarrior.FusionFix\"" }
   defines { "rsc_LegalCopyright=\"MIT license\""}
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{cfg.buildtarget.name}\"" }
   defines { "rsc_FileDescription=\"Sniper: Ghost Warrior Fusion Fix\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/TGP482/SniperGhostWarrior.FusionFix\"" }

   local major = os.date("%d")
   local minor = os.date("%m")
   local build = os.date("%Y")
   local revision = os.date("%H") .. os.date("%M")

   if _OPTIONS["with-version"] then
      local t = {}
      for i in _OPTIONS["with-version"]:gmatch("([^.]+)") do
         t[#t + 1], _ = i:gsub("%D+", "")
      end
      while #t < 4 do t[#t + 1] = 0 end
      major    = math.min(tonumber(t[1]), 255)
      minor    = math.min(tonumber(t[2]), 255)
      build    = math.min(tonumber(t[3]), 65535)
      revision = math.min(tonumber(t[4]), 65535)
   end

   local githash = ""
   local f = io.popen("git rev-parse --short HEAD")
   if f then
      githash = f:read("*a"):gsub("%s+", "")
      f:close()
   end

   local productVersion = major .. "." .. minor .. "." .. build .. "." .. revision
   if githash ~= "" then
      productVersion = productVersion .. "-" .. githash
   end

   defines { "rsc_FileVersion_MAJOR=" .. major }
   defines { "rsc_FileVersion_MINOR=" .. minor }
   defines { "rsc_FileVersion_BUILD=" .. build }
   defines { "rsc_FileVersion_REVISION=" .. revision }
   defines { "rsc_FileVersion=\"" .. major .. "." .. minor .. "." .. build .. "\"" }
   defines { "rsc_ProductVersion=\"" .. productVersion .. "\"" }
   defines { "rsc_GitSHA1=\"" .. githash .. "\"" }
   defines { "rsc_GitSHA1W=L\"" .. githash .. "\"" }

   defines { "_CRT_SECURE_NO_WARNINGS" }

   includedirs { "source" }
   includedirs { "source/includes" }
   files { "source/*.h", "source/*.hpp", "source/*.cpp", "source/*.hxx", "source/*.ixx" }
   files { "source/resources/Versioninfo.rc" }

   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
   includedirs { "external/injector/safetyhook/include" }
   includedirs { "external/injector/zydis" }
   includedirs { "external/inireader" }
   files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
   files { "external/injector/safetyhook/include/safetyhook.hpp" }
   files {
      "external/injector/safetyhook/src/allocator.cpp",
      "external/injector/safetyhook/src/easy.cpp",
      "external/injector/safetyhook/src/inline_hook.cpp",
      "external/injector/safetyhook/src/mid_hook.cpp",
      "external/injector/safetyhook/src/os.windows.cpp",
      "external/injector/safetyhook/src/utility.cpp",
      "external/injector/safetyhook/src/vmt_hook.cpp",
   }
   files { "external/injector/zydis/Zydis.h", "external/injector/zydis/Zydis.c" }
   files { "data/bin/plugins/*.ini" }

   pbcommands = {
      "for %%P in (\"!SGWDIR!.\") do set \"SGWDIR=%%~fP\"",
      "for %%S in (\"$(TargetPath)\") do (set \"SGWSRC=%%~fS\" & set \"SGWNAME=%%~nxS\")",
      "set \"SGWDST=!SGWDIR!\\!SGWNAME!\"",
      "if not exist \"!SGWDIR!\\\" goto :SGWDONE",
      "if /I \"!SGWSRC!\"==\"!SGWDST!\" goto :SGWDONE",
      "copy /y \"!SGWSRC!\" \"!SGWDST!\" >nul",
      ":SGWDONE",
      "endlocal",
      "exit /b 0" }

   function setpaths (gamepath, exepath, pluginspath)
      pluginspath = pluginspath or "plugins/"
      if (gamepath) then
         local cmdcopy = {
            "setlocal EnableExtensions EnableDelayedExpansion",
            "set \"SGWDIR=" .. (gamepath .. pluginspath):gsub("([^/\\])$", "%1/") .. "\"",
         }
         for _, cmd in ipairs(pbcommands) do
            table.insert(cmdcopy, cmd)
         end
         postbuildcommands (cmdcopy)
         debugdir (gamepath)
         if (exepath) then
            debugcommand (gamepath .. exepath)
            dir, file = exepath:match'(.*/)(.*)'
            debugdir (gamepath .. (dir or ""))
         end
      end
      targetdir ("bin")
   end

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

project "SniperGhostWarrior.FusionFix"
   setpaths("C:/Program Files (x86)/Steam/steamapps/common/Sniper Ghost Warrior/", "SniperGhostWarrior.exe", "plugins/")
