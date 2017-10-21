PROJECT_NAME ?= wakatiwai
OUT_FILE_NAME ?= $(PROJECT_NAME)
CONFIG ?= Release
CCENV =
GYPTARGET = ninja

compile = \
	($(CCENV) gyp $(1) --depth=. -f $(GYPTARGET) \
		--no-duplicate-basename-check \
		-D project_name=$(PROJECT_NAME) \
		-D out_file_name=$(OUT_FILE_NAME) && \
	ninja -C out/$(CONFIG))

.PHONY: all

all: executable

clean:
	ninja -v -C out/Debug -t clean
	ninja -v -C out/Release -t clean

nuke:
	rm -rf out build

executable:
	$(call compile, $(PROJECT_NAME).gyp)
