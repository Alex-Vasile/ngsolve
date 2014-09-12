print ("Hello from init.py")
import code
import ngfem_cf
import ngmpi
import sys

def mydir(x=None):
    if x==None:
        return []
    else:
        return [i for i in dir(x) if not '__' in i]

def execfile(fname):
    exec(open(fname).read())

def startConsole():
    import code
    try:
        import readline
        import rlcompleter
        readline.parse_and_bind("tab:complete") # autocomplete
    except:
        try:
            import pyreadline as readline
            import rlcompleter
            readline.parse_and_bind("tab:complete") # autocomplete
        except:
            print('readline not found')
    vars = globals()
    vars.update(locals())
    shell = code.InteractiveConsole(vars)
    shell.interact()

class InteractiveMPIConsole(code.InteractiveConsole):
# Function copied from /usr/lib/python3.4/code.py line 38 
    def runsource(self, source, filename="<input>", symbol="single"):
        try:
            compiled_code = self.compile(source, filename, symbol)
        except (OverflowError, SyntaxError, ValueError):
            # Case 1
            self.showsyntaxerror(filename)
            return False

        if compiled_code is None:
            # Case 2
            return True

        # Case 3 -- first send code to other mpi processes
        ngmpi.SendCommand('ngs_py '+source)
        # then run it on master
        code.InteractiveConsole.runcode(self, compiled_code)

        # Avoid the prompt to show up before other processes' output
        self.Barrier()
        return False

    def interact(self):
        self.write("MPI Shell\n")
        self.write("================\n")
        self.write("Use pprint(str) to print with MPI ranks\n\n")
        self.runsource("from ngmpi import *\n")
        self.runsource("pprint=lambda p='':print('Process ' + str(ngmpi.Rank()) + '\\n'+str(p)+'\\n')\n")
        self.locals.update(locals())
        self.locals.update(globals())
        sys.ps1 = "MPI >>> "
        sys.ps2 = "MPI ... "
        code.InteractiveConsole.interact(self,'')
        sys.ps1 = ">>> "
        sys.ps2 = "... "
    def Barrier(self):
        source = 'ngmpi.Barrier()'
        ngmpi.SendCommand('ngs_py '+source)
        code.InteractiveConsole.runsource(self, source)


def MpiShell():
    import code
    try:
        import readline
        import rlcompleter
        readline.parse_and_bind("tab:complete") # autocomplete
    except:
        try:
            import pyreadline as readline
            import rlcompleter
            readline.parse_and_bind("tab:complete") # autocomplete
        except:
            print('readline not found')
    vars = globals()
    vars.update(locals())
    mshell = InteractiveMPIConsole(vars)
    mshell.interact()

startConsole()


