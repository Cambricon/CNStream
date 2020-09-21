from flask import Flask, render_template, request, json, jsonify, Response
from http.server import HTTPServer, BaseHTTPRequestHandler
import os, socket, pickle, logging
import subprocess, threading
import cv2, time
import queue
import numpy as np
import signal
from sys import path
path.append("webserver")
import pycnservice

preview_video_size = (1080, 620)
render_fps = 30
timeouts = 100
data_path = "./webui/static/data/"
json_path = "./webui/static/json/"
upload_json_path = "./webui/static/json/user/"

app = Flask("CNStream Service", template_folder="./webui/template", static_folder="./webui/static")
logging.basicConfig(level=logging.INFO)
cnservice = pycnservice.PyCNService()

run_demo_subprocess = 0

def refreshPreview():
  global cnservice
  if cnservice.is_running():
    cnservice.stop()
    time.sleep(2)

def render_empty_frame():
  yield (b'--frame\r\n' + b'Content-Type: image/jpeg\r\n\r\n')

def getPreviewFrame():
  global cnservice
  render_with_interval = False
  logging.info("cnservice get one frame")
  frame_info = pycnservice.CNSFrameInfo()
  frame_data = np.ndarray([preview_video_size[1], preview_video_size[0], 3], dtype=np.uint8)
  background_img = cv2.imread(data_path + "black.jpg")
  ret, encode_img = cv2.imencode('.jpg', background_img)
  background_img_bytes = encode_img.tobytes()
  timeout_cnt = 0
  while True:
    if cnservice.is_running():
      start = time.time()
      ret = cnservice.read_one_frame(frame_info, frame_data)
      if (False == ret):
        timeout_cnt += 1
        if timeout_cnt > timeouts:
          logging.info("read frame timeout, stop")
          cnservice.stop()
      elif (True == frame_info.eos_flag):
        logging.info("read frame eos, stop")
        cnservice.stop()
      else:
        timeout_cnt = 0
        ret, jpg_data = cv2.imencode(".jpg", cv2.resize(frame_data, preview_video_size))
        if ret:
          frame_bytes = jpg_data.tobytes()
          yield (b'--frame\r\n'
                 b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
      done = time.time()
      elapsed = done - start
      if render_with_interval:
        if 1/render_fps - elapsed > 0:
          time.sleep(1/render_fps - elapsed)
      render_with_interval = True
    else:
      yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + background_img_bytes + b'Content-Type: image/jpeg\r\n\r\n')
      time.sleep(1)
  yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + background_img_bytes + b'Content-Type: image/jpeg\r\n\r\n')
  del frame_data

demo_console_output_que = queue.Queue()
def getDemoConsleOutput():
  while not demo_console_output_que.empty():
    yield demo_console_output_que.get()

def process(filename, config_json):
  global run_demo_subprocess
  cwd = os.getcwd() + "/"
  run_demo_subprocess = subprocess.Popen(["../../../samples/bin/demo", "--data_name=" + cwd + filename,
      "--data_path=../files.list_video", "--perf_db_dir=./perf_cache",
      "--src_frame_rate=30", "--wait_time=0", "--config_fname", config_json,
      "--alsologtostderr"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  
  while run_demo_subprocess.poll() is None:
    output_stdout = run_demo_subprocess.stdout.readline().strip()
    if output_stdout:
      print(output_stdout.decode("utf8"))
      demo_console_output_que.put(output_stdout.decode("utf8") + "\n")
  output_stderr = run_demo_subprocess.stderr.readlines()
  for line in output_stderr:
    line = line.strip()
    print(line.decode("utf8"))
  demo_console_output_que.put("!@#$run demo done")

def getSourceUrl(filename):
  global render_fps
  if filename:
    render_fps = 30
    if filename == "cars":
      filename = "cars.mp4"
    elif filename == "people":
      filename = "1080P.h264"
    elif filename == "images":
      filename = "%d.jpg"
      render_fps = 1
    elif filename == "objects":
      filename = "objects.mp4"
    else :
      filename = "user/" + filename
    filename = data_path + filename
  return filename
