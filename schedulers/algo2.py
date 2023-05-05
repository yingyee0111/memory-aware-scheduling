from sortedcontainers import SortedList
import math

class HFPScheduler:
    def __init__(self, tasks = [], num_proc = 4, cache_block_size = 8, max_mem = 10):
        self.tasks = tasks ## (task_id, dataset list)
        self.num_proc = num_proc
        self.cache_block_size = cache_block_size
        self.max_mem = max_mem
        self.packages = SortedList()
        for (task_id, dataset) in tasks:
            dataset = set(map(self.get_cache_block_id, dataset))
            self.packages.add((1, [task_id], dataset))
        self.schedule = SortedList()

    def get_cache_block_id(self, data_id):
        return data_id - (data_id % self.cache_block_size)
        

    def hfp_schedule(self):
        num_package_to_form = self.num_proc
        rd1_packages = []

        def merge(mem_bound):
            nonlocal num_package_to_form
            smallest_package = self.packages[0]
            (task_count, task_list, dataset) = smallest_package
            affinity = []
            for j in range(1, len(self.packages)):
                (task_count_j, task_list_j, dataset_j) = self.packages[j]
                if mem_bound and len(dataset.union(dataset_j)) > self.max_mem:
                    continue
                common_data = dataset.intersection(dataset_j)
                affinity.append((len(common_data), j))
            if len(affinity) == 0:
                assert(mem_bound)
                rd1_packages.append(smallest_package)
                self.packages.pop(0)
                num_package_to_form -= 1
                return
            (affn, chosen_package) = min(affinity)
            (task_count_j, task_list_j, dataset_j) = self.packages.pop(chosen_package)
            task_list.extend(task_list_j)
            new_package = (task_count+task_count_j, task_list, dataset.union(dataset_j))
            self.packages.pop(0)
            self.packages.add(new_package)

        
        while self.packages and len(self.packages) > num_package_to_form:
            merge(True)
        self.packages.update(rd1_packages)

        while len(self.packages) > self.num_proc:
            merge(False)

        
    def load_balance(self):
        for (task_count, task_list, dataset) in self.packages:
            self.schedule.add((task_count, task_list))
        avg_load_upperbound = math.ceil(len(self.tasks) / self.num_proc)
        while self.schedule[-1][0] > avg_load_upperbound:
            (task_count_smallest, task_list_smallest) = self.schedule.pop(0)
            (task_count_largest, task_list_largest) = self.schedule.pop(-1)
            extra = task_count_largest - avg_load_upperbound
            need = avg_load_upperbound - task_count_smallest
            transfer_count = min(extra, need)
            task_list_smallest.extend(task_list_largest[-transfer_count:])
            self.schedule.add((task_count_smallest+transfer_count, task_list_smallest))
            task_list_largest = task_list_largest[:-transfer_count]
            self.schedule.add((task_count_largest-transfer_count, task_list_largest))

    def get_schedule(self):
        self.hfp_schedule()
        self.load_balance()
        schedule = {}
        i = 0
        for (count, task_list) in self.schedule:
            schedule[i] = task_list
            i += 1
        return schedule
    

# tasks = [
#     (0, [4,8,9,12,16]),
#     (1, [12,16,20,24,25]),
#     (2, [20,24,28,32,33]),
#     (3, [28,32,36,40,41,44]),
# ]
# scheduler2 = HFPScheduler(tasks, 2, 4, 10)
# print("schedule using algo 2")
# print(scheduler2.get_schedule())
    