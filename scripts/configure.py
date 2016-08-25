#!/usr/bin/python3

import csv
import signal
import readline
import sys
import getopt
import os


class Completer(object):
    def __init__(self, options):
        self.options = sorted(options)
        self.matches = None
        return

    def complete(self, text, state):
        if state == 0:
            if text:
                self.matches = \
                    [s for s in self.options if s and s.startswith(text)]
            else:
                self.matches = self.options[:]

        try:
            response = self.matches[state]
        except IndexError:
            response = None
        return response


class Asker:
    @staticmethod
    def ask(query, values, default):
        readline.set_completer(Completer(values).complete)
        choice = "/".join(values)
        while True:
            read_input = input(
                "{} [{}/def:{}] ".format(query, choice, default)
            ).strip("\r\n")
            if not read_input and default:
                return default
            elif read_input in values:
                return read_input
        return ""


class FileWriter:

    name = ""

    def __init__(self, file):
        self.output_file = file

    def write_variable(self, name, value):
        return ""


class CMakeWriter(FileWriter):

    name = "cmake"

    def __init__(self, file):
        super().__init__(file)
        self.output_file = file

    def write_variable(self, name, value):
        self.output_file.write("set({} {})\r\n".format(name, value))


class CHeaderWriter(FileWriter):

    name = "c"

    def __init__(self, file):
        super().__init__(file)
        self.output_file = file

    def write_variable(self, name, value):
        if value == 'y':
            self.output_file.write("#define {} 1\r\n".format(name))
        elif value == 'n':
            self.output_file.write("//#define {} 0\r\n".format(name))
        else:
            self.output_file.write("#define {} {}\r\n".format(name, value))


def error_handler(error_string, filename, line, line_number):
    print("Error in {} at line {}: {}".format(
        filename, line_number, error_string))
    print("\t-> {}".format(line.strip("\r\n")))
    sys.exit(1)


class VariableList:

    def __init__(self):
        self.variables = {}

    def get_variable(self, name):
        if name in self.variables:
            return self.variables[name]
        return os.environ.get(name)

    def set_variable(self, name, value):
        self.variables[name] = value

    def unset_variable(self, name):
        pass

    def get(self):
        values = []
        names = []
        for key, value in self.variables.items():
            names.append(key)
            values.append(value)
        return names, values


class Environment:

    asker = None
    file_readers = None
    file_writers = None
    variables = None
    condition_stack = None

    def __init__(
        self, asker, variables, condition_stack, file_readers, file_writers
    ):
        self.asker = asker
        self.variables = variables
        self.condition_stack = condition_stack
        self.file_readers - file_readers
        self.file_writers = file_writers
        pass


class InterpreterError(Exception):
    def __init__(self, message, errors):
        super(InterpreterError, self).__init__(message)


