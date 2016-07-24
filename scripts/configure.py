#!/bin/python3

import csv
import signal
import sys
import readline

class SimpleCompleter(object):
    def __init__(self, options):
        self.options = sorted(options)
        return

    def complete(self, text, state):
        response = None
        if state == 0:
            # This is the first time for this text, so build a match list.
            if text:
                self.matches = [s
                                for s in self.options
                                if s and s.startswith(text)]
            else:
                self.matches = self.options[:]

        # Return the state'th item from the match list,
        # if we have that many.
        try:
            response = self.matches[state]
        except IndexError:
            response = None
        return response


class ReadBool:
    @staticmethod
    def read_input(query, *args):
        readline.set_completer(SimpleCompleter(['y', 'n']).complete)
        default = "n" if not args else ''.join(args[0]).strip('[]')
        choice = "Y/n" if default == "y" else "y/N"
        read_input = str(input("{} [{}] ".format(query, choice))).strip("\r\n")
        return default if not read_input else read_input[0]


class ReadOption:
    @staticmethod
    def read_input(query, args):
        readline.set_completer(SimpleCompleter(args[0:len(args) - 1]).complete)
        read_input = str(input("{} [{}] ".format(query, '/'.join(args[0:len(args) - 1])))).strip("\r\n")
        return read_input


class CMakeGenerator:
    @staticmethod
    def generate_define(name, value):
        return "set({} {})".format(name, value)


class CHeaderGenerator:
    @staticmethod
    def generate_define(name, value):
        return "#define {} {}".format(name, 1 if value == "y" else 0)


def signal_handler(s, f):
    print("")
    sys.exit(0)


def parse_line(_line: str) -> int:
    for parsed in csv.reader([_line], delimiter=' ', quotechar='"'):
        if len(parsed) < 3:
            return 1
        type_ = parsed[0]
        if type_ == "bool":
            reader = ReadBool()
        elif type_ == "option":
            reader = ReadOption()
        else:
            print("No such command: {}".format(parsed[0]))
            return 1
        if len(parsed) > 3:
            val = reader.read_input(parsed[1], parsed[3:len(parsed)])
        else:
            val = reader.read_input(parsed[1])
        for generator in generators:
            print(generator.generate_define(parsed[2], val))
    return 0

readline.parse_and_bind('tab: complete')
generators = [CMakeGenerator(), CHeaderGenerator()]
signal.signal(signal.SIGINT, signal_handler)
inputFile = open('main_config.in')
for line in inputFile:
    parse_line(line)
