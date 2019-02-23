COMPONENT_OWNBUILDTARGET = true
COMPONENT_OWNCLEANTARGET = true
COMPONENT_ADD_INCLUDEDIRS =
COMPONENT_ADD_LDFLAGS =
COMPONENT_LIBRARIES =

.PHONY: build
build: python

PROTOC = protoc

PYTHON_OUT_DIR = $(COMPONENT_PATH)../../../backend_stub/protobuf

SRCDIRS = $(COMPONENT_PATH)
SRCEXTS = .proto
SOURCES = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS))))
# $(error $(SOURCES))

#
# c: *.proto
# 	$(PROTOC) --c_out=../proto-c/ -I . *.proto

python: $(SOURCES) $(PYTHON_OUT_DIR)
	$(PROTOC) --python_out=$(PYTHON_OUT_DIR) --proto_path $(COMPONENT_PATH) $(SOURCES)
#
# python_proto: *.proto
# 	@protoc --python_out=../python/ -I . *.proto

$(PYTHON_OUT_DIR):
	mkdir -p $(PYTHON_OUT_DIR)
