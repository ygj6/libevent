#!/usr/bin/env python3
#
# Check if find_package(Libevent COMPONENTS xxx) can get the correct library.
# Note: this script has only been tested on python3.
# Usage:
#   cd cmake-build-dir
#   cmake <options> .. && cmake --build .
#   python3 /path/to/test-export.py [static|shared]

import sys
import os
import shutil
import platform
import subprocess
import tempfile
import unittest

results = ("success", "failure")
FNULL = open(os.devnull, 'wb')
script_dir = os.path.split(os.path.realpath(sys.argv[0]))[0]
# working_dir is cmake build dir
working_dir = os.getcwd()
if len(sys.argv) > 1 and sys.argv[1] == "static":
    link_type = sys.argv[1]
else:
    link_type = "shared"

is_windows = platform.system() == "Windows"

need_exportdll = False
if link_type == "shared" and is_windows:
    need_exportdll = True

cmake_prefix_path = ""
export_dll_dir = ""


def exec_cmd(cmd, silent):
    if silent:
        p = subprocess.Popen(cmd, stdout=FNULL, stderr=FNULL, shell=True)
    else:
        p = subprocess.Popen(cmd, shell=True)
    p.communicate()
    return p.poll()


def link_and_run(link, code):
    """Check if the source code matches the library component.

    Compile source code relative to one component and link to another component.
    Then run the generated executor.

    Args:
        link: The name of component that the source code will link with.
        code: The source code related component name.

    Returns:
        Returns 0 if links and runs successfully, otherwise 1.
    """
    exec_cmd("cmake --build . --target clean", True)
    generator = ''
    if is_windows:
        generator = '-G "Visual Studio 15 2017 Win64"'
    cmd = 'cmake .. %s -DEVENT__LINK_COMPONENT=%s -DEVENT__CODE_COMPONENT=%s' % (
        generator, link, code)
    if link_type == "static":
        cmd = "".join([cmd, " -DLIBEVENT_STATIC_LINK=1"])
    r = exec_cmd(cmd, True)
    if r == 0:
        r = exec_cmd('cmake --build .', True)
        if r == 0:
            r = exec_cmd('ctest', True)
    if r != 0:
        r = 1
    return r


def config_restore():
    if os.path.isfile("tempconfig") and not os.path.isfile("LibeventConfig.cmake"):
        os.rename("tempconfig", "LibeventConfig.cmake")


def config_backup():
    if os.path.isfile("tempconfig"):
        os.remove("tempconfig")
    if os.path.isfile("LibeventConfig.cmake"):
        os.rename("LibeventConfig.cmake", "tempconfig")


# On Windows, we need to add the directory containing the dll to the
# 'PATH' environment variable so that the program can call it.
def export_dll(dir):
    if need_exportdll:
        os.environ["PATH"] += os.pathsep + dir


def unexport_dll(dir):
    if need_exportdll:
        paths = os.environ["PATH"].split(os.pathsep)
        paths = list(set(paths))
        if dir in paths:
            paths.remove(dir)
        os.environ["PATH"] = os.pathsep.join(paths)


class test_export(unittest.TestCase):
    """
    Dependency relationships between libevent libraries:
        core:        pthread(linux)
        extra:       core
        pthreads:    core
        openssl:     core,openssl
    expect  0:success 1:failure
    """
    @classmethod
    def setUpClass(cls):
        os.chdir(script_dir)
        if not os.path.isdir("build"):
            os.mkdir("build")
        os.chdir("build")
        os.environ["CMAKE_PREFIX_PATH"] = cmake_prefix_path
        export_dll(export_dll_dir)

    @classmethod
    def tearDownClass(cls):
        del os.environ["CMAKE_PREFIX_PATH"]
        unexport_dll(export_dll_dir)
        os.chdir(working_dir)
        exec_cmd("cmake --build . --target uninstall", True)

    def test_link_core_run_core(self):
        self.assertEqual(link_and_run("core", "core"), 0)

#     def test_link_extra_run_extra(self):
#         self.assertEqual(link_and_run("extra", "extra"), 0)

#     def test_link_openssl_run_openssl(self):
#         self.assertEqual(link_and_run("openssl", "openssl"), 0)

