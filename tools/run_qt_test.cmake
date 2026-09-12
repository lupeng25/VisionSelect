# QtTest 在部分 Windows 无交互环境中无法输出到标准输出。
# 保留文件日志，并由 CMake 转发给 CTest，保证失败时能看到具体断言。
if(NOT DEFINED TEST_PROGRAM OR NOT DEFINED TEST_LOG)
    message(FATAL_ERROR "测试程序和日志路径必须明确指定。")
endif()
file(REMOVE "${TEST_LOG}")
execute_process(COMMAND "${TEST_PROGRAM}" -o "${TEST_LOG},txt"
    RESULT_VARIABLE test_result)
if(EXISTS "${TEST_LOG}")
    file(READ "${TEST_LOG}" test_output)
    message("${test_output}")
endif()
if(NOT test_result STREQUAL "0")
    message(FATAL_ERROR "QtTest 失败，退出结果：${test_result}")
endif()
