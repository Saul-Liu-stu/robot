# -*- coding: utf-8 -*-
# 用真实金字塔因子 1.1^k 模拟 OpenCV, 扫描窗口偏移
import cv2
import numpy as np
import xml.etree.ElementTree as ET

tree = ET.parse(r'D:/stm322025program/robot_four_leg/QT_APP/haarcascade_frontalface_default.xml')
root = tree.getroot()
feats = []
for f in root.find('.//features').findall('_'):
    rects = []
    for r in f.find('rects').findall('_'):
        nums = r.text.split()
        rects.append((int(nums[0]), int(nums[1]), int(nums[2]), int(nums[3]), float(nums[4])))
    feats.append(rects)
stages = []
for s in root.find('.//stages').findall('_'):
    thr = float(s.find('stageThreshold').text)
    stumps = []
    for w in s.find('weakClassifiers').findall('_'):
        in_ = w.find('internalNodes').text.split()
        lv = w.find('leafValues').text.split()
        stumps.append((int(in_[2]), float(in_[3]), float(lv[0]), float(lv[1])))
    stages.append((thr, stumps))

img = cv2.imread(r'D:/stm322025program/robot_four_leg/tools/facetest/lena.jpg')
gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
INV = 1.0 / 576.0

def rectsum(ii, x, y, w, h):
    return int(ii[y+h, x+w] - ii[y, x+w] - ii[y+h, x] + ii[y, x])

def eval_win(win24):
    ii = np.zeros((25, 25), dtype=np.int64)
    ii[1:, 1:] = win24.cumsum(axis=0).cumsum(axis=1)
    for si, (thr, stumps) in enumerate(stages):
        total = 0.0
        for fi, t, lv0, lv1 in stumps:
            val = 0.0
            for rx, ry, rw, rh, wt in feats[fi]:
                val += wt * rectsum(ii, rx, ry, rw, rh)
            val *= INV
            total += (lv0 if val < t else lv1)
        if total < thr:
            return si, total
    return 25, None

# 真实金字塔层: factor = 1.1^k, 脸 174px 在该层 23.5px
factor = 1.1 ** 21
layer = cv2.resize(gray, None, fx=1.0/factor, fy=1.0/factor, interpolation=cv2.INTER_LINEAR)
layer = cv2.equalizeHist(layer)
x0 = int(216 / factor); y0 = int(202 / factor)
print('层尺寸:', layer.shape, '窗口起点:', x0, y0)
best = (0, None)
for dy in range(-3, 4):
    for dx in range(-3, 4):
        win = layer[y0+dy:y0+dy+24, x0+dx:x0+dx+24]
        if win.shape != (24, 24):
            continue
        si, total = eval_win(win)
        if si > best[0]:
            best = (si, (dx, dy))
        if si == 25:
            print('★ 全级通过! dx=%d dy=%d' % (dx, dy))
print('最佳: 通过', best[0], '级, 偏移', best[1])
