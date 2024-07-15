#pragma once

#include <stdint.h>
#include <kernel/dev.h>
#include <kernel/compiler.h>
#include <kernel/api/types.h>

#define EXT2_BLOCK_SIZE         1024
#define EXT2_SUPERBLOCK_OFFSET  1024
#define EXT2_NDIR_BLOCKS        12
#define EXT2_IND_BLOCK          EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK         (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK         (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS           (EXT2_TIND_BLOCK + 1)
#define EXT2_NAME_LEN           255
#define EXT2_ROOT_INO           2
#define EXT2_SIGNATURE          0xef53
#define EXT2_LOST_FOUND_INO     11

struct file;

typedef struct ext2_data ext2_data_t;
typedef struct ext2_superblock ext2_sb_t;
typedef struct ext2fs_inode ext2_inode_t;
typedef struct ext2_dir_entry ext2_dir_entry_t;
typedef struct ext2_block_group_desc ext2_bgd_t;

struct ext2fs_inode
{
    uint16_t    mode;           /* File mode */
    uint16_t    uid;            /* Low 16 bits of Owner Uid */
    uint32_t    size;           /* Size in bytes */
    uint32_t    atime;          /* Access time */
    uint32_t    ctime;          /* Creation time */
    uint32_t    mtime;          /* Modification time */
    uint32_t    dtime;          /* Deletion Time */
    uint16_t    gid;            /* Low 16 bits of Group Id */
    uint16_t    links_count;    /* Links count */
    uint32_t    blocks;         /* Blocks count */
    uint32_t    flags;          /* File flags */
    uint32_t    reserved1;
    uint32_t    block[EXT2_N_BLOCKS]; /* Pointers to blocks */
    uint32_t    generation;     /* File version (for NFS) */
    uint32_t    file_acl;       /* File ACL */
    uint32_t    dir_acl;        /* Directory ACL */
    uint32_t    faddr;          /* Fragment address */
    uint8_t     frag;           /* Fragment number */
    uint8_t     fsize;          /* Fragment size */
    uint16_t    pad1;
    uint16_t    uid_high;
    uint16_t    gid_high;
    uint32_t    reserved2;
};

struct ext2_dir_entry
{
    uint32_t    inode;          /* Inode number */
    uint16_t    rec_len;        /* Directory entry length */
    uint8_t     name_len;       /* Name length */
    uint8_t     file_type;
    char        name[EXT2_NAME_LEN]; /* File name */
};

struct ext2_block_group_desc
{
    uint32_t    block_bitmap;       /* Blocks bitmap block */
    uint32_t    inode_bitmap;       /* Inodes bitmap block */
    uint32_t    inode_table;        /* Inodes table block */
    uint16_t    free_blocks_count;  /* Free blocks count */
    uint16_t    free_inodes_count;  /* Free inodes count */
    uint16_t    used_dirs_count;    /* Directories count */
    uint16_t    pad;
    uint32_t    reserved[3];
};

struct ext2_superblock
{
    uint32_t    inodes_count;       /* Inodes count */
    uint32_t    blocks_count;       /* Blocks count */
    uint32_t    r_blocks_count;     /* Reserved blocks count */
    uint32_t    free_blocks_count;  /* Free blocks count */
    uint32_t    free_inodes_count;  /* Free inodes count */
    uint32_t    first_data_block;   /* First Data Block */
    uint32_t    log_block_size;     /* Block size */
    uint32_t    log_frag_size;      /* Fragment size */
    uint32_t    blocks_per_group;   /* # Blocks per group */
    uint32_t    frags_per_group;    /* # Fragments per group */
    uint32_t    inodes_per_group;   /* # Inodes per group */
    uint32_t    mtime;              /* Mount time */
    uint32_t    wtime;              /* Write time */
    uint16_t    mnt_count;          /* Mount count */
    uint16_t    max_mnt_count;      /* Maximal mount count */
    uint16_t    magic;              /* Magic signature */
    uint16_t    state;              /* File system state */
    uint16_t    errors;             /* Behaviour when detecting errors */
    uint16_t    minor_rev_level;    /* minor revision level */
    uint32_t    lastcheck;          /* time of last check */
    uint32_t    checkinterval;      /* max. time between checks */
    uint32_t    creator_os;         /* OS */
    uint32_t    rev_level;          /* Revision level */
    uint16_t    def_resuid;         /* Default uid for reserved blocks */
    uint16_t    def_resgid;         /* Default gid for reserved blocks */
    uint32_t    first_ino;          /* First non-reserved inode */
    uint16_t    inode_size;         /* size of inode structure */
    uint16_t    block_group_nr;     /* block group # of this superblock */
    uint32_t    feature_compat;     /* compatible feature set */
    uint32_t    feature_incompat;   /* incompatible feature set */
    uint32_t    feature_ro_compat;  /* readonly-compatible feature set */
    uint8_t     uuid[16];           /* 128-bit uuid for volume */
    char        volume_name[16];    /* volume name */
    char        last_mounted[64];   /* directory where last mounted */
    uint32_t    algorithm_usage_bitmap; /* For compression */
    uint8_t     prealloc_blocks;    /* Nr of blocks to try to preallocate*/
    uint8_t     prealloc_dir_blocks;/* Nr to preallocate for dirs */
    uint16_t    padding1;
    uint8_t     journal_uuid[16];   /* uuid of journal superblock */
    uint32_t    journal_inum;       /* inode number of journal file */
    uint32_t    journal_dev;        /* device number of journal file */
    uint32_t    last_orphan;        /* start of list of inodes to delete */
    uint32_t    hash_seed[4];       /* HTREE hash seed */
    uint8_t     def_hash_version;   /* Default hash version to use */
    uint8_t     reserved_char_pad;
    uint16_t    reserved_word_pad;
    uint32_t    default_mount_opts;
    uint32_t    first_meta_bg;      /* First metablock block group */
    uint32_t    reserved[190];      /* Padding to the end of the block */
};

struct ext2_data
{
    uint32_t    last_ind_block;
    uint32_t    first_dind_block;
    uint32_t    last_dind_block;
    uint32_t    addr_per_block;
    uint32_t    inodes_per_group;
    uint32_t    inodes_per_block;
    ext2_sb_t*  sb;
    dev_t       dev;
    struct file* file;
};

