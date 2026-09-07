"""Portable replacement for DXVK's unused native Windows resource output."""
import pathlib
import sys

for name in sys.argv[1:]:
    pathlib.Path(name).touch()
