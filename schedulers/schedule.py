## sample script to parse memory trace and run scheduler

from algo1 import DMDAScheduler
from greedy import GreedyScheduler
from algo2 import HFPScheduler
from algo3 import HFPHeterScheduler
from copy import deepcopy


f = open("pinatrace_mm.out", "r")
content = f.read()
lines = content.split('\n')

threads = []
time_est = {}
data = {}

for l in lines:
    if l == "#eof":
        break
    l = l.strip('(')
    l = l.strip(')')
    tmp1 = l.split('[')
    read = tmp1[1].split(']')[0].split(',')
    for r in range(len(read)):
        read[r] = int(read[r])
    write = tmp1[2].split(']')[0].split(',')
    for w in range(len(write)):
        write[w] = int(write[w])
    thread_id = int(tmp1[0].split(',')[0])
    time = int(tmp1[0].split(',')[1])
    read.extend(write)
    threads.append((thread_id, time, read))
    time_est[thread_id] = time
    data[thread_id] = deepcopy(read)


def reorder(task_list): 
    waiting_to_fetch = {}
    for t in task_list:
        waiting_to_fetch[t] = set(deepcopy(data[t]))

    local_data = set()
    final_order = []
    while len(waiting_to_fetch) > 0:
        missing = []
        for t in waiting_to_fetch:
            data_needed = waiting_to_fetch[t]
            num_data_needed = len(data_needed - local_data)
            missing.append((num_data_needed, t))
        (x, smallest_task) = min(missing)
        final_order.append(smallest_task)
        local_data = local_data.union(data[smallest_task])
        waiting_to_fetch.pop(smallest_task)
    return final_order

threads = threads[::-1][1:]
threads.sort(key= lambda x: x[1], reverse=True)

scheduler1 = DMDAScheduler(threads, 7, 64)
print("schedule using algo 1")
sch = scheduler1.get_schedule()
assigned_core = [0] * (len(time_est))
for k in sch:
    print(k, sch[k])
    time = 0
    for task in sch[k]:
        time += time_est[task]
        assigned_core[task] = k
    print("time", time)

print(assigned_core)
        
print("max time:", scheduler1.get_longest_time())

