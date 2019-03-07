#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
# COMPONENT_EXTRA_CLEAN := html/index.html
# COMPONENT_EMBED_FILES := html/index.html
# COMPONENT_EMBED_TXTFILES := html/index.html

CFLAGS += -Wall
CPPFLAGS += -Wall

COMPONENT_SRCDIRS := . hsluv/src
