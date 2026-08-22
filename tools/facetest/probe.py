# 数值实验: 用 numpy 模拟 OpenCV 检测流程, 评估 lena 脸部窗口的级联输出
import cv2
import numpy as np
import xml.etree.ElementTree as ET

tree = ET.parse(r'D:/stm322025program/robot_four_leg/QT_APP/haarcascade_frontalface_default.xml')
root = tree.getroot()

# 解析 features
feats = []
for f in root.find('.//features').findall('_'):
    rects = []
    for r in f.find('rects').findall('_'):
        nums = r.text.split()
        rects.append((int(nums[0]), int(nums[1]), int(nums[2]), int(nums[3]), float(nums[4])))
    feats.append(rects)

# 解析 stages
stages = []
for s in root.find('.//stages').findall('_'):
    thr = float(s.find('stageThreshold').text)
    stumps = []
    for w in s.find('weakClassifiers').findall('_'):
        in_ = w.find('internalNodes').text.split()
        lv = w.find('leafValues').text.split()
        stumps.append((int(in_[2]), float(in_[3]), float(lv[0]), float(lv[1])))
    stages.append((thr, stumps))
print('features:', len(feats), 'stages:', len(stages))

# lena 窗口 (OpenCV 检出 x=216 y=202 w=174 h=174) → 金字塔缩小为 24x24
img = cv2.imread(r'D:/stm322025program/robot_four_leg/tools/facetest/lena.jpg')
gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
gray = cv2.equalizeHist(gray)
win24 = cv2.resize(gray[202:202+174, 216:216+174], (24, 24), interpolation=cv2.INTER_AREA)

# 积分图 (含 padding)
ii = np.zeros((25, 25), dtype=np.int64)
ii[1:, 1:] = win24.cumsum(axis=0).cumsum(axis=1)

def rectsum(x, y, w, h):
    return int(ii[y+h, x+w] - ii[y, x+w] - ii[y+h, x] + ii[y, x])

INV = 1.0 / (24 * 24)
passed = 0
for si, (thr, stumps) in enumerate(stages):
    total = 0.0
    for fi, t, lv0, lv1 in stumps:
        val = 0.0
        for rx, ry, rw, rh, wt in feats[fi]:
            val += wt * rectsum(rx, ry, rw, rh)
        val *= INV
        total += (lv0 if val < t else lv1)
    if total < thr:
        print(f'stage {si}: sum={total:.3f} < thr={thr:.3f}  REJECT')
        print('通过的 stage 数:', passed)
        break
    passed += 1
else:
    print('全部 25 级通过! 窗口是人脸')
