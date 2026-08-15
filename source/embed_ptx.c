#include <stdio.h>

static int emit_escaped(FILE* out, int ch) {
    switch (ch) {
        case '\\': return fputs("\\\\", out) >= 0;
        case '"':  return fputs("\\\"", out) >= 0;
        case '\r': return fputs("\\r", out) >= 0;
        case '\n': return fputs("\\n\"\n\"", out) >= 0;
        case '\t': return fputs("\\t", out) >= 0;
        default:   return fputc(ch, out) != EOF;
    }
}

int main(int argc, char** argv) {
    const char* input_name = argc > 1 ? argv[1] : "gpu_kernel.ptx";
    const char* output_name = argc > 2 ? argv[2] : "gpu_kernel_ptx.h";
    FILE* in = fopen(input_name, "rb");
    FILE* out;
    int ch;

    if (!in) {
        fprintf(stderr, "cannot open %s\n", input_name);
        return 1;
    }
    out = fopen(output_name, "wb");
    if (!out) {
        fprintf(stderr, "cannot create %s\n", output_name);
        fclose(in);
        return 1;
    }

    fputs("#ifndef GPU_KERNEL_PTX_H\n"
          "#define GPU_KERNEL_PTX_H\n"
          "static const char g_gpu_kernel_ptx[] =\n\"", out);

    while ((ch = fgetc(in)) != EOF) {
        if (!emit_escaped(out, ch)) {
            fclose(in);
            fclose(out);
            return 1;
        }
    }

    fputs("\";\n#endif\n", out);
    if (ferror(in) || ferror(out)) {
        fclose(in);
        fclose(out);
        return 1;
    }

    fclose(in);
    fclose(out);
    return 0;
}
