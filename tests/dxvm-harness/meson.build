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
  'src/main.cpp',
)

dxvm_harness_exe = executable('dxvm-harness', dxvm_harness_src,
  include_directories : dxvm_harness_inc,
  cpp_args            : [ '-DNOMINMAX', '-D_WIN32_WINNT=0xa00' ],
  install             : false)