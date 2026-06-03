#!/usr/bin/env python3
import argparse
import os
import struct
import sys

SECTOR_SIZE = 512
META_SECTORS = 16
DATA_START_LBA = META_SECTORS
MAX_ENTRIES = 48
DEFAULT_IMAGE_SIZE = 8 * 1024 * 1024

TINYFS_MAGIC = 0x53464454
TINYFS_VERSION = 1

NODE_FILE = 1
NODE_DIR = 2

ATTR_DIRECTORY = 0x10
ATTR_ARCHIVE = 0x20

HEADER_STRUCT = struct.Struct("<IIIII123I")
ENTRY_STRUCT = struct.Struct("<64sIIIII")

BOOT_DIRS = {
    "C:\\",
    "C:\\APPS",
    "C:\\DOOM",
    "C:\\SYSTEM",
    "C:\\TEMP",
}

BOOT_FILES = {
    "C:\\README.TXT",
    "C:\\SYSTEM\\KERNEL.SYS",
    "C:\\APPS\\WELCOME.TAP",
    "C:\\APPS\\FILES.TAP",
    "C:\\APPS\\MEMORY.TAP",
    "C:\\APPS\\NATIVE.APP",
    "C:\\DOOM\\DOOM1.WAD",
}


class TinyFSError(Exception):
    pass


def normalize_path(path):
    if not path:
        raise TinyFSError("empty path")

    path = path.strip().replace("/", "\\")
    if len(path) >= 2 and path[1] == ":":
        drive = path[0].upper()
        rest = path[2:]
    else:
        drive = "C"
        rest = path

    if drive != "C":
        raise TinyFSError("only C: paths are supported")

    if not rest:
        rest = "\\"
    elif not rest.startswith("\\"):
        rest = "\\" + rest

    parts = [part for part in rest.split("\\") if part]
    if parts:
        normalized = "C:\\" + "\\".join(part.upper() for part in parts)
    else:
        normalized = "C:\\"

    if len(normalized.encode("ascii", errors="ignore")) != len(normalized):
        raise TinyFSError("paths must be ASCII")
    if len(normalized) >= 64:
        raise TinyFSError("path is too long for TinyFS")
    return normalized


def parent_path(path):
    if path == "C:\\":
        return None
    slash = path.rfind("\\")
    if slash <= 2:
        return "C:\\"
    return path[:slash]


def name_part(path):
    if path == "C:\\":
        return path
    return path[path.rfind("\\") + 1:]


def sectors_for(size):
    return (size + SECTOR_SIZE - 1) // SECTOR_SIZE


