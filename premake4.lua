
solution 'vlib'
  configurations { 'debug', 'release' }

  -- Global config

  language 'C'
  includedirs { 'include' }
  buildoptions { '-std=gnu99' }

  links { }

  configuration 'debug'
    defines { 'DEBUG' }
    flags { 'Symbols', 'ExtraWarnings' }

  configuration 'release'
    defines { 'NDEBUG' }
    flags { 'Optimize' }

  project 'vlib'
    kind 'StaticLib'
    files { 'src/*.c', 'include/*.h' }

