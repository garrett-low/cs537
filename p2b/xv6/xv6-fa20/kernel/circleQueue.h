#include "param.h"
#include "proc.h"

typedef struct {
   int pid[NPROC]; // store the PIDs in a queue
   int head;
   int tail;
   int size;
} circleQueue;

char isEmpty(circleQueue pq);
int enqueue(circleQueue pq, int pid);
int dequeue(circleQueue pq);
void setQueueEmpty(circleQueue pq);
int peek(circleQueue pq);