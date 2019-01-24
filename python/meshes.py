from netgen.meshing import *
from netgen.csg import *
import ngsolve

def Make1DMesh(n, mapping = None, periodic=False):
    """
    Generate an equidistant 1D mesh with N cells

    Parameters
    ----------
    n : int
      Number of cells.

    mapping: lamda
      Mapping to transform the generated points. If None, the identity mapping is used.

    periodic: bool
      If True, the endpoints are identified to generate a periodic mesh.

    Returns
    -------
    (ngsolve.mesh)
      Returns generated 1D NGSolve mesh

    """
    mesh = Mesh(dim=1)
    pids = []
    for i in range(n+1):
        x = i/n
        if mapping:
            x = mapping(x)
        pids.append (mesh.Add (MeshPoint(Pnt(x, 0, 0))))
    for i in range(n):
        mesh.Add(Element1D([pids[i],pids[i+1]],index=1))
    mesh.Add (Element0D( pids[0], index=1))
    mesh.Add (Element0D( pids[n], index=2))
    mesh.SetBCName(0,"left")
    mesh.SetBCName(1,"right")
    if periodic:
        mesh.AddPointIdentification(pids[0],pids[n],1,2)
    ngsmesh = ngsolve.Mesh(mesh)
    return ngsmesh

def MakeStructured2DMesh(quads=True, nx=10, ny=10, secondorder=False, periodic_x=False, periodic_y=False, mapping = None):
    """
    Generate a structured 2D mesh

    Parameters
    ----------
    quads : bool
      If True, a quadrilateral mesh is generated. If False, the quads are split to triangles.

    nx : int
      Number of cells in x-direction.

    ny : int
      Number of cells in y-direction.

    periodic_x: bool
      If True, the left and right boundaries are identified to generate a periodic mesh in x-direction.

    periodic_y: bool
      If True, the top and bottom boundaries are identified to generate a periodic mesh in y-direction.

    mapping: lamda
      Mapping to transform the generated points. If None, the identity mapping is used.
    

    Returns
    -------
    (ngsolve.mesh)
      Returns generated 2D NGSolve mesh

    """
    mesh = Mesh()
    mesh.dim=2

    pids = []
    if periodic_y:
        slavei = []
        masteri = []
    if periodic_x:        
        slavej = []
        masterj = []
    for i in range(ny+1):
        for j in range(nx+1):
            x,y = j/nx, i/ny
            # if mapping:
            #    x,y = mapping(x,y)
            pids.append(mesh.Add (MeshPoint(Pnt(x,y,0))))
            if periodic_y:
                if i == 0:
                    slavei.append(pids[-1])
                if i == ny:
                    masteri.append(pids[-1])  
            if periodic_x:                       
                if j == 0:
                    slavej.append(pids[-1])
                if j == nx:
                    masterj.append(pids[-1])        
    if periodic_y:
        for i in range(len(slavei)):   
            mesh.AddPointIdentification(masteri[i],slavei[i],identnr=1,type=2)
    if periodic_x:            
        for j in range(len(slavej)):        
            mesh.AddPointIdentification(masterj[j],slavej[j],identnr=2,type=2)                                       

    mesh.Add(FaceDescriptor(surfnr=1,domin=1,bc=1))
    
    for i in range(ny):
        for j in range(nx):
            base = i * (nx+1) + j
            if quads:
                pnum = [base,base+1,base+nx+2,base+nx+1]
                elpids = [pids[p] for p in pnum]
                el = Element2D(1,elpids)
                if not mapping:
                    el.curved=False
                mesh.Add(el)
            else:
                pnum1 = [base,base+1,base+nx+1]
                pnum2 = [base+1,base+nx+2,base+nx+1]
                elpids1 = [pids[p] for p in pnum1]
                elpids2 = [pids[p] for p in pnum2]
                mesh.Add(Element2D(1,elpids1)) 
                mesh.Add(Element2D(1,elpids2))                          

    for i in range(nx):
        mesh.Add(Element1D([pids[i], pids[i+1]], index=1))
    for i in range(ny):
        mesh.Add(Element1D([pids[i*(nx+1)+nx], pids[(i+1)*(nx+1)+nx]], index=2))
    for i in range(nx):
        mesh.Add(Element1D([pids[ny*(nx+1)+i+1], pids[ny*(nx+1)+i]], index=3))
    for i in range(ny):
        mesh.Add(Element1D([pids[(i+1)*(nx+1)], pids[i*(nx+1)]], index=4))

    mesh.SetBCName(0, "bottom")        
    mesh.SetBCName(1, "right")        
    mesh.SetBCName(2, "top")        
    mesh.SetBCName(3, "left")  

    mesh.Compress()       
    
    if secondorder:
        mesh.SecondOrder()
    
    if mapping:
        for p in mesh.Points():
            x,y,z = p.p
            x,y = mapping(x,y)
            p[0] = x
            p[1] = y
            
    ngsmesh = ngsolve.Mesh(mesh)
    return ngsmesh

