# DXVM Direct3D test harness
#
# A Windows console executable that reproduces the DXML test matrix against
# DXVM. Enabled with -Dharness=true (or auto, which enables it for Windows
# cross builds only).

dxvm_harness_inc = include_directories(
  'src',
  '../../include/native/directx',
  '../../include/native/windows',
)

dxvm_harness_src = files(
  'src/Report.cpp',
  'src/Registry.cpp',
  'src/main.cpp',
)

dxvm_harness_exe = executable('dxvm-harness', dxvm_harness_src,
  include_directories : dxvm_harness_inc,
  cpp_args            : [ '-DNOMINMAX', '-D_WIN32_WINNT=0xa00' ],
  install             : false)