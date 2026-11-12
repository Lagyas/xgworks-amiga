"""A made-up XG4 folder in XGworks' voice list format (CRLF, cp1252) to test
the reading on the PC and on the 68k (vamos). Every name here is invented;
nothing of Yamaha's lists is in the repository.
    python3 tools/gen_xg4_fixture.py <folder>"""
import os
import sys

d = sys.argv[1]
os.makedirs(d, exist_ok=True)


def w(name, lines):
    with open(os.path.join(d, name), 'wb') as f:
        f.write(('\r\n'.join(lines) + '\r\n').encode('cp1252'))


w('ws.ini', ['[other]', 'x=1', '[voice list]', '0=Tones', '16=Samples', '64=Effects', '121=Second',
             '126=FX Kits', '127=Kits', '[kit list]', '126=FX Kits', '127=Kits', '[bank lsb]', '0=Capital'])
tones = ['[Tones]'] + ['%d=Tone %03d' % (p, p + 1) for p in range(128)]
for p in range(128):
    tones.append('[Tone %03d]' % (p + 1))
    if p % 3 == 0:
        tones += ['0=Tone %03d' % (p + 1), '1=Tone %03d Bright *4' % (p + 1), '18=Tone %03d Dark ***' % (p + 1)]
w('Tones.ini', [';bank msb = 0'] + tones)
w('Samples.ini', [';msb=16', '[Samples]'] + ['%d=Sample %d' % (p, p) for p in range(0, 128, 7)] +
  ['[Sample 0]', '0=Sample zero *6', '1=Sample one'])
w('Effects.ini', [';bank msb = 64', '[Effects]', '0=Noise A', '1=Noise B', '[Noise A]', '0=Noise A', '[Noise B]'])
w('Second.ini', [';bank msb = 121', '[Second]', '0=SecA', '1=SecB', '[SecA]', '0=SecA', '1=SecA2', '[SecB]'])
kits = [';bank msb = 127', '[Kits]', '0=Basic Kit', '25=Machine Kit ***', '', '[Basic Kit]']
kits += ['%d=Drum %d' % (k, k) for k in range(13, 85)]
kits += ['[Machine Kit ***]', '36=Machine Kick', '38=Machine Snare', '42=Machine Hat']
w('Kits.ini', kits)
w('FX Kits.ini', [';bank msb = 126', '[FX Kits]', '0=FX Kit', '[FX Kit]', '40=Zap', '41=Boom'])
user = ['[Other]', '0=0x', '[User]']
order = list(range(0, 31)) + [32, 37, 34, 51, 49, 44, 46, 42, 31, 40, 38, 33, 35, 36, 50, 48, 47, 45, 43, 41, 39]
order += [k for k in range(128) if k not in order]
for r, k in enumerate(order):
    user += ['%d=%d%s' % (r, k, '************' if k < 13 else 'Drum %d' % k), 'InstColor%d=%d' % (r, (k * 7) % 6)]
w('inst.ini', user)
with open(os.path.join(d, 'midi-file-editor.html'), 'w', encoding='utf-8') as f:
    f.write('<script>\n// made up\nconst OTHER = [[1,2]];\nconst SC8850_VOICES = [\n')
    f.write(',\n'.join('[%d,4,%d,"GS %d.%d"]' % (m, p, m, p) for p in range(128) for m in (0, 8) if m == 0 or p % 4 == 0))
    f.write('\n];\nconst GS_VOICES = [' + ','.join('[%d,1,%d,"Map %d.%d"]' % (m, p, m, p) for p in range(128) for m in (0, 16) if m == 0 or p % 8 == 0))
    f.write('];\nconst SX900_VOICES = [[0,122,0,"Piano","Kbd Grand \\u00e9","S.Art!"],[104,11,0,"Piano","Kbd Pop","x"],[127,0,0,"Drums","Kbd Kit","Drum"]];\n')
    f.write('const MU1000_VOICES = [[0,0,0,"Normal \u2014 Native","MU Grand"],[0,1,0,"Normal","MU Grand 2"],[64,0,3,"SFX","MU Noise"],[127,0,0,"Drum Kit","MU Std Kit"]];\n')
    f.write("const GS_BASE_DRUM_KITS = [ ['Base Standard',0],['Base Room',8] ];\n")
    f.write('const GS_DRUM_KITS = [["GS Standard", 0], ["GS \\"Room\\"", 8], ["GS 808", 25]];\n</script>\n')
print('fixture in', d)