def MakeQuadMesh(nx=10, ny=10, periodic_x=False, periodic_y=False, mapping = None):
    """
    Generate a structured quadrilateral 2D mesh

    Parameters
    ----------
    nx : int
      Number of cells in x-direction.

    ny : int
      Number of cells in y-direction.

    periodic_x: bool
      If True, the left and right boundaries are identified to generate a periodic mesh in x-direction.

    periodic_y: bool
      If True, the top and bottom boundaries are identified to generate a periodic mesh in y-direction.

    mapping: lambda
      Mapping to transform the generated points. If None, the identity mapping is used.
    

    Returns
    -------
    (ngsolve.mesh)
      Returns generated 2D NGSolve mesh

    """
    return MakeStructured2DMesh(quads=True, nx=nx, ny=ny, periodic_x=periodic_x, periodic_y=periodic_y, mapping=mapping)    

def MakeStructured3DMesh(hexes=True, nx=10, ny=None, nz=None, secondorder=False, periodic_x=False, periodic_y=False, periodic_z=False, mapping = None, cuboid_mapping=False):
    """
    Generate a structured quadrilateral 2D mesh

    Parameters
    ----------
    hexes: bool
      If True, a mesh consisting of hexahedra is generated.

    nx : int
      Number of cells in x-direction.

    ny : int
      Number of cells in y-direction.

    nz : int
      Number of cells in z-direction.

    periodic_x: bool
      If True, the left and right boundaries are identified to generate a periodic mesh in x-direction.

    periodic_y: bool
      If True, the top and bottom boundaries are identified to generate a periodic mesh in y-direction.

    periodic_z: bool
      If True, the top and bottom boundaries are identified to generate a periodic mesh in z-direction.

    mapping: lambda
      Mapping to transform the generated points. If None, the identity mapping is used.
    
    cuboid_mapping: bool
      If True, a straight geometry is assumed.


    Returns
    -------
    (ngsolve.mesh)
      Returns generated 3D NGSolve mesh

    """
    if nz == None:
        if ny == None:
            nz = nx
        else:
            raise("MakeStructured3DMesh: No default value for nz if nx and ny are provided")
    if ny == None:
        ny = nx
        
    netmesh = Mesh()
    netmesh.dim = 3

    if cuboid_mapping:
        P1 = (0,0,0)
        P2 = (1,1,1)
        if mapping:
            P1 = mapping(*P1)
            P2 = mapping(*P2)
        cube = OrthoBrick(Pnt(P1[0], P1[1], P1[2]), Pnt(P2[0], P2[1], P2[2])).bc(1)
        geom = CSGeometry()
        geom.Add(cube)
        netmesh.SetGeometry(geom)

    pids = []
    if periodic_x:
        slavei = []
        masteri = []
    if periodic_y:        
        slavej = []
        masterj = []
    if periodic_z:        
        slavek = []
        masterk = []        
    for i in range(nx+1):
        for j in range(ny+1):
            for k in range(nz+1):
                # x,y,z = mapping(i / nx, j / ny, k / nz)
                x,y,z = i / nx, j / ny, k / nz
                # if mapping:
                #   x,y,z = mapping(x,y,z)
                pids.append(netmesh.Add(MeshPoint(Pnt( x,y,z ))))
                if periodic_x:
                    if i == 0:
                        slavei.append(pids[-1])
                    if i == nx:
                        masteri.append(pids[-1])  
                if periodic_y:           
                    if j == 0:
                        slavej.append(pids[-1])
                    if j == ny:
                        masterj.append(pids[-1]) 
                if periodic_z:                    
                    if k == 0:
                        slavek.append(pids[-1])
                    if k == nz:
                        masterk.append(pids[-1])
    if periodic_x:
        for i in range(len(slavei)):   
            netmesh.AddPointIdentification(masteri[i],slavei[i],identnr=1,type=2)     
    if periodic_y:        
        for j in range(len(slavej)):            
            netmesh.AddPointIdentification(masterj[j],slavej[j],identnr=2,type=2) 
    if periodic_z:        
        for k in range(len(slavek)):            
            netmesh.AddPointIdentification(masterk[k],slavek[k],identnr=3,type=2)                                                      

    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                base = i * (ny+1)*(nz+1) + j*(nz+1) + k
                baseup = base+(ny+1)*(nz+1)
                pnum = [base, base+1, base+(nz+1)+1, base+(nz+1),
                        baseup, baseup+1, baseup+(nz+1)+1, baseup+(nz+1)]
                if hexes:
                    elpids = [pids[p] for p in pnum]
                    el = Element3D(1, elpids)
                    if not mapping:
                        el.curved = False
                    netmesh.Add(el)
                else:
                    #  a poor mans kuhn triangulation of a cube
                    for qarr in [[0, 4, 5, 6],
                                 [0, 6, 7, 4],
                                 [0, 3, 7, 6],
                                 [0, 1, 6, 5],
                                 [0, 1, 2, 6],
                                 [0, 3, 6, 2]]:
                        elpids = [pids[p] for p in [pnum[q] for q in qarr]]
                        netmesh.Add(Element3D(1, elpids))

    def AddSurfEls(p1, dxi, nxi, deta, neta, facenr):
        def add_seg(i, j, os):
            base = p1 + i*dxi + j*deta
            pnum = [base, base+os]
            elpids = [pids[p] for p in pnum]
            netmesh.Add(Element1D(elpids, index=facenr))
        for i in range(nxi):
            for j in [0,neta]:
                add_seg(i,j,dxi)
        for i in [0,nxi]:
            for j in range(neta):
                add_seg(i,j,deta)
        for i in range(nxi):
            for j in range(neta):
                base = p1 + i*dxi+j*deta
                pnum = [base, base+dxi, base+dxi+deta, base+deta]
                if hexes:
                    elpids = [pids[p] for p in pnum]
                    netmesh.Add(Element2D(facenr, elpids))
                else:
                    qarrs = [[0, 1, 2], [0, 2, 3]]
                    for qarr in qarrs:
                        elpids = [pids[p] for p in [pnum[q] for q in qarr]]
                        netmesh.Add(Element2D(facenr, elpids))

    for i in range(6):
        netmesh.Add(FaceDescriptor(surfnr=i, domin=1, bc=i+1))

    # y-z-plane, smallest x-coord: ("back")
    AddSurfEls(0, 1, nz,  nz+1, ny, facenr=1) # y-z-plane
    # x-z-plane, smallest y-coord: ("left")
    AddSurfEls(0, (ny+1)*(nz+1), nx, 1, nz,facenr=2)
    # y-z-plane, largest x-coord: ("front")
    AddSurfEls((nx+1)*(ny+1)*(nz+1)-1, -(nz+1), ny, -1, nz, facenr=3) 
    # x-z-plane, largest y-coord: ("right")
    AddSurfEls((nx+1)*(ny+1)*(nz+1)-1, -1, nz, -(ny+1)*(nz+1), nx, facenr=4)
    # x-y-plane, smallest z-coord: ("bottom")
    AddSurfEls(0, nz+1, ny, (ny+1)*(nz+1), nx,facenr=5) 
    # x-y-plane, largest z-coord: ("top")
    AddSurfEls((nx+1)*(ny+1)*(nz+1)-1, -(ny+1)*(nz+1), nx, -(nz+1), ny, facenr=6) 

    if cuboid_mapping:
        netmesh.SetBCName(0,"back")
        netmesh.SetBCName(1,"left")
        netmesh.SetBCName(2,"front")
        netmesh.SetBCName(3,"right")
        netmesh.SetBCName(4,"bottom")
        netmesh.SetBCName(5,"top")
    
    netmesh.Compress()

    if secondorder:
        netmesh.SecondOrder()
    
    if mapping:
        for p in netmesh.Points():
            x,y,z = p.p
            x,y,z = mapping(x,y,z)
            p[0] = x
            p[1] = y
            p[2] = z
            
    ngsmesh = ngsolve.Mesh(netmesh)
    # ngsmesh.ngmesh.Save("tmp.vol.gz")
    # ngsmesh = ngsolve.Mesh("tmp.vol.gz")
    return ngsmesh

