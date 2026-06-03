# TinyDoomOS App Development

TinyDoomOS supports three small app models:

- built-in C apps linked into the kernel
- `.TAP` executables loaded from the DOS-like filesystem under `C:\APPS`
- raw i386 `.APP` executables loaded from the DOS-like filesystem under `C:\APPS`

The C app model is still best for GUI experiments. The `.TAP` model is the
first sandboxed exec path. The `.APP` model is the first native binary path,
but it is not hardware-isolated yet.

## App Shape

An app is a function that returns an `app_result_t`:

```c
#include "app.h"

app_result_t app_my_app_main(void)
{
    return APP_RESULT_EXIT;
}
```

Return values:

- `APP_RESULT_EXIT` returns to the graphical shell.
- `APP_RESULT_HALT` asks the shell to halt the OS.

Doom is special because it takes over the framebuffer while running. Press
`F10` to request exit back to the shell.

## Add A Built-In App

1. Create a file in `apps/`, for example `apps/my_app.c`.
2. Implement `app_my_app_main`.
3. Add the file to `APP_SRC` in `Makefile`.
4. Add a descriptor to `apps/app_table.c`.
5. Run `make`.

Descriptor example:

```c
app_result_t app_my_app_main(void);

const app_descriptor_t builtin_apps[] = {
    { "my-app", "MY APP", "A SMALL TEST APP", app_my_app_main, 0 },
};
```

## Add A Loadable `.TAP` App

For now built-in `.TAP` files live in `kernel/fs.c` as boot filesystem entries.
Add a script, add it to the boot node table, then add a descriptor in
`kernel/exec.c`.
User-created files can persist on TinyFS, but boot-time app descriptors still
point at the known built-in `C:\APPS` paths.

Example script:

```text
PRINT HELLO FROM A SEPARATE FILE
PROC
MEM
WAIT
EXIT 0
```

Supported commands:

- `PRINT text`
- `DIR C:\PATH`
- `MEM`
- `PROC`
- `ALLOC bytes`
- `SLEEP ms`
- `WAIT`
- `EXIT code`

## Add A Native `.APP` App

The current native app example is `user_apps/native_hello.S`. It is assembled
as an i386 object, converted to a raw binary with `objcopy`, embedded with
`xxd`, and exposed under `C:\APPS\NATIVE.APP`.

The binary starts with:

```text
APP1 magic
entry offset
image size
flags
```

The entry function receives a pointer to a small process API table:

```c
typedef struct {
    void (*write)(const char *text);
    void (*mem_info)(void);
    void (*proc_info)(void);
    void (*sleep)(uint32_t ms);
    int (*list_dir)(const char *path);
    void (*exit)(int code);
} native_app_api_t;
```

This proves the native loader path, but it still runs in the kernel address
space. Paging and ring 3 are the next step for real native-process isolation.

## APIs Available Today

Apps may include the same low-level headers used by the shell:

- `app.h` for app lifecycle
- `app_ui.h` for simple app windows and key polling helpers
- `framebuffer.h` for pixels, rectangles, and text
- `keyboard.h` for key events
- `timer.h` for sleeping and ticks
- `fs.h`, `memory.h`, `process.h`, and `syscall.h` for kernel/runtime work

This is not a stable ABI yet. The next step is to wrap these into a smaller
public app API before adding native loadable apps.

`printf`, `puts`, and `putchar` write to the serial log, not to the graphical
framebuffer. Apps should draw UI explicitly through framebuffer or future GUI
toolkit calls.

The current built-in app suite is also useful as examples:

- `apps/notepad_app.c`: editable in-memory text input
- `apps/calc_app.c`: keyboard-driven state machine
- `apps/clock_app.c`: timer-based redraw loop
- `apps/paint_app.c`: simple canvas state
- `apps/sysinfo_app.c`: reading OS/kernel-facing information

## Minimal Interactive App

```c
#include "app.h"
#include "app_ui.h"
#include "doomkeys.h"
#include "framebuffer.h"
#include "timer.h"

app_result_t app_my_app_main(void)
{
    app_window_t win = app_draw_window(420, 220, "MY APP");
    framebuffer_draw_text_scaled(win.x + 40, win.y + 70, "MY APP", 0x00fff08a, 2);
    framebuffer_draw_text(win.x + 42, win.y + 124, "ENTER OR ESC TO RETURN", 0x00a7f3ff);

    for (;;) {
        unsigned char key;

        while (app_read_key(&key)) {
            if (key == KEY_ENTER || key == KEY_ESCAPE || key == ' ') {
                return APP_RESULT_EXIT;
            }
        }

        timer_sleep_ms(10);
    }
}
```

## Road To Native Loadable Apps

The intended progression is:

1. Built-in C apps linked with the kernel.
2. `C:\APPS` `.TAP` apps with syscalls and sandbox heaps.
3. `C:\APPS` raw i386 `.APP` apps with a fixed API table.
4. Disk-loaded ELF-like apps.
5. Process-like native apps with paging, syscalls, and separate heaps.

Stage 1 is enough for small experiments: demos, simple games, calculators,
text viewers, diagnostics, and GUI toolkit tests.
