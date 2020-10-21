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
import signal
import logging
import subprocess

from webserver import core
from webserver.core import json_path
from webserver.core import app, render_template, Response, request

from sys import path
path.append("webserver")
import pycnservice

basedir = os.path.abspath(os.path.dirname(__file__))
logging.info('[CNStreamService] webserver path: ' + basedir)

@app.route("/")
def home():
  logging.info('[CNStreamService] root page')
  core.refreshPreview()
  return render_template('index.html', current_time = int(time.time()))

@app.route("/home")
def homepage():
  logging.info('[CNStreamService] home page')
  core.refreshPreview()
  return render_template('index.html', current_time = int(time.time()))

@app.route("/design")
def design():
  logging.info('[CNStreamService] design page')
  core.refreshPreview()
  return render_template('design.html')

@app.route('/video_feed')
def video_feed():
  return Response(core.getPreviewFrame(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/getPreviewStatus', methods=['GET', 'POST'])
def getPreviewStatus():
  return str(core.cnservice.is_running())

@app.route('/getDemoResult', methods=['GET', 'POST'])
def getDemoResult():
  return Response(core.getDemoConsleOutput())

@app.route('/getPreview', methods=['GET', 'POST'])
def getPreview():
  return render_template("index.html", current_time = int(time.time()))

@app.route("/runDemo", methods=['POST'])
def runDemo():
  logging.info('[CNStreamService] start run pipeline')
  if request.method == 'POST':
    subprocess.run(["rm", "-rf", "perf_cache"])
    input_filename = request.form.get('filename')
    config_json = request.form.get('json')
    mode = request.form.get('mode')
    input_filename = core.getSourceUrl(input_filename)
    config_json = json_path + config_json
    if mode == "status":
      if core.run_demo_subprocess == 0 or core.run_demo_subprocess.poll() is not None:
        core.process(input_filename, config_json)
      else:
        return "Run demo failed"
    elif mode == "preview":
      if core.cnservice.is_running():
        return "Demo is running"
      preview_info = pycnservice.CNServiceInfo()
      preview_info.fps = core.render_fps
      preview_info.register_data = True
      preview_info.dst_width = core.preview_video_size[0]
      preview_info.dst_height = core.preview_video_size[1]
      preview_info.cache_size = 100
      core.cnservice.init_service(preview_info)
      if core.cnservice.start(input_filename, config_json):
        logging.info('[CNStreamService] preview start stream: ' + input_filename)
      else:
        core.cnservice.stop()
        logging.info('[CNStreamService] preview stop !!!!!!')
        return "Run demo failed"
    return "Run demo succeed"

@app.route("/stopDemo", methods=['POST'])
def stopDemo():
  logging.info('[CNStreamService] stop pipeline')
  if request.method == 'POST':
    mode = request.form.get('mode')
    if mode == "status":
      logging.info('[CNStreamService] stop run pipeline status mode')
      if core.run_demo_subprocess == 0 or core.run_demo_subprocess.poll() is not None:
        return "Demo is not running"
      else:
        core.run_demo_subprocess.send_signal(signal.SIGINT)
        return "Stop demo succeed"
    elif mode == "preview":
      logging.info('[CNStreamService] stop run pipeline preview mode')
      if core.cnservice.is_running():
        core.cnservice.stop()
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
    print("receive file : ", upload_file.filename)
    if data_type == "media":
      dst_dir = core.data_path + "/user/"
    else:
      dst_dir = core.upload_json_path
    path_is_exist = os.path.exists(dst_dir)
    if not path_is_exist:
      os.makedirs(dst_dir)
      print("create path ", dst_dir)
    if os.path.exists(dst_dir + upload_file.filename):
      os.remove(dst_dir + upload_file.filename)
    upload_file.save(dst_dir + upload_file.filename)
    ret = "Upload Succeed"

  return ret

@app.route("/checkJson", methods=['POST'])
def checkJson():
  path_is_exist = os.path.exists(core.upload_json_path)
  if not path_is_exist:
    os.makedirs(core.upload_json_path)
    print("create path ", core.upload_json_path)
  with open(core.upload_json_path + 'custom_config.json', 'w') as f:
    json.dump(request.get_json(), f)
    f.close()
  result = subprocess.run(["../../bin/cnstream_inspect", "-c", core.upload_json_path + "custom_config.json"],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  print("Check Json Result: ", str(result.stdout, "utf-8"), str(result.stderr, "utf-8"))

  return str(result.stdout, "utf-8") + "\n" + str(result.stderr, "utf-8")
