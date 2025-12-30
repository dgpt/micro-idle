# CMake generated Testfile for 
# Source directory: /home/dgpt/code/games/micro-idle
# Build directory: /home/dgpt/code/games/micro-idle/build_native
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/home/dgpt/code/games/micro-idle/build_native/tests-88df431_include.cmake")
add_test(visual::headless_game_run "/home/dgpt/code/games/micro-idle/build_native/tests" "[visual]")
set_tests_properties(visual::headless_game_run PROPERTIES  LABELS "visual;e2e" TIMEOUT "20" _BACKTRACE_TRIPLES "/home/dgpt/code/games/micro-idle/CMakeLists.txt;223;add_test;/home/dgpt/code/games/micro-idle/CMakeLists.txt;0;")
subdirs("_deps/raylib-build")
subdirs("_deps/flecs-build")
subdirs("_deps/catch2-build")
subdirs("JoltBuild")
