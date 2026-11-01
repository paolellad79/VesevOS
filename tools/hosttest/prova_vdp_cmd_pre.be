# Funzioni finte delle primitive (le vere sono nel firmware): una microSD in memoria. Poi il codice [cmd] del driver, poi le prove.
# Il testo stampato si confronta con prova_vdp_cmd_atteso.txt; lo stato della scheda finta e controllato con assert.
var FS = {}
var MT = true
var FR = 0     # formattazione in corso (come fmt_run di sd_status)
var FL = 0     # esito dell'ultima: 0 mai, 1 riuscita, 2 non riuscita
var FLS = 0
var FE = ''
def sd_status() return {'state': MT ? 1 : 2, 'fs': 'FAT32', 'total_kb': 7600000, 'used_kb': 10, 'khz': 1000, 'fmt_run': FR, 'fmt_secs': FR ? 75 : 0, 'fmt_last': FL, 'fmt_last_secs': FLS, 'fmt_err': FE} end
def sd_mount() MT = true return true end
def sd_format(w) if w != 'FORMATTA' raise 'value_error', 'serve la parola' end FS = {} FR = 1 return true end
def sd_eject() return true end
def sd_mkdir(p) FS[p] = true return true end
def sd_write(p, d, a) if a && FS.contains(p) FS[p] = FS[p] + d else FS[p] = d end return true end
def sd_read(p, o) if FS.contains(p) && type(FS[p]) == 'string' return bytes().fromstring(FS[p]) end return nil end
def sd_rm(p) if FS.contains(p) FS.remove(p) return true end raise 'io_error', 'non c e' end
var LD = []
def listdir(p, i) LD.push(p) return {'items': [{'name': 'a.txt', 'dir': false, 'size': 12, 'mtime': 0}, {'name': 'dir', 'dir': true, 'size': 0, 'mtime': 0}], 'more': true, 'next': 2} end
