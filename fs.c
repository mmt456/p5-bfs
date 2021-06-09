// ============================================================================
// fs.c - user FileSytem API
// ============================================================================
#include "bfs.h"
#include "fs.h"

// ============================================================================
// Close the file currently open on file descriptor 'fd'.
// ============================================================================
i32 fsClose(i32 fd) { 
  i32 inum = bfsFdToInum(fd);
  bfsDerefOFT(inum);
  return 0; 
}



// ============================================================================
// Create the file called 'fname'.  Overwrite, if it already exsists.
// On success, return its file descriptor.  On failure, EFNF
// ============================================================================
i32 fsCreate(str fname) {
  i32 inum = bfsCreateFile(fname);
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Format the BFS disk by initializing the SuperBlock, Inodes, Directory and 
// Freelist.  On succes, return 0.  On failure, abort
// ============================================================================
i32 fsFormat() {
  FILE* fp = fopen(BFSDISK, "w+b");
  if (fp == NULL) FATAL(EDISKCREATE);

  i32 ret = bfsInitSuper(fp);               // initialize Super block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitInodes(fp);                  // initialize Inodes block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitDir(fp);                     // initialize Dir block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitFreeList();                  // initialize Freelist
  if (ret != 0) { fclose(fp); FATAL(ret); }

  fclose(fp);
  return 0;
}


// ============================================================================
// Mount the BFS disk.  It must already exist
// ============================================================================
i32 fsMount() {
  FILE* fp = fopen(BFSDISK, "rb");
  if (fp == NULL) FATAL(ENODISK);           // BFSDISK not found
  fclose(fp);
  return 0;
}



// ============================================================================
// Open the existing file called 'fname'.  On success, return its file 
// descriptor.  On failure, return EFNF
// ============================================================================
i32 fsOpen(str fname) {
  i32 inum = bfsLookupFile(fname);        // lookup 'fname' in Directory
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Read 'numb' bytes of data from the cursor in the file currently fsOpen'd on
// File Descriptor 'fd' into 'buf'.  On success, return actual number of bytes
// read (may be less than 'numb' if we hit EOF).  On failure, abort
// ============================================================================
i32 fsRead(i32 fd, i32 numb, void* buf) {
  // if not fail, read from fd numb number of times then put into buff
  // i32 bfsFdToInum(i32 fd);
  i32 inum = bfsFdToInum(fd);
  i32 fbn = 0;

  i32 ofte = bfsFindOFTE(inum);
  i32 cursor = g_oft[ofte].curs;

  while(cursor >= BYTESPERBLOCK) {
    cursor = cursor - BYTESPERBLOCK;
    fbn++;
  }

  i8 block[BYTESPERBLOCK];
  bfsRead(inum, fbn, block);

  Inode inode; 
  bfsReadInode(inum, &inode);
  i32 file_size = inode.size;

  i8 *temp_buf = buf;
  i32 count = 0;

  while (numb > 0) {
    i32 i_min = BYTESPERBLOCK;
    if (cursor + numb < i_min) {
      i_min = cursor + numb;
    }
    for (int i = cursor; i < i_min; i++) {
      temp_buf[count] = block[i];
      g_oft[ofte].curs++;
      count++;
      numb--;
      if (g_oft[ofte].curs >= file_size) {
        return count;
      }
    } 
    if (numb > 0) {
      bfsRead(inum, ++fbn, block);
      cursor = 0;
    }
  }
  return count;
}


// ============================================================================
// Move the cursor for the file currently open on File Descriptor 'fd' to the
// byte-offset 'offset'.  'whence' can be any of:
//
//  SEEK_SET : set cursor to 'offset'
//  SEEK_CUR : add 'offset' to the current cursor
//  SEEK_END : add 'offset' to the size of the file
//
// On success, return 0.  On failure, abort
// ============================================================================
i32 fsSeek(i32 fd, i32 offset, i32 whence) {

  if (offset < 0) FATAL(EBADCURS);
 
  i32 inum = bfsFdToInum(fd);
  i32 ofte = bfsFindOFTE(inum);
  
  switch(whence) {
    case SEEK_SET:
      g_oft[ofte].curs = offset;
      break;
    case SEEK_CUR:
      g_oft[ofte].curs += offset;
      break;
    case SEEK_END: {
        i32 end = fsSize(fd);
        g_oft[ofte].curs = end + offset;
        break;
      }
    default:
        FATAL(EBADWHENCE);
  }
  return 0;
}



// ============================================================================
// Return the cursor position for the file open on File Descriptor 'fd'
// ============================================================================
i32 fsTell(i32 fd) {
  return bfsTell(fd);
}



// ============================================================================
// Retrieve the current file size in bytes.  This depends on the highest offset
// written to the file, or the highest offset set with the fsSeek function.  On
// success, return the file size.  On failure, abort
// ============================================================================
i32 fsSize(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  return bfsGetSize(inum);
}



// ============================================================================
// Write 'numb' bytes of data from 'buf' into the file currently fsOpen'd on
// filedescriptor 'fd'.  The write starts at the current file offset for the
// destination file.  On success, return 0.  On failure, abort
// ============================================================================
i32 fsWrite(i32 fd, i32 numb, void* buf) {
  i32 inum = bfsFdToInum(fd);
  i32 fbn = 0;

  i32 ofte = bfsFindOFTE(inum);
  i32 cursor = g_oft[ofte].curs;

  // find which fileblock curosr is in
  while(cursor >= BYTESPERBLOCK) { 
    cursor = cursor - BYTESPERBLOCK;
    fbn++;
  }

  i8 block[BYTESPERBLOCK];
  bfsRead(inum, fbn, block);

  Inode inode; 
  bfsReadInode(inum, &inode);
  i32 file_size = inode.size;

  i8 *temp_buf = buf;
  i32 count = 0;

  while (numb > 0) {
    i32 i_min = BYTESPERBLOCK;
    if (cursor + numb < i_min) {
      i_min = cursor + numb;
    }
    for (int i = cursor; i < i_min; i++) {
      block[i] = temp_buf[count];
      g_oft[ofte].curs++;
      count++;
      numb--;
      if (g_oft[ofte].curs >= file_size) {
        return count;
      }
    } 
    i32 dbn = bfsFbnToDbn(inum, fbn);
    bioWrite(dbn, block);
    if (numb > 0) {
      bfsRead(inum, ++fbn, block);
      cursor = 0;
    }
  }
  return count;


  FATAL(ENYI);                                  // Not Yet Implemented!
  return 0;
}
