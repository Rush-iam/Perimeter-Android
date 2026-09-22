# Android DXVK memory investigation

## Finding

In the older-device comparison supplied during the DXVK 1/DXVK 2 investigation, the process-memory readings were close, while Android's graphics accounting differed substantially:

| Reading | DXVK 1 | DXVK 2 | Difference |
| --- | ---: | ---: | ---: |
| `pssMiB` | 722.2 | 715.2 | +7.0 MiB |
| `rssMiB` | 836.3 | 829.5 | +6.8 MiB |
| `privateDirtyMiB` | 691.6 | 684.9 | +6.7 MiB |
| `androidSummaryTotalPssMiB` | 1627.9 | 1021.4 | +606.5 MiB |
| `graphicsPssMiB` | 1317.2 | 696.6 | +620.6 MiB |
| `nativeHeapAllocatedMiB` | 281.7 | 264.9 | +16.8 MiB |

A prior pair showed a similar graphics-PSS gap: 1316.9 MiB for DXVK 1 and 696.7 MiB for DXVK 2. The difference therefore appears mainly in graphics memory accounting or Vulkan allocations, rather than ordinary process PSS or the native heap. Do not add `graphicsPssMiB` to `pssMiB` or `androidSummaryTotalPssMiB`; these are overlapping accounting views. The reported `systemAvailableMiB` values were 1542.7 and 1513.9 MiB, respectively, and both samples reported `lowMemory=false`.

The pinned source versions also have different Vulkan allocation-pool policies. DXVK 1 adds backing chunks to its memory-type chunk list and returns freed suballocations to those chunks; the normal free path does not remove an empty chunk. Its upstream default chunk size is 128 MiB, reduced for small heaps; the Android DXVK 1 patch uses 64 MiB by default. DXVK 2 has explicit empty-chunk cleanup: it allows one unused maximum-size device-local chunk per pool, frees excess or undersized chunks, and can free an idle chunk after 20 seconds. Its timed allocator work runs at 500 ms intervals when called from the submission queue.

This makes retained empty Vulkan chunks a strong explanation for the DXVK 1 graphics-PSS difference and for graphics memory remaining high after returning to the menu. It does not establish a live-resource leak: the retained chunks may be available for reuse, and Android graphics PSS comes from device-specific graphics accounting. Roughly 620 MiB is on the scale of several 128 MiB chunks, but the supplied readings do not tell us the actual chunk count or live bytes.

## First DXVK 1 allocator sample

The new counters were captured before and after loading the Tutorial mission:

| Point | DXVK backing allocated | DXVK live used | Allocator free capacity | Chunks / empty chunks | `graphicsPssMiB` |
| --- | ---: | ---: | ---: | ---: | ---: |
| Main menu, 01:17:53 | 768 MiB | 293.4 MiB | 474.6 MiB | 6 / 0 | 804.6 MiB |
| Tutorial loaded, 01:18:13 | 1280 MiB | 488.0 MiB | 792.0 MiB | 10 / 0 | 1317.8 MiB |
| Gameplay, 01:19:33 | 1280 MiB | 498.2 MiB | 781.8 MiB | 10 / 0 | 1318.2 MiB |
| Main menu, 01:25:46 | 1280 MiB | 434.2 MiB | 845.8 MiB | 10 / 1 | 1318.4 MiB |
| Second mission, 01:31:37 | 1280 MiB | 505.4 MiB | 774.6 MiB | 10 / 1 | 1318.5 MiB |
| Main menu after second mission, 01:32:27 | 1280 MiB | 434.2 MiB | 845.8 MiB | 10 / 1 | 1318.9 MiB |
| Third mission, 01:42:50 | 1280 MiB | 498.2 MiB | 781.8 MiB | 10 / 0 | 1316.4 MiB |
| Main menu after third mission, 01:43:40 | 1280 MiB | 434.2 MiB | 845.8 MiB | 10 / 1 | 1317.0 MiB |

Loading the mission added 512 MiB of Vulkan backing allocations and about 194.6 MiB of live DXVK allocations. During the following roughly 80 seconds, backing allocation stayed at 1280 MiB while live usage increased about 10.3 MiB. Process PSS increased from about 709 MiB to 724 MiB in that interval, while graphics PSS stayed nearly flat. This short increase could be ongoing mission allocation or a leak; it is not enough to distinguish them.

After returning to the main menu, live usage fell about 64.1 MiB from the gameplay sample, and one 128 MiB type-6 chunk became completely empty. Total Vulkan backing allocation remained at 1280 MiB and graphics PSS remained at about 1318 MiB. This directly shows that DXVK 1 retains at least one empty backing chunk after mission resources are released. It also retained about 717.8 MiB of free capacity across partially used chunks. Live usage in the menu was still about 140.8 MiB above the pre-mission baseline; a repeated mission cycle is needed to distinguish reusable renderer/game state from accumulating live resources.

