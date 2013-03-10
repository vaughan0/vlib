
solution 'vlib'

  features = {
    gdbm = {links = 'gdbm'},
    zmq = {links = 'zmq'},
  }
  dofile 'features.lua'

  configurations { 'debug', 'release' }

  -- Global config

  language 'C'
  buildoptions { '-std=gnu99', '-fPIC', '-Wno-trampolines', '-Werror' }
  includedirs '.'

  links { 'm', 'pthread', 'rt' }

  -- Configurations

  configuration 'debug'
    defines { 'DEBUG' }
    flags { 'Symbols', 'ExtraWarnings' }

  configuration 'release'
    defines { 'NDEBUG' }
    flags { 'Optimize' }

  -- Projects

  project 'vlib'
    kind 'StaticLib'
    files { 'src/*.c', 'include/*.h' }

  project 'test'
    kind 'ConsoleApp'
    links 'vlib'
    includedirs 'src'
    targetdir 'test'
    targetname 'run'
    files { 'test/*.c' }
