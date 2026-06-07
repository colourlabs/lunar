cmake = import('cmake')
curl_opt = cmake.subproject_options()
curl_opt.add_cmake_defines({'BUILD_TESTING': 'OFF', 'BUILD_CURL_EXE': 'OFF'}) 

curl_subproject = cmake.subproject('curl', options: curl_opt)
libcurl_dep = curl_subproject.dependency('libcurl')