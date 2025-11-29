# PoCo

TPS framework for multi-producer multi-consumer (MPMC) shared buffer support.  
This changes buffer accesses from the typical producer-consumer fashion to a generic transactor-transactor access 
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


<details>
<summary>Expand this to see example usage.</summary>

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


