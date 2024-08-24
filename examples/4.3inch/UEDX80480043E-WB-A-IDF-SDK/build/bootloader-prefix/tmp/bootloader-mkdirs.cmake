# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/ESP/Espressif/frameworks/esp-idf-v5.2.2/components/bootloader/subproject"
  "D:/ESP/Espressif/frameworks/esp-idf-v5.2.2/examples/get-started/UEDX80480043E-WB-A-IDF-SDK/build/bootloader"
  "D:/ESP/Espressif/frameworks/esp-idf-v5.2.2/examples/get-started/UEDX80480043E-WB-A-IDF-SDK/build/bootloader-prefix"
  "D:/ESP/Espressif/frameworks/esp-idf-v5.2.2/examples/get-started/UEDX80480043E-WB-A-IDF-SDK/build/bootloader-prefix/tmp"
  "D:/ESP/Espressif/frameworks/esp-idf-v5.2.2/examples/get-started/UEDX80480043E-WB-A-IDF-SDK/build/bootloader-prefix/src/bootloader-stamp"
  "D:/ESP/Espressif/frameworks/esp-idf-v5.2.2/examples/get-started/UEDX80480043E-WB-A-IDF-SDK/build/bootloader-prefix/src"
  "D:/ESP/Espressif/frameworks/esp-idf-v5.2.2/examples/get-started/UEDX80480043E-WB-A-IDF-SDK/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/ESP/Espressif/frameworks/esp-idf-v5.2.2/examples/get-started/UEDX80480043E-WB-A-IDF-SDK/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/ESP/Espressif/frameworks/esp-idf-v5.2.2/examples/get-started/UEDX80480043E-WB-A-IDF-SDK/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
