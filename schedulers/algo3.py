from sortedcontainers import SortedList
import math

class HFPHeterScheduler:
    def __init__(self, tasks = [], num_proc = 4, cache_block_size = 8, max_mem = 10):
        self.tasks = tasks ## (task_id, time, dataset list)
        self.est_time = {}
        self.num_proc = num_proc
        self.cache_block_size = cache_block_size
        self.max_mem = max_mem
        self.packages = SortedList()
        for (task_id, time, dataset) in tasks:
            dataset = set(map(self.get_cache_block_id, dataset))
            self.packages.add((1, [task_id], dataset))
            self.est_time[task_id] = time
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
        total_load = 0
        for (task_count, task_list, dataset) in self.packages:
            local_load = 0
            for t in task_list:
                local_load += self.est_time[t]
            self.schedule.add((local_load, task_count, task_list))
            total_load += local_load

        avg_load_upperbound = (total_load / self.num_proc) * 1.2

        final_schedule = []

        while self.schedule[-1][0] > avg_load_upperbound:
            smallest_item = self.schedule.pop(0)
            (local_load_smallest, task_count_smallest, task_list_smallest) = smallest_item
            largest_item = self.schedule.pop(-1)
            (local_load_largest, task_count_largest, task_list_largest) = largest_item
            extra = task_count_largest - avg_load_upperbound
            need = avg_load_upperbound - task_count_smallest
            transfer_load = min(extra, need)

            transfer_tasks = []
            while transfer_load:
                if task_list_largest[-1] > transfer_load:
                    final_schedule.append((local_load_largest, task_count_largest, task_list_largest))
                    break
                task_to_transfer = task_list_largest.pop()
                task_weight =  self.est_time[task_to_transfer]
                transfer_tasks.append(task_to_transfer)
                local_load_largest -= task_weight
                task_count_largest -= 1
                transfer_load -= task_weight
            for t in transfer_tasks:
                task_list_smallest.append(t)
                t_weight =  self.est_time[t]
                local_load_smallest += t_weight
                task_count_smallest += 1
            self.schedule.add((local_load_smallest, task_count_smallest, task_list_smallest))
        
        self.schedule.update(final_schedule)


    def get_schedule(self):
        self.hfp_schedule()
        self.load_balance()
        schedule = {}
        i = 0
        for (load, count, task_list) in self.schedule:
            schedule[i] = task_list
            i += 1
        return schedule
    
    