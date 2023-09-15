#include <stdio.h>     
#include <stdlib.h>   
#include <stdint.h>  
#include <inttypes.h>  
#include <errno.h>     // for EINTR
#include <fcntl.h>     
#include <unistd.h>    
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include "common.h"

// Print out the usage of the program and exit.
void Usage(char*);
// Hash function
uint32_t jenkins_one_at_a_time_hash(const uint8_t* , uint64_t );

void* tree(void*);

// block size
#define BSIZE 4096

struct treeParam{
  uint numThread;
  uint chunkSize;
  uint tid;
  uint8_t* mapAddr;
};

int main(int argc, char** argv) 
{
  int32_t fd;
  uint32_t nblocks;

  // input checking 
  if (argc != 3)
    Usage(argv[0]);

  // open input file
  fd = open(argv[1], O_RDWR);
  if (fd == -1) {
    perror("open failed");
    exit(EXIT_FAILURE);
  }
  // use fstat to get file size
  struct stat fileStat;
  fstat(fd, &fileStat);
  off_t fileSize = fileStat.st_size;
  
  // calculate nblocks (might need fixing)
  if (fileSize%BSIZE != 0){
    nblocks = (fileSize/BSIZE) + 1;
  }
  else{
    nblocks = (fileSize/BSIZE);
  }

  // num. of threads & block allocation per thread as requested by user for parallel processing
  uint numThread = atoi(argv[2]);
  uint numBlocksThread = (nblocks/numThread);
  uint chunkSize = numBlocksThread * BSIZE;
  
  // mapped memory of file
  uint8_t* mapAddr = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);

  // pass required parameters into the tree hash function
  struct treeParam param;
  param.numThread = numThread;
  param.chunkSize = chunkSize;
  param.tid = 0;
  param.mapAddr = mapAddr;

  // shows the user num. of blocks within file, and blocks allocated per thread
  printf(" no. of blocks = %u \n", nblocks);
  printf("Blocks per thread: %u \n", numBlocksThread);

  double start = GetTime();

  // calculate hash value of the input file
  pthread_t hashThread;
  void* hash;
  
  pthread_create(&hashThread, NULL, tree, &param);
  pthread_join(hashThread, &hash);

  
  double end = GetTime();
  printf("hash value = %u \n", (uint32_t)(uintptr_t)hash);
  printf("time taken = %f \n", (end - start));
  munmap(mapAddr, fileSize);
  close(fd);
  return EXIT_SUCCESS;
}

// the thread
void* tree(void* arg) 
{
  struct treeParam* param = (struct treeParam*) arg;

  uint32_t hashValue = jenkins_one_at_a_time_hash(param->mapAddr + param->tid * param->chunkSize, param->chunkSize);

  int leftIndex = 2 * param->tid + 1;
  int rightIndex = leftIndex + 1;
  
  pthread_t left;
  pthread_t right;
  void* leftHash;
  void* rightHash;  

  if (leftIndex < param->numThread) {
    struct treeParam leftParam = *param;
    leftParam.tid = leftIndex;
    pthread_create(&left, NULL, tree, &leftParam);
  }

  if (rightIndex < param->numThread) {
    struct treeParam rightParam = *param;
    rightParam.tid = rightIndex;
    pthread_create(&right, NULL, tree, &rightParam);
  }
  
  if (leftIndex < param->numThread) {
    pthread_join(left, &leftHash);
  }

  if (rightIndex < param->numThread) {
    pthread_join(right, &rightHash);
  }

  if (leftIndex >= param->numThread && rightIndex >= param->numThread) {
    pthread_exit((void*)(uintptr_t)hashValue);
  }

  char currentHashVal[100];
  char leftHashVal[100];
  char rightHashVal[100];
  char concatHash[300];
  sprintf(currentHashVal, "%u", hashValue);
  sprintf(leftHashVal, "%u", (uint32_t)(uintptr_t)leftHash);
  sprintf(rightHashVal, "%u", (uint32_t)(uintptr_t)rightHash);

  if (leftIndex < param->numThread) {
    strcpy(concatHash, currentHashVal); 
    strcat(concatHash, leftHashVal);    
    if (rightIndex < param->numThread) {
      strcat(concatHash, rightHashVal);
    }
    hashValue = jenkins_one_at_a_time_hash((uint8_t*)concatHash, strlen(concatHash));
  }

  return (void*)(uintptr_t)hashValue;
}

// hash function
uint32_t jenkins_one_at_a_time_hash(const uint8_t* key, uint64_t length) 
{
  uint64_t i = 0;
  uint32_t hash = 0;

  while (i != length) {
    hash += key[i++];
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}
// tells user the appropriate arguments for the program
void Usage(char* s) 
{
  fprintf(stderr, "Usage: %s filename num_threads \n", s);
  exit(EXIT_FAILURE);
}
