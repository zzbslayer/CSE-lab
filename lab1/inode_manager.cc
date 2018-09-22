#include "inode_manager.h"

void printInode(struct inode *inode){
  fprintf(stderr, "temp:%d\n", inode->size / BLOCK_SIZE);
  int iBlockNum = inode->size / BLOCK_SIZE + (inode->size % BLOCK_SIZE > 0);
  blockid_t *blocks = inode->blocks;
  for (int i= 0 ; i < iBlockNum; i++){
    fprintf(stderr,"\t\t\tblocks[%d]=%d",i,blocks[i]);
  }
  fprintf(stderr, "\n");
}
// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  memcpy(buf, (char *)blocks[id], BLOCK_SIZE); 
}

void
disk::write_block(blockid_t id, const char *buf)
{
  memcpy((char *)blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  for (int i=2+INODE_NUM; i<BLOCK_NUM; i++){
    if (using_blocks[i] == 0){
      using_blocks[i] = 1;
      fprintf(stderr, "\t\tim: alloc_block %d\n", i);
      return i;
    }
  }

  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  fprintf(stderr, "\t\tim: free_block %d\n", id);
  if (using_blocks.count(id)){
    using_blocks[id] = 0;
  }
  
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
  
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  fprintf(stderr, "\t\tim: read_block %d\n", id);
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  fprintf(stderr, "\t\tim: write_block %d\n", id);
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  int i;
  for (i = 1; i < INODE_NUM + 1; i++){
    if (!get_inode(i)){
      fprintf(stderr, "\t\tim: alloc_inode %d\n", i);

      struct inode inode;
      inode.type = type;
      inode.size = 0;
      put_inode(i, &inode);
      //bm->write_block(IBLOCK(i, bm->sb.nblocks), buf);
      return i;
    }
  } 
  fprintf(stderr, "\t\tim: alloc_inode fail\n");
  return i;
}

void
inode_manager::free_inode(uint32_t inum)
{
  struct inode *inode_disk = get_inode(inum);
  if (!inode_disk)
    return;

  printf("\tim: free_inode %d\n", inum);
  inode_disk->type = 0;
  put_inode(inum, inode_disk);
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];
  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  printInode(ino);
  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;
  printInode(ino);

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  struct inode *inode_disk = get_inode(inum);
  if (!inode_disk)
    return;
  
  fprintf(stderr, "read file inode num:%d, size:%d\n", inum, *size);
  int blockNum = inode_disk->size / BLOCK_SIZE + (inode_disk->size % BLOCK_SIZE > 0);
  char *temp = (char *) malloc(blockNum * BLOCK_SIZE);
  int indirect[NINDIRECT];

  printInode(inode_disk);

  if (blockNum > NDIRECT)
    bm->read_block(inode_disk->blocks[NDIRECT], (char *)indirect);
  for (int i = 0; i < blockNum; i++){
    if (i < NDIRECT)
      bm->read_block(inode_disk->blocks[i], temp + i * BLOCK_SIZE);
    else 
      bm->read_block(indirect[i-NDIRECT], temp + i * BLOCK_SIZE);
  }
  *buf_out = temp;
  *size = inode_disk->size;
  return;
}



/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  struct inode *inode_disk = get_inode(inum);
  if (!inode_disk)
    return;
  fprintf(stderr, "write file inode num:%d, size:%d\n", inum, size);

  int isize = inode_disk->size;
  int blockNum = size / BLOCK_SIZE + (size % BLOCK_SIZE > 0);
  int iBlockNum = isize / BLOCK_SIZE + (isize % BLOCK_SIZE > 0);
  if (blockNum > MAXFILE)
    return;
  printInode(inode_disk);
  char zero[BLOCK_SIZE];
  bzero(zero, sizeof(zero));
  int indirect[NINDIRECT];
  
  fprintf(stderr, "iBlockNum:%d, blockNum:%d\n", iBlockNum, blockNum);
  bm->read_block(inode_disk->blocks[NDIRECT], (char *)indirect);

  if (iBlockNum > blockNum){
    if (iBlockNum < NDIRECT) {
      for (int i = blockNum; i < iBlockNum; i++){
        bm->free_block(inode_disk->blocks[i]);
      }
    }
    else {
      bm->read_block(inode_disk->blocks[NDIRECT], (char *)indirect);
      if (blockNum >= NDIRECT){
        for (int i = NDIRECT; i < iBlockNum; i++){
          bm->free_block(indirect[i - NDIRECT]);
        }
      }
      else{
        for (int i = blockNum; i < NDIRECT; i++){
          bm->free_block(inode_disk->blocks[i]);
        }
        for (int i = NDIRECT; i < iBlockNum; i++){
          bm->free_block(indirect[i - NDIRECT]);
        }
      }
      
      if (blockNum <= NDIRECT){
        bm->free_block(inode_disk->blocks[NDIRECT]);
        inode_disk->blocks[NDIRECT] = 0;
      }
    }
  }
  else if (iBlockNum < blockNum) {
    if (blockNum < NDIRECT){
      for (int i = iBlockNum; i < blockNum; i++){
        inode_disk->blocks[i] = bm->alloc_block();
      }
    }
    else{
      if (iBlockNum < NDIRECT){

        for (int i = iBlockNum; i < NDIRECT; i++){
          inode_disk->blocks[i] = bm->alloc_block();
        }
        inode_disk->blocks[NDIRECT] = bm->alloc_block();

      }
      else {
        for (int i = iBlockNum; i < blockNum; i++){
          indirect[i-NDIRECT] = bm->alloc_block();
        }
      }
      bm->write_block(inode_disk->blocks[NDIRECT], (char *) indirect);
    }
  }

  for (int i = 0; i < blockNum; i++) {
      if (i < NDIRECT) {
        fprintf(stderr, "i:%d  block:%d\n",i,inode_disk->blocks[i]);
        bm->write_block(inode_disk->blocks[i], zero);
        bm->write_block(inode_disk->blocks[i], buf + i * BLOCK_SIZE);
      }
      else {
        fprintf(stderr, "indirect   i:%d  block:%d\n",i,inode_disk->blocks[i]);
        bm->write_block(indirect[i-NDIRECT], zero);
        bm->write_block(indirect[i-NDIRECT], buf + i * BLOCK_SIZE);
      }
  }

  inode_disk->size = size;
  put_inode(inum, inode_disk);
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */

  struct inode *inode_disk = get_inode(inum);
  if (!inode_disk)
    return;
  a.atime = inode_disk->atime;
  a.mtime = inode_disk->mtime;
  a.ctime = inode_disk->ctime;
  a.size = inode_disk->size;
  a.type = inode_disk->type;
  
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  struct inode *inode_disk = get_inode(inum);
  if (!inode_disk)
    return;

  fprintf(stderr, "remove file inode num:%d\n", inum);
  int isize = inode_disk->size;
  int iBlockNum = isize / BLOCK_SIZE + (isize % BLOCK_SIZE > 0);
  if (iBlockNum < NDIRECT){
    for (int i = 0; i < iBlockNum; i++){
      bm->free_block(inode_disk->blocks[i]);
    }
  }
  else{
    int indirect[NINDIRECT];
    bm->read_block(inode_disk->blocks[NDIRECT], (char *)indirect);

    for (int i = 0; i< NDIRECT; i++){
      bm->free_block(inode_disk->blocks[i]);
    }
    for (int i = NDIRECT; i<iBlockNum; i++){
      bm->free_block(indirect[i-NDIRECT]);
    }
    bm->free_block(inode_disk->blocks[NDIRECT]);
  }
  free_inode(inum);
  return;
}
