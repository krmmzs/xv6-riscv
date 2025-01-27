// disk buffer
// The sleep-lock protects reads and writes of the block’s buffered content,
// the bcache.lock protects information about which blocks are cached.
// sleeplock protects valid, disk, data[]
// other fields are protected by bcache.lock(spinlock)
struct buf {
    int valid;   // has data been read from disk?
    int disk;    // does disk "own" buf?
    uint dev;
    uint blockno;
    struct sleeplock lock;
    uint refcnt; // whether this block is in use and the use count.
    struct buf *prev; // LRU cache list
    struct buf *next;
    uchar data[BSIZE];
};

