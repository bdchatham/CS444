/*
 * elevator noop
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

//Our shortest seek time first (SSTF) data object. 
struct sstf_data {
	struct list_head queue;
	int direction;//Forward: 1, Backward: 0
	sector_t pos;
};

static void sstf_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

//Our altered dispatch function.
//Process:
static int sstf_dispatch(struct request_queue *q, int force)
{
	struct list_head *listHead;
	struct sstf_data *sData = q->elevator->elevator_data;
	sector_t sstSector;
	secotr_t iteratorSector;
	struct request *sst;
	struct request *start;
	struct request *iterator;

	//Minimal sector time.
	sstSector = ULONG_MAX;

	//Check if the list is not empty.
	if (!list_empty(&sData->queue)) {

		//Get the next I/O request from the list.
		sst = list_entry(sData->queue.next, struct request, queuelist);
		//Set a starting point from the given request.
		start = list_entry(sData->queue.next, struct request, queuelist);

		//There was only one item in the list.... dispatch.
		if(sst == start) {
			//Deletes the node from the list.
			list_del_init(&sst->queuelist);
			//
			sData->pos = blk_rq_pos(sst) + blk_rq_sectors(sst);
			//
			elv_dispatch_sort(q, sst);
			//Inserts sst into the dispatch queue q to be used by specific elevators.
			elv_dispatch_sort(q, sst);
			return 1;
		}

		//Foreach list head in our queue of requests...
		list_for_each(listHead, sData->queue) {
			//Get the list head.
			iterator = list_entry(listHead, struct request, queuelist);
			//Get the sector of the iterator.
			iteratorSector = blk_rq_pos(ittr);
			
			//Check if the current seek time is less than that of the sstf request.
			if(sstf_distance(iteratorSector, sData->pos) < sstf_distance(sstSector, sData->pos)) {
				//Check which direction we are going and if the position of the itterator request is in that direction.
				//1 == Forward, 0 == Backward
				if((sData->direction == 1) && (iteratorSector > sData->pos)) {
					//Set sst to iterator value.
					sst = iterator;
					sstSector = iteratorSector;

				//Check if we are going in the correct direction as done previously, but in the opposite direction.
				} else if((sData->direction == 0) && (iteratorSector < sData->pos)) {
					//Set sst to iterator value. 
					sst = iterator;
					sstSector = iteratorSector;

				//Reverse the direction.
				} else {
				sData->direction = !sData->direction;
				}
			}
		} 
	//List is empty.
	} else {
		if(sData->direction == 1) {
			sData->pos = ULONG_MAX/2;
			sData->direction = !sData->direction;
		} else {
			sData->pos = 0;
			sData->direction = !sData->direction;
		}
	}

	return 0;
}

static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *sData = q->elevator->elevator_data;

	list_add_tail(&rq->queuelist, &sData->queue);
}

static struct request *
sstf_former_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *sData = q->elevator->elevator_data;

	if (rq->queuelist.prev == &sData->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
sstf_latter_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *sData = q->elevator->elevator_data;

	if (rq->queuelist.next == &sData->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct sstf_data *sData;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	sData = kmalloc_node(sizeof(*sData), GFP_KERNEL, q->node);
	if (!sData) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = sData;

	INIT_LIST_HEAD(&sData->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void sstf_exit_queue(struct elevator_queue *e)
{
	struct sstf_data *sData = e->elevator_data;

	BUG_ON(!list_empty(&sData->queue));
	kfree(sData);
}

static struct elevator_type elevator_sstf = {
	.ops = {
		.elevator_merge_req_fn		= sstf_merged_requests,
		.elevator_dispatch_fn		= sstf_dispatch,
		.elevator_add_req_fn		= sstf_add_request,
		.elevator_former_req_fn		= sstf_former_request,
		.elevator_latter_req_fn		= sstf_latter_request,
		.elevator_init_fn		= sstf_init_queue,
		.elevator_exit_fn		= sstf_exit_queue,
	},
	.elevator_name = "sstf",
	.elevator_owner = THIS_MODULE,
};

static int __init sstf_init(void)
{
	return elv_register(&elevator_sstf);
}

static void __exit sstf_exit(void)
{
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Jens Axboe");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op IO scheduler");
