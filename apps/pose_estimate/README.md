Instructions: 

- `./test_data/`: some test videos
- `./openpose_config_mlu100.json`: config file for openpose on mlu100
- `./run_mlu100.sh`: bash file to run openpose app on mlu100, which will check and download openpose cambricon model automaticly

Usage(make sure neuware version is v8.2.0):

1. compile cnstream(enable build_apps option)
2. `./run_mlu100.sh`
3. output file for the app:`./output/0.avi`
