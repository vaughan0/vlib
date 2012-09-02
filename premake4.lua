
solution 'vlib'
  configurations { 'debug', 'release' }

  -- Global config

  language 'C'
  buildoptions { '-std=gnu99' }
  includedirs '.'

  configuration 'debug'
    defines { 'DEBUG' }
    flags { 'Symbols', 'ExtraWarnings' }

  configuration 'release'
    defines { 'NDEBUG' }
    flags { 'Optimize' }

  project 'vlib'
    kind 'StaticLib'
    files { 'src/*.c', 'include/*.h' }

  project 'test'
    kind 'ConsoleApp'
    links 'vlib'
    includedirs 'src'
    targetdir 'test'
    targetname 'run'
    files { 'test/main.c' }


