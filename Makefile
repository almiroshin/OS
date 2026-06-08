CROSS ?= i686-elf-
PATH := /opt/homebrew/bin:/usr/local/bin:$(PATH)
export PATH
CC := $(firstword \
	$(wildcard /opt/homebrew/bin/$(CROSS)gcc) \
	$(wildcard /usr/local/bin/$(CROSS)gcc) \
	$(CROSS)gcc)
AS := $(CC)
OBJCOPY := $(firstword \
	$(wildcard /opt/homebrew/bin/$(CROSS)objcopy) \
	$(wildcard /usr/local/bin/$(CROSS)objcopy) \
	$(CROSS)objcopy)
QEMU ?= $(firstword \
	$(wildcard /opt/homebrew/bin/qemu-system-i386) \
	$(wildcard /usr/local/bin/qemu-system-i386) \
	qemu-system-i386)
PYTHON ?= python3
GRUB_MKRESCUE ?= $(firstword \
	$(wildcard /opt/homebrew/bin/grub-mkrescue) \
	$(wildcard /opt/homebrew/bin/i686-elf-grub-mkrescue) \
	$(wildcard /usr/local/bin/grub-mkrescue) \
	$(wildcard /usr/local/bin/i686-elf-grub-mkrescue) \
	$(shell command -v grub-mkrescue 2>/dev/null) \
	$(shell command -v i686-elf-grub-mkrescue 2>/dev/null))
DOOM1_WAD_ZIP_URL ?= https://www.jbserver.com/downloads/games/doom/misc/shareware/doom1.wad.zip

BUILD := build
ISO_ROOT := $(BUILD)/iso
KERNEL := $(BUILD)/kernel.elf
ISO := $(BUILD)/TinyDoomOS.iso
DISK := $(BUILD)/tinydoom.img
FS_PATH ?= C:/
WAD := assets/doom1.wad
NATIVE_APP := $(BUILD)/user_apps/native_hello.app
APP_BLOBS_C := $(BUILD)/app_blobs.c

COMMON_CFLAGS := -std=gnu99 -ffreestanding -fno-builtin -fno-stack-protector \
	-fno-pic -m32 -march=i686 -O2 -Wall -Wextra -Wno-unused-parameter \
	-DNORMALUNIX -DDOOMGENERIC_RESX=640 -DDOOMGENERIC_RESY=400 \
	-Iinclude -Ikernel -Iapps -Ithird_party/doomgeneric

LDFLAGS := -ffreestanding -nostdlib -m32 -T boot/linker.ld -Wl,--gc-sections

KERNEL_SRC := \
	kernel/boot.S \
	kernel/app.c \
	kernel/disk.c \
	kernel/entry.c \
	kernel/doom_os.c \
	kernel/doom_video.c \
	kernel/exec.c \
	kernel/framebuffer.c \
	kernel/fs.c \
	kernel/gui.c \
	kernel/idt.S \
	kernel/interrupts.c \
	kernel/keyboard.c \
	kernel/memory.c \
	kernel/pic.c \
	kernel/ports.c \
	kernel/process.c \
	kernel/serial.c \
	kernel/syscall.c \
	kernel/timer.c \
	kernel/wad_embedded.c \
	libc/ctype.c \
	libc/errno.c \
	libc/stdlib.c \
	libc/string.c \
	libc/stdio.c

APP_SRC := \
	apps/app_ui.c \
	apps/app_table.c \
	apps/command_app.c \
	apps/notepad_app.c \
	apps/calc_app.c \
	apps/clock_app.c \
	apps/paint_app.c \
	apps/fileman_app.c \
	apps/sysinfo_app.c \
	apps/doom_app.c \
	apps/about_app.c \
	apps/hello_app.c

DOOM_SRC_NAMES := dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c \
	d_items.c d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c \
	g_game.c hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c \
	i_scale.c i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c \
	m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c \
	p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c \
	p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c \
	p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c \
	r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c sha1.c sounds.c \
	statdump.c st_lib.c st_stuff.c s_sound.c tables.c v_video.c wi_stuff.c \
	w_checksum.c w_file.c w_main.c w_wad.c z_zone.c doomgeneric.c

DOOM_SRC := $(addprefix third_party/doomgeneric/,$(DOOM_SRC_NAMES))
WAD_C := $(BUILD)/wad_blob.c
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(filter %.c,$(KERNEL_SRC) $(APP_SRC) $(DOOM_SRC))) \
	$(patsubst %.S,$(BUILD)/%.o,$(filter %.S,$(KERNEL_SRC))) \
	$(BUILD)/wad_blob.o \
	$(BUILD)/app_blobs.o

.PHONY: all clean run run-persistent disk-image fs-format fs-ls fs-put fs-get fs-mkdir fs-rm install-sample-apps check-tools fetch-wad

