#!/usr/bin/env python3
"""Разбор скриншота KSnake: геометрия панелей, обрезка текста, OCR.

Использование:
    python3 tests/check_shot.py tests/build/u.bmp.240.bmp

Печатает по каждому файлу:
  * размер кадра и число «светлых» пикселей на границе (edge) — должно быть 0,
    иначе панель или текст вылезает за окно;
  * bbox жёлтой кнопки Check for updates и число янтарных пикселей;
  * bbox панелей (цвет 26,26,26), если на кадре есть экран с панелью;
  * распознанный текст (нужен tesseract; при отсутствии строка пропускается).

Код возврата всегда 0: скрипт предназначен для чтения глазами, а не для CI.
"""

import glob
import os
import subprocess
import sys
from collections import Counter

try:
    from PIL import Image, ImageFile
    ImageFile.LOAD_TRUNCATED_IMAGES = True
except ImportError:
    sys.exit("check_shot: нужен Pillow (pip install pillow)")


def edge_pixels(im):
    w, h = im.size
    px = im.load()
    n = 0
    for y in range(h):
        for x in (0, w - 1):
            if sum(px[x, y]) > 250:
                n += 1
    for x in range(w):
        for y in (0, h - 1):
            if sum(px[x, y]) > 250:
                n += 1
    return n


def bbox_where(im, pred):
    w, h = im.size
    px = im.load()
    xs, ys = [], []
    for y in range(h):
        for x in range(w):
            if pred(px[x, y]):
                xs.append(x)
                ys.append(y)
    if not xs:
        return None, 0
    return (min(xs), max(xs), min(ys), max(ys)), len(xs)


def is_amber(c):
    r, g, b = c
    return r > 200 and 150 < g < 225 and b < 130


def is_panel(c):
    return abs(c[0] - 26) <= 3 and abs(c[1] - 26) <= 3 and abs(c[2] - 26) <= 3


def ocr(path):
    try:
        up = path + ".2x.png"
        im = Image.open(path).convert("RGB")
        im.resize((im.width * 2, im.height * 2), Image.LANCZOS).save(up)
        out = subprocess.run(["tesseract", up, "-"], capture_output=True, text=True)
        os.remove(up)
        return " | ".join(l.strip() for l in out.stdout.splitlines() if l.strip())
    except FileNotFoundError:
        return "(tesseract не установлен)"


def main():
    paths = []
    for arg in sys.argv[1:]:
        paths.extend(sorted(glob.glob(arg)) or [arg])

    for path in paths:
        im = Image.open(path).convert("RGB")
        w, h = im.size
        print("== %s  %dx%d  edge=%d" % (os.path.basename(path), w, h, edge_pixels(im)))

        panel, n_panel = bbox_where(im, is_panel)
        if n_panel:
            print("   блок (26,26,26): x=%d..%d y=%d..%d (w=%d h=%d)" % (
                panel[0], panel[1], panel[2], panel[3], panel[1] - panel[0] + 1,
                panel[3] - panel[2] + 1))

        amber, n_amber = bbox_where(im, is_amber)
        if n_amber:
            print("   янтарных=%d bbox=%s" % (n_amber, amber))

        raw = im.tobytes()
        colors = Counter(raw[i:i + 3] for i in range(0, len(raw), 3))
        colors = Counter({tuple(k): v for k, v in colors.items()})
        print("   цвета: %s" % ", ".join("%s×%d" % (c, n) for c, n in colors.most_common(3)))
        print("   текст: %s" % ocr(path))


if __name__ == "__main__":
    main()