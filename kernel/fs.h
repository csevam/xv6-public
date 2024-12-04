

#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

#define IPB           (BSIZE / sizeof(struct dinode))

#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

#define BPB           (BSIZE*8)

#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

