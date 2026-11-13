#!/usr/bin/env python3
"""Build original MIT-licensed IME tables and bounded system word dictionary.

Source strings are UTF-8 for review; runtime artifacts contain Shift-JIS only.
No external dictionary, network, locale or Python package is required.
"""
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parent

def sjis(text):
    return text.encode('shift_jis')

def literal(text):
    return '"' + ''.join('\\%03o' % b for b in sjis(text)) + '"'

def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xffffffff
    return h

def tables():
    roman = {}
    for consonant, row in [('', 'あいうえお'), ('k', 'かきくけこ'),
            ('g', 'がぎぐげご'), ('s', 'さしすせそ'), ('z', 'ざじずぜぞ'),
            ('t', 'たちつてと'), ('d', 'だぢづでど'), ('n', 'なにぬねの'),
            ('h', 'はひふへほ'), ('b', 'ばびぶべぼ'), ('p', 'ぱぴぷぺぽ'),
            ('m', 'まみむめも'), ('r', 'らりるれろ')]:
        roman.update({consonant + v: k for v, k in zip('aiueo', row)})
    roman.update(dict(ya='や', yu='ゆ', yo='よ', wa='わ', wi='うぃ', we='うぇ', wo='を',
        shi='し', chi='ち', tsu='つ', fu='ふ', ji='じ', sha='しゃ', shu='しゅ', sho='しょ',
        cha='ちゃ', chu='ちゅ', cho='ちょ', ja='じゃ', ju='じゅ', jo='じょ',
        she='しぇ', che='ちぇ', je='じぇ', ye='いぇ', fa='ふぁ', fi='ふぃ', fe='ふぇ', fo='ふぉ',
        tsa='つぁ', tsi='つぃ', tse='つぇ', tso='つぉ', thi='てぃ', thu='てゅ', dhi='でぃ', dhu='でゅ',
        twu='とぅ', dwu='どぅ', kwa='くぁ', gwa='ぐぁ', whi='うぃ', whe='うぇ', who='うぉ',
        va='ヴぁ', vi='ヴぃ', vu='ヴ', ve='ヴぇ', vo='ヴぉ'))
    for stem, kana in [('ky','き'),('gy','ぎ'),('sy','し'),('zy','じ'),('ty','ち'),('cy','ち'),
            ('dy','ぢ'),('ny','に'),('hy','ひ'),('by','び'),('py','ぴ'),('my','み'),('ry','り'),('jy','じ')]:
        for vowel, small in [('a','ゃ'),('u','ゅ'),('o','ょ'),('e','ぇ'),('i','ぃ')]:
            roman[stem+vowel] = kana+small
    for prefix in ['x', 'l']:
        for suffix, kana in [('a','ぁ'),('i','ぃ'),('u','ぅ'),('e','ぇ'),('o','ぉ'),
                ('ya','ゃ'),('yu','ゅ'),('yo','ょ'),('tu','っ'),('tsu','っ'),('wa','ゎ'),('ka','ヵ'),('ke','ヶ')]:
            roman[prefix+suffix] = kana
    kana = dict(zip('1234567890-^\\qwertyuiopasdfghjkl;:]zxcvbnm,./_',
        'ぬふあうえおやゆよわほへーたていすかんなにらせちとしはきくまのりれけむつさそひこみもねるめろ'))
    small = dict(zip('34567890ez', 'ぁぅぇぉゃゅょをぃっ'))
    # Shifted printable codes produced by the JIS BIOS, independent of CapsLock.
    shifted = dict(zip('!"#$%&\'()=~|<>?', '1234567890-\\,./'))
    accents = []
    for a, b in zip('かきくけこさしすせそたちつてとはひふへほう', 'がぎぐげござじずぜぞだぢづでどばびぶべぼヴ'):
        accents.append((a, b, dict(zip('はひふへほ','ぱぴぷぺぽ')).get(a, '')))
    out = ['/* Generated from original UTF-8 tables by ime-build.py. */',
        'typedef struct {const char *key,*text;} KiMapping;',
        'static const KiMapping ki_roman[] = {']
    out += ['{%s,%s},' % (json.dumps(k),literal(v)) for k,v in sorted(roman.items())]
    out += ['};', 'static const KiMapping ki_kana[] = {']
    out += ['{%s,%s},' % (json.dumps(k),literal(v)) for k,v in kana.items()]
    out += ['};', 'static const KiMapping ki_small[] = {']
    out += ['{%s,%s},' % (json.dumps(k),literal(v)) for k,v in small.items()]
    out += ['};', 'static const KiMapping ki_shifted[] = {']
    out += ['{%s,%s},' % (json.dumps(k),json.dumps(v)) for k,v in shifted.items()]
    out += ['};', 'static const char *ki_accents[][3] = {']
    out += ['{%s,%s,%s},' % tuple(literal(x) for x in row) for row in accents]
    out += ['};', 'static const KiMapping ki_half[] = {']
    half = '。「」、・をぁぃぅぇぉゃゅょっーあいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわん゛゜'
    out += ['{%s,%s},' % (literal(chr(0xff61+i)),literal(k)) for i,k in enumerate(half)]
    out += ['};', 'static const char *ki_script[][2] = {']
    for code in range(0x3041, 0x3094):
        out.append('{%s,%s},' % (literal(chr(code)),literal(chr(code+0x60))))
    return '\n'.join(out + ['};', ''])

def dictionary():
    rows, seen = [], set()
    for line in (ROOT/'ime/words.tsv').read_text().splitlines():
        if not line or line.startswith('#'): continue
        key, *values = line.split('\t')
        assert key not in seen and values and len(values) <= 12, key
        seen.add(key)
        encoded = [sjis(s) for s in [key,*values]]
        assert all(0 < len(s) <= 96 and not any(b < 32 for b in s) for s in encoded), key
        assert len(set(values)) == len(values), key
        payload = encoded[0] + b''.join(bytes([len(s)])+s for s in encoded[1:])
        rows.append(struct.pack('<HBB',4+len(payload),len(encoded[0]),len(values))+payload)
    body = b''.join(rows)
    result = struct.pack('<8s6I',b'KGIMED1\0',1,32+len(body),len(rows),fnv(body),0,0)+body
    assert len(result) <= 32768 and 0 < len(rows) <= 1024
    return result, len(rows)

if __name__ == '__main__':
    dest = Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'build'
    dest.mkdir(parents=True,exist_ok=True)
    data,count = dictionary()
    (dest/'ime-tables.h').write_text(tables())
    (dest/'KGIME.DIC').write_bytes(data)
    print(f'Original IME dictionary: {count} readings, {len(data)} bytes')
