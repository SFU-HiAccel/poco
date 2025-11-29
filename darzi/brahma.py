#!/usr/bin/python3
import argparse
import os
import sys
import logging
from typing import TextIO
from pathlib import Path
import json

logging.basicConfig(filename='.log', filemode='w', level=logging.DEBUG)
logger = logging.getLogger(__name__)

def create_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
    prog='brahma_darzi',
    description='Stitches a source kernel file with multiple BRAHMA cores',
  )
  parser.add_argument(
    '-v',
    '--verbose',
    action='count',
    dest='verbose',
    help='Increase logging verbosity',
  )
  parser.add_argument(
    '--adir',
    type=str,
    dest='asset_directory',
    required=True,
    help='Use this directory to find all the CPP assets to stitch into the source kernel',
  )
  parser.add_argument(
    '--src',
    type=str,
    metavar='file',
    dest='src_kernel_file',
    required=True,
    help='Use this file as the source kernel to stitch Brahma into',
  )
  parser.add_argument(
    '--dest',
    type=str,
    metavar='file',
    dest='dest_kernel_file',
    default='out/kernel_wbrahma.cpp',
    help='Use this file as the destination monolith',
  )
  parser.add_argument(
    '--top',
    type=str,
    metavar='top_func',
    dest='top_func',
    required=True,
    help='The top function or the kernel name to work for',
  )
  parser.add_argument(
    '--unit_sb_top',
    dest='unit_sb_top',
    action='store_true',
    help='Collapse all tasks inside the shared buffer to a single topwrap task. This will force Autobridge to keep all of shared buffer in one slot',
  )
  parser.add_argument(
    '--buffer_config',
    dest='buffer_config',
    required=True,
    help='JSON structure that contains the configuration for the shared buffer'
  )
  return parser

def process_json(json_file_path):
  # Open and read the JSON file
  with open(json_file_path, 'r') as file:
    # Parse the JSON data into a Python dictionary
    data = json.load(file)
  # Print the parsed data
  # print("Parsed JSON data:")
  # print(json.dumps(data, indent=2))  # Print with pretty formatting  
  return data

class SharedBuffer:
  config = {}
  decl = ''
  asset_directory = ''
  keys = ['datatype', 'length', 'partition', 'num_parts', 'memcore', 'blocks', 'pages']
  def __init__(self):
    pass

  def update_sb_config_from_json(self, filepath:any):
    with open(filepath, 'r') as file:
      # Parse the JSON data into a Python dictionary
      _data = json.load(file)
    data = _data[0]
    for key in self.keys:
      self.config[key] = data[key]
    
    strategy = self.config['partition']
    if(strategy == 'block' or strategy == 'cyclic'):
      self.config['num_partitions'] = data['num_parts']
    else:
      self.config['num_partitions'] = 0


  def get_backend_pages_decl(self, indent_level):
    num_parts = self.config['num_partitions']
    if (num_parts != 0):
      num_parts_str = f'<{num_parts}>'
    else:
      num_parts_str = ''
    
    datatype = self.config['datatype']
    pagesize = self.config['length']
    numpages = self.config['pages']
    memcore  = self.config['memcore']
    strategy = self.config['partition']
    partition_str = f'tapa::array_partition<tapa::{strategy}{num_parts_str}>'
    
    self.decl = f'{" "*indent_level}'                         \
              + f'tapa::buffers<{datatype}[{pagesize}], ' \
              + f'{numpages}, 1, '                        \
              + f'{partition_str}, '                      \
              + f'tapa::memcore<{memcore}>> '             \
              + f'backend_pages;'

    return self.decl

  def add_cpp_assets(self, directory):
    """
    Add CPP assets/tasks from directory to a monolith file object
    """
    self.directory = directory
    monolith = ""
    monolith += ("//////////////////////////////////////\n")
    monolith += ("/// BRAHMA: TASKS\n")
    monolith += ("//////////////////////////////////////\n")
    cpp_files = Path(self.directory).glob('*.cpp')
    # for filename in ['arbiter', 'rqr', 'crp', 'drp', 'pgm', 'ihd', 'ohd', 'rsg']:
      # filename += ".cpp"
      # asset_path = os.path.join(directory, filename)
    ignorelist = ['top_wrapper', 'loopback', 'dummy']
    print(f"Inserting BRAHMA")
    for filename in cpp_files:
      # print(filename, filename.stem)
      if(filename.stem in ignorelist):
        continue
      # print(f"Adding {filename}")
      monolith += ("\n")

      with open(filename, 'r') as cpp_asset:
        cpp_contents = cpp_asset.read()
      monolith += (cpp_contents)

    return monolith

  def get_internal_decls(self, indent_level):
    """
    Add sparse SB TOP so that it can be spread across the entire fabric
    """
    sb_decl_contents = ''
    # add declarations
    with open(os.path.join(self.directory, 'sb_internal_declarations.h'), 'r') as sb_decl_f:
        _sb_decl_contents = sb_decl_f.readlines()
    for i,line in enumerate(_sb_decl_contents):
      sb_decl_contents += (' '*indent_level) + _sb_decl_contents[i]
    # sb_decl_contents.append('\n')
    return sb_decl_contents

  def get_sb_task_invokes(self, indent_level):
    # add invokes
    sb_invokes = ''
    with open(os.path.join(self.directory, 'sb_invokes.h'), 'r') as sb_invokes_f:
        _sb_invokes = sb_invokes_f.readlines()
    for i,line in enumerate(_sb_invokes):
      sb_invokes += (' '*(indent_level*2)) + _sb_invokes[i]
    return sb_invokes
  
