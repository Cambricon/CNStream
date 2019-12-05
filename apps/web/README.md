# Cambricon Web Demo #

The web demo is mainly developed based on CNStream SDK, The modules involved in the development are flask, gunicorn and nginx.
th following operations is in the directory of cnstream/apps/web/.

## Quick Start ##

1. Run the following command to prepare the  required environment.
   ```bash
   ./pre_required_helper.sh
   ```

2. Run the following command to start your Web, the default address is 0.0.0.0:10001
   ```bash
   ./run.sh
   ```

## Use Skills ##

1. the following is content of web.conf
   ```code
   bind = "0.0.0.0:10001"
   workers = 4
   backlog = 2048
   worker_class = "gevent"
   daemon = False
   ```
   You can modify "bind" to change the ip and port of th binding, modify "workers" to change the number of processes started and modify "daemon" to True to run in the background.

2. Run the following command to config nginx.
   ```bash
   sudo service nginx stop
   sudo nginx -c $PWD/nginx/nginx.conf
   sudo service nginx start
   ``` 
   You can visit your web by your ip.

## Tips ##

1. If you have some problems just like the following
   ```bash
   Traceback (most recent call last):
     File "/usr/local/lib/python3.5/dist-packages/gunicorn/app/base.py", line 100, in get_config_from_filename
       mod = importlib.util.module_from_spec(spec)
     File "<frozen importlib._bootstrap>", line 574, in module_from_spec
   AttributeError: 'NoneType' object has no attribute 'loader'
   ```
   It's possible that the version of gunicorn doesn't match, the following command maybe help you!
   ```bash
   pip3 install gunicorn==19.9.0 -i https://pypi.doubanio.com/simple
   ```
