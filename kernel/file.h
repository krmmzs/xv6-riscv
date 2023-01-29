// user-mode only.
// fopen(), fread(), fwrite(), fclose().
// file struct has no lock, fields is protected by other wrapper struct.
// ref is protected by the lock in ftable struct...
// type is set when allocated and does not change.
// off is protected by the lock in the inode struct.
// Other fields do not change once allocated.
struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
    int ref; // reference count
    char readable;
    char writable;
    struct pipe *pipe; // FD_PIPE
    struct inode *ip;  // FD_INODE and FD_DEVICE
    uint off;          // FD_INODE
    short major;       // FD_DEVICE
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

// in-memory copy of an inode
// ---------------------------
// Cache in in-memory
struct inode {
    uint dev;           // Device number, in order to map to inode in disk
    uint inum;          // Inode number, in order to map to inode in disk
    int ref;            // Reference count, can think of it as the number of threads that are currently using this cached, (inode C pointer)
    struct sleeplock lock; // protects everything below here
    int valid;          // inode has been read from disk?

    short type;         // copy of disk inode
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT+1];
};

// map major device number to device functions.
struct devsw {
    int (*read)(int, uint64, int);
    int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
