from collections import defaultdict

class GreedyScheduler:
    def __init__(self, tasks = [], num_proc = 4, cache_block_size = 8):
        self.num_proc = num_proc
        self.cache_block_size = cache_block_size
        self.deque = tasks ## (task_id, estimated compute time, dataset list)
        self.schedule = defaultdict(list) ## proc_id: list of tasks
        self.avail_time = defaultdict(int) ## proc_id: earliest available time


    def allocate(self, task_id, chosen_proc, new_endtime):
        self.avail_time[chosen_proc] = new_endtime
        self.schedule[chosen_proc].append(task_id)


    def get_schedule(self):
        while self.deque:
            (task_id, compute_time, data_ids) = self.deque.pop(0)
            potential_end_time = [] ##(endtime, proc_id)
            for proc_id in range(self.num_proc):
                endtime = self.avail_time[proc_id] + compute_time
                potential_end_time.append((endtime, proc_id))
            (chosen_endtime, chosen_proc) = min(potential_end_time)
            self.allocate(task_id, chosen_proc, chosen_endtime)
        return self.schedule
    
    def get_longest_time(self):
        return max(self.avail_time.values())
    





            