class TinyFSImage:
    def __init__(self, path):
        self.path = path
        self.image = bytearray()
        self.entries = []
        self.generation = 0
        self.valid = False

    def load(self):
        if not os.path.exists(self.path):
            raise TinyFSError(f"image not found: {self.path}")
        with open(self.path, "rb") as f:
            self.image = bytearray(f.read())
        if len(self.image) < META_SECTORS * SECTOR_SIZE:
            raise TinyFSError("image is too small")
        self._parse()

    def create_empty(self):
        self.image = bytearray(DEFAULT_IMAGE_SIZE)
        self.entries = []
        self.generation = 0
        self.valid = False

    def _parse(self):
        header = HEADER_STRUCT.unpack_from(self.image, 0)
        magic, version, generation, node_count, data_start_lba = header[:5]
        self.entries = []
        self.generation = 0
        self.valid = False

        if magic != TINYFS_MAGIC or version != TINYFS_VERSION:
            return
        if data_start_lba != DATA_START_LBA or node_count > MAX_ENTRIES:
            raise TinyFSError("invalid TinyFS metadata")

        for index in range(node_count):
            offset = SECTOR_SIZE + index * ENTRY_STRUCT.size
            raw_path, node_type, attr, size, data_lba, data_sectors = ENTRY_STRUCT.unpack_from(self.image, offset)
            path = raw_path.split(b"\0", 1)[0].decode("ascii")
            if not path:
                continue
            if node_type not in (NODE_FILE, NODE_DIR):
                raise TinyFSError(f"invalid node type for {path}")

            data = b""
            if node_type == NODE_FILE:
                if sectors_for(size) > data_sectors:
                    raise TinyFSError(f"invalid data sector count for {path}")
                start = data_lba * SECTOR_SIZE
                end = start + size
                if end > len(self.image):
                    raise TinyFSError(f"file data is outside image for {path}")
                data = bytes(self.image[start:end])

            self.entries.append({
                "path": path,
                "type": node_type,
                "attr": attr,
                "size": size,
                "data": data,
            })

        self.generation = generation
        self.valid = True

    def format_empty(self):
        self.entries = [{
            "path": "C:\\TEMP",
            "type": NODE_DIR,
            "attr": ATTR_DIRECTORY,
            "size": 0,
            "data": b"",
        }]
        self.generation = 0
        self.valid = True

    def ensure_valid_for_write(self):
        if not self.valid:
            self.format_empty()

    def find_index(self, path):
        for index, entry in enumerate(self.entries):
            if entry["path"] == path:
                return index
        return -1

    def dir_exists(self, path):
        if path in BOOT_DIRS:
            return True
        index = self.find_index(path)
        return index >= 0 and self.entries[index]["type"] == NODE_DIR

    def ensure_parent_exists(self, path):
        parent = parent_path(path)
        if not parent or not self.dir_exists(parent):
            raise TinyFSError(f"parent directory does not exist: {parent or path}")

    def write_image(self):
        if len(self.entries) > MAX_ENTRIES:
            raise TinyFSError("too many TinyFS entries")

        sector_count = len(self.image) // SECTOR_SIZE
        metadata = bytearray(META_SECTORS * SECTOR_SIZE)
        next_lba = DATA_START_LBA
        entry_records = []

        for entry in self.entries:
            data_lba = 0
            data_sectors = 0
            size = len(entry["data"]) if entry["type"] == NODE_FILE else 0

            if entry["type"] == NODE_FILE:
                data_sectors = sectors_for(size)
                data_lba = next_lba
                if next_lba + data_sectors >= sector_count:
                    raise TinyFSError("not enough space in image")
                start = data_lba * SECTOR_SIZE
                end = start + data_sectors * SECTOR_SIZE
                self.image[start:end] = b"\0" * (end - start)
                self.image[start:start + size] = entry["data"]
                next_lba += data_sectors

            encoded = entry["path"].encode("ascii")
            encoded = encoded + b"\0" * (64 - len(encoded))
            entry_records.append((encoded, entry["type"], entry["attr"], size, data_lba, data_sectors))

        self.generation += 1
        HEADER_STRUCT.pack_into(
            metadata,
            0,
            TINYFS_MAGIC,
            TINYFS_VERSION,
            self.generation,
            len(entry_records),
            DATA_START_LBA,
            *([0] * 123),
        )

        for index, record in enumerate(entry_records):
            ENTRY_STRUCT.pack_into(metadata, SECTOR_SIZE + index * ENTRY_STRUCT.size, *record)

        self.image[:len(metadata)] = metadata
        with open(self.path, "wb") as f:
            f.write(self.image)
        self.valid = True

    def put_file(self, host_src, tiny_path):
        self.ensure_valid_for_write()
        path = normalize_path(tiny_path)
        if path in BOOT_DIRS or path in BOOT_FILES:
            raise TinyFSError(f"cannot overwrite kernel boot path: {path}")
        self.ensure_parent_exists(path)

        with open(host_src, "rb") as f:
            data = f.read()

        entry = {
            "path": path,
            "type": NODE_FILE,
            "attr": ATTR_ARCHIVE,
            "size": len(data),
            "data": data,
        }

        index = self.find_index(path)
        if index >= 0:
            if self.entries[index]["type"] == NODE_DIR:
                raise TinyFSError("destination is a directory")
            self.entries[index] = entry
        else:
            self.entries.append(entry)
        self.write_image()

    def mkdir(self, tiny_path):
        self.ensure_valid_for_write()
        path = normalize_path(tiny_path)
        if path in BOOT_DIRS:
            return
        if path in BOOT_FILES:
            raise TinyFSError(f"boot file already exists: {path}")
        self.ensure_parent_exists(path)
        index = self.find_index(path)
        if index >= 0:
            if self.entries[index]["type"] != NODE_DIR:
                raise TinyFSError("file already exists at that path")
            return
        self.entries.append({
            "path": path,
            "type": NODE_DIR,
            "attr": ATTR_DIRECTORY,
            "size": 0,
            "data": b"",
        })
        self.write_image()

    def remove(self, tiny_path):
        if not self.valid:
            raise TinyFSError("TinyFS is not formatted")
        path = normalize_path(tiny_path)
        index = self.find_index(path)
        if index < 0:
            raise TinyFSError(f"not found in TinyFS image: {path}")
        if self.entries[index]["type"] == NODE_DIR:
            for entry in self.entries:
                if parent_path(entry["path"]) == path:
                    raise TinyFSError("directory is not empty")
        del self.entries[index]
        self.write_image()

    def get_file(self, tiny_path, host_dst):
        if not self.valid:
            raise TinyFSError("TinyFS is not formatted")
        path = normalize_path(tiny_path)
        index = self.find_index(path)
        if index < 0 or self.entries[index]["type"] != NODE_FILE:
            raise TinyFSError(f"file not found in TinyFS image: {path}")

        parent = os.path.dirname(host_dst)
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(host_dst, "wb") as f:
            f.write(self.entries[index]["data"])

    def list_dir(self, tiny_path):
        if not self.valid:
            raise TinyFSError("TinyFS is not formatted")
        path = normalize_path(tiny_path)
        if not self.dir_exists(path):
            raise TinyFSError(f"directory not found: {path}")

        rows = []
        seen = set()
        for boot_dir in sorted(BOOT_DIRS):
            if boot_dir != path and parent_path(boot_dir) == path:
                rows.append(("BOOT", NODE_DIR, 0, boot_dir))
                seen.add(boot_dir)

        for entry in self.entries:
            if entry["path"] in seen:
                continue
            if entry["path"] != path and parent_path(entry["path"]) == path:
                rows.append(("DISK", entry["type"], len(entry["data"]) if entry["type"] == NODE_FILE else 0, entry["path"]))
                seen.add(entry["path"])

        print(f"Directory of {path}")
        for source, node_type, size, row_path in rows:
            kind = "<DIR>" if node_type == NODE_DIR else "     "
            print(f"{kind:5} {size:8d} {source:4} {name_part(row_path)}")
        print(f"{len(rows)} item(s), generation {self.generation}")


