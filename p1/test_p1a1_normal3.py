#!/usr/bin/env python3

import os, sys
from subprocess import Popen, PIPE, CalledProcessError
import signal
from TestFramework import *

# test parameters
inputfile = 'inputfile'
outputfile = 'outputfile'


class TestCompile(Test):
    name = "compilation test"
    description = "compiles source code"
    cmd = 'gcc -Wall -Werror -o reverse reverse.c'.split()

    @classmethod
    def test(cls):
        cls.popen_as_child(cls.cmd)


def reverse(lines):
    """python implementation of the reverse program"""
    output = []
    for line in lines:
        assert line[-1] == '\n', 'Every line must be newline-terminated'
        output.append(line[:-1][::-1] + '\n')
    output.reverse()
    return output


class ReverseTest(Test):
    """abstract class for all test that runs the reverse program to extend"""
    cmd = './reverse -i %s -o %s' % (inputfile, outputfile)
    stdout = ''
    stderr = ''

    @staticmethod
    def generate_lines():
        """generate inputfile lines for testing"""
        return []

    @classmethod
    def test(cls):
        if os.path.isfile(outputfile):
            os.remove(outputfile)

        input_lines = cls.generate_lines()
        with open(inputfile, 'w') as f:
            f.write(''.join(input_lines))

        cmd = (cls.cmd).split()
        cls.stdout, cls.stderr, rc = cls.popen_as_child(cmd, check=False)
        cls.stdout = cls.stdout.decode("ascii")
        cls.stderr = cls.stderr.decode("ascii")
        assert rc == 0, 'Non-zero exit status %d' % rc

        with open(outputfile, 'r') as f:
            test_output_lines = f.read()
        output_lines = ''.join(reverse(input_lines))

        assert output_lines == test_output_lines, 'Output does not match'


class ExpectErrorTest(ReverseTest):
    """abstract class for all reverse tests that expect an error"""
    returncode = 1
    err_msg = ''

    @classmethod
    def test(cls):
        try:
            super(ExpectErrorTest, cls).test()
        except AssertionError as e:
            assert e.args[0] == 'Non-zero exit status %d' % cls.returncode, 'Expect non-zero exit status'
            assert cls.stdout == '', 'Expect nothing to be printed in stdout'
            assert cls.stderr == cls.err_msg, 'Unexpected error message'
            return cls.point_value
        assert False, 'Expect an error from the program'


class TestNormal3(ReverseTest):
    name = "normal test 3"
    description = "tests on 5000 varying length lines"
    point_value = 5

    @staticmethod
    def generate_lines():
        return [('%d\n' % i).rjust(i % 511, '*') for i in range(5000)]


def run_tests(tests, short_circuit=True, cleanup=False):
    points = 0
    tot_points = sum(test.point_value for test in tests)
    succeeded = failed = 0
    for test in tests:
        point = test.run()
        if point == 0:
            failed += 1
            if short_circuit:
                break
        else:
            succeeded += 1
        points += point
    if cleanup:
        print('-'*21 + 'cleaning up'.center(30) + '-'*21)
        for f in ['inputfile', 'outputfile', 'reverse', 'reverse.o']:
            if os.path.isfile(f):
                print('removing %s' % f)
                # os.remove(f)
        print('-'*21 + 'cleanup done'.center(30) + '-'*21)
        print()

    notrun = len(tests)-succeeded-failed
    print('succeeded: %d' % succeeded)
    print('failed: %d' % failed)
    print('not-run: %d\n' % notrun)
    print('score: %d (out of %d)' % (points, tot_points))


def main():
    testall = True
    cleanup = True
    for arg in sys.argv[1:]:
        if arg.startswith('-a'):
            testall = True
        elif arg.startswith('-c'):
            cleanup = True

    run_tests([TestCompile,
        TestNormal3], short_circuit=not testall, cleanup=cleanup)

if __name__ == '__main__':
    main()
