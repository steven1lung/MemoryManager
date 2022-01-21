## TLB Replacement Policies
### Random
Pros: Easy to implement.
Cons: Hit rate is relatively low because it might choose the mostly
used page to replace.
### LRU
Pros: Hit rate is relatively high, since the pages may have spatial or
temporal localities
Cons: Implementing LRU needs additional hardware to record the last
reference time for each page, so the cost is higher.
## Page Replacement Policies
### FIFO
Pros:
Easy to implement
Cons:
Page fault ratio is relatively high, belady anomaly may occur.
### Clock
Pros: Page fault ratio is relatively low because a reference bit is used
to get a second chance.
Cons: Needs additonal hardware support for reference bit, so cost is
higher.
## Frame Allocation Policies
### Global
Pros:
Memory utilization is better than local since the victim pages we can
choose is not restricted to the process.
Cons:
When thrashing occurs it may spread to the whole memory, leading
to CPU utilization dropping rapidly.
### Local
Pros:
Thrashing will not spread to the whole memory because we restrict
the victim pages we can choose to the current process.
Cons:
Memory utilization isn’t as good as global, some processes may have
finished, but due to the allocation policy, we can’t swap the finished
processes out, so memory utilization isn’t that good.
