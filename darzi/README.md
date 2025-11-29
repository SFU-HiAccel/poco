# darzi

`darzi` (or Tailor) is the utility to stitch an existing program with BRAHMA cores.

Currently, it supports only 1 BRAHMA core, but can be scaled to add multiple cores as well.  
The primary reason for the existence of this utility is that `PASTA` expects everything to be in a giant monolith file, which makes maintenance really difficult.


Everything that can be stitched is found in `brahma_assets`.
I need to add a JSON parser that connects different tasks together to the RQR/RSG interfaces.

## Examples

```
./brahma_darzi.py --adir brahma_assets/spphd_ap --src ../tests/darzi_ap_spphd/src/add.pre.cpp --dest ../tests/darzi_ap_spphd/src/add.cpp
./brahma_darzi.py --adir brahma_assets/mpphd_64 --src ../tests/darzi_ap_spphd/src/add.pre.cpp --dest ../tests/darzi_ap_spphd/src/add.cpp
```


### Optimizer:

The `Optimizer._scan_body` flow might seem counterintuitive at first.
Why are we locating the API calls by parsing the `DECL_STMT` nodes, and then parsing the API again using `_parse_control_API` and `_parse_dataio_API` functions?  Also, why, after we seemingly have everything related to the API declaration, do we pass the `func_cursor` again?
The simple reason is that I want to standardize the *recognition* of the API calls, which requires that I differentiate between how they are transformed/optimized.  
The recognition happens by scanning all `DECL_STMT` nodes. However, the `func_cursor` is also passed to the `parse_x_API` functions because the optimization phase needs to perform some analysis first, which might require understanding the structure of the parent AST.

## Bugs
* [ ] Two different requests in the same loop cannot be optimized right now.