def MakeHexMesh(nx=10, ny=10, nz=10, secondorder=secondorder, periodic_x=False, periodic_y=False, periodic_z=False, mapping = None, cuboid_mapping=False):
    """
    Generate a structured quadrilateral 2D mesh

    Parameters
    ----------
    nx : int
      Number of cells in x-direction.

    ny : int
      Number of cells in y-direction.

    nz : int
      Number of cells in z-direction.

    periodic_x: bool
      If True, the left and right boundaries are identified to generate a periodic mesh in x-direction.

    periodic_y: bool
      If True, the top and bottom boundaries are identified to generate a periodic mesh in y-direction.

    periodic_z: bool
      If True, the top and bottom boundaries are identified to generate a periodic mesh in z-direction.

    mapping: lambda
      Mapping to transform the generated points. If None, the identity mapping is used.
    
    cuboid_mapping: bool
      If True, a straight geometry is assumed.


    Returns
    -------
    (ngsolve.mesh)
      Returns generated 3D NGSolve mesh consisting of only hexahedra

    """
    return MakeStructured3DMesh(hexes=True, nx=nx, ny=ny, nz=nz, secondorder=secondorder, periodic_x=periodic_x, periodic_y=periodic_y, periodic_z=periodic_z, mapping=mapping, cuboid_mapping=cuboid_mapping)