#     def test_link_all_run_all(self):
#         self.assertEqual(link_and_run("", ""), 0)

#     def test_link_extra_run_core(self):
#         self.assertEqual(link_and_run("extra", "core"), 0)

#     def test_link_openssl_run_core(self):
#         self.assertEqual(link_and_run("openssl", "core"), 0)

#     def test_link_core_run_extra(self):
#         self.assertEqual(link_and_run("core", "extra"), 1)

#     def test_link_core_run_openssl(self):
#         self.assertEqual(link_and_run("core", "openssl"), 1)

#     def test_link_extra_run_openssl(self):
#         self.assertEqual(link_and_run("extra", "openssl"), 1)

#     def test_link_openssl_run_extra(self):
#         self.assertEqual(link_and_run("openssl", "extra"), 1)

#     @unittest.skipIf(is_windows, "not supported on Windows")
#     def test_link_pthreads_run_pthreads(self):
#         self.assertEqual(link_and_run("pthreads", "pthreads"), 0)

#     @unittest.skipIf(is_windows, "not supported on Windows")
#     def test_link_pthreads_run_core(self):
#         self.assertEqual(link_and_run("pthreads", "core"), 0)

#     @unittest.skipIf(is_windows, "not supported on Windows")
#     def test_link_core_run_pthreads(self):
#         self.assertEqual(link_and_run("core", "pthreads"), 1)

#     @unittest.skipIf(is_windows, "not supported on Windows")
#     def test_link_pthreads_run_extra(self):
#         self.assertEqual(link_and_run("pthreads", "extra"), 1)

#     @unittest.skipIf(is_windows, "not supported on Windows")
#     def test_link_extra_run_pthreads(self):
#         self.assertEqual(link_and_run("extra", "pthreads"), 1)

#     @unittest.skipIf(is_windows, "not supported on Windows")
#     def test_link_pthreads_run_openssl(self):
#         self.assertEqual(link_and_run("pthreads", "openssl"), 1)

#     @unittest.skipIf(is_windows, "not supported on Windows")
#     def test_link_openssl_run_pthreads(self):
#         self.assertEqual(link_and_run("openssl", "pthreads"), 1)


shutil.rmtree(os.path.join(script_dir, "build"), ignore_errors=True)
print("[test-export] use %s library" % link_type)

if __name__ == '__main__':
    runner = unittest.TextTestRunner(verbosity=2)

    print("[test-export] test for build tree")
    config_restore()
    export_dll_dir = os.path.join(working_dir, "bin", "Debug")
    cmake_prefix_path = working_dir
    suite = unittest.TestLoader().loadTestsFromTestCase(test_export)
    runner.run(suite)

    # Install libevent libraries to system path. Remove LibeventConfig.cmake
    # from build directory to avoid confusion when using find_package().
    print("[test-export] test for install tree(in system-wide path)")
    if is_windows:
        prefix = "C:\\Program Files\\libevent"
        export_dll_dir = os.path.join(prefix, "lib")
    else:
        prefix = "/usr/local"
    exec_cmd('cmake -DCMAKE_INSTALL_PREFIX="%s" ..' % prefix, True)
    exec_cmd("cmake --build . --target install", True)
    cmake_prefix_path = os.path.join(prefix, "lib/cmake/libevent")
    config_backup()
    suite = unittest.TestLoader().loadTestsFromTestCase(test_export)
    runner.run(suite)

    # Uninstall the libraries installed in the above steps. Install the libraries
    # into a temporary directory. Same as above, remove LibeventConfig.cmake from
    # build directory to avoid confusion when using find_package().
    print("[test-export] test for install tree(in non-system-wide path)")
    tempdir = tempfile.TemporaryDirectory()
    exec_cmd('cmake -DCMAKE_INSTALL_PREFIX="%s" ..' % tempdir.name, True)
    exec_cmd("cmake --build . --target install", True)
    cmake_prefix_path = os.path.join(tempdir.name, "lib/cmake/libevent")
    export_dll_dir = os.path.join(tempdir.name, "lib")
    config_backup()
    suite = unittest.TestLoader().loadTestsFromTestCase(test_export)
    runner.run(suite)
    config_restore()
