from ngsolve.fem import *
from ngsolve.comp import *
from ngsolve.comp import HCurlFunctionsWrap

x = CoordCF(0)
y = CoordCF(1)
z = CoordCF(2)

def Laplace (coef):
    return BFI("laplace", coef=coef)

def Mass (coef):
    return BFI("mass", coef=coef)


def Source (coef):
    return LFI("source", coef=coef)

def Neumann (coef):
    return LFI("neumann", coef=coef)



def H1(mesh, **args):
    """
    Create H1 finite element space.
    documentation of arguments is available in FESpace.
    """
    fes = FESpace("h1ho", mesh, **args)
    return fes

def L2(mesh, **args):
    """ Create L2 finite element space. """
    return FESpace("l2ho", mesh, **args)

class HCurl(HCurlFunctionsWrap):
    def __init__(self,mesh,**args):
        FESpace.__init__(self,"hcurlho",mesh,**args)

#def HCurl(mesh, **args):
#    """ Create H(curl) finite element space. """
#    return FESpace("hcurlho", mesh, **args)

def HDiv(mesh, **args):
    """ Create H(div) finite element space. """
    return FESpace("hdivho", mesh, **args)

def FacetFESpace(mesh, **args):
    """ Create Facet finite element space. """
    return FESpace("facet", mesh, **args)



def grad(func):
    if func.derivname != "grad":
        raise Exception("cannot form grad")
    return func.Deriv()

def curl(func):
    if func.derivname != "curl":
        raise Exception("cannot form curl")
    return func.Deriv()

def div(func):
    if func.derivname != "div":
        raise Exception("cannot form div")
    return func.Deriv()


import pickle

def NgsPickler(*args, **kargs):
    pickler = pickle.Pickler(*args, **kargs)
    dumped_pids = []

    def my_persistent_id(obj):
        try:
            pid = obj.__ngsid__()
            if pid in dumped_pids:
                return dumped_pids.index(pid)
            else:
                dumped_pids.append(pid)
                return obj
        except:
            return None

    pickler.persistent_id = my_persistent_id
    return pickler

def NgsUnpickler(*args, **kargs):
    unpickler = pickle.Unpickler(*args, **kargs)
    loaded_pids = []

    def my_persistent_load(pid):
        if hasattr(pid,'__ngsid__'):
            loaded_pids.append(pid)
            return pid
        else:
            return loaded_pids[pid]

    unpickler.persistent_load = my_persistent_load
    return unpickler


__all__ = ['x', 'y', 'z', 'Laplace', 'Mass', 'Source', 'Neumann', 'H1', 'FacetFESpace', 'HCurl', 'HDiv', 'L2', 'grad', 'curl', 'div','NgsPickler', 'NgsUnpickler' ]
