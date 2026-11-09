"""Bounded Mach-O64 metadata inspection, independent of dyld's residency policy."""
from pathlib import Path
import struct
import uuid

MACHO64_MAGIC = 0xfeedfacf
ARM64_CPU = 0x0100000c
DYLIB_FILE_TYPE = 6
HAS_TLV_DESCRIPTORS = 0x800000
SEGMENT64_COMMAND = 0x19
UUID_COMMAND = 0x1b
BUILD_VERSION_COMMAND = 0x32


def inspect_image(path: Path):
    data = path.read_bytes()
    assert len(data) >= 32
    magic, cpu, subtype, filetype, count, command_bytes, flags, reserved = struct.unpack_from('<8I', data)
    assert magic == MACHO64_MAGIC and 32 + command_bytes <= len(data)
    offset = 32
    sections = []
    image_uuid = None
    build_versions = []
    for _ in range(count):
        command, size = struct.unpack_from('<II', data, offset)
        assert size >= 8 and offset + size <= 32 + command_bytes
        if command == SEGMENT64_COMMAND:
            assert size >= 72
            section_count = struct.unpack_from('<I', data, offset + 64)[0]
            assert 72 + section_count * 80 <= size
            for index in range(section_count):
                section = data[offset + 72 + index * 80:offset + 72 + (index + 1) * 80]
                name = section[:16].split(b'\0', 1)[0].decode('ascii')
                segment = section[16:32].split(b'\0', 1)[0].decode('ascii')
                sections.append(segment + ',' + name)
        elif command == UUID_COMMAND:
            assert size == 24 and image_uuid is None
            image_uuid = str(uuid.UUID(bytes=data[offset + 8:offset + 24]))
        elif command == BUILD_VERSION_COMMAND:
            platform, minimum, sdk, tools = struct.unpack_from('<4I', data, offset + 8)
            build_versions.append(dict(platform=platform, minimum=minimum, sdk=sdk, tools=tools))
        offset += size
    assert offset == 32 + command_bytes
    return dict(cpu=cpu, subtype=subtype, filetype=filetype, flags=flags, uuid=image_uuid,
                build_versions=build_versions, sections=sections,
                objc_metadata=any(',__objc_' in name for name in sections),
                tlv_metadata=bool(flags & HAS_TLV_DESCRIPTORS)
                    or any(',__thread_' in name for name in sections))
