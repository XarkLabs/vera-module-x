# VERA module - Video Embedded Retro Adapter

This repository contains the files related to the VERA module. This module is developed for the Commander X16 computer by The 8-Bit Guy.

## Xark OSS modifications

This [repository branch](https://github.com/XarkLabs/vera-module-x/tree/oss-x) is tracking branch <https://github.com/fvdhoef/vera-module/tree/rev4> with the following changes

* XARK_OSS      changes required to build with OSS FPGA tools (from [YosysHQ nightly builds](https://github.com/YosysHQ/oss-cad-suite-build/releases/latest))
* XARK_BUGFIX   changes to address a few minor VERA bugs
* XARK_UPDUINO  changes required to (somewhat) build for UPduino FPGA board (vs official VERA board)
* Minor Verilog cleanup (rigourous no default nettype and some harmless typos fixed)

For OSS tools, build from `cd fpga` and you can specify options on command line:

```bash
UPDUINO=true BUGFIX=true make clean bin
```

It should also still build normally for VERA board with Lattice Radiant (optionally you can `` `define XARK_BUGFIX`` to enable fixes).

**NOTE**: The Commander X16 logo is part of the Commander X16 project and should not be used when the module is used with other projects.
