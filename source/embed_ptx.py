from pathlib import Path
src=Path('gpu_kernel.ptx')
out=Path('gpu_kernel_ptx.h')
text=src.read_text()
with out.open('w', newline='\n') as f:
    f.write('#ifndef GPU_KERNEL_PTX_H\n#define GPU_KERNEL_PTX_H\nstatic const char g_gpu_kernel_ptx[] =\n')
    for line in text.splitlines(True):
        e=line.replace('\\','\\\\').replace('"','\\"').replace('\r','\\r').replace('\n','\\n')
        f.write('"'+e+'"\n')
    f.write(';\n#endif\n')
