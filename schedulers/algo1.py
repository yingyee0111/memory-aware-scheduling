from collections import defaultdict

class DMDAScheduler:
    def __init__(self, tasks = [], num_proc = 4, cache_block_size = 8):
        self.num_proc = num_proc
        self.cache_block_size = cache_block_size
        self.data_on_proc = defaultdict(set) ## proc_id : set of data_ids local
        self.deque = tasks ## (task_id, estimated compute time, dataset list)
        self.schedule = defaultdict(list) ## proc_id: list of tasks
        self.avail_time = defaultdict(int) ## proc_id: earliest available time


    def get_data_cost(self, data_id, dest):
        ## returns a memory fetching cost
        return 400
    
    def get_cache_block_id(self, data_id):
        return data_id - (data_id % self.cache_block_size)
    
    def get_task_cost(self, compute_time, dataset, proc_id): 
        data_transfer_time = 0
        data_fetched = set()
        local_data = self.data_on_proc[proc_id]
        for data_id in dataset:
            if data_id not in local_data and data_id not in data_fetched:
                data_transfer_time += self.get_data_cost(data_id, proc_id)
                data_fetched.add(data_id)
        return (compute_time + data_transfer_time, data_fetched)
    
    def allocate(self, task_id, chosen_proc, new_endtime, data_to_fetech):
        self.avail_time[chosen_proc] = new_endtime
        for new_data_id in data_to_fetech:
            self.data_on_proc[chosen_proc].add(new_data_id)
        self.schedule[chosen_proc].append(task_id)


    def get_schedule(self):
        while self.deque:
            (task_id, compute_time, data_ids) = self.deque.pop(0)
            data_ids = list(map(self.get_cache_block_id, data_ids))
            potential_end_time = [] 
            for proc_id in range(self.num_proc):
                (time_taken, data_to_fetech) = self.get_task_cost(compute_time, data_ids, proc_id)
                endtime = self.avail_time[proc_id] + time_taken
                potential_end_time.append((endtime, proc_id, data_to_fetech))
            (chosen_endtime, chosen_proc, chosen_data_to_fetch) = min(potential_end_time)
            self.allocate(task_id, chosen_proc, chosen_endtime, chosen_data_to_fetch)
        return self.schedule
    
    def get_longest_time(self):
        return max(self.avail_time.values())
    




# tasks = [
#     (0, 10, [4,8,9,12,16]),
#     (1, 10, [12,16,20,24,25]),
#     (2, 10, [20,24,28,32,33]),
#     (3, 10, [28,32,36,40,41,44]),
# ]
# scheduler1 = DequeScheduler(tasks, 2, 4)
# print("schedule using algo 1")
# print(scheduler1.get_schedule())
# print("max time:", scheduler1.get_longest_time())





            



