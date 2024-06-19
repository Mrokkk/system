#pragma once

#include <stdint.h>
#include <kernel/fs.h>
#include <kernel/api/types.h>
#include <kernel/byteorder.h>

// References:
// https://wiki.osdev.org/ISO_9660
// https://ftpmirror.your.org/pub/misc/bitsavers/projects/cdrom/Rock_Ridge_Interchange_Protocol.pdf

typedef struct lsb_msb16 lsb_msb16_t;
typedef struct lsb_msb32 lsb_msb32_t;
typedef struct iso9660_sb iso9660_sb_t;
typedef struct iso9660_pvd iso9660_pvd_t;
typedef struct iso9660_data iso9660_data_t;
typedef struct iso9660_dirent iso9660_dirent_t;
typedef struct px px_t;
typedef struct nm nm_t;
typedef struct rrip rrip_t;

enum
{
    ISO9660_VOLUME_BOOT_RECORD      = 0,
    ISO9660_VOLUME_PRIMARY          = 1,
    ISO9660_VOLUME_SUPPLEMENTARY    = 2,
    ISO9660_VOLUME_PARTITION        = 3,
    ISO9660_VOLUME_TERMINATOR       = 255,

    ISO9660_BLOCK_SIZE              = 2048,
    ISO9660_START_BLOCK             = 16,
    ISO9660_ROOT_INO                = 2
};

#define ISO9660_SIGNATURE       "CD001"
#define ISO9660_SIGNATURE_LEN   (sizeof(ISO9660_SIGNATURE) - 1)

#define PX_SIGNATURE    U16('P', 'X')
#define NM_SIGNATURE    U16('N', 'M')
#define NM_NAME_LEN(rr) ((size_t)(rr)->len - 5)

struct lsb_msb16
{
    uint16_t lsb;
    uint16_t msb;
};

struct lsb_msb32
{
    uint32_t lsb;
    uint32_t msb;
};

static inline uint32_t _lsb32_get(void* data)
{
    return ((lsb_msb32_t*)data)->lsb;
}

static inline uint16_t _lsb16_get(void* data)
{
    return ((lsb_msb16_t*)data)->lsb;
}

#define GET(value) \
    _Generic(value, \
        uint8_t: value, \
        uint16_t: value, \
        uint32_t: value, \
        char*: value, \
        lsb_msb16_t: _lsb16_get(&value), \
        lsb_msb32_t: _lsb32_get(&value))

struct iso9660_dirent
{
    uint8_t         len;
    uint8_t         ext_attr_len;
    lsb_msb32_t     lba;
    lsb_msb32_t     data_len;
    uint8_t         date[7];
    uint8_t         flags;
    uint8_t         file_unit_len;
    uint8_t         interleave_gap;
    lsb_msb16_t     vol_id;
    uint8_t         name_len;
    char            name[0];
} PACKED;

struct px
{
    uint64_t    mode;
    uint64_t    nlink;
    uint64_t    uid;
    uint64_t    gid;
    uint64_t    ino;
};

struct nm
{
    uint8_t     flags;
    char        name[0];
};

struct rrip
{
    uint16_t    sig;
    uint8_t     len;
    uint8_t     ver;
    union
    {
        px_t    px;
        nm_t    nm;
    };
};

struct iso9660_pvd
{
    char        sysident[32];
    char        volident[32];
    uint8_t     reserved1[8];
    lsb_msb32_t space_size;
    uint8_t     reserved2[32];
    lsb_msb16_t set_size;
    lsb_msb16_t sequence_nr;
    lsb_msb16_t block_size;
    lsb_msb32_t path_table_size;
    uint32_t    path_table_location;
    uint32_t    path_table_location_opt;
    uint32_t    unused1;
    uint32_t    unused2;
    union
    {
        iso9660_dirent_t root;
        uint8_t alignment[34];
    };
};

struct iso9660_sb
{
    uint8_t     type;
    char        identifier[5];
    uint8_t     version;
    uint8_t     unused;
    union
    {
        iso9660_pvd_t pvd;
    };
};

struct iso9660_data
{
    iso9660_sb_t* raw_sb;
    dev_t dev;
    file_t* file;
    iso9660_dirent_t* root;
};
