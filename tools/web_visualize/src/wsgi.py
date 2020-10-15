#!/usr/bin/python
from webserver import app

if __name__ == "__main__":
  app.run(debug=True, host="0.0.0.0", port=9099)
else:
  gunicorn_app =app
