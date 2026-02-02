cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "module/face" "module/uvc_ctrl")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(webcam_chan)
