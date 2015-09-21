
if (-not $config) { $config='Release' }

$options = "/p:configuration=$config"

#msbuild $options banglib.vcxproj
#msbuild $options arraylib.vcxproj
#msbuild $options hashlib.vcxproj
msbuild $options bangall.vcxproj
msbuild $options bang.vcxproj
msbuild $options mathlib.vcxproj
msbuild $options stringlib.vcxproj
msbuild $options iolib.vcxproj