from math import pi
from ngsolve import Draw, sin, cos
if __name__ == "__main__":

    mesh = Make1DMesh(n=4)
    print("simple 1D mesh -- no visualization -- ")

    mesh = MakeQuadMesh(nx=4, ny=4)
    Draw(mesh)
    input("simple quad mesh -- press any key to continue -- ")

    mesh = MakeStructured2DMesh(quads=False, nx=4, ny=4)
    Draw(mesh)
    input("simple trig mesh -- press any key to continue -- ")    

    mesh = MakeQuadMesh(nx=4, ny=4, periodic_x=True, periodic_y=False)
    Draw(mesh)
    input("x-periodic quad mesh -- press any key to continue -- ") 

    mesh = MakeStructured2DMesh(quads=False, nx=16, ny=4, periodic_x=False, periodic_y=True)
    Draw(mesh)
    input("y-periodic non-uniform trig mesh -- press any key to continue -- ")        
    
    mesh = MakeStructured2DMesh(quads=True, nx=32, ny=16,
                            mapping = lambda x,y : ((1.1+sin(pi*(y-0.5)))*sin(pi*x),
                                                (1.1+sin(pi*(y-0.5)))*cos(pi*x)))
    Draw(mesh)
    input("structured quad half circle with a whole -- press any key to continue -- ")
    
    mesh = MakeHexMesh(nx=8)
    Draw(mesh)
    input("simple cube mesh -- press any key to continue -- ")

    mesh = MakeHexMesh(nx=8, periodic_x=True, periodic_y=True, periodic_z=True)
    Draw(mesh)
    input("periodic cube mesh -- press any key to continue -- ")    
    
    mesh = MakeStructured3DMesh(hexes=False, nx=3, ny=6, nz=10,
                            mapping = lambda x,y,z : (x,0.5*y*(y+x),exp(z)),
                            cuboid_mapping=False )
    Draw(mesh)
    input("mapped, anisotropic linear non-cuboid mesh -- press any key to continue -- ")
    
    mesh = MakeStructured3DMesh(hexes=True, nx=8, ny=16, nz=8,
                            mapping = lambda x,y,z : (5*x*x*(0.5-x/3),10*y*y*(0.5-y/3),5*z*z*(0.5-z/3)),
                            cuboid_mapping=True )
    Draw(mesh)
    input("mapped, anisotropic non-linear cuboid mesh -- press any key to continue --")

    mesh = MakeStructured3DMesh(hexes=True, nx=8, ny=16, nz=8, periodic_x=True,
                            mapping = lambda x,y,z : (5*x*x*(0.5-x/3),10*y*y*(0.5-y/3),5*z*z*(0.5-z/3)),
                            cuboid_mapping=True )
    Draw(mesh)
    print("x-periodic, mapped, anisotropic non-linear cuboid mesh -- finished.")    
