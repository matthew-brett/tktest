# -*- Mode: Python -*-
"""
Test looking for tk modules
"""

from __future__ import print_function


cdef extern:
    int load_tkinter_funcs()


def search_for_tk():
    return load_tkinter_funcs()