def load_or_create(path, allow_create):
    image = TinyFSImage(path)
    if os.path.exists(path):
        image.load()
    elif allow_create:
        image.create_empty()
    else:
        raise TinyFSError(f"image not found: {path}")
    return image


def main(argv):
    parser = argparse.ArgumentParser(description="TinyFS raw image tool")
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("format", help="format TinyFS metadata")
    p.add_argument("image")
    p.add_argument("--force", action="store_true")

    p = sub.add_parser("ls", help="list a TinyFS directory")
    p.add_argument("image")
    p.add_argument("path", nargs="?", default="C:/")

    p = sub.add_parser("put", help="copy a host file into TinyFS")
    p.add_argument("image")
    p.add_argument("src")
    p.add_argument("dst")

    p = sub.add_parser("get", help="copy a TinyFS file to the host")
    p.add_argument("image")
    p.add_argument("src")
    p.add_argument("dst")

    p = sub.add_parser("mkdir", help="create a TinyFS directory")
    p.add_argument("image")
    p.add_argument("path")

    p = sub.add_parser("rm", help="remove a TinyFS entry")
    p.add_argument("image")
    p.add_argument("path")

    args = parser.parse_args(argv)

    try:
        if args.command == "format":
            image = load_or_create(args.image, allow_create=True)
            if image.valid and image.entries and not args.force:
                raise TinyFSError("image already has TinyFS entries; pass --force to format")
            image.format_empty()
            image.write_image()
            print(f"formatted {args.image}")
        elif args.command == "ls":
            image = load_or_create(args.image, allow_create=False)
            image.list_dir(args.path)
        elif args.command == "put":
            image = load_or_create(args.image, allow_create=False)
            image.put_file(args.src, args.dst)
            print(f"put {args.src} -> {normalize_path(args.dst)}")
        elif args.command == "get":
            image = load_or_create(args.image, allow_create=False)
            image.get_file(args.src, args.dst)
            print(f"got {normalize_path(args.src)} -> {args.dst}")
        elif args.command == "mkdir":
            image = load_or_create(args.image, allow_create=False)
            image.mkdir(args.path)
            print(f"mkdir {normalize_path(args.path)}")
        elif args.command == "rm":
            image = load_or_create(args.image, allow_create=False)
            image.remove(args.path)
            print(f"removed {normalize_path(args.path)}")
    except TinyFSError as exc:
        print(f"tinyfs: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
