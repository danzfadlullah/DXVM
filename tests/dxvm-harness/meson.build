# DXVM Direct3D test harness
#
# A Windows console executable that reproduces the DXML test matrix against
# DXVM. Enabled with -Dharness=true (or auto, which enables it for Windows
# cross builds only).

# Windows targets use the mingw-w64 system headers (windows.h, d3d11.h,
# d3d10.h, d3d9.h, dxgi.h). include/native/* is DXVK's dxvk-native shim and
# would shadow them here, so it is deliberately not on this path.
dxvm_harness_inc = include_directories('src')

dxvm_harness_src = files(
  'src/Report.cpp',
  'src/Registry.cpp',
  'src/Child.cpp',
  'src/SelfTestScenes.cpp',
  'src/Device.cpp',
  'src/scenes/Msaa.cpp',
  'src/scenes/DeviceScenes.cpp',
  'src/scenes/MsaaScenes.cpp',
  'src/main.cpp',
)

# The harness is linked against the Direct3D import libraries. At runtime it
# picks up whichever d3d11.dll / dxgi.dll / d3d10core.dll / d3d9.dll sit next to
# the executable, which is how it runs against DXVM rather than the system DLLs.
#
# d3d10core is deliberately absent: mingw-w64 ships no libd3d10core.a, so the
# D3D10 suite loads d3d10core.dll with LoadLibrary/GetProcAddress instead.
dxvm_harness_libs = []
foreach name : [ 'd3d11', 'dxgi', 'd3d9', 'd3dcompiler' ]
  dxvm_harness_libs += [ '-l' + name ]
endforeach

dxvm_harness_exe = executable('dxvm-harness', dxvm_harness_src,
  include_directories : dxvm_harness_inc,
  cpp_args            : [ '-DNOMINMAX', '-D_WIN32_WINNT=0xa00' ],
  link_args           : dxvm_harness_libs,
  install             : false)