#!/usr/bin/env python3

import subprocess
import sys
import re

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def run(args):
  result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  return result.returncode, result.stdout.decode(), result.stderr.decode()

def parse_expectations(file):
  expect_output = []
  expect_parser_errors = []
  expect_runtime_errors = []

  with open(file) as f:
    for line_index, line in enumerate(f):

      # expect output
      m = re.search(r'// expect: (.+)', line)
      if m:
        expect_output.append(m.group(1))

      # parse error with line number
      m = re.search(r'^// (\[line (\d+)\] Error(.+))', line)
      if m:
        expect_parser_errors.append(m.group(1))

      # parse error without line number
      m = re.search(r'.// (Error (.+))', line)
      if m:
        # construct expected parse error with line number
        line_number = line_index + 1
        error = m.group(1)
        full_error = f"[line {line_number}] {error}"
        expect_parser_errors.append(full_error)

      # runtime error
      m = re.search(r'// expect runtime error: (.+)', line)
      if m:
        expect_runtime_errors.append(m.group(1))

  return expect_output, expect_parser_errors, expect_runtime_errors

#
# main
#

if len(sys.argv) < 2:
  print("Usage: test.py <test.lox>")
  exit(2)

file = sys.argv[1]

# parse expectations from input file
expect_output, expect_parser_errors, expect_runtime_errors = parse_expectations(file)

# run input file to get actual output and exit_code
exit_code, stdout, stderr = run(['bin/clox', file])

# split stdout into non-blank lines
stdout_lines = [line for line in stdout.splitlines() if line]

# split stderr into non-blank lines, and which don't match something like "[line 1] in script"
stderr_lines = [line for line in stderr.splitlines() if line and not re.search(r'^\[line (\d+)\] in script$', line)]


# print("testing:", file)
# print(bcolors.OKGREEN + "expect_output:" + bcolors.ENDC, expect_output)
# print(bcolors.FAIL + "expect_parser_errors:" + bcolors.ENDC, expect_parser_errors)
# print(bcolors.FAIL + "expect_runtime_errors:" + bcolors.ENDC, expect_runtime_errors)
# print("exit_code:", exit_code)
# print("stdout_lines:", stdout_lines)
# print("stderr_lines:", stderr_lines)


PASS = bcolors.OKGREEN + "PASS" + bcolors.ENDC
FAIL = bcolors.FAIL + "FAIL" + bcolors.ENDC

SUCCESS_EXIT_CODE = 0
PARSER_EXIT_CODE = 65
RUNTIME_EXIT_CODE = 70

fail = 0

if exit_code not in [SUCCESS_EXIT_CODE, PARSER_EXIT_CODE, RUNTIME_EXIT_CODE]:
  print(FAIL + ": Unexpected exit code: " + str(exit_code))
  fail = 1
else:

  if expect_parser_errors:
    if exit_code != PARSER_EXIT_CODE or not stderr_lines:
      expected = expect_parser_errors[0]
      print(FAIL + ": Expected parser error: " + expected)
      fail = 1
    else:
      for expected, actual in zip(expect_parser_errors, stderr_lines):
        if expected == actual:
          print(PASS + ": expect parser error: " + expected)
        else:
          print(FAIL + ": Expected parser error: " + expected + " - got: " + actual)
          fail = 1

      if len(expect_parser_errors) > len(stderr_lines):
        index = len(stderr_lines)
        expected = expect_parser_errors[index]
        print(FAIL + ": Expected parser error: " + expected)
        fail = 1

      if len(expect_parser_errors) < len(stderr_lines):
        index = len(expect_parser_errors)
        actual = stderr_lines[index]
        print(FAIL + ": Unexpected parser error: " + actual)
        fail = 1

  elif exit_code == PARSER_EXIT_CODE and stderr_lines:
    actual = stderr_lines[0]
    print(FAIL + ": Unexpected parser error: " + actual)
    fail = 1


  if expect_runtime_errors:
    if exit_code != RUNTIME_EXIT_CODE or not stderr_lines:
      expected = expect_runtime_errors[0]
      print(FAIL + ": Expected runtime error: " + expected)
      fail = 1
    else:
      for expected, actual in zip(expect_runtime_errors, stderr_lines):
        if expected == actual:
          print(PASS + ": expect runtime error: " + expected)
        else:
          print(FAIL + ": Expected runtime error: " + expected + " - got: " + actual)
          fail = 1

      if len(expect_runtime_errors) > len(stderr_lines):
        index = len(stderr_lines)
        expected = expect_runtime_errors[index]
        print(FAIL + ": Expected runtime error: " + expected)
        fail = 1

      if len(expect_runtime_errors) < len(stderr_lines):
        index = len(expect_runtime_errors)
        actual = stderr_lines[index]
        print(FAIL + ": Unexpected runtime error: " + actual)
        fail = 1

  elif exit_code == RUNTIME_EXIT_CODE and stderr_lines:
    actual = stderr_lines[0]
    print(FAIL + ": Unexpected runtime error: " + actual)
    fail = 1


  if exit_code == SUCCESS_EXIT_CODE:
    for expected, actual in zip(expect_output, stdout_lines):
      if expected == actual:
        print(PASS + ": expect: " + expected)
      else:
        print(FAIL + ": Expected output: " + expected + " - got: " + actual)
        fail = 1

    if len(expect_output) > len(stdout_lines):
      index = len(stdout_lines)
      expected = expect_output[index]
      print(FAIL + ": Expected output: " + expected + " - got nothing")
      fail = 1

    if len(expect_output) < len(stdout_lines):
      index = len(expect_output)
      actual = stdout_lines[index]
      print(FAIL + ": Unexpected output: " + actual)
      fail = 1


exit(fail)
