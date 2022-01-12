###############################################################################
# Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

import logging
from logging.handlers import QueueHandler
import queue

log_queue = queue.Queue()

def setup_logger() -> logging.Logger:
  # create logger
  logger = logging.getLogger('cnstream_service')
  logger.propagate = False

  # create formatter
  # formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
  formatter = logging.Formatter('%(levelname)s - %(message)s')

  # create console handler and set level to debug
  ch = logging.StreamHandler()
  ch.setLevel(logging.DEBUG)
  ch.setFormatter(formatter)

  # create queue handler and set level to info
  qh = QueueHandler(log_queue)
  qh.setLevel(logging.INFO)
  qh.setFormatter(formatter)

  # add handler to logger
  logger.addHandler(ch)
  logger.addHandler(qh)

  return logger

logger = setup_logger()

