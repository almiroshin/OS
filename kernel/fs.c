#include "fs.h"
#include "disk.h"
#include "memory.h"
#include "serial.h"
#include <string.h>

#define FS_MAX_NODES 64
#define TINYFS_MAGIC 0x53464454u
#define TINYFS_VERSION 1u
#define TINYFS_META_SECTORS 16u
#define TINYFS_DATA_START_LBA TINYFS_META_SECTORS
#define TINYFS_MAX_ENTRIES 48u

extern unsigned char doom_iwad[];
extern unsigned int doom_iwad_len;
extern unsigned char native_hello_app[];
extern unsigned int native_hello_app_len;

typedef struct {
    const char *path;
    fs_node_type_t type;
    const unsigned char *data;
    size_t size;
    uint32_t attr;
} boot_node_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t generation;
    uint32_t node_count;
    uint32_t data_start_lba;
    uint32_t reserved[123];
} __attribute__((packed)) tinyfs_header_t;

typedef struct {
    char path[64];
    uint32_t type;
    uint32_t attr;
    uint32_t size;
    uint32_t data_lba;
    uint32_t data_sectors;
} __attribute__((packed)) tinyfs_entry_t;

static const unsigned char kernel_text[] =
    "TinyDoomOS kernel image placeholder\r\n";

static const unsigned char readme_text[] =
    "TinyDoomOS C: FILE SYSTEM\r\n"
    "\r\n"
    "C:\\APPS contains .TAP and .APP executables.\r\n"
    "C:\\TEMP is persistent when build/tinydoom.img is attached.\r\n"
    "C:\\DOOM contains the embedded shareware WAD.\r\n";

static const unsigned char welcome_tap[] =
    "# Tiny app loaded through exec loader\r\n"
    "PRINT WELCOME TO TINYDOOMOS\r\n"
    "PRINT THIS PROGRAM IS A SEPARATE FILE\r\n"
    "PRINT PATH C:\\APPS\\WELCOME.TAP\r\n"
    "PROC\r\n"
    "PRINT PRESS A KEY TO RETURN\r\n"
    "WAIT\r\n"
    "EXIT 0\r\n";

static const unsigned char files_tap[] =
    "PRINT C: FILE SYSTEM DIRECTORY\r\n"
    "DIR C:\\\r\n"
    "DIR C:\\APPS\r\n"
    "DIR C:\\DOOM\r\n"
    "PRINT END OF DIRECTORY LIST\r\n"
    "WAIT\r\n"
    "EXIT 0\r\n";

static const unsigned char memory_tap[] =
    "PRINT MEMORY AND PROCESS INFO\r\n"
    "MEM\r\n"
    "PROC\r\n"
    "ALLOC 1024\r\n"
    "ALLOC 4096\r\n"
    "MEM\r\n"
    "WAIT\r\n"
    "EXIT 0\r\n";

static fs_node_t nodes[FS_MAX_NODES];
static int node_total;
static int persist_mounted;
static uint32_t persist_generation;
static unsigned char tinyfs_meta[TINYFS_META_SECTORS * DISK_SECTOR_SIZE];

static const boot_node_t boot_nodes[] = {
    { "C:\\", FS_NODE_DIR, 0, 0, FS_ATTR_DIRECTORY | FS_ATTR_READONLY },
    { "C:\\APPS", FS_NODE_DIR, 0, 0, FS_ATTR_DIRECTORY | FS_ATTR_READONLY },
    { "C:\\DOOM", FS_NODE_DIR, 0, 0, FS_ATTR_DIRECTORY | FS_ATTR_READONLY },
    { "C:\\SYSTEM", FS_NODE_DIR, 0, 0, FS_ATTR_DIRECTORY | FS_ATTR_READONLY },
    { "C:\\TEMP", FS_NODE_DIR, 0, 0, FS_ATTR_DIRECTORY },
    { "C:\\README.TXT", FS_NODE_FILE, readme_text, sizeof(readme_text) - 1, FS_ATTR_ARCHIVE | FS_ATTR_READONLY },
    { "C:\\SYSTEM\\KERNEL.SYS", FS_NODE_FILE, kernel_text, sizeof(kernel_text) - 1, FS_ATTR_ARCHIVE | FS_ATTR_READONLY },
    { "C:\\APPS\\WELCOME.TAP", FS_NODE_FILE, welcome_tap, sizeof(welcome_tap) - 1, FS_ATTR_ARCHIVE | FS_ATTR_READONLY },
    { "C:\\APPS\\FILES.TAP", FS_NODE_FILE, files_tap, sizeof(files_tap) - 1, FS_ATTR_ARCHIVE | FS_ATTR_READONLY },
    { "C:\\APPS\\MEMORY.TAP", FS_NODE_FILE, memory_tap, sizeof(memory_tap) - 1, FS_ATTR_ARCHIVE | FS_ATTR_READONLY },
    { "C:\\APPS\\NATIVE.APP", FS_NODE_FILE, 0, 0, FS_ATTR_ARCHIVE | FS_ATTR_READONLY },
    { "C:\\DOOM\\DOOM1.WAD", FS_NODE_FILE, 0, 0, FS_ATTR_ARCHIVE | FS_ATTR_READONLY },
};

