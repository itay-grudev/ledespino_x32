#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
PROJECT_NAME := ledespino_x32

include $(IDF_PATH)/make/project.mk

all: protobuf

##
# Protocol Buffers
#
PYTHON_OUT_DIR = backend_stub/protobuf
JAVASCRIPT_OUT_DIR = backend_stub/protobuf
JAVASCRIPT_OUT_DIR = backend_stub/protobuf

PROTOC = protoc
PROTOBUF_SRCDIR = protobuf
PROTOBUF_SRCEXTS = .proto
PROTOBUF_SOURCES = $(foreach f, $(wildcard $(addprefix $(PROTOBUF_SRCDIR)/*,$(PROTOBUF_SRCEXTS))), $(notdir $(f)))

protobuf: protobuf-python protobuf-javascript protobuf-c
protobuf-python: $(PYTHON_OUT_DIR)
	mkdir -p $(PYTHON_OUT_DIR)
	$(PROTOC) --python_out=$(PYTHON_OUT_DIR) --proto_path $(PROTOBUF_SRCDIR) $(PROTOBUF_SOURCES)

protobuf-javascript: $(PYTHON_OUT_DIR)
	mkdir -p $(PYTHON_OUT_DIR)
	$(PROTOC) --js_out=$(PYTHON_OUT_DIR) --proto_path $(PROTOBUF_SRCDIR) $(PROTOBUF_SOURCES)

protobuf-c:
