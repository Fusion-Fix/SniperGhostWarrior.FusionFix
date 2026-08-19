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
   buildoptions { "/dxifcInlineFunctions- /Zc:__cplusplus /utf-8" }
   staticruntime "On"
   multiprocessorcompile ("On")
   startproject "SniperGhostWarrior.FusionFix"

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

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

   filter {}

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

   function setpaths (gamepath, exepath, scriptspath)
      scriptspath = scriptspath or "plugins/"
      if (gamepath) then
         local cmdcopy = {
            "setlocal EnableExtensions EnableDelayedExpansion",
            "set \"SGWDIR=" .. (gamepath .. scriptspath):gsub("([^/\\])$", "%1/") .. "\"",
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

project "SniperGhostWarrior.FusionFix"
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"
   targetextension ".asi"
   characterset ("Unicode")

   defines { "rsc_CompanyName=\"SniperGhostWarrior.FusionFix\"" }
   defines { "rsc_LegalCopyright=\"MIT\""}
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{cfg.buildtarget.name}\"" }
   defines { "rsc_FileDescription=\"Sniper: Ghost Warrior Fusion Fix\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/Fusion-Fix/SniperGhostWarrior.FusionFix\"" }
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
   files { "source/**.h", "source/**.hpp", "source/**.cpp", "source/**.hxx", "source/**.ixx" }
   files { "source/resources/Versioninfo.rc" }
   files { "data/bin/plugins/*.ini" }

   -- ##BEGIN_EXTERNAL_SUBMODULES## (managed by setup.py - do not edit this line)
   includedirs { "external/injector/include" }
   includedirs { "external/injector/safetyhook/include" }
   includedirs { "external/injector/zydis" }
   files { "external/injector/safetyhook/include/**.hpp", "external/injector/safetyhook/src/**.cpp" }
   files { "external/injector/zydis/**.h", "external/injector/zydis/**.c" }
   includedirs { "external/hooking" }
   files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
   includedirs { "external/inireader" }
   -- ##END_EXTERNAL_SUBMODULES## (managed by setup.py - do not edit this line)

   -- Set game install path here for local debugging (not committed):
   setpaths("C:/Program Files (x86)/Steam/steamapps/common/Sniper Ghost Warrior/", "Sniper_x86.exe", "plugins/")
