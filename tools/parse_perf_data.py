import argparse
import os
import sqlite3
import sys
import time

date_format_list = ["%Y-%m-%d %H:%M:%S", "%H:%M:%S", "%M:%S"]

def convert_ts_to_date(ts, ts_micro, convert_format):
  time_array = time.localtime(ts)
  style_time = time.strftime(convert_format, time_array)
  result = style_time + "." + str(ts_micro // 1000).rjust(3, '0') + "." + str(ts_micro % 1000).rjust(3, '0')
  return result

def write2File(file_name, lines):
  with open(file_name, 'a+') as f:
    lines = [line + "\n" if not line.endswith("\n") else line for line in lines]
    f.writelines(lines)

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("-i", dest="input", required=True, help="input database file name")
  parser.add_argument("-o", dest="output", default="perf_data_output", help="output csv file name")
  parser.add_argument("-format", dest="date_format", type=int, default=0, choices=[0, 1, 2], help="date format, 0: Y-M-D H:M:S.ms.us, 1: H:M:S.ms.us, 2: M:S.ms.us")
  args = parser.parse_args()

  filename = args.input
  print("perf database file name : ", filename)
  result_filename = args.output + '.csv'
  if os.path.exists(result_filename):
    os.remove(result_filename)
    print("remove file " + result_filename)
  convert_format = date_format_list[args.date_format]

  sql_conn = sqlite3.connect(filename)
  sql_cur = sql_conn.cursor()

  sql_cur.execute("select name from sqlite_master where type='table'")
  tab_names = sql_cur.fetchall()
  tab_names = [line[0] for line in tab_names]

  time_covert = lambda item : convert_ts_to_date(item // 1000000, item % 1000000, convert_format)
  for table_index, name in enumerate(tab_names):
    sql_cur.execute('pragma table_info({})'.format(name))
    col_name = sql_cur.fetchall()
    col_valid_value = [x[0] for x in col_name if x[1].endswith("time")]
    file_string = []
    col_head = [x[1] for x in col_name]
    file_string.append(name +  ", " + ", ".join(col_head))
    print("Table name : ", name, "\nIndex : ", col_head)

    result = sql_cur.execute("select * from  " + name)
    for line in result:
      line = [time_covert(item) if item and i in col_valid_value else item for i, item in enumerate(line)]
      line = map(str, line)
      file_string.append(name + ", " + ", ".join(line))
      #print(line)
    write2File(result_filename, file_string)
  print("result is wirtten to output file : " + result_filename)
  sql_conn.close()

