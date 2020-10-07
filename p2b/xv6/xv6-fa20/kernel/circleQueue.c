#include "circleQueue.h"
#include "param.h"

/**
 * Returns 1 if the set is currently empty, 0 otherwise
 */
char isEmpty(circleQueue pq) { return (pq.head == -1 || pq.tail == -1) }

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

  pq[pq.tail] = pid;
  pq.size++;
  return 0;
}

/**
 * Remove from the front of the queue by removing the head node
 */
int dequeue(circleQueue pq) {
  if (isEmpty(pq)) {
    return -1;
  }

  int retVal = pq[pq.head];
  pq[pq.head] = 0;

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