class Interpreter:

    condition_instructions = ["endif", "else"]

    def __init__(self, reader):
        self.reader = reader
        self.variable_list = VariableList()
        self.condition_stack = []
        self.file_writers = []
        self.file_readers = []
        self.modules = []
        pass

    def builtin_bool(self, arguments):
        query = arguments[0]
        variable_name = arguments[1]
        default = arguments[2].strip('[]') if len(arguments) > 2 else 'n'
        self.variable_list.set_variable(
            variable_name, Asker.ask(
                query, ['y', 'n'], default))

    def builtin_option(self, arguments):
        query = arguments[0]
        variable_name = arguments[1]
        default_value = arguments[2].strip('[]') if len(arguments) > 1 else ''
        self.variable_list.set_variable(
            variable_name, Asker.ask(
                query, arguments[2:len(arguments) - 1], default_value))

    def builtin_comment(self, arguments):
        print(' '.join(arguments))

    def builtin_if(self, arguments):
        left = self.variable_list.get_variable(arguments[0])
        condition = arguments[1]
        if not left:
            left = arguments[0]
        right = self.variable_list.get_variable(arguments[2])
        if not right:
            right = arguments[2]
        if condition == "==":
            if left == right:
                self.condition_stack.append(True)
            else:
                self.condition_stack.append(False)

    def builtin_else(self, arguments):
        self.condition_stack[-1] = not self.condition_stack[-1]

    def builtin_endif(self, arguments):
        if not len(self.condition_stack):
            raise IndexError
        self.condition_stack.pop()

    def builtin_set(self, arguments):
        self.variable_list.set_variable(arguments[0], arguments[1])

    def builtin_die(self, arguments):
        print("Error: {}".format(" ".join(arguments)))
        sys.exit(1)

    def builtin_include(self, arguments):
        filename = arguments[0]
        self.file_readers.append(self.reader(filename))

    def builtin_output(self, arguments):
        writer = eval(arguments[1] + "Writer")
        self.file_writers.append(writer(open(arguments[0], "w")))

    def builtin_import(self, arguments):
        self.modules.append(__import__(arguments[0]))

    def get_function(self, name):
        if hasattr(Interpreter, "builtin_" + name):
            function = eval("self.builtin_" + name)
            return function
        elif len(self.modules):
            if hasattr(self.modules[0], "extern_" + name):
                return getattr(self.modules[0], "extern_" + name).execute
        else:
            raise NotImplementedError

    def interpret_line(self, parsed):
        self.function_name = parsed[0]
        arguments = parsed[1:len(parsed)]
        if len(self.condition_stack):
            if self.condition_stack[-1] is False:
                if self.function_name not in self.condition_instructions:
                    return
        function = self.get_function(self.function_name)
        function(arguments)

    def generate_output(self):
        print("\nConfiguration:")
        names, values = self.variable_list.get()
        for name, value in zip(names, values):
            print("{} = {}".format(name, value))
            for writer in self.file_writers:
                writer.write_variable(name, value)

    def run(self, filename):
        self.file_readers.append(self.reader(filename))
        parser = Parser
        while True:
            try:
                line = self.file_readers[-1].readline()
                if line:
                    if self.interpret_line(parser.parse_line(line)):
                        return
                else:
                    self.file_readers.pop()
                    if not len(self.file_readers):
                        return
            except IndexError:
                error_handler(
                    "Not enough arguments for: \'{}\'".format(
                        self.function_name),
                    self.file_readers[-1].filename,
                    self.file_readers[-1].line,
                    self.file_readers[-1].line_number)
            except NotImplementedError:
                error_handler("No such keyword: \'{}\'".format(
                    self.function_name),
                    self.file_readers[-1].filename,
                    self.file_readers[-1].line,
                    self.file_readers[-1].line_number)


class FileReader:

    def __init__(self, filename):
        self.filename = filename
        self.file = open(filename)
        self.line = ""
        self.line_number = 0

    def __del__(self):
        self.file.close()

    def readline(self):
        self.line = ""
        self.line = self.file.readline()
        self.line_number += 1
        if not self.line:
            return None
        if self.line == "\n" or self.line == '\r\n':
            return self.readline()
        return self.line


class ConsoleReader:

    def __init__(self, filename):
        self.filename = "stdin"
        self.line = ""
        self.line_number = 0

    def readline(self):
        read_input = ""
        while read_input == "":
            read_input = input("> ")
        return read_input.strip("\r\n")


class Parser:

    @staticmethod
    def parse_line(line):
        if line == "":
            return None
        for parsed in csv.reader([line], delimiter=' ', quotechar='"'):
            while not parsed[0]:
                parsed = parsed[1:len(parsed)]
            return parsed


def signal_handler(s, f):
    print("")
    sys.exit(1)


def main():
    signal.signal(signal.SIGINT, signal_handler)
    readline.parse_and_bind('tab: complete')

    try:
        opts, args = getopt.getopt(sys.argv[1:], "hi:", ["ifile="])
    except getopt.GetoptError:
        print('configure.py -i <inputfile>')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print('configure.py -i <inputfile>')
            sys.exit(0)
        elif opt in ("-i", "--ifile"):
            input_filename = arg
        else:
            print("Not recognized option: {}".format(arg))
            sys.exit(1)

    interpreter = Interpreter(FileReader)
    interpreter.run(input_filename)
    interpreter.generate_output()


if __name__ == '__main__':
    main()
