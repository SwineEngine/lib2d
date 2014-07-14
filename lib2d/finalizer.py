import weakref
from functools import partial

# Adapted from
# http://code.activestate.com/recipes/577242-calling-c-level-finalizers-without-__del__/
# to make a python 2 compatible finalizer to match python 3 weakref.finalize
class OwnerRef(weakref.ref):
    """A simple weakref.ref subclass, so attributes can be added."""
    pass

def _run_finalizer(ref):
    """Internal weakref callback to run finalizers"""
    del _finalize_refs[id(ref)]
    finalizer = ref.finalizer
    try:
        finalizer()
    except Exception:
        traceback.print_exc()

_finalize_refs = {}

def finalize(owner, finalizer):
    ref = OwnerRef(owner, _run_finalizer)
    ref.finalizer = finalizer
    _finalize_refs[id(ref)] = ref

    return partial(_run_finalizer, ref)