"""
Add unit SB TOP top limit it to one SLR
"""
def add_unit_sb_top(monolith:TextIO, directory:any, filename:any, sb_obj:SharedBuffer):
  monolith.write("//////////////////////////////////////\n")
  monolith.write("/// BRAHMA: TOP WRAPPER\n")
  monolith.write("//////////////////////////////////////\n")
  asset_path = os.path.join(directory, filename)
  print(f"Adding {asset_path}")
  monolith.write("\n")

  with open(asset_path, 'r') as asset:
    contents = asset.read()
  monolith.write(contents)


# GLOBAL VARIABLES
sb_tag_list = ['BRAHMA', 'BRAHMA INTERFACE', 'BRAHMA INVOKES']
sb_tags_index = {}

###########
### MAIN
###########
def main():
  parser = create_parser()
  args = parser.parse_args()
  print(args.src_kernel_file, args.dest_kernel_file)

  sb_obj = SharedBuffer()
  sb_obj.update_sb_config_from_json(args.buffer_config)

  # get all contents
  with open(args.src_kernel_file, 'r') as kernel_fo:
    contents = kernel_fo.readlines()
  insert_index = 0
  for tag in sb_tag_list:
    sb_tags_index[tag] = 0

  # get the different tags inserted for BRAHMA
  for line in contents:
    insert_index += 1
    for tag in sb_tag_list:
      # print(line.strip(), f'/// INSERT {tag} ///')
      if (line.strip() == f'/// INSERT {tag} ///'):
        sb_tags_index[tag] = insert_index
  try:
    print(contents[sb_tags_index['BRAHMA']])
  except IndexError as e:
    sys.exit("No tag for inserting BRAHMA core found ( '/// INSERT BRAHMA ///' )")

  # start writing the file
  with open(args.dest_kernel_file, 'w+') as monolith:
    # write the first part (header) uptil the `BRAHMA`` tag
    for line_idx in range(0, sb_tags_index['BRAHMA']):
      monolith.write(contents[line_idx])
    
    # add all SB tasks
    monolith.write("//#include \"brahma.h\"\n\n")
    # monolith = add_cpp_assets(contents, monolith, args.asset_directory, sb_obj)

    if(args.unit_sb_top):
      add_unit_sb_top(monolith, args.asset_directory, "top_wrapper.cpp", sb_obj)
    else:
      monolith = add_sparse_sb_top(monolith, args.asset_directory, args.top_func, sb_obj)

    for line_idx in range(insert_index+1,len(contents)):
      monolith.write(contents[line_idx])

if __name__ == "__main__":
  main()
