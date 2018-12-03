cmake_minimum_required(VERSION 2.8)

project(TradeBook)
set(SOURCES
    main.cpp
    book.cpp
    tests.cpp)
set(HEADERS
    io.h
    lib.h
    book.h)
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
