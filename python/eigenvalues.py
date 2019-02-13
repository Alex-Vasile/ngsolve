from ngsolve.la import InnerProduct
from math import sqrt
from ngsolve import Projector, Norm, Matrix

try:
    import scipy.linalg
    from scipy import random
except:
    pass


def PINVIT(mata, matm, pre, num=1, maxit=20, printrates=True):
    """preconditioned inverse iteration"""

    r = mata.CreateRowVector()
    Av = mata.CreateRowVector()
    Mv = mata.CreateRowVector()

    uvecs = []
    for i in range(num):
        uvecs.append (mata.CreateRowVector())
    
    vecs = []
    for i in range(2*num):
        vecs.append (mata.CreateRowVector())

    for v in uvecs:
        r.FV().NumPy()[:] = random.rand(len(r.FV()))
        v.data = pre * r

    asmall = Matrix(2*num, 2*num)
    msmall = Matrix(2*num, 2*num)
    lams = num * [1]

    for i in range(maxit):
        
        for j in range(num):
            vecs[j].data = uvecs[j]
            r.data = mata * vecs[j] - lams[j] * matm * vecs[j]
            vecs[num+j].data = pre * r

        for j in range(2*num):
            Av.data = mata * vecs[j]
            Mv.data = matm * vecs[j]
            for k in range(2*num):
                asmall[j,k] = InnerProduct(Av, vecs[k])
                msmall[j,k] = InnerProduct(Mv, vecs[k])

        ev,evec = scipy.linalg.eigh(a=asmall, b=msmall)
        lams[:] = ev[0:num]
        if printrates:
            print (i, ":", lams)
    
        for j in range(num):
            uvecs[j][:] = 0.0
            for k in range(2*num):
                uvecs[j].data += float(evec[k,j]) * vecs[k]

    return lams, uvecs
