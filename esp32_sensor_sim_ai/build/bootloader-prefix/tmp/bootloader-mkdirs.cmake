# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/esp/v6.0.2/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "C:/esp/v6.0.2/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/esp32_sensor_sim_ai/build/bootloader"
  "C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/esp32_sensor_sim_ai/build/bootloader-prefix"
  "C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/esp32_sensor_sim_ai/build/bootloader-prefix/tmp"
  "C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/esp32_sensor_sim_ai/build/bootloader-prefix/src/bootloader-stamp"
  "C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/esp32_sensor_sim_ai/build/bootloader-prefix/src"
  "C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/esp32_sensor_sim_ai/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/esp32_sensor_sim_ai/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/esp32_sensor_sim_ai/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
