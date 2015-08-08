from . import fem
from . import la

##from ngsolve import __platform
##if __platform.startswith('linux') or __platform.startswith('darwin'):
##    # Linux or Mac OS X
##    from libngcomp.comp import *

##if __platform.startswith('win'):
##    # Windows
##    from ngslib.comp import *

from ngslib.comp import *

__all__ = ['BND', 'BilinearForm', 'COUPLING_TYPE', 'CompoundFESpace', 'ElementId', 'BndElementId', 'FESpace', 'GridFunction', 'LinearForm', 'Mesh', 'Preconditioner', 'VOL', 'NumProc', 'PDE', 'PyNumProc', 'Integrate', 'IntegrateLF', 'SymbolicLFI', 'SymbolicBFI', 'IntegrateBF', 'ngsglobals']

