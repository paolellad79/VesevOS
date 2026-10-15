#!/usr/bin/env python3
# VesevOS - genera le schede modulo di esempio (.vdp) in docs/devs. Icone disegnate da noi (forme semplici, nessun marchio); lo schema lo disegna la pagina dai dati (power, level, part).
# Uso: python3 tools/mkdevs.py   (poi tools/hosttest/run.sh le controlla con il vero parser)
import os
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'devs')
G, B, R, K, W, Y = '#1e7a4c', '#2d6cdf', '#c0392b', '#222222', '#ffffff', '#d4a017'

def svg(w, h, body): return '<svg viewBox="0 0 %d %d">%s</svg>' % (w, h, body)

ICON_RELAY = svg(24, 24, '<rect x="3" y="6" width="18" height="12" rx="2" fill="none" stroke="#2d6cdf" stroke-width="2"/><path d="M8 12h3l2-3v6l2-3h1" fill="none" stroke="#2d6cdf" stroke-width="2"/>')
ICON_LED = svg(24, 24, '<circle cx="12" cy="10" r="6" fill="none" stroke="#c0392b" stroke-width="2"/><path d="M9 16v4M15 16v4" fill="none" stroke="#c0392b" stroke-width="2"/>')
ICON_BTN = svg(24, 24, '<circle cx="12" cy="12" r="8" fill="none" stroke="#555" stroke-width="2"/><circle cx="12" cy="12" r="4" fill="#555"/>')
ICON_PIR = svg(24, 24, '<path d="M4 16a8 8 0 0 1 16 0z" fill="none" stroke="#1e7a4c" stroke-width="2"/><circle cx="12" cy="16" r="2" fill="#1e7a4c"/>')
ICON_ADC = svg(24, 24, '<path d="M3 17c3-8 6 4 9-3s6 3 9-6" fill="none" stroke="#d4a017" stroke-width="2"/>')

CARDS = []
def relay(n):
    sig = '\n'.join('IN%d' % (i + 1) for i in range(n))
    return ('rele-%dch' % n, 'Rele %d canal%s' % (n, 'e' if n == 1 else 'i'), 'rele', 'low', None, '5V',
            'Contatti verso la rete elettrica: serve un installatore. Verifica active=low',
            sig, ICON_RELAY, 'level=3V3\n')
CARDS += [relay(1), relay(2), relay(4)]
CARDS += [('led-5mm', 'LED 5 mm con resistenza', 'led', 'high', None, 'none', 'Metti una resistenza da 330 ohm in serie: mai il LED diretto sul pin', 'OUT', ICON_LED, 'level=3V3\npart=serie,330 ohm,OUT\n'),
          ('pulsante', 'Pulsante verso massa', 'ingresso', 'low', 'up', 'none', 'Un capo sul pin, l\'altro su GND: la resistenza e interna (pull-up)', 'IN', ICON_BTN, 'level=3V3\n'),
          ('pir-hc-sr501', 'Sensore di movimento PIR', 'ingresso', 'high', 'none', '5V', 'Il segnale e a 3,3 V sulla maggior parte dei moduli: controlla il tuo', 'OUT', ICON_PIR, 'level=3V3\n'),
          ('ldr-partitore', 'Sensore di luce LDR', 'analogico', 'high', None, '3V3', 'Partitore con resistenza da 10 kohm; non superare 3,3 V sul pin', 'A0', ICON_ADC, 'level=3V3\npart=gnd,10 kohm,A0\n')]

def write():
    os.makedirs(OUT, exist_ok=True)
    for (name, title, cls, act, pull, power, warn, sig, icon, extra) in CARDS:
        t = '# Scheda modulo di esempio VesevOS (dati propri, licenza CC0). Controlla sempre i collegamenti sul datasheet del tuo modulo.\n[module]\nname=%s\ntitle=%s\nclass=%s\n' % (name, title, cls)
        if act == 'low': t += 'active=low\n'
        if pull: t += 'pull=%s\n' % pull
        t += 'power=%s\n%sauthor=VesevOS\nversion=1.1\nlicense=CC0\nwarn=%s\n[signals]\n%s\n[icon]\n%s\n' % (power, extra, warn, sig, icon)
        assert len(warn) <= 80, (name, len(warn))
        open(os.path.join(OUT, name + '.vdp'), 'w').write(t)
        print('scritto', name + '.vdp', len(t), 'byte')
write()
