/**
 * Code from
 * https://www.geeksforgeeks.org/c/c-program-to-implement-priority-queue/
 */

#ifndef __AUX_DS_H_
#define __AUX_DS_H_

#include <stdio.h>
#include <stdlib.h>

#include <dla_interface.h>

// Define maximum size of the priority queue
#define MAX_CPU_QUEUE_ITEMS 100

struct cpu_op {
	struct dla_common_op_desc *op_desc;
};

// Define priority_queue structure
struct priority_queue {
    // int items[MAX_CPU_QUEUE_ITEMS];
    struct cpu_op items[MAX_CPU_QUEUE_ITEMS];
    int size;

    struct cpu_op d_item;

    void (*enqueue)(struct priority_queue* pq, struct dla_common_op_desc *value);
    struct cpu_op *(*dequeue)(struct priority_queue* pq);
    struct cpu_op *(*peek)(struct priority_queue* pq);
};

void swap(struct cpu_op *op_a, struct cpu_op *op_b);

void heapifyUp(struct priority_queue *pq, int index);

void _enqueue(struct priority_queue* pq, struct dla_common_op_desc *value);

int heapifyDown(struct priority_queue* pq, int index);

struct cpu_op *_dequeue(struct priority_queue* pq);

struct cpu_op *_peek(struct priority_queue* pq);


////////////////////////////////////////////////////

/*
// Define swap function to swap two integers
void swap(int* a, int* b);

// Define heapifyUp function to maintain heap property
// during insertion
void heapifyUp(struct priority_queue* pq, int index);

// Define enqueue function to add an item to the queue
void _enqueue(struct priority_queue* pq, int value);

// Define heapifyDown function to maintain heap property
// during deletion
int heapifyDown(struct priority_queue* pq, int index);

// Define dequeue function to remove an item from the queue
int _dequeue(struct priority_queue* pq);

// Define peek function to get the top item from the queue
int _peek(struct priority_queue* pq);
*/

#endif
