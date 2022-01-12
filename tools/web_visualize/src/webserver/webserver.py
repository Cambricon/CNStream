###############################################################################
# Copyright (C) [2020] by Cambricon, Inc. All rights reserved
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
###############################################################################

import os
import time
import json
import logging
logging.basicConfig(level=logging.INFO)
from flask import Flask, render_template, request, json, Response
from webserver import service
from webserver.service import json_path
from webserver.service import cnstream_service

app = Flask("CNStream Service", template_folder="./webui/template", static_folder="./webui/static")

basedir = os.path.abspath(os.path.dirname(__file__))
logging.info('[CNStreamService] webserver path: ' + basedir)

@app.route("/")
def home():
  logging.info('[CNStreamService] root page')
  return render_template('index.html', current_time = int(time.time()))

@app.route("/home")
def homepage():
  logging.info('[CNStreamService] home page')
  return render_template('index.html', current_time = int(time.time()))

@app.route("/design")
def design():
  logging.info('[CNStreamService] design page')
  return render_template('design.html')

@app.route('/video_feed')
def video_feed():
  logging.info('[CNStreamService] video feed')
  return Response(service.getPreviewFrame(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/getPreviewStatus', methods=['GET', 'POST'])
def getPreviewStatus():
  return str(cnstream_service.is_running())

@app.route('/getDemoResult', methods=['GET', 'POST'])
def getDemoResult():
  logging.debug("[CNStreamService] request service log")
  return Response(service.getDemoConsoleOutput())

@app.route('/getPreview', methods=['GET', 'POST'])
def getPreview():
  logging.info('[CNStreamService] render preview')
  return render_template("index.html", current_time = int(time.time()))

@app.route("/runDemo", methods=['POST'])
def runDemo():
  logging.info('[CNStreamService] start Demo')
  if request.method == 'POST':
    input_filename = request.form.get('filename')
    config_json = request.form.get('json')
    mode = request.form.get('mode')
    input_filename = service.getSourceUrl(input_filename)
    config_json = json_path + config_json
    if mode == "status":
      logging.warning("The 'status' mode is deprecated")
    elif mode == "preview":
      if cnstream_service.is_running():
        return "Demo is running"
      if cnstream_service.Start(input_filename, config_json):
        logging.info('Start stream: ' + input_filename)
      else:
        logging.error('Failed to start stream {}'.format(input_filename))
        cnstream_service.Stop()
        return "Run demo failed"
    logging.info('[CNStreamService] start Demo done')
    return "Run demo succeed"

@app.route("/stopDemo", methods=['POST'])
def stopDemo():
  logging.info('[CNStreamService] stop Demo')
  if request.method == 'POST':
    mode = request.form.get('mode')
    if mode == "status":
      logging.warning("The 'status' mode is deprecated")
    elif mode == "preview":
      logging.info('[CNStreamService] stop run pipeline preview mode')
      if service.cnstream_service.is_running():
        service.cnstream_service.Stop()
        return "Stop demo succeed"
      else:
        return "Demo is not running"
  return ""

@app.route("/uploadFile", methods=['POST'])
def saveFile():
  upload_file = request.files.get('data')
  data_type = request.form.get('type')
  ret = "Upload Failed"
  if (upload_file):
    logging.info("receive file : {}".format(upload_file.filename))
    if data_type == "media":
      dst_dir = service.data_path + service.user_media + "/"
    elif data_type == "json":
      dst_dir = service.upload_json_path
    else:
      dst_dir = service.upload_json_path
    path_is_exist = os.path.exists(dst_dir)
    if not path_is_exist:
      os.makedirs(dst_dir)
      logging.info("create path " + dst_dir)
    if os.path.exists(dst_dir + upload_file.filename):
      os.remove(dst_dir + upload_file.filename)
    upload_file.save(dst_dir + upload_file.filename)
    ret = "Upload Succeed"

  return ret

@app.route("/saveJson", methods=['POST'])
def saveJson():
  logging.info('save JSON string to {}'.format(service.upload_json_path))
  path_is_exist = os.path.exists(service.upload_json_path)
  if not path_is_exist:
    os.makedirs(service.upload_json_path)
    logging.info("create path {}".format(service.upload_json_path))
  with open(service.upload_json_path + 'custom_config.json', 'w') as f:
    json.dump(request.get_json(silent=False), f)
  return "save file done"
