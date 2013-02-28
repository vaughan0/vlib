
local enables = {}

local function applyOptions(options)
  if options.links then
    links(options.links)
  end
end

for feature, options in pairs(features) do
  newoption {
    trigger = 'with-'..feature,
    description = 'Enable '..feature..' support',
  }
  if _OPTIONS['with-'..feature] then
    enables[feature] = true
    applyOptions(options)
  end
end

local fout = io.open('include/config.h', 'w')
fout:write[[
// This file is generated by premake4.lua, so don't expect any changes to last.

#ifndef VLIB_CONFIG_H
#define VLIB_CONFIG_H

]]
for feature, _ in pairs(enables) do
  fout:write('#define VLIB_ENABLE_'..feature:upper()..'\n')
end
fout:write("\n#endif\n")
fout:close()