The second and third missions reused the same ten chunks: allocated backing stayed at 1280 MiB while live usage reached about 505 MiB and 498 MiB. After each mission, live usage returned to exactly 434.2 MiB, with one empty chunk. Graphics PSS remained near 1317 to 1319 MiB. This is stable across three mission cycles and does not show cumulative growth in DXVK live allocations. The elevated post-first-mission baseline is consistent with retained reusable game or renderer state; a longer run would be needed to rule out slower growth.

The per-chunk menu sample shows why most free capacity cannot be reclaimed by simply dropping empty blocks. Of ten 128 MiB chunks, type 6 chunk 4 is entirely empty, while type 0 chunk 2 has only 28 KiB in use. The other eight chunks hold substantial live allocations, ranging from about 7.5 MiB to 91 MiB. DXVK 1 only allocates from a chunk when its memory flags and priority match, and the rows show priorities 0, 0.5, and 1 across the two memory types. This divides allocations among separate 128 MiB blocks and leaves about 718 MiB free inside partially used chunks. Releasing the one empty chunk could save 128 MiB; reclaiming most of the remaining slack would require changing chunk granularity or relocating live allocations.

## 64 MiB Android default

The Android-only 64 MiB maximum chunk-size policy was run through a mission and back to the menu on the same device class. Compared with the 128 MiB baseline, it reduced the backing pool and graphics accounting without materially changing live DXVK usage:

| Point | 128 MiB baseline | 64 MiB cap | Change |
| --- | ---: | ---: | ---: |
| Mission backing allocation | 1280 MiB | 1088 MiB | -192 MiB |
| Mission live usage | 498.2 MiB | 484.6 MiB | -13.6 MiB |
| Mission graphics PSS | 1316–1318 MiB | 1123.9 MiB | about -193 MiB |
| Menu backing allocation | 1280 MiB | 1088 MiB | -192 MiB |
| Menu live usage | 434.2 MiB | 428.6 MiB | -5.6 MiB |
| Menu graphics PSS | about 1317 MiB | 1124.9 MiB | about -192 MiB |

The smaller chunks increased the pool from 10 chunks to 17, but reduced unused backing capacity. The menu retained two empty 64 MiB chunks (128 MiB total), while the 128 MiB run retained one empty 128 MiB chunk. Android reported `lowMemory=false` and more than 2.7 GiB system-available memory during this run. This result supports 64 MiB as the Android DXVK 1 default: it lowers the larger-device graphics footprint without the excessive chunk count seen at 32 MiB. It is not a complete solution for the 4 GiB peak-memory failure.

## 32 MiB chunk experiment

The 32 MiB build completed a basic Tutorial mission on the larger device and returned to the menu:

| Point | 64 MiB result | 32 MiB result | Change |
| --- | ---: | ---: | ---: |
| Mission backing allocation | 1088 MiB | 960 MiB | -128 MiB |
| Mission live usage | about 484.6 MiB | 484.6 MiB | unchanged |
| Mission graphics PSS | about 1124 MiB | 996.3 MiB | about -128 MiB |
| Menu backing allocation | 1088 MiB | 960 MiB | -128 MiB |
| Menu live usage | about 428.6 MiB | 433.1 MiB | +4.5 MiB |
| Menu graphics PSS | about 1125 MiB | 996.8 MiB | about -128 MiB |

The 32 MiB pool used 30 chunks (13 type-0 and 17 type-6), compared with 17 chunks in the 64 MiB run. At the menu it retained seven empty chunks, or 224 MiB, compared with 128 MiB in the 64 MiB run. The smaller chunks reduce total backing for this Tutorial workload, but they also increase allocation count and can leave more individually empty chunks. This is a useful result, not a production conclusion; the larger 4 GiB mission must be repeated because its roughly 990 MiB live peak may produce a very different chunk count and backing ratio.

The larger 4 GiB mission was then repeated with the 32 MiB cap and also crashed. Near the end, DXVK had 39 chunks and 1248 MiB allocated, with 876.9 to 887.5 MiB live. This was only 32 to 96 MiB less Vulkan backing than the 64 MiB run, while native heap allocation reached about 859 MiB and swap PSS reached 675 to 691 MiB. The Android summary and graphics PSS became unavailable under pressure. The smaller chunks therefore did not provide enough backing reduction to prevent termination and may increase total pressure through the larger allocation count and associated overhead.

## 4 GiB-device failure with the 64 MiB cap

The same experiment failed on the 4 GiB device during a larger mission. The last samples were:

