import flask
import logging
import os
import sys
import werkzeug
import datetime
from utils import exifutil, osd
from PIL import Image
if sys.version_info.major == 2:
    import cStringIO as StringIO
    import urllib2
else:
    from io import StringIO
    from io import BytesIO
    from urllib import request
    import base64
from interface import detector
import cv2 as cv


UPLOAD_FOLDER = os.path.abspath(
    os.path.dirname(os.path.abspath(__file__)) + '/img_uploads')

app = flask.Flask(__name__)


my_detector = detector.Detector()
config_path = "./detection_config.json"
my_detector.build_pipeline_by_JSONFile(config_path)
label = osd.parse_label(config_path)
colors = osd.generate_colors(len(label));

@app.route('/', methods=['GET', 'objPOST'])
def classify_index():
    string_buffer = None

    if flask.request.method == 'GET':
        url = flask.request.args.get('url')
        if url:
            logging.info('Image: %s', url)
            string_buffer = request.urlopen(url).read()

        file = flask.request.args.get('file')
        if file:
            logging.info('Image: %s', file)
            string_buffer = open(file, 'rb').read()

        if not string_buffer:
            imagesrc = flask.url_for("static", filename="image/1.jpg")
            return flask.render_template('detection.html', imagesrc=imagesrc, has_result=True, jsonstr="")

    elif flask.request.method == 'POST':
        string_buffer = flask.request.stream.read()

    if not string_buffer:
        resp = flask.make_response()
        resp.status_code = 400
        return resp
    # names, time_cost, accuracy = app.clf.classify_image(file)
    names, time_cost, accuracy = ["cat"], 0.1314, [0.54]
    return flask.make_response(u",".join(names), 200, {'ClassificationAccuracy': accuracy})


@app.route('/detection_url', methods=['GET'])
def detection_url():
    imageurl = flask.request.args.get('image_url', '')
    filename_ = str(datetime.datetime.now()).replace(' ', '_') + 'samplefile.png'
    localfile = os.path.join(UPLOAD_FOLDER, filename_)
    
    try:
        bytes = request.urlopen(imageurl).read()
        # save to tmp,just for temp use for my classify api.

        tmpfile = open(localfile, 'wb')
        tmpfile.write(bytes)
        tmpfile.close();

        if sys.version_info.major == 2:
            string_buffer = StringIO.StringIO(bytes)
        else:
            string_buffer = BytesIO(bytes)
        image = exifutil.open_oriented_im(localfile)

    except Exception as err:
        # For any exception we encounter in reading the image, we will just
        # not continue.
        logging.info('URL Image open error: %s', err)
        return flask.render_template(
            'detection.html', imagesrc="", has_result=False,
            jsonstr='Cannot open image from URL.'
        )

    app.logger.info('Image: %s', imageurl)

    # names, time_cost, probs = app.clf.classify_image(localfile)
    chn_idx = my_detector.detect_image(localfile)
    json_file = "/tmp/" + str(chn_idx) + ".json";
    image_shape = image.shape
    ids, cls, bboxs, scores, json_str = osd.parse_json_file(json_file, image_shape, label)
    osd.draw_labels(image, ids, cls, bboxs, scores, colors)    
    os.remove(localfile)
    return flask.render_template(
        'detection.html',
        imagesrc=embed_image_html(image), has_result=True,
        jsonstr=json_str
    )


@app.route('/detection_upload', methods=['POST'])
def classify_upload():
    try:
        # We will save the file to disk for possible data collection.
        imagefile = flask.request.files['image_file']
        filename_ = str(datetime.datetime.now()).replace(' ', '_') + \
            werkzeug.secure_filename(imagefile.filename)
        filename = os.path.join(UPLOAD_FOLDER, filename_)
        imagefile.save(filename)
        path, extension = os.path.splitext(filename)
        if extension == '.png':
            im = Image.open(filename)
            im = im.convert("RGB")
            os.remove(filename)
            filename = "%s.jpg" % path
            im.save(filename)

        logging.info('Saving to %s.', filename)
        image = exifutil.open_oriented_im(filename)

    except Exception as err:
        logging.info('Uploaded image open error: %s', err)
        return flask.render_template(
            'detection.html', imagesrc="", has_result=False,
            jsonstr='Cannot open uploaded image.'
        )

    # names,time_cost, probs = app.clf.classify_image(filename)
    chn_idx = my_detector.detect_image(filename)
    json_file = "/tmp/" + str(chn_idx) + ".json";
    image_shape = image.shape
    ids, cls, bboxs, scores, json_str = osd.parse_json_file(json_file, image_shape, label)
    osd.draw_labels(image, ids, cls, bboxs, scores, colors)    
    os.remove(filename)

    return flask.render_template(
        'detection.html', imagesrc=embed_image_html(image), has_result=True, jsonstr=json_str
    )


def embed_image_html(image):
    """Creates an image embedded in HTML base64 format."""
    image_pil = Image.fromarray((255 * image).astype('uint8'))
    if sys.version_info.major == 2:
        string_buf=StringIO.StringIO()
        image_pil.save(string_buf, format='png')
        data = string_buf.getvalue().encode('base64').replace('\n', '')
    else:
        _buf = BytesIO()
        image_pil.save(_buf, format='png')
        _buf.seek(0)
        b64_buf = base64.b64encode(_buf.getvalue())
        string_buf = StringIO(b64_buf.decode('utf-8', errors='replace'))
        data =string_buf.getvalue().replace('\n', '')

    return 'data:image/png;base64,' + data


logging.getLogger().setLevel(logging.INFO)

if not os.path.exists(UPLOAD_FOLDER):
    os.makedirs(UPLOAD_FOLDER)

if __name__ == "__main__":
    app.run()

