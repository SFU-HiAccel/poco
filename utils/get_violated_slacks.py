#!/usr/bin/python3
import sys
import argparse
import logging

#logger = logging.getLogger.getChild(__name__)


class SlackViolation:
  def __init__(self, name):
    self.name = name
    self.data = {
        "slack": -1,
        "source": '',
        "destination": '',
        "pathgroup": '',
        "pathtype": '',  # Setup or Hold
        "requirement": -1,
        "data_path_delay_logic": -1,
        "data_path_delay_route": -1,
        "logic_level_lut2": -1,
        "logic_level_lut3": -1,
        "logic_level_lut4": -1,
        "logic_level_lut5": -1,
        "logic_level_lut6": -1,
        "clock_path_skew_destination_clock_delay": -1,
        "clock_path_skew_source_clock_delay": -1,
        "clock_path_skew_clock_pessimism_removal": -1,
        "clock_uncertainty_total_system_jitter": -1,
        "clock_uncertainty_discrete_jitter": -1,
        "clock_uncertainty_phase_error": -1,
        "clock_net_delay_source": -1,
        "clock_net_delay_destination": -1,
    }
  def parse_violation(self, line_buffer):
    for line in line_buffer:
      try:
        x,y = line.strip().split(':')
        #update_self(x
      except ValueError as e:
        pass # ignore this line since it has more details about the source/destination
  

def create_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
    prog='get_violated_slacks',
    description='Grabs all the violated slacks into a tabular format'
  )
  parser.add_argument(
  '-if',
  '--input-file',
  type=str,
  dest='input_file',
  required=True,
  help='the report file that should be used to get the slack violations',
  )
  return parser

def process_slack_violations(arg_rpt_file, violations):
  with open(arg_rpt_file, 'r') as rpt_file:
    contents = rpt_file.readlines()
    for line_start_idx in range(len(contents)):
      line_buffer = []
      if "Slack (VIOLATED)" in contents[line_start_idx]:
        violation = SlackViolation(line_start_idx)
        line_end_idx = line_start_idx
        last_line = contents[line_end_idx]
        line_buffer.append(last_line)
        # read until the last line's regex
        while "Clock Net Delay (Destination)" not in last_line:
          line_end_idx+=1
          last_line = contents[line_end_idx]
          line_buffer.append(last_line)
        violation.parse_violation(line_buffer)
        violations.append(violation)
        line_start_idx = line_end_idx

violations = []

if __name__ == "__main__":
  parser = create_parser()
  args = parser.parse_args()
  process_slack_violations(args.input_file, violations)


