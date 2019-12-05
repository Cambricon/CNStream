import json
import cv2 as cv
import random

def parse_json_file(file_path, image_shape, label):
    ids = []
    names = []
    bboxs = []
    scores = []
    with open(file_path, "r") as f:
        data = json.load(f)
    objs = data["objs"]
    for obj in objs:
        x = int(obj["bbx"]["x"] * image_shape[1])
        y = int(obj["bbx"]["y"] * image_shape[0])
        w = int(obj["bbx"]["w"] * image_shape[1])
        h = int(obj["bbx"]["h"] * image_shape[0])
        bbox = [x, y, w, h]
        bboxs.append(bbox)
        ids.append(int(obj["id"]))
        names.append(label[int(obj["id"])])
        scores.append(obj["score"])
        obj["name"] = label[int(obj["id"])]
        json_str = json.dumps(data)
    return ids, names, bboxs, scores, json_str


def parse_label(config_path):
    label = []
    with open(config_path, "r") as f:
        data = json.load(f)
    label_path = data["osd"]["custom_params"]["label_path"]
    with open(label_path, "r") as lines:
        for line in lines:
            label.append(line.split()[0])
    return label

def HSV2RGB(h, s, v):
    h_i = int(h * 6)
    f = h * 6 - h_i
    p = v * (1 - s)
    q = v * (1 - f * s)
    t = v * (1 - (1 - f) * s)
    if h_i == 0:
        r = v
        g = t
        b = p
    elif h_i == 1:
        r = q
        g = v
        b = t
    elif h_i == 2:
        r = p
        g = v
        b = t
    elif h_i == 3:
        r = p
        g = q
        b = v
    elif h_i == 4:
        r = t
        g = p
        b = v
    elif h_i == 5:
        r = v
        g = p
        b = q
    else:
        r = 1
        g = 1
        b = 1
    return (r, g, b)


def generate_colors(n):
    random.seed(0)
    colors = []
    s = 0.3
    v = 0.99
    for i in range(n):
        h = random.uniform(0.0, 1.0) % 1.0
        colors.append(HSV2RGB(h, s, v))
    return colors

def draw_labels(img, ids, cls, bboxs, scores, colors):
    for i in range(len(bboxs)):
        r, g, b = colors[ids[i]]
        text = cls[i] + " " + str(round(scores[i], 2))
        cv.rectangle(img, (bboxs[i][0], bboxs[i][1]), (bboxs[i][0]+bboxs[i][2], bboxs[i][1]+bboxs[i][3]), (r, g, b), thickness=2)
        cv.rectangle(img, (bboxs[i][0], bboxs[i][1]), (bboxs[i][0]+130, bboxs[i][1]+30), (r, g, b), thickness=-1)
        cv.putText(img, text, (bboxs[i][0]+10, bboxs[i][1]+20), cv.FONT_HERSHEY_SIMPLEX, 0.7, (1-r, 1-g, 1-b), 2)
#    image = img * 255;
#    image = cv.cvtColor(image, cv.COLOR_RGB2BGR)
#    cv.imwrite("./0.jpg", image)
    return img
