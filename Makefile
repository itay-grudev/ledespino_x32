#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
PROJECT_NAME := ledespino_x32

CPPFLAGS += -DESP32

include $(IDF_PATH)/make/project.mk
