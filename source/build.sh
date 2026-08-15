#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CLANG=${CLANG:-clang}
LLD=${LLD:-lld-link}
HOSTCC=${HOSTCC:-cc}
cd "$HERE"

# Convert the checked-in PTX device program into the C string header embedded by
# gpu_solver.c. This build step is implemented in C; Python is not required.
$HOSTCC -O2 embed_ptx.c -o embed_ptx_host
./embed_ptx_host gpu_kernel.ptx gpu_kernel_ptx.h
rm -f embed_ptx_host embed_ptx_host.exe

for n in kernel32 user32 gdi32; do
  $LLD /dll /noentry /machine:x64 /def:imports/$n.def /out:imports/${n}_stub.dll /implib:imports/$n.lib
  rm -f imports/${n}_stub.dll
 done

CFLAGS="-target x86_64-pc-windows-msvc -fshort-wchar -ffreestanding -fno-stack-protector -fno-builtin -nostdlibinc -O2"
$CLANG $CFLAGS -c own_solver.c -o own_solver.obj
$CLANG $CFLAGS -c gpu_solver.c -o gpu_solver.obj
$CLANG $CFLAGS -c traditional.c -o traditional.obj
$CLANG $CFLAGS -c main.c -o main.obj

RESARG=""
if [ -f app.res ]; then RESARG="app.res"; fi

$LLD /subsystem:windows /entry:WinMainCRTStartup /machine:x64 /nodefaultlib /opt:ref /opt:icf \
  /manifest:embed /manifestinput:app.manifest \
  main.obj own_solver.obj gpu_solver.obj traditional.obj $RESARG imports/kernel32.lib imports/user32.lib imports/gdi32.lib \
  /out:ZeroCandidatePrime.exe
