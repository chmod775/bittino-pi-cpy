# Build the testmod.mpy smoke-test module.
#
# Usage:
#   make MPY_DIR=/path/to/circuitpython ARCH=armv6m     # RP2040
#   make MPY_DIR=/path/to/circuitpython ARCH=xtensawin  # ESP32-S2
#
# MPY_DIR must point at your CircuitPython checkout (the directory containing
# the py/ folder). The patched tools/mpy_ld.py from that tree will be used.

# Name of the module
MOD = testmod

# Source files
SRC = testmod.c

# Default architecture (override with ARCH= on the command line)
ARCH ?= armv6m

# Path to CircuitPython checkout
MPY_DIR = /home/chmod775/repos/bittino-pi-cpy

# Include the standard dynruntime Makefile from CircuitPython
include $(MPY_DIR)/py/dynruntime.mk
