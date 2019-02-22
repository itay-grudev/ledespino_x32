index.html:
	echo "<h1>Hello World</h1>" > $(COMPONENT_BUILD_DIR)/$@

COMPONENT_EMBED_SPECIAL_FILES ?= index.html
COMPONENT_EMBED_SPECIAL_TXTFILES ?=

COMPONENT_EMBED_OBJS ?= $(addsuffix .bin.o,$(notdir $(COMPONENT_EMBED_FILES))) $(addsuffix .txt.o,$(notdir $(COMPONENT_EMBED_TXTFILES)))  $(addsuffix .bin.o,$(notdir $(COMPONENT_EMBED_SPECIAL_FILES))) $(addsuffix .txt.o,$(notdir $(COMPONENT_EMBED_SPECIAL_TXTFILES)))

OBJCOPY_EMBED_ARGS := --input-target binary --output-target elf32-xtensa-le --binary-architecture xtensa --rename-section .data=.rodata.embedded

# Generate pattern for embedding text or binary files into the app
# $(1) is name of file (as relative path inside component)
# $(2) is txt or bin depending on file contents
#
# txt files are null-terminated before being embedded (otherwise
# identical behaviour.)
#
define GenerateEmbedSpecialTarget

# copy the input file into the build dir (using a subdirectory
# in case the file already exists elsewhere in the build dir)
embed_bin/$$(notdir $(1)): $(call resolvepath,$(1),$(COMPONENT_PATH)) | embed_bin
	cp $$< $$@

embed_txt/$$(notdir $(1)): $(call resolvepath,$(1),$(COMPONENT_PATH)) | embed_txt
	cp $$< $$@
	printf '\0' >> $$@  # null-terminate text files

# messing about with the embed_X subdirectory then using 'cd' for objcopy is because the
# full path passed to OBJCOPY makes it into the name of the symbols in the .o file
$$(notdir $(1)).$(2).o: $$(notdir $(1)) embed_$(2)/$$(notdir $(1))
	$(summary) EMBED $$(patsubst $$(PWD)/%,%,$$(CURDIR))/$$@
	cd embed_$(2); $(OBJCOPY) $(OBJCOPY_EMBED_ARGS) $$(notdir $$<) ../$$@

CLEAN_FILES += embed_$(2)/$$(notdir $(1))
endef

$(foreach binfile,$(COMPONENT_EMBED_SPECIAL_FILES), $(eval $(call GenerateEmbedSpecialTarget,$(binfile),bin)))
$(foreach txtfile,$(COMPONENT_EMBED_SPECIAL_TXTFILES), $(eval $(call GenerateEmbedSpecialTarget,$(txtfile),txt)))