all: check-tools $(ISO)

check-tools:
	@command -v $(CC) >/dev/null || { echo "Missing $(CC). Install i686-elf-gcc."; exit 1; }
	@command -v $(OBJCOPY) >/dev/null || { echo "Missing $(OBJCOPY). Install i686-elf-binutils."; exit 1; }
	@test -n "$(GRUB_MKRESCUE)" || { echo "Missing grub-mkrescue. Install i686-elf-grub or GRUB."; exit 1; }
	@test -f $(WAD) || { echo "Missing $(WAD). Add the Doom shareware WAD."; exit 1; }

fetch-wad:
	@mkdir -p assets $(BUILD)
	curl -L -o $(BUILD)/doom1.wad.zip $(DOOM1_WAD_ZIP_URL)
	unzip -p $(BUILD)/doom1.wad.zip DOOM1.WAD > $(WAD)
	@test -s $(WAD) || { echo "Failed to extract doom1.wad from doom1.wad.zip"; exit 1; }

$(WAD_C): $(WAD)
	@mkdir -p $(dir $@)
	@printf '#include <stddef.h>\n#include <stdint.h>\n' > $@
	@xxd -i -n doom_iwad $(WAD) >> $@

$(BUILD)/user_apps/%.o: user_apps/%.S
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) -c $< -o $@

$(NATIVE_APP): $(BUILD)/user_apps/native_hello.o
	$(OBJCOPY) -j .text -O binary $< $@

$(APP_BLOBS_C): $(NATIVE_APP)
	@mkdir -p $(dir $@)
	@printf '#include <stddef.h>\n#include <stdint.h>\n' > $@
	@xxd -i -n native_hello_app $(NATIVE_APP) >> $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(COMMON_CFLAGS) -c $< -o $@

$(BUILD)/wad_blob.o: $(WAD_C)
	$(CC) $(COMMON_CFLAGS) -c $< -o $@

$(BUILD)/app_blobs.o: $(APP_BLOBS_C)
	$(CC) $(COMMON_CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) boot/linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -lgcc -o $@

$(ISO): $(KERNEL) boot/grub.cfg
	@mkdir -p $(ISO_ROOT)/boot/grub
	@cp $(KERNEL) $(ISO_ROOT)/boot/kernel.elf
	@cp boot/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(ISO_ROOT)

run: check-tools $(ISO)
	$(QEMU) -m 256M -cdrom $(ISO) -vga std -serial stdio -no-reboot

disk-image:
	@mkdir -p $(BUILD)
	@if [ ! -f $(DISK) ]; then dd if=/dev/zero of=$(DISK) bs=1m count=8; fi

fs-format: disk-image
	$(PYTHON) tools/tinyfs.py format $(DISK) --force

fs-ls: disk-image
	$(PYTHON) tools/tinyfs.py ls $(DISK) "$(FS_PATH)"

fs-put: disk-image
	@test -n "$(SRC)" || { echo "Usage: make fs-put SRC=host-file DST=C:/APPS/APP.TAP"; exit 1; }
	@test -n "$(DST)" || { echo "Usage: make fs-put SRC=host-file DST=C:/APPS/APP.TAP"; exit 1; }
	$(PYTHON) tools/tinyfs.py put $(DISK) "$(SRC)" "$(DST)"

fs-get: disk-image
	@test -n "$(SRC)" || { echo "Usage: make fs-get SRC=C:/APPS/APP.TAP DST=host-file"; exit 1; }
	@test -n "$(DST)" || { echo "Usage: make fs-get SRC=C:/APPS/APP.TAP DST=host-file"; exit 1; }
	$(PYTHON) tools/tinyfs.py get $(DISK) "$(SRC)" "$(DST)"

fs-mkdir: disk-image
	@test -n "$(FS_PATH)" || { echo "Usage: make fs-mkdir FS_PATH=C:/TEMP/TOOLS"; exit 1; }
	$(PYTHON) tools/tinyfs.py mkdir $(DISK) "$(FS_PATH)"

fs-rm: disk-image
	@test -n "$(FS_PATH)" || { echo "Usage: make fs-rm FS_PATH=C:/APPS/APP.TAP"; exit 1; }
	$(PYTHON) tools/tinyfs.py rm $(DISK) "$(FS_PATH)"

install-sample-apps: disk-image $(NATIVE_APP)
	$(PYTHON) tools/tinyfs.py put $(DISK) user_apps/hello_disk.tap C:/APPS/HELLODSK.TAP
	$(PYTHON) tools/tinyfs.py put $(DISK) $(NATIVE_APP) C:/APPS/NATIVE2.APP

run-persistent: check-tools $(ISO) disk-image
	$(QEMU) -m 256M -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide,media=disk -boot d -vga std -serial stdio -no-reboot

clean:
	rm -rf $(BUILD)
