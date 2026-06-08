# TinyDoomOS

TinyDoomOS is a small 32-bit Multiboot hobby operating system for pet projects.
It boots in a VM, opens a Windows 3.x-inspired graphical shell, and runs
built-in C applications plus loadable `.TAP` and raw i386 `.APP` programs.
Doom is one of those apps, not the whole OS anymore.

This is not a modern full OS yet: there is no native ELF loader, user-mode
paging, mouse driver, sound, or preemptive scheduler. The current
goal is a compact, understandable OS skeleton that can grow into a small
graphical environment for experiments.

## Current Features

- GRUB/Multiboot boot into protected mode
- 32-bit freestanding kernel
- linear framebuffer graphics
- PIT timer
- PIC/IDT interrupt setup
- polled PS/2 keyboard input
- serial diagnostics
- ATA PIO disk driver for QEMU IDE disks
- tiny freestanding libc layer
- free-list kernel heap allocator
- DOS-like filesystem mounted as `C:\`
- TinyFS persistence on an attached raw disk image
- writable `C:\TEMP` files and directories
- process table for loadable app runs
- syscall dispatcher for sandboxed apps
- `.TAP` script executable loader
- raw i386 `.APP` executable loader MVP
- tiny C SDK for disk-installed `.APP` programs
- graphical desktop launcher
- GUI file manager for `C:\`
- built-in app registry
- mixed app launcher: built-in apps plus `C:\APPS` executables
- basic built-in app suite
- sample `HELLO` app
- `COMMAND`, `NOTEPAD`, `CALC`, `CLOCK`, `PAINT`, `FILES`, and `SYS INFO`
- `ABOUT OS` app
- Doom shareware app through `doomgeneric`
- Doom WAD exposed as `C:\DOOM\DOOM1.WAD`

## Requirements On macOS

Install Homebrew first if `brew` is not available:

```sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
eval "$(/opt/homebrew/bin/brew shellenv)"
```

Then install a cross compiler, GRUB tooling, and emulator:

```sh
brew install i686-elf-gcc i686-elf-grub qemu xorriso
```

If the Homebrew installer prints different `brew shellenv` commands, use the
commands it prints.

## Add Doom WAD

Put the shareware WAD here:

```text
assets/doom1.wad
```

You can use the official Doom shareware `doom1.wad`. Commercial WAD files are
not included in this repository.

To fetch the Doom 1 shareware WAD automatically:

```sh
make fetch-wad
```

## Build

```sh
make
```

The build creates:

- `build/kernel.elf`
- `build/TinyDoomOS.iso`
- `build/tinydoom.img` when you create a persistent disk image

## Run

```sh
make run
```

This runs the OS without a writable disk image. `C:\TEMP` still works, but new
files disappear after reboot.

For persistent files, create and attach the TinyFS disk image:

```sh
make disk-image
make run-persistent
```

`make disk-image` creates `build/tinydoom.img` if it does not exist.
`make run-persistent` boots the ISO and attaches that image as an IDE disk.
On first boot TinyDoomOS formats it as TinyFS; later boots mount it and keep
files created from `COMMAND`, such as `C:\TEMP\NOTE.TXT`.

You can also copy apps into the disk image from macOS while QEMU is not running:

```sh
make user-apps
make install-sample-apps
make fs-ls FS_PATH=C:/APPS
make fs-put SRC=user_apps/hello_disk.tap DST=C:/APPS/MYAPP.TAP
make fs-get SRC=C:/APPS/MYAPP.TAP DST=build/MYAPP.TAP
```

Files copied to `C:\APPS` with `.TAP` or `.APP` extensions appear in the GUI
launcher on the next boot and can also be run from `COMMAND`, for example
`RUN HELLODSK.TAP`. Embedded boot apps such as `WELCOME.TAP` and `NATIVE.APP`
are still part of the kernel image, so use different names for disk-installed
apps.

Or boot `build/TinyDoomOS.iso` in UTM, VirtualBox, VMware, or another BIOS x86
VM. For persistence, attach `build/tinydoom.img` as a raw IDE disk.

Shell controls:

- Arrow keys select an app.
- Enter starts the selected app.
- Esc halts the OS from the shell.

Built-in apps:

- `COMMAND`: DOS-style shell. Supports `DIR`, `TYPE`, `WRITE`, `APPEND`, `DEL`, `MKDIR`, `RUN`, `FILES`, `MEM`, `SYNC`, `CLS`, `VER`, and `EXIT`.
- `NOTEPAD`: in-memory text editor. Type text, Backspace deletes, Enter adds a line, Esc returns.
- `CALC`: integer calculator. Use `0-9`, `A/S/M/D` for add/subtract/multiply/divide, Enter equals, `C` clears, Esc returns.
- `CLOCK`: live uptime clock.
- `PAINT`: keyboard drawing grid. Arrows move, Space draws, Backspace erases, `1-4` change color, `C` clears.
- `FILES`: GUI file manager. Browse `C:\`, open folders, view `.TXT/.TAP`, run `.TAP/.APP`, create folders, and delete files.
- `SYS INFO`: framebuffer, app count, uptime, WAD size, and driver summary.

Loadable `C:\APPS` apps:

- `WELCOME.TAP`: loaded from `C:\APPS\WELCOME.TAP`.
- `FILES.TAP`: lists directories from the DOS-like filesystem.
- `MEMORY.TAP`: calls memory and process syscalls from the sandbox.
- `NATIVE.APP`: raw i386 executable image with an `APP1` header and process API table.
- `CHELLO.APP`: sample disk-installed native app built from C with the tiny SDK.

Doom uses the usual keyboard controls. Press `F10` inside Doom to return to the
TinyDoomOS shell. This version has no sound driver yet.

## Writing Apps

There are now three app models, with disk installation for loadable files:

- built-in C modules linked into the kernel and registered in `apps/app_table.c`
- `.TAP` executables stored under `C:\APPS`
- raw i386 `.APP` executables built from assembly or C under `user_apps/`
- disk-installed `.TAP` and `.APP` files copied into `build/tinydoom.img`

Start with:

- `include/tinyos_app.h`
- `user_apps/native_c_hello.c`
- `apps/hello_app.c`
- `apps/notepad_app.c`
- `apps/app_ui.h`
- `apps/about_app.c`
- `kernel/app.h`
- `docs/APP_DEVELOPMENT.md`

The `.TAP` loader is intentionally tiny, while `.APP` proves the native binary
path before we add ELF-style loading, paging, and user mode. Persistent user
files live in TinyFS on `build/tinydoom.img` when the disk image is attached.

## Architecture

See `docs/ARCHITECTURE.md` for the current architecture and roadmap.
