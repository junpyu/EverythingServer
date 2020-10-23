set(http_SRCS
  HttpServer.cc
  HttpResponse.cc
  HttpContext.cc
  )

add_library(muduo_http ${http_SRCS})
target_link_libraries(muduo_http muduo_net)

install(TARGETS muduo_http DESTINATION lib)
set(HEADERS
  HttpContext.h
  HttpRequest.h
  HttpResponse.h
  HttpServer.h
  )
install(FILES ${HEADERS} DESTINATION include/http)

if(MUDUO_BUILD_EXAMPLES)
add_executable(httpserver_test tests/HttpServer_test.cc)
target_link_libraries(httpserver_test muduo_http)

endif()

# add_subdirectory(tests)
