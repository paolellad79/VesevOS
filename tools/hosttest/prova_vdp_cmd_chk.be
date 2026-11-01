
def run(sub, args) print('-- ' + sub + ' ' + str(args)) on_cmd(sub, args) end
run('status', [])
run('ls', [])
assert(LD[0] == '/sd')
run('ls', ['x/y', '2'])
assert(LD[1] == '/sd/x/y')                       # percorso relativo sulla scheda
run('ls', ['/media/sd/z'])
assert(LD[2] == '/media/sd/z')                   # percorso gia completo: non si tocca
run('write', ['/a.txt', 'ciao mondo'])
assert(FS['/sd/a.txt'] == 'ciao mondo')
run('append', ['/a.txt', ' piu'])
assert(FS['/sd/a.txt'] == 'ciao mondo piu')
run('read', ['/a.txt'])
run('read', ['/zz'])
run('rm', ['/a.txt'])
assert(!FS.contains('/sd/a.txt'))
run('rm', ['/a.txt'])                            # l'errore di una primitiva diventa un messaggio, non un'eccezione
run('mkdir', ['/d'])
assert(FS.contains('/sd/d'))
run('mount', [])
run('unmount', [])
run('test', [])
assert(!FS.contains('/sd/provavos.tmp'))
print('-- widget')
on_widget()
MT = false
print('-- widget senza scheda')
on_widget()
MT = true
print('-- format senza conferma')
run('format', [])
print('-- format avviata')
run('format', ['FORMATTA'])
print('-- status durante la formattazione')
run('status', [])
print('-- widget durante la formattazione')
on_widget()
FR = 0
FL = 1
FLS = 95
print('-- status dopo la formattazione')
run('status', [])
FL = 2
FE = 'scheda protetta'
print('-- status dopo una formattazione fallita')
run('status', [])
