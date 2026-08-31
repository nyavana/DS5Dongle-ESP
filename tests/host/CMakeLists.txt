cmake_minimum_required(VERSION 3.16)
project(ds5_s31_host_contracts LANGUAGES CXX)

enable_testing()

add_executable(config_contract_test
    config_contract_test.cpp
    ../../components/config/config_contract.cpp
)
target_compile_features(config_contract_test PRIVATE cxx_std_17)
target_compile_options(config_contract_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(config_contract_test PRIVATE
    ../../components/config/include
    ../../components/common/include
)

add_test(NAME config_contract COMMAND config_contract_test)

add_executable(cmd_contract_test
    cmd_contract_test.cpp
    ../../components/cmd/cmd.cpp
    ../../components/cmd/cmd_contract.cpp
)
target_compile_features(cmd_contract_test PRIVATE cxx_std_17)
target_compile_options(cmd_contract_test PRIVATE -Wall -Wextra -Werror)
target_include_directories(cmd_contract_test PRIVATE
    ../../components/cmd/include
    ../../components/config/include
    ../../components/bthost/include
    ../../components/battery_led/include
    ../../components/usb/include
)

add_test(NAME cmd_contract COMMAND cmd_contract_test)
