# ============================================================================ #
# Copyright (c) 2022 - 2023 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

add_test(NAME py-cudaq-tests 
        COMMAND ${Python_EXECUTABLE} -m pytest 
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(py-cudaq-tests PROPERTIES 
                    ENVIRONMENT 
                    "PYTHONPATH=$ENV{PYTHONPATH}:${CMAKE_BINARY_DIR}/python")  
