#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

set(rabbitmq_sources
	./src/rabbitmq.c
)

set(rabbitmq_headers
	./inc/rabbitmq.h
)

set(rabbitmq_static_sources
	${rabbitmq_sources}
)

set(rabbitmq_static_headers
	${rabbitmq_headers}
)

set(rabbitmq_hl_sources
	./src/rabbitmq_hl.c
)

set(rabbitmq_hl_headers
	./inc/rabbitmq_hl.h
)

set(rabbitmq_hl_static_sources
	${rabbitmq_hl_sources}
)

set(rabbitmq_hl_static_headers
	${rabbitmq_hl_headers}
)


include_directories(./inc)
include_directories(${GW_INC})

#this builds the rabbitmq dynamic library
add_library(rabbitmq MODULE ${rabbitmq_sources}  ${rabbitmq_headers})
target_link_libraries(rabbitmq gateway)

#this builds the rabbitmq static library
add_library(rabbitmq_static STATIC ${rabbitmq_static_sources} ${rabbitmq_static_headers})
target_compile_definitions(rabbitmq_static PRIVATE BUILD_MODULE_TYPE_STATIC)
target_link_libraries(rabbitmq_static gateway)

#this builds the rabbitmq dynamic library (by default it uses rabbitmq module linked statically)
add_library(rabbitmq_hl MODULE ${rabbitmq_hl_sources} ${rabbitmq_hl_headers})
target_link_libraries(rabbitmq_hl rabbitmq_static gateway)

#this builds the rabbitmq static library (by default it uses rabbitmq module linked statically)
add_library(rabbitmq_hl_static STATIC ${rabbitmq_hl_static_sources} ${rabbitmq_hl_static_headers})
target_compile_definitions(rabbitmq_hl_static PRIVATE BUILD_MODULE_TYPE_STATIC)
target_link_libraries(rabbitmq_hl_static rabbitmq_static gateway)

linkSharedUtil(rabbitmq)
linkSharedUtil(rabbitmq_static)
linkSharedUtil(rabbitmq_hl)
linkSharedUtil(rabbitmq_hl_static)

add_module_to_solution(rabbitmq)

if(install_executables)
	install(TARGETS rabbitmq LIBRARY DESTINATION lib) 
    install(TARGETS rabbitmq_hl LIBRARY DESTINATION lib) 
endif()


