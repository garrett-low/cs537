#include "circleQueue.h"
#include "param.h"

/**
 * Returns 1 if the set is currently empty, 0 otherwise
 */
char isEmpty(circleQueue pq) { return (pq.head == -1 || pq.tail == -1) }

/**
 * Add to the back of the queue by inserting after tail node
 */
int enqueue(circleQueue pq, int pid) {
  if (pq.size >= NPROC) {
    return -1;
  }

  if (isEmpty(pq)) {
    pq.head++;
    pq.tail++;
  } else {
    pq.tail = (pq.tail + 1) % NPROC
  }

  pq.pid[pq.tail] = pid;
  pq.size++;
  return pid;
}

/**
 * Remove from the front of the queue by removing the head node
 */
int dequeue(circleQueue pq) {
  if (isEmpty(pq)) {
    return -1;
  }

  int retVal = pq[pq.head];
  pq.pid[pq.head] = 0;

  if (pq.head == pq.tail) {
    setQueueEmpty(pq);
  } else {
    pq.head = (pq.head + 1) % NPROC
  }
  
  pq.size--;
  return retVal;
}

void setQueueEmpty(circleQueue pq) {
  pq.head = -1;
  pq.tail = -1;
  pq.size = 0;
}

/**
 * Returns pid of front of queue without dequeuing
 */ 
int peek(circleQueue pq) {
  if (isEmpty(pq))
    return -1;
  return pq.pid[pq.head];
}

/**
 * Move the specified pid to the head where it can be dequeued if needed
 * 
 * Returns:
 * -1 on error
 * Otherwise, returns pid of the old head that was swapped.
 */
int swapHead(circleQueue pq, int pid) {
  if (isEmpty(pq))
    return -1;
  
  // pid already at head if only item in line
  if (pq.size == 1)
    return pid;
  
  // find pid in PQ
  int currIndex = -1;
  for(int i = 0; i < NPROC; i++) {
    if (pq.pid[i] == pid) {
      currIndex = i;
      break;
    }
  }
  if (currIndex == -1) { // not found
    return -1; 
  }
  
  // swap
  int temp = pq.pid[head];
  pq.pid[head] = pid;
  pq.pid[currIndex] = temp;
  
  return temp;
}