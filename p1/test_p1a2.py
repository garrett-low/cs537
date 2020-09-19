#!/usr/bin/env python3

import os, sys
from subprocess import Popen, PIPE, CalledProcessError
import signal
from TestFramework import *

# test parameters
outputfile = 'test.tar'
cleanup_list = []


class TestCompile(Test):
    name = "compilation test"
    description = "compiles source code"
    cmd = 'gcc -Wall -Werror -o wis-tar wis-tar.c'.split()

    @classmethod
    def test(cls):
        cls.popen_as_child(cls.cmd)


def wistar(filenames):
    output = []
    for file in filenames:
        with open(file, 'rb') as f:
            output.append(bytes(file, "ascii").ljust(256, b'\0'))
            output.append(os.stat(file).st_size.to_bytes(8, sys.byteorder))
            output.append(f.read())
            output.append(bytes(8))
    return b''.join(output)


class WisTarTest(Test):
    cmd = './wis-tar %s' % outputfile
    stdout = b''
    stderr = b'' # binary
    cmd_no_extend = False

    @staticmethod
    def generate_files():
        """generate input files for testing"""
        # return a list of tuples (filename, content)
        return []

    @classmethod
    def test(cls):
        if os.path.isfile(outputfile):
            os.remove(outputfile)

        files = cls.generate_files()
        filenames = []
        for filename, content in files:
            filenames.append(filename)
            with open(filename, 'wb') as f:
                f.write(content)
        global cleanup_list
        cleanup_list = filenames # regist a list to cleanup

        output_tar = wistar(filenames)

        if (cls.cmd_no_extend):
            cmd = cls.cmd.split()
        else:
            cmd = cls.cmd.split() + filenames
        cls.stdout, cls.stderr, rc = cls.popen_as_child(cmd, check=False)
        assert rc == 0, 'Non-zero exit status %d' % rc

        with open(outputfile, 'rb') as f:
            test_output_tar = f.read()
        cleanup_list.append(outputfile)
        
        assert output_tar == test_output_tar, 'Output does not match'


class ExpectErrorTest(WisTarTest):
    """abstract class for all reverse tests that expect an error"""
    returncode = 1
    err_msg = ''

    @classmethod
    def test(cls):
        try:
            super(ExpectErrorTest, cls).test()
        except AssertionError as e:
            assert e.args[0] == 'Non-zero exit status %d' % cls.returncode, 'Expect non-zero exit status'
            assert cls.stdout == b'', 'Expect nothing to be printed in stdout'
            assert cls.stderr.decode("ascii") == cls.err_msg, 'Unexpected error message'
            return cls.point_value
        assert False, 'Expect an error from the program'


class TestInvalidFile(ExpectErrorTest):
    name = "invalid file test"
    description = "tests on a non-existing file"

    inputfile_not_exist  = 'youshouldnothavethisfileinyourdirectory'
    cmd_no_extend = True
    cmd = './wis-tar %s %s' % (outputfile, inputfile_not_exist)
    err_msg = 'wis-tar: Cannot open file: %s\n' % inputfile_not_exist


class TestInvalidArg(ExpectErrorTest):
    name = "invalid argument test"
    description = "tests on too few of arguments"
    point_value = 5

    cmd = './wis-tar %s' % outputfile
    cmd_no_extend = True
    returncode = 1
    err_msg = "Usage: wis-tar ARCHIVE [FILE ...]\n"


class TestExample(WisTarTest):
    name = "example test"
    description = "comes from the project specification"

    @staticmethod
    def generate_files():
        return [("a.txt", bytes("CS 537, Fall 2020\n", "ascii")),
                ("b.txt", bytes("Operating Systems\n", "ascii"))]


class TestArchiveExist(WisTarTest):
    name = "pre-exist output test"
    description = "if archive already exists, overwrite it without reporting an error"
    point_value = 5

    @staticmethod
    def generate_files():
        with open(outputfile, "wb") as f:
            f.write(b"somegarbagecontent")
        return [("a.txt", bytes("CS 537, Fall 2020\n", "ascii")),
                ("b.txt", bytes("Operating Systems\n", "ascii"))]


class TestSingleInput(WisTarTest):
    name = "single input test"
    description = "test on only one input"
    point_value = 5

    @staticmethod
    def generate_files():
        return [("a.txt", bytes("CS 537, Fall 2020\n", "ascii"))]


class TestEmptyFile(WisTarTest):
    name = "empty file test"
    description = "tests on empty files"

    @staticmethod
    def generate_files():
        return [("a.txt", bytes()),
                ("b.txt", bytes())]


class TestNormal1(WisTarTest):
    name = "normal test 1"
    description = "tests on 2 small files"

    @staticmethod
    def generate_files():
        return [('%d.txt' % i, bytes('some\ncontent\n%d' % (i * 537), "ascii")) for i in range(2)]


class TestNormal2(WisTarTest):
    name = "normal test 2"
    description = "tests on 11 small files"
    point_value = 5

    @staticmethod
    def generate_files():
        return [('%d.txt' % i, bytes('some\ncontent\n%d' % (i * 537), "ascii")) for i in range(11)]


class TestNormal3(WisTarTest):
    name = "normal test 3"
    description = "tests on 2 small binary files"

    @staticmethod
    def generate_files():
        return [("a.bin", (123456).to_bytes(8, sys.byteorder)),
                ("b.bin", (789012).to_bytes(8, sys.byteorder))]


class TestLongfilename(WisTarTest):
    name = "long filename test"
    description = "tests on 2 long-filename files"

    @staticmethod
    def generate_files():
        return [("a.txt".rjust(255, 'o'), bytes("CS 537, Fall 2020\n", "ascii")),
                ("b.txt".rjust(255, 'x'), bytes("Operating Systems\n", "ascii"))]


class TestStress1(WisTarTest):
    name = "stress test 1"
    description = "tests on 2 large files (~50MB)"
    point_value = 5
    timeout = 30

    @staticmethod
    def generate_files():
        return [("a.large", bytes(''.join(
                    [('%d' % i).rjust(512, '*') for i in range(100000)]
                ), "ascii")),
                ("b.large", bytes(''.join(
                    [('%d' % i).ljust(512, 'x') for i in range(100000)]
                ), "ascii"))]


class TestStress2(WisTarTest):
    name = "stress test 2"
    description = "tests on 100 large files (~5MB)"
    point_value = 5
    timeout = 120

    @staticmethod
    def generate_files():
        return [("%d.medium" % j, bytes(''.join(
                    [('%d%d' % (i, j)).rjust(512, '*') for i in range(10000)]
                ), "ascii")) for j in range(100)]


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
            for f in cleanup_list:
                print(f)
                if os.path.isfile(f):
                    print('removing %s' % f)
                    os.remove(f)
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
        TestInvalidFile,
        TestInvalidArg,
        TestExample,
        TestArchiveExist,
        TestSingleInput,
        TestEmptyFile,
        TestNormal1,
        TestNormal2,
        TestNormal3,
        TestLongfilename,
        TestStress1,
        TestStress2], short_circuit=not testall, cleanup=cleanup)

if __name__ == '__main__':
    main()
