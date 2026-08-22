# 修正实验: 精确模拟 OpenCV 语义 (金字塔层: 缩小 INTER_LINEAR → 该层均衡 → 24x24 窗口)
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

def rectsum(ii, x, y, w, h):
    return int(ii[y+h, x+w] - ii[y, x+w] - ii[y+h, x] + ii[y, x])

INV = 1.0 / 576.0

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

# 精确模拟: 金字塔层 scale = 24/174 (脸部在层上正好 24px)
# OpenCV: resize(gray, fx=scale, INTER_LINEAR) → equalizeHist(该层) → 窗口
scale = 24.0 / 174.0
layer = cv2.resize(gray, None, fx=scale, fy=scale, interpolation=cv2.INTER_LINEAR)
layer = cv2.equalizeHist(layer)
x0 = int(216 * scale); y0 = int(202 * scale)
# 扫描附近偏移找最佳窗口
best = (0, None)
for dy in range(-2, 3):
    for dx in range(-2, 3):
        win = layer[y0+dy:y0+dy+24, x0+dx:x0+dx+24]
        if win.shape != (24, 24):
            continue
        si, _ = eval_win(win)
        if si > best[0]:
            best = (si, (dx, dy))
        if si == 25:
            print(f'★ 窗口 dx={dx} dy={dy} 全级通过!')
            break
print('最佳: 通过', best[0], '级, 偏移', best[1])
