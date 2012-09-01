
solution 'vlib'
  configurations { 'debug', 'release' }

  -- Global config

  language 'C'
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
    includedirs 'src'
    files { 'src/*.c', 'include/*.h' }