static char upper_char(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

static int path_eq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static int is_doom_basename(const char *path)
{
    const char *base = path;

    while (*path) {
        if (*path == '\\') {
            base = path + 1;
        }
        path++;
    }

    return strcmp(base, "DOOM1.WAD") == 0;
}

static int copy_path(char *dst, const char *src)
{
    if (strlen(src) >= 64) {
        return -1;
    }
    strcpy(dst, src);
    return 0;
}

static fs_node_t *find_mutable_normalized(const char *normalized)
{
    for (int i = 0; i < node_total; i++) {
        if (path_eq(nodes[i].path, normalized)) {
            return &nodes[i];
        }
    }
    return 0;
}

static int parent_path(const char *path, char *out, size_t out_size)
{
    size_t slash = 0;
    size_t len = strlen(path);

    if (!out || out_size < 4 || len < 3) {
        return -1;
    }

    for (size_t i = 0; i < len; i++) {
        if (path[i] == '\\') {
            slash = i;
        }
    }

    if (slash == 0) {
        return -1;
    }

    if (slash == 2) {
        if (out_size < 4) {
            return -1;
        }
        strcpy(out, "C:\\");
        return 0;
    }

    if (slash >= out_size) {
        return -1;
    }

    memcpy(out, path, slash);
    out[slash] = 0;
    return 0;
}

static int parent_exists(const char *normalized)
{
    char parent[64];
    fs_node_t *node;

    if (parent_path(normalized, parent, sizeof(parent)) != 0) {
        return 0;
    }

    node = find_mutable_normalized(parent);
    return node && node->type == FS_NODE_DIR;
}

static fs_node_t *create_node(const char *normalized, fs_node_type_t type, uint32_t attr)
{
    fs_node_t *node;

    if (node_total >= FS_MAX_NODES || !parent_exists(normalized)) {
        return 0;
    }

    node = &nodes[node_total++];
    memset(node, 0, sizeof(*node));
    if (copy_path(node->path, normalized) != 0) {
        node_total--;
        return 0;
    }
    node->type = type;
    node->attr = attr;
    if (type == FS_NODE_DIR) {
        node->attr |= FS_ATTR_DIRECTORY;
    } else {
        node->attr |= FS_ATTR_ARCHIVE;
    }
    return node;
}

static uint32_t ceil_sectors(size_t size)
{
    return (uint32_t)((size + DISK_SECTOR_SIZE - 1) / DISK_SECTOR_SIZE);
}

static int node_is_persistable(const fs_node_t *node)
{
    if (!node || node->path[0] == 0) {
        return 0;
    }
    if (node->attr & FS_ATTR_READONLY) {
        return 0;
    }
    if (strcmp(node->path, "C:\\") == 0) {
        return 0;
    }
    return 1;
}

static int tinyfs_write_file_data(uint32_t start_lba, const unsigned char *data, size_t size)
{
    unsigned char sector[DISK_SECTOR_SIZE];
    uint32_t sectors_needed = ceil_sectors(size);
    size_t pos = 0;

    for (uint32_t i = 0; i < sectors_needed; i++) {
        size_t n = size - pos;
        if (n > DISK_SECTOR_SIZE) {
            n = DISK_SECTOR_SIZE;
        }
        memset(sector, 0, sizeof(sector));
        if (n && data) {
            memcpy(sector, data + pos, n);
        }
        if (disk_write_sector(start_lba + i, sector) != 0) {
            return -1;
        }
        pos += n;
    }
    return 0;
}

static int tinyfs_read_file_data(uint32_t start_lba, unsigned char *data, size_t size)
{
    unsigned char sector[DISK_SECTOR_SIZE];
    uint32_t sectors_needed = ceil_sectors(size);
    size_t pos = 0;

    for (uint32_t i = 0; i < sectors_needed; i++) {
        size_t n = size - pos;
        if (n > DISK_SECTOR_SIZE) {
            n = DISK_SECTOR_SIZE;
        }
        if (disk_read_sector(start_lba + i, sector) != 0) {
            return -1;
        }
        if (n) {
            memcpy(data + pos, sector, n);
        }
        pos += n;
    }
    return 0;
}

int fs_flush(void)
{
    tinyfs_header_t *header = (tinyfs_header_t *)tinyfs_meta;
    tinyfs_entry_t *entries = (tinyfs_entry_t *)(tinyfs_meta + DISK_SECTOR_SIZE);
    uint32_t next_lba = TINYFS_DATA_START_LBA;
    uint32_t entry_count = 0;

    if (!persist_mounted || !disk_is_present()) {
        return -1;
    }

    memset(tinyfs_meta, 0, sizeof(tinyfs_meta));

    for (int i = 0; i < node_total && entry_count < TINYFS_MAX_ENTRIES; i++) {
        fs_node_t *node = &nodes[i];
        tinyfs_entry_t *entry;
        uint32_t sectors_needed = 0;

        if (!node_is_persistable(node)) {
            continue;
        }

        entry = &entries[entry_count++];
        strncpy(entry->path, node->path, sizeof(entry->path));
        entry->path[sizeof(entry->path) - 1] = 0;
        entry->type = (uint32_t)node->type;
        entry->attr = node->attr & ~FS_ATTR_READONLY;
        entry->size = (uint32_t)node->size;

        if (node->type == FS_NODE_FILE) {
            sectors_needed = ceil_sectors(node->size);
            entry->data_lba = next_lba;
            entry->data_sectors = sectors_needed;
            if (next_lba + sectors_needed >= disk_sector_count()) {
                return -1;
            }
            if (tinyfs_write_file_data(next_lba, node->data, node->size) != 0) {
                return -1;
            }
            next_lba += sectors_needed;
        }
    }

    persist_generation++;
    header->magic = TINYFS_MAGIC;
    header->version = TINYFS_VERSION;
    header->generation = persist_generation;
    header->node_count = entry_count;
    header->data_start_lba = TINYFS_DATA_START_LBA;

    for (uint32_t i = 0; i < TINYFS_META_SECTORS; i++) {
        if (disk_write_sector(i, tinyfs_meta + i * DISK_SECTOR_SIZE) != 0) {
            return -1;
        }
    }

    return 0;
}

static int tinyfs_load(void)
{
    tinyfs_header_t *header;
    tinyfs_entry_t *entries;

    for (uint32_t i = 0; i < TINYFS_META_SECTORS; i++) {
        if (disk_read_sector(i, tinyfs_meta + i * DISK_SECTOR_SIZE) != 0) {
            return -1;
        }
    }

    header = (tinyfs_header_t *)tinyfs_meta;
    entries = (tinyfs_entry_t *)(tinyfs_meta + DISK_SECTOR_SIZE);

    if (header->magic != TINYFS_MAGIC || header->version != TINYFS_VERSION ||
        header->node_count > TINYFS_MAX_ENTRIES) {
        return -1;
    }

    persist_generation = header->generation;

    for (uint32_t i = 0; i < header->node_count; i++) {
        tinyfs_entry_t *entry = &entries[i];
        fs_node_t *node;

        if (entry->path[0] == 0 || find_mutable_normalized(entry->path)) {
            continue;
        }

        node = create_node(entry->path, (fs_node_type_t)entry->type, entry->attr & ~FS_ATTR_READONLY);
        if (!node) {
            continue;
        }

        if (node->type == FS_NODE_FILE) {
            unsigned char *copy = kmalloc(entry->size ? entry->size : 1);
            if (!copy) {
                continue;
            }
            if (tinyfs_read_file_data(entry->data_lba, copy, entry->size) != 0) {
                kfree(copy);
                continue;
            }
            node->data = copy;
            node->size = entry->size;
            node->capacity = entry->size;
            node->dynamic_data = 1;
        }
    }

    return 0;
}

void fs_init(void)
{
    node_total = 0;
    persist_mounted = 0;
    persist_generation = 0;
    memset(nodes, 0, sizeof(nodes));

    for (size_t i = 0; i < sizeof(boot_nodes) / sizeof(boot_nodes[0]); i++) {
        fs_node_t *node = &nodes[node_total++];
        memset(node, 0, sizeof(*node));
        copy_path(node->path, boot_nodes[i].path);
        node->type = boot_nodes[i].type;
        node->data = boot_nodes[i].data;
        node->size = boot_nodes[i].size;
        node->capacity = boot_nodes[i].size;
        node->attr = boot_nodes[i].attr;
        node->dynamic_data = 0;

        if (path_eq(node->path, "C:\\DOOM\\DOOM1.WAD")) {
            node->data = doom_iwad;
            node->size = doom_iwad_len;
            node->capacity = doom_iwad_len;
        } else if (path_eq(node->path, "C:\\APPS\\NATIVE.APP")) {
            node->data = native_hello_app;
            node->size = native_hello_app_len;
            node->capacity = native_hello_app_len;
        }
    }

    disk_init();
    if (!disk_is_present()) {
        serial_write("TinyDoomOS: TinyFS volatile mode\n");
        return;
    }

    persist_mounted = 1;
    if (tinyfs_load() == 0) {
        serial_write("TinyDoomOS: TinyFS mounted\n");
    } else {
        serial_write("TinyDoomOS: TinyFS formatting\n");
        persist_generation = 0;
        fs_flush();
    }
}

int fs_normalize_path(const char *path, char *out, size_t out_size)
{
    size_t pos = 0;
    int has_drive = 0;
    int previous_slash = 0;

    if (!path || !out || out_size < 4) {
        return -1;
    }

    while (*path == ' ') {
        path++;
    }

    if (path[0] && path[1] == ':') {
        if (pos + 2 >= out_size) {
            return -1;
        }
        out[pos++] = upper_char(path[0]);
        out[pos++] = ':';
        path += 2;
        has_drive = 1;
    }

    if (!has_drive) {
        out[pos++] = 'C';
        out[pos++] = ':';
    }

    if (*path != '\\' && *path != '/') {
        if (pos + 1 >= out_size) {
            return -1;
        }
        out[pos++] = '\\';
        previous_slash = 1;
    }

    while (*path && pos + 1 < out_size) {
        char c = *path++;

        if (c == '/') {
            c = '\\';
        }
        if (c == '\\') {
            if (previous_slash) {
                continue;
            }
            previous_slash = 1;
            out[pos++] = '\\';
            continue;
        }

        previous_slash = 0;
        out[pos++] = upper_char(c);
    }

    if (*path) {
        return -1;
    }

    while (pos > 3 && out[pos - 1] == '\\') {
        pos--;
    }

    out[pos] = 0;

    if (strcmp(out, "C:") == 0) {
        strcpy(out, "C:\\");
    }
    if (is_doom_basename(out)) {
        strcpy(out, "C:\\DOOM\\DOOM1.WAD");
    }

    return 0;
}

const fs_node_t *fs_find(const char *path)
{
    char normalized[64];

    if (fs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        return 0;
    }

    return find_mutable_normalized(normalized);
}

int fs_stat(const char *path, fs_stat_t *out)
{
    const fs_node_t *node = fs_find(path);

    if (!node || !out) {
        return -1;
    }

    strncpy(out->path, node->path, sizeof(out->path));
    out->path[sizeof(out->path) - 1] = 0;
    out->type = node->type;
    out->size = node->size;
    out->attr = node->attr;
    return 0;
}

int fs_read_file(const char *path, const unsigned char **data, size_t *size)
{
    const fs_node_t *node = fs_find(path);

    if (!node || node->type != FS_NODE_FILE) {
        return -1;
    }

    if (data) {
        *data = node->data;
    }
    if (size) {
        *size = node->size;
    }
    return 0;
}

int fs_write_file(const char *path, const void *data, size_t size)
{
    char normalized[64];
    fs_node_t *node;
    unsigned char *copy;

    if (fs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }

    node = find_mutable_normalized(normalized);
    if (node && ((node->attr & FS_ATTR_READONLY) || node->type != FS_NODE_FILE)) {
        return -1;
    }
    if (!node) {
        node = create_node(normalized, FS_NODE_FILE, 0);
        if (!node) {
            return -1;
        }
    }

    copy = kmalloc(size ? size : 1);
    if (!copy) {
        return -1;
    }
    if (size && data) {
        memcpy(copy, data, size);
    }

    if (node->dynamic_data && node->data) {
        kfree((void *)node->data);
    }

    node->data = copy;
    node->size = size;
    node->capacity = size;
    node->dynamic_data = 1;
    node->attr &= ~FS_ATTR_DIRECTORY;
    node->attr |= FS_ATTR_ARCHIVE;
    fs_flush();
    return 0;
}

int fs_append_file(const char *path, const void *data, size_t size)
{
    const fs_node_t *old = fs_find(path);
    unsigned char *combined;
    size_t old_size = old && old->type == FS_NODE_FILE ? old->size : 0;
    size_t next_size = old_size + size;
    int result;

    combined = kmalloc(next_size ? next_size : 1);
    if (!combined) {
        return -1;
    }

    if (old_size) {
        memcpy(combined, old->data, old_size);
    }
    if (size && data) {
        memcpy(combined + old_size, data, size);
    }

    result = fs_write_file(path, combined, next_size);
    kfree(combined);
    return result;
}

int fs_delete(const char *path)
{
    char normalized[64];
    fs_node_t *node = 0;
    int index = -1;

    if (fs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }

    for (int i = 0; i < node_total; i++) {
        if (path_eq(nodes[i].path, normalized)) {
            node = &nodes[i];
            index = i;
            break;
        }
    }

    if (index < 0 || (node->attr & FS_ATTR_READONLY) || node->type == FS_NODE_DIR) {
        return -1;
    }

    if (node->dynamic_data && node->data) {
        kfree((void *)node->data);
    }

    for (int i = index + 1; i < node_total; i++) {
        nodes[i - 1] = nodes[i];
    }
    node_total--;
    memset(&nodes[node_total], 0, sizeof(nodes[node_total]));
    fs_flush();
    return 0;
}

int fs_mkdir(const char *path)
{
    char normalized[64];
    fs_node_t *node;

    if (fs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }
    if (find_mutable_normalized(normalized)) {
        return 0;
    }

    node = create_node(normalized, FS_NODE_DIR, 0);
    if (!node) {
        return -1;
    }
    fs_flush();
    return 0;
}

static int parent_matches(const char *parent, const char *child)
{
    char child_parent[64];

    if (parent_path(child, child_parent, sizeof(child_parent)) != 0) {
        return 0;
    }

    return strcmp(parent, child_parent) == 0;
}

int fs_list(const char *path, fs_list_callback_t callback, void *ctx)
{
    char normalized[64];
    const fs_node_t *dir;

    if (!callback || fs_normalize_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }

    dir = fs_find(normalized);
    if (!dir || dir->type != FS_NODE_DIR) {
        return -1;
    }

    for (int i = 0; i < node_total; i++) {
        if (strcmp(nodes[i].path, normalized) == 0) {
            continue;
        }
        if (parent_matches(normalized, nodes[i].path)) {
            callback(&nodes[i], ctx);
        }
    }

    return 0;
}

int fs_node_count(void)
{
    return node_total;
}

int fs_file_count(void)
{
    int count = 0;
    for (int i = 0; i < node_total; i++) {
        if (nodes[i].type == FS_NODE_FILE) {
            count++;
        }
    }
    return count;
}

int fs_free_node_count(void)
{
    return FS_MAX_NODES - node_total;
}

int fs_is_persistent(void)
{
    return persist_mounted;
}

uint32_t fs_persist_generation(void)
{
    return persist_generation;
}
