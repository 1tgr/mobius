/* $Id: ext2.h,v 1.1 2002/12/21 09:48:44 pavlovskii Exp $ */

#ifndef EXT2_H__
#define EXT2_H__

#define EXT2_OS_LINUX                   0
#define EXT2_OS_HURD                    1
#define EXT2_OS_MASIX                   2
#define EXT2_OS_FREEBSD                 3
#define EXT2_OS_LITES                   4
#define EXT2_OS_LAST                    4

#define EXT2_GOOD_OLD_REV               0       /* The good old (original) format */
#define EXT2_DYNAMIC_REV                1       /* V2 format w/ dynamic inode sizes */

#define EXT2_GOOD_OLD_INODE_SIZE        128
#define EXT2_VALID_FS                   0x0001  /* Unmounted cleanly */
#define EXT2_ERROR_FS                   0x0002  /* Errors detected */
#define EXT2_MAGIC                      0xEF53

#define EXT2_ERRORS_CONTINUE            1 /* continue as if nothing happened */
#define EXT2_ERRORS_RO                  2 /* remount read-only */
#define EXT2_ERRORS_PANIC               3 /* cause a kernel panic */
#define EXT2_ERRORS_DEFAULT             EXT2_ERRORS_RO

#define EXT2_BAD_INO                    1      /* Bad blocks inode */
#define EXT2_ROOT_INO                   2      /* Root inode */
#define EXT2_ACL_IDX_INO                3      /* ACL inode */                         #define EXT2_ACL_DATA_INO        4      /* ACL inode */
#define EXT2_BOOT_LOADER_INO            5      /* Boot loader inode */
#define EXT2_UNDEL_DIR_INO              6      /* Undelete directory inode */
#define EXT2_GOOD_OLD_FIRST_INO         11

#define EXT2_NDIR_BLOCKS                12
#define EXT2_IND_BLOCK                  EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK                 (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK                 (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS                   (EXT2_TIND_BLOCK + 1)

#define EXT2_SECRM_FL                   0x00000001 /* Secure deletion */
#define EXT2_UNRM_FL                    0x00000002 /* Undelete */
#define EXT2_COMPR_FL                   0x00000004 /* Compress file */
#define EXT2_SYNC_FL                    0x00000008 /* Synchronous updates */
#define EXT2_IMMUTABLE_FL               0x00000010 /* Immutable file */
#define EXT2_APPEND_FL                  0x00000020 /* writes to file may only append */
#define EXT2_NODUMP_FL                  0x00000040 /* do not dump file */
#define EXT2_NOATIME_FL                 0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define EXT2_DIRTY_FL                   0x00000100
#define EXT2_COMPRBLK_FL                0x00000200 /* One or more compressed clusters */
#define EXT2_NOCOMP_FL                  0x00000400 /* Don't compress */
#define EXT2_ECOMPR_FL                  0x00000800 /* Compression error */
/* End compression flags --- maybe not all used */
#define EXT2_BTREE_FL                   0x00001000 /* btree format dir */
#define EXT2_RESERVED_FL                0x80000000 /* reserved for ext2 lib */
#define EXT2_FL_USER_VISIBLE            0x00001FFF /* User visible flags */
#define EXT2_FL_USER_MODIFIABLE         0x000000FF /* User modifiable flags */

#define EXT2_S_IFMT                     0xF000
#define EXT2_S_IFIFO                    0x1000
#define EXT2_S_IFCHR                    0x2000
#define EXT2_S_IFDIR                    0x4000
#define EXT2_S_IFBLK                    0x6000
#define EXT2_S_IFREG                    0x8000
#define EXT2_S_IFSOCK                   0xA000
#define EXT2_S_IFLNK                    0xC000
#define EXT2_S_IRUSR                    0x0100
#define EXT2_S_IWUSR                    0x0080
#define EXT2_S_IXUSR                    0x0040
#define EXT2_S_IRGRP                    0x0020
#define EXT2_S_IWGRP                    0x0010
#define EXT2_S_IXGRP                    0x0008
#define EXT2_S_IROTH                    0x0004
#define EXT2_S_IWOTH                    0x0002
#define EXT2_S_IXOTH                    0x0001

#define EXT2_FT_UNKNOWN                 0x00
#define EXT2_FT_REG_FILE                0x01
#define EXT2_FT_DIR                     0x02
#define EXT2_FT_CHRDEV                  0x03
#define EXT2_FT_BLKDEV                  0x04
#define EXT2_FT_FIFO                    0x05
#define EXT2_FT_SOCK                    0x06
#define EXT2_FT_SYMLINK                 0x07
#define EXT2_FT_MAX                     0x08

#define EXT2_NAME_LEN                   255

