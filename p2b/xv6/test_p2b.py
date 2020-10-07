from testing.runtests import main
from testing import Xv6Build, Xv6Test


class Test0(Xv6Test):
    name = "test_getpinfo"
    description = """Workload: Check getpinfo works
    Expected: pstat fields filled by getpinfo"""
    tester = name + ".c"
    timeout = 30
    point_value = 5
    make_qemu_args = "CPUS=1"


class Test1(Xv6Test):
    name = "test_ptable"
    description = """Workload: Checks getpinfo mirrors ptable
    Expected: init, sh, tester are in the ptable"""
    tester = name + ".c"
    timeout = 30
    point_value = 5
    make_qemu_args = "CPUS=1"


class Test2(Xv6Test):
    name = "test_getpri"
    description = """Workload: Checks getpri works
    Expected: tester should in priority 3"""
    tester = name + ".c"
    timeout = 30
    point_value = 10
    make_qemu_args = "CPUS=1"

class Test3(Xv6Test):
    name = "test_setpri"
    description = """Workload: Checks setpri works
    Expected: tester's child should be set to the corresponding priority level"""
    tester = name + ".c"
    timeout = 30
    point_value = 5
    make_qemu_args = "CPUS=1"

class Test4(Xv6Test):
    name = "test_setpri_samelevel"
    description = """Workload: Checks setpri works when set to the same level
    Expected: tester's child should be set to the corresponding priority level"""
    tester = name + ".c"
    timeout = 30
    point_value = 5
    make_qemu_args = "CPUS=1"

class Test5(Xv6Test):
    name = "test_invalidarg"
    description = """Workload: Checks invalid argument handling
    Expected: getpinfo/getpri/setpri return -1 when arguments are invalid"""
    tester = name + ".c"
    timeout = 30
    point_value = 10
    make_qemu_args = "CPUS=1"

class Test6(Xv6Test):
    name = "test_fullslice"
    description = """Workload: Checks whether a process is given full slices
    Expected: tester's child should always run full slices"""
    tester = name + ".c"
    timeout = 30
    point_value = 10
    make_qemu_args = "CPUS=1"

class Test7(Xv6Test):
    name = "test_qtail"
    description = """Workload: Checks whether a process's qtail match ticks
    Expected: tester's child: (qtail[3] - 1) * 8 == ticks[3]"""
    tester = name + ".c"
    timeout = 30
    point_value = 10
    make_qemu_args = "CPUS=1"

class Test8(Xv6Test):
    name = "test_starve"
    description = """Workload: Checks always the highest priority processes run
    Expected: one test's child is running on priority 3 and the other child in priority 2 should starve"""
    tester = name + ".c"
    timeout = 30
    point_value = 10
    make_qemu_args = "CPUS=1"


all_tests = [Test0, Test1, Test2, Test3, Test4, Test5, Test6, Test7, Test8]

# import toolspath
main(Xv6Build, all_tests)
