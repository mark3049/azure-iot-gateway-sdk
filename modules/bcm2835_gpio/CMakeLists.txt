#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

set(bcm2835_gpio_sources
	./src/bcm2835_gpio.c
	./src/bcm2835_hw.c
)

set(bcm2835_gpio_headers
	./inc/bcm2835_gpio.h
	./inc/bcm2835_hw.h
)

set(bcm2835_gpio_static_sources
	${bcm2835_gpio_sources}
)

set(bcm2835_gpio_static_headers
	${bcm2835_gpio_headers}
)

set(bcm2835_gpio_hl_sources
	./src/bcm2835_gpio_hl.c
)

set(bcm2835_gpio_hl_headers
	./inc/bcm2835_gpio_hl.h
)

set(bcm2835_gpio_hl_static_sources
	${bcm2835_gpio_hl_sources}
)

set(bcm2835_gpio_hl_static_headers
	${bcm2835_gpio_hl_headers}
)


include_directories(./inc)
include_directories(${GW_INC})

#this builds the bcm2835_gpio dynamic library
add_library(bcm2835_gpio MODULE ${bcm2835_gpio_sources}  ${bcm2835_gpio_headers})
target_link_libraries(bcm2835_gpio gateway)

#this builds the bcm2835_gpio static library
add_library(bcm2835_gpio_static STATIC ${bcm2835_gpio_static_sources} ${bcm2835_gpio_static_headers})
target_compile_definitions(bcm2835_gpio_static PRIVATE BUILD_MODULE_TYPE_STATIC)
target_link_libraries(bcm2835_gpio_static gateway)

#this builds the bcm2835_gpio dynamic library (by default it uses bcm2835_gpio module linked statically)
add_library(bcm2835_gpio_hl MODULE ${bcm2835_gpio_hl_sources} ${bcm2835_gpio_hl_headers})
target_link_libraries(bcm2835_gpio_hl bcm2835_gpio_static gateway)

#this builds the bcm2835_gpio static library (by default it uses bcm2835_gpio module linked statically)
add_library(bcm2835_gpio_hl_static STATIC ${bcm2835_gpio_hl_static_sources} ${bcm2835_gpio_hl_static_headers})
target_compile_definitions(bcm2835_gpio_hl_static PRIVATE BUILD_MODULE_TYPE_STATIC)
target_link_libraries(bcm2835_gpio_hl_static bcm2835_gpio_static gateway)

linkSharedUtil(bcm2835_gpio)
linkSharedUtil(bcm2835_gpio_static)
linkSharedUtil(bcm2835_gpio_hl)
linkSharedUtil(bcm2835_gpio_hl_static)

add_module_to_solution(bcm2835_gpio)

if(install_executables)
	install(TARGETS bcm2835_gpio LIBRARY DESTINATION lib) 
    install(TARGETS bcm2835_gpio_hl LIBRARY DESTINATION lib) 
endif()