typedef struct ext2_super_block_t ext2_super_block_t;
struct ext2_super_block_t
{
    uint32_t    s_inodes_count;         /* Inodes count */
    uint32_t    s_blocks_count;         /* Blocks count */
    uint32_t    s_r_blocks_count;       /* Reserved blocks count */
    uint32_t    s_free_blocks_count;    /* Free blocks count */
    uint32_t    s_free_inodes_count;    /* Free inodes count */
    uint32_t    s_first_data_block;     /* First Data Block */
    uint32_t    s_log_block_size;       /* Block size */
    int32_t     s_log_frag_size;        /* Fragment size */
    uint32_t    s_blocks_per_group;     /* # Blocks per group */
    uint32_t    s_frags_per_group;      /* # Fragments per group */
    uint32_t    s_inodes_per_group;     /* # Inodes per group */
    uint32_t    s_mtime;                /* Mount time */
    uint32_t    s_wtime;                /* Write time */
    uint16_t    s_mnt_count;            /* Mount count */
    int16_t     s_max_mnt_count;        /* Maximal mount count */
    uint16_t    s_magic;                /* Magic signature */
    uint16_t    s_state;                /* File system state */
    uint16_t    s_errors;               /* Behaviour when detecting errors */
    uint16_t    s_minor_rev_level;      /* minor revision level */
    uint32_t    s_lastcheck;            /* time of last check */
    uint32_t    s_checkinterval;        /* max. time between checks */
    uint32_t    s_creator_os;           /* OS */
    uint32_t    s_rev_level;            /* Revision level */
    uint16_t    s_def_resuid;           /* Default uid for reserved blocks */
    uint16_t    s_def_resgid;           /* Default gid for reserved blocks */
    uint32_t    s_first_ino;            /* First non-reserved inode */
    uint16_t    s_inode_size;           /* size of inode structure */
    uint16_t    s_block_group_nr;       /* block group # of this superblock */
    uint32_t    s_feature_compat;       /* compatible feature set */
    uint32_t    s_feature_incompat;     /* incompatible feature set */
    uint32_t    s_feature_ro_compat;    /* readonly-compatible feature set */
    uint8_t     s_uuid[16];             /* 128-bit uuid for volume */
    char        s_volume_name[16];      /* volume name */
    char        s_last_mounted[64];     /* directory where last mounted */
    uint32_t    s_algorithm_usage_bitmap; /* For compression */
    uint8_t     s_prealloc_blocks;      /* Nr of blocks to try to preallocate*/
    uint8_t     s_prealloc_dir_blocks;  /* Nr to preallocate for dirs */
    uint16_t    s_padding1;             
    uint32_t    s_reserved[204];        /* Padding to the end of the block */
};

typedef struct ext2_group_desc_t ext2_group_desc_t;
struct ext2_group_desc_t
{
    uint32_t    bg_block_bitmap;        /* Blocks bitmap block */
    uint32_t    bg_inode_bitmap;        /* Inodes bitmap block */
    uint32_t    bg_inode_table;         /* Inodes table block */
    uint16_t    bg_free_blocks_count;   /* Free blocks count */
    uint16_t    bg_free_inodes_count;   /* Free inodes count */
    uint16_t    bg_used_dirs_count;     /* Directories count */
    uint16_t    bg_pad;
    uint32_t    bg_reserved[3];
};

typedef struct ext2_inode_t ext2_inode_t;
struct ext2_inode_t
{
    uint16_t    i_mode;                 /* File mode */
    uint16_t    i_uid;                  /* Low 16 bits of Owner Uid */
    uint32_t    i_size;                 /* Size in bytes */
    uint32_t    i_atime;                /* Access time */
    uint32_t    i_ctime;                /* Creation time */
    uint32_t    i_mtime;                /* Modification time */
    uint32_t    i_dtime;                /* Deletion Time */
    uint16_t    i_gid;                  /* Low 16 bits of Group Id */
    uint16_t    i_links_count;          /* Links count */
    uint32_t    i_blocks;               /* Blocks count */
    uint32_t    i_flags;                /* File flags */
    union
    {
        struct
        {
            uint32_t    l_i_reserved1;
        } linux1;
        struct
        {
            uint32_t    h_i_translator;
        } hurd1;
        struct
        {
            uint32_t    m_i_reserved1;
        } masix1;
    } osd1;                             /* OS dependent 1 */
    uint32_t    i_block[EXT2_N_BLOCKS]; /* Pointers to blocks */
    uint32_t    i_generation;           /* File version (for NFS) */
    uint32_t    i_file_acl;             /* File ACL */
    uint32_t    i_dir_acl;              /* Directory ACL */
    uint32_t    i_faddr;                /* Fragment address */
    union
    {
        struct
        {
            uint8_t     l_i_frag;       /* Fragment number */
            uint8_t     l_i_fsize;      /* Fragment size */
            uint16_t    i_pad1;
            uint16_t    l_i_uid_high;   /* these 2 fields    */
            uint16_t    l_i_gid_high;   /* were reserved2[0] */
            uint32_t    l_i_reserved2;
        } linux2;
        struct
        {
            uint8_t     h_i_frag;       /* Fragment number */
            uint8_t     h_i_fsize;      /* Fragment size */
            uint16_t    h_i_mode_high;
            uint16_t    h_i_uid_high;
            uint16_t    h_i_gid_high;
            uint32_t    h_i_author;
        } hurd2;
        struct
        {
            uint8_t    m_i_frag;       /* Fragment number */
            uint8_t    m_i_fsize;      /* Fragment size */
            uint16_t   m_pad1;     
            uint32_t   m_i_reserved2[2];
        } masix2;
    } osd2;                             /* OS dependent 2 */
};

typedef struct ext2_dir_entry_t ext2_dir_entry_t;
struct ext2_dir_entry_t
{
    uint32_t    inode;                  /* Inode number */
    uint16_t    rec_len;                /* Directory entry length */
    uint8_t     name_len;               /* Name length */
    uint8_t     padding;
    char        name[EXT2_NAME_LEN];    /* File name */
};

#endif
