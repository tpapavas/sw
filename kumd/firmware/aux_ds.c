#include "aux_ds.h"
#include <nvdla_interface.h>

// Define swap function to swap two integers
void swap(struct cpu_op *op_a, struct cpu_op *op_b)
{
    struct cpu_op temp_cpu_op;
    
    dla_memcpy(&temp_cpu_op, op_a, sizeof(struct cpu_op));
    dla_memcpy(op_a, op_b, sizeof(struct cpu_op));
    dla_memcpy(op_b, &temp_cpu_op, sizeof(struct cpu_op));
    // temp_cpu_op.op_desc = op_a->op_desc;
    // op_a->op_desc = op_b->op_desc;
    // op_b->op_desc = temp_cpu_op.op_desc;
    // temp_cpu_op.op_desc = NULL;
}

// Define heapifyUp function to maintain heap property
// during insertion
void heapifyUp(struct priority_queue *pq, int index)
{
    if (index
        && pq->items[(index - 1) / 2].op_desc->index > pq->items[index].op_desc->index) {
        swap(&pq->items[(index - 1) / 2],
             &pq->items[index]);
        heapifyUp(pq, (index - 1) / 2);
    }
}

// Define enqueue function to add an item to the queue
void _enqueue(struct priority_queue* pq, struct dla_common_op_desc *value)
{
    if (pq->size == MAX_CPU_QUEUE_ITEMS) {
        printf("Priority queue is full\n");
        return;
    }

    dla_debug("[GEM5-PLUS][CPU-EXECUTION] adding item %d in queue\n",
        value);
    /**
     * TODO: add any other needed information
     */
    pq->items[pq->size++].op_desc = value;
    heapifyUp(pq, pq->size - 1);
}

// Define heapifyDown function to maintain heap property
// during deletion
int heapifyDown(struct priority_queue* pq, int index)
{
    int smallest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < pq->size
        && pq->items[left].op_desc->index < pq->items[smallest].op_desc->index)
        smallest = left;

    if (right < pq->size
        && pq->items[right].op_desc->index < pq->items[smallest].op_desc->index)
        smallest = right;

    if (smallest != index) {
        swap(&pq->items[index], &pq->items[smallest]);
        heapifyDown(pq, smallest);
    }
}

// Define dequeue function to remove an item from the queue
struct cpu_op *_dequeue(struct priority_queue* pq)
{
    if (!pq->size) {
        printf("Priority queue is empty\n");
        return -1;
    }

    /**
     * TODO: return item info, not pointer (it is overwritten)
     */
    struct cpu_op *item = &pq->items[0];
    pq->items[0] = pq->items[--pq->size];
    heapifyDown(pq, 0);
    return item;
}

// Define peek function to get the top item from the queue
struct cpu_op *_peek(struct priority_queue* pq)
{
    if (!pq->size) {
        printf("Priority queue is empty\n");
        return -1;
    }
    return &pq->items[0];
}












////////////////////////////////////////////////////////////////////////////

// THIS IS FOR PRIORITY QUEUE OF INTEGER ITEMS

/*
// Define swap function to swap two integers
void swap(int* a, int* b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

// Define heapifyUp function to maintain heap property
// during insertion
void heapifyUp(struct priority_queue* pq, int index)
{
    if (index
        && pq->items[(index - 1) / 2] > pq->items[index]) {
        swap(&pq->items[(index - 1) / 2],
             &pq->items[index]);
        heapifyUp(pq, (index - 1) / 2);
    }
}

// Define enqueue function to add an item to the queue
void _enqueue(struct priority_queue* pq, int value)
{
    if (pq->size == MAX_CPU_QUEUE_ITEMS) {
        printf("Priority queue is full\n");
        return;
    }

    dla_debug("[GEM5-PLUS][CPU-EXECUTION] adding item %d in queue\n",
        value);
    pq->items[pq->size++] = value;
    heapifyUp(pq, pq->size - 1);
}

// Define heapifyDown function to maintain heap property
// during deletion
int heapifyDown(struct priority_queue* pq, int index)
{
    int smallest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < pq->size
        && pq->items[left] < pq->items[smallest])
        smallest = left;

    if (right < pq->size
        && pq->items[right] < pq->items[smallest])
        smallest = right;

    if (smallest != index) {
        swap(&pq->items[index], &pq->items[smallest]);
        heapifyDown(pq, smallest);
    }
}

// Define dequeue function to remove an item from the queue
int _dequeue(struct priority_queue* pq)
{
    if (!pq->size) {
        printf("Priority queue is empty\n");
        return -1;
    }

    int item = pq->items[0];
    pq->items[0] = pq->items[--pq->size];
    heapifyDown(pq, 0);
    return item;
}

// Define peek function to get the top item from the queue
int _peek(struct priority_queue* pq)
{
    if (!pq->size) {
        printf("Priority queue is empty\n");
        return -1;
    }
    return pq->items[0];
}
*/