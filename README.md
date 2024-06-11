# kibis2spice
This is a standalone version of the ibis to spice converter inside of KiCad.

## Compile
```
CXX -std=c++20 -Ikicad_compat kibis2spice.cpp
```

## Usage
```
Usage:
  kibis2spice [options] IBIS_MODEL OUTPUT_DIR
OPTIONS
  -b bits       number of bits of prbs
  -d delay      delay before output happens in seconds
  -f frequency  frequency of prbs
```
