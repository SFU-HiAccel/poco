# PoCo

TPS framework for multi-producer multi-consumer (MPMC) shared buffer support.  
PoCo changes buffer accesses from the typical producer-consumer fashion to a generic transactor-transactor model 
wherein any task can act either as a producer or as a consumer, while still keeping II=1 given that the 
memory addressing is latency insensitive.

Depends on [Laneswitching Buffers](https://github.com/SFU-HiAccel/pasta-lsbuffer.git) for the SPSC toolset.

# Framework

Dependence hierarchy:
```
              PoCo
        .------''------.
        V              V
   SPSC tools   |   MPMC tools
--------------------------------   
[ LS Buffers ]  |   [ BRAHMA ] 
      ^         |
  [ PASTA ]     |
      ^         |
  [ TAPA ]      |

```

The MPMC interface can be interpreted as a virtual layer over the SPSC toolset. This layer makes it easy to program 
the application, but the actual implementation still happens in single-producer single-consumer fashion.

We recommend using a Conda environment for the PASTA project. If a different environment is being used, please follow 
the detailed installation steps to build from source.

### Download the repository

```
git clone https://github.com/SFU-HiAccel/poco.git
### OR ###
git clone git@github.com:SFU-HiAccel/poco.git
```

## Summary of Usage

PoCo standardizes memory interfaces between several tasks at once, allowing them to share a common memory view. This 
makes the user tasks appear as plug-and-play IPs connected via a high-performance datapath. The underlying LS-buffers 
preserve write-read dependence if strict dataflow is assumed between different tasks.

Expand this to see example usage.

### MPMC channel configuration
An MPMC shared buffer channel declaration looks like the following:

```cpp
using mybuf = tapa::mpmcbuffer<sb_hmsg_t[32],
    tapa::array_partition<tapa:
      :block<1>>,
    tapa::memcore<tapa::bram>,
    tapa::blocks<4>,
    tapa::pages<8>>;
mybuf sharedbuffer;

```

This repo is undergoing active updates to provide an easier installation experience.

## Citation

If you use HiSpMM in your research, please cite:

```bibtex
@article{10.1145/3771938,
author = {Baranwal, Akhil Raj and Fang, Zhenman},
title = {PoCo: Extending Task-Parallel HLS Programming with Shared Multi-Producer Multi-Consumer Buffer Support},
year = {2025},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
issn = {1936-7406},
url = {https://doi.org/10.1145/3771938},
doi = {10.1145/3771938},
abstract = {Advancements in High-Level Synthesis (HLS) tools have enabled task-level parallelism on FPGAs. However, prevailing frameworks predominantly employ single-producer-single-consumer (SPSC) models for task communication, thus limiting application scenarios. Analysis of designs becomes non-trivial with an increasing number of tasks in task-parallel systems. Adding features to existing designs often requires re-profiling of several task interfaces, redesign of the overall inter-task connectivity, and describing a new floorplan. This paper proposes PoCo, a novel framework to design scalable multi-producermulti- consumer (MPMC) models on task-parallel systems. PoCo introduces a shared-buffer abstraction that facilitates dynamic and high-bandwidth access to shared on-chip memory resources, incorporates latency-insensitive communication, and implements placement-aware design strategies to mitigate routing congestion. The frontend provides convenient APIs to access the buffer memory, while the backend features an optimized and pipelined datapath. Empirical evaluations demonstrate that PoCo achieves up to 50\% reduction in on-chip memory utilization on SPSC models without performance degradation. Additionally, three case studies on distinct real-world applications reveal up to 1.5\texttimes{} frequency improvements and simplified dataflow management in heterogeneous FPGA accelerator designs.},
note = {Just Accepted},
journal = {ACM Trans. Reconfigurable Technol. Syst.},
month = oct,
keywords = {Multi-producer multi-consumer, buffer optimization, floorplan optimization, multi-die FPGA, high-level synthesis}
}
```

