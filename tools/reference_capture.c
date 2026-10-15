/* Read the original DF1 test process. Output is a PPM image and JSON metadata. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DWORD target_pid;
static unsigned target_count;

static BOOL CALLBACK find_game(HWND window, LPARAM unused) {
    char name[64], title[128];
    (void)unused;
    GetClassNameA(window, name, sizeof(name));
    GetWindowTextA(window, title, sizeof(title));
    if (!strcmp(name, "NLGAMECLASS") && !strcmp(title, "Delta Force 1.00.03.03P")) {
        GetWindowThreadProcessId(window, &target_pid);
        ++target_count;
    }
    return TRUE;
}

static int read_bytes(HANDLE process, uintptr_t address, void* bytes, SIZE_T count) {
    SIZE_T received = 0;
    return ReadProcessMemory(process, (const void*)address, bytes, count, &received) && received == count;
}

int main(int argc, char** argv) {
    static const unsigned char expected_dispatcher[16] = {
        0x53, 0x56, 0x57, 0x55, 0x8b, 0x44, 0x24, 0x14,
        0xbb, 0x10, 0, 0, 0, 0x31, 0xd2, 0xa3
    };
    HANDLE process = NULL;
    FILE* output = NULL;
    unsigned char* pixels = NULL;
    unsigned char dispatcher[16];
    uint32_t state, buffer, width, height, depth;
    char state_name[33] = {0};
    int result = 1;
    if (argc != 2) { fprintf(stderr, "reference_capture OUTPUT.ppm\n"); return 1; }
    EnumWindows(find_game, 0);
    if (target_count != 1) { fprintf(stderr, "Expected one DF1 1.00.03.03P test window\n"); return 1; }
    process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, target_pid);
    if (!process) { fprintf(stderr, "Cannot read the DF1 test process\n"); goto done; }
    if (!read_bytes(process, 0x461270, dispatcher, sizeof(dispatcher)) ||
        memcmp(dispatcher, expected_dispatcher, sizeof(dispatcher))) {
        fprintf(stderr, "Original dispatcher bytes differ\n"); goto done;
    }
    if (!read_bytes(process, 0x50130c, &buffer, 4) ||
        !read_bytes(process, 0xd092c8, &width, 4) ||
        !read_bytes(process, 0xd092cc, &height, 4) ||
        !read_bytes(process, 0xd09314, &depth, 4) ||
        !read_bytes(process, 0x8ee1d0, &state, 4)) {
        fprintf(stderr, "Cannot read display metadata\n"); goto done;
    }
    if (!buffer || !width || width > 4096 || !height || height > 4096 || depth != 32) {
        fprintf(stderr, "Expected a ready 32-bit display buffer, up to 4096 by 4096\n"); goto done;
    }
    if (state && !read_bytes(process, state, state_name, 32)) {
        fprintf(stderr, "Cannot read the current state\n"); goto done;
    }
    const size_t count = (size_t)width * height;
    pixels = (unsigned char*)malloc(count * 4);
    if (!pixels || !read_bytes(process, buffer, pixels, count * 4)) {
        fprintf(stderr, "Cannot read the display pixels\n"); goto done;
    }
    output = fopen(argv[1], "wbx");
    if (!output) { fprintf(stderr, "Use a new output path\n"); goto done; }
    if (fprintf(output, "P6\n%u %u\n255\n", width, height) < 0) goto done;
    for (size_t i = 0; i < count; ++i) {
        const unsigned char rgb[3] = {pixels[i * 4 + 2], pixels[i * 4 + 1], pixels[i * 4]};
        if (fwrite(rgb, 1, 3, output) != 3) goto done;
    }
    if (fclose(output)) { output = NULL; goto done; }
    output = NULL;
    printf("{\"pid\":%lu,\"width\":%u,\"height\":%u,\"depth\":%u,\"state_va\":%u,\"state\":\"",
           (unsigned long)target_pid, width, height, depth, state);
    for (size_t i = 0; state_name[i]; ++i) {
        const unsigned char c = (unsigned char)state_name[i];
        if (c == '"' || c == '\\') printf("\\%c", c);
        else if (c >= 32 && c < 127) putchar(c);
        else printf("\\u%04x", c);
    }
    puts("\"}");
    result = 0;
done:
    if (output) fclose(output);
    free(pixels);
    if (process) CloseHandle(process);
    return result;
}
