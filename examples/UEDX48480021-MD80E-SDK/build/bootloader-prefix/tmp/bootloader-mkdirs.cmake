# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Espressif/Espressif/frameworks/esp-idf-v5.1.1/components/bootloader/subproject"
  "D:/Espressif/Espressif/frameworks/esp-idf-v5.1.1/examples/get-started/YYSJ_S3_knob-W_Final_version/build/bootloader"
  "D:/Espressif/Espressif/frameworks/esp-idf-v5.1.1/examples/get-started/YYSJ_S3_knob-W_Final_version/build/bootloader-prefix"
  "D:/Espressif/Espressif/frameworks/esp-idf-v5.1.1/examples/get-started/YYSJ_S3_knob-W_Final_version/build/bootloader-prefix/tmp"
  "D:/Espressif/Espressif/frameworks/esp-idf-v5.1.1/examples/get-started/YYSJ_S3_knob-W_Final_version/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Espressif/Espressif/frameworks/esp-idf-v5.1.1/examples/get-started/YYSJ_S3_knob-W_Final_version/build/bootloader-prefix/src"
  "D:/Espressif/Espressif/frameworks/esp-idf-v5.1.1/examples/get-started/YYSJ_S3_knob-W_Final_version/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Espressif/Espressif/frameworks/esp-idf-v5.1.1/examples/get-started/YYSJ_S3_knob-W_Final_version/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Espressif/Espressif/frameworks/esp-idf-v5.1.1/examples/get-started/YYSJ_S3_knob-W_Final_version/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