| Point | 20:25:27 | 20:25:37 |
| --- | ---: | ---: |
| Process PSS | 141.0 MiB | 163.6 MiB |
| Swap PSS | 526.4 MiB | 508.8 MiB |
| Android summary total PSS | 2169.2 MiB | 2169.2 MiB |
| Graphics PSS | 1502.0 MiB | 1502.0 MiB |
| Native heap allocated | 749.6 MiB | 747.8 MiB |
| System available | 495.8 MiB | 503.6 MiB |

The DXVK samples immediately around the failure show heap 0 growing from 1280 MiB allocated / 977.7 MiB used across 20 chunks to 1344 MiB allocated / 990.3 MiB used across 21 chunks. The 64 MiB cap therefore limits chunk size but not total pool growth. It still permits many chunks as the mission loads resources, and the process is already swapping heavily while graphics PSS is about 1.5 GiB. `lowMemory=false` only means the system did not cross Android's generic low-memory threshold at the sampling instant; it does not guarantee that a Vulkan allocation or the vendor graphics driver can succeed.

There is no native traceback, `DxvkMemoryAllocator: Memory allocation failed` message, or Vulkan error in the supplied tail. The failure is therefore most consistent with system or vendor graphics-memory pressure, or a driver/device-loss path that was not logged before termination. The low `smaps_rollup` PSS is misleading here because hundreds of MiB are swapped; `androidSummaryTotalPssMiB` and graphics PSS are separate Android accounting views and must not be added together. The 4 GiB result means the 64 MiB cap is not sufficient as a production safeguard by itself.

The aggregate log does not show whether the remaining free capacity is concentrated in sparsely used chunks or fragmented across many chunks, so per-chunk occupancy is still needed before choosing a cleanup policy. Driver budget/usage was unavailable, and Android reported `lowMemory=false` with about 1.8 GiB system-available memory during gameplay and in the menu.

## Evidence and source pins

The Android build pins the DXVK 1 native fork to `43aedc756cbd620b9ee8b1cf2c17b17cc49b3781` and DXVK 2 to `c3dd74be6baec53786d4e064a572185b70347a17` in [`AndroidDxvk.cmake`](../app/src/main/cpp/cmake/AndroidDxvk.cmake#L159).

- [DXVK 1 allocator source at its pinned revision](https://github.com/IonAgorria/dxvk-native/blob/43aedc756cbd620b9ee8b1cf2c17b17cc49b3781/src/dxvk/dxvk_memory.cpp): chunk insertion near line 328, suballocation freeing near lines 408 to 414, and default chunk sizing near lines 426 to 438.
- [DXVK 2 allocator source at its pinned revision](https://github.com/doitsujin/dxvk/blob/c3dd74be6baec53786d4e064a572185b70347a17/src/dxvk/dxvk_memory.cpp): empty-chunk policy near lines 1602 to 1655 and timed cleanup near lines 2550 to 2585. The submission queue invokes timed tasks in [`dxvk_queue.cpp`](https://github.com/doitsujin/dxvk/blob/c3dd74be6baec53786d4e064a572185b70347a17/src/dxvk/dxvk_queue.cpp#L217).

## Next steps

1. Keep 64 MiB as the Android DXVK 1 default. Treat 32 MiB as rejected for the 4 GiB DXVK 1 path: it still crashed, created 39 chunks, and entered heavier swap pressure.
2. Use Sokol on the 4 GiB device, where the same mission has already been tested successfully. Capture the same memory fields if a quantitative comparison is needed; DXVK 2 is unavailable on this device.
3. Compare DXVK 2 on a device that supports it, using the same mission and return-to-menu sequence. Its empty-chunk cleanup and different allocation policy may avoid this growth pattern; it is not available on the 4 GiB device tested here.
4. If longer DXVK 1 sessions show live usage rising across mission cycles, trace the still-live allocations by resource type before changing allocator policy. The current 8 GiB three-cycle data shows stable reuse, not cumulative growth.
5. A conservative cleanup policy can release wholly empty chunks after an idle delay or under heap-budget pressure, while retaining a warm chunk. This can recover empty blocks, but it will not by itself prevent the 4 GiB mission peak shown here. Capture Vulkan allocation failures and device-loss events with heap budget, allocated/used bytes, and requested allocation size, and correlate them with Android crash/low-memory records.

**Status:** The 128 MiB run showed stable 1280 MiB backing allocation and a 434.2 MiB live-usage menu baseline across three mission cycles. On the larger-device Tutorial run, 64 MiB reduced backing to 1088 MiB and 32 MiB reduced it further to 960 MiB, while live usage stayed about 485 MiB. On the 4 GiB large mission, 64 MiB and 32 MiB both crashed; 32 MiB created 39 chunks and entered heavier native and swap pressure while reducing backing only modestly. Sokol has been tested successfully on that 4 GiB mission, so the DXVK 1 cap is restored to 64 MiB and Sokol is the working fallback there. No memory guard or allocator cleanup policy change is included.
