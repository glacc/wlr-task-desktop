# ------------------------------ CONFIGURATIONS -------------------------------

APP_NAME?=wlr-task-desktop

C_DIR=./ ./Test ./sw-render/SWRender
PKGS=wayland-client freetype2 libavformat libavcodec libavutil libswscale libunibreak

# define
CFLAGS+=-D_SWRENDER_USE_NON_RGBA_PIXFMT_

# includes
CFLAGS+=-I./sw-render/ -I./sw-render/CommonInclude

# std
CFLAGS+=-std=c23

CFLAGS+=-g


PKG_CONFIG?=pkg-config

# ----------------------- WAYLAND PROTOCOL CONFIGURATION ----------------------

WL_PROTOCOL_SCAN_OUTPUT_DIR=protocol

WL_PROTOCOL_DIR!=$(PKG_CONFIG) --variable=pkgdatadir wayland-protocols
WL_PROTOCOL_XDG_SHELL=$(WL_PROTOCOL_DIR)/stable/xdg-shell/xdg-shell.xml
WL_PROTOCOL_VIEWPORTER=$(WL_PROTOCOL_DIR)/stable/viewporter/viewporter.xml

WLR_PROTOCOL_DIR!=$(PKG_CONFIG) --variable=pkgdatadir wlr-protocols
WLR_PROTOCOL_FOREIGN_TOPLEVEL_MANAGEMENT=$(WLR_PROTOCOL_DIR)/unstable/wlr-foreign-toplevel-management-unstable-v1.xml
WLR_PROTOCOL_LAYER_SHELL=$(WLR_PROTOCOL_DIR)/unstable/wlr-layer-shell-unstable-v1.xml

WL_PROTOCOLS=\
	$(WL_PROTOCOL_XDG_SHELL)\
	$(WL_PROTOCOL_VIEWPORTER)\
	$(WLR_PROTOCOL_LAYER_SHELL)\
	$(WLR_PROTOCOL_FOREIGN_TOPLEVEL_MANAGEMENT)

# -----------------------------------------------------------------------------

default: all bear

CFLAGS_PKG_CONFIG!=$(PKG_CONFIG) --cflags $(PKGS)
CFLAGS+=$(CFLAGS_PKG_CONFIG) -MMD
LIBS!=$(PKG_CONFIG) --libs $(PKGS)

ALL_OBJS=

# ------------------------- WAYLAND PROTOCOL SCANNING -------------------------

CFLAGS+=-I./$(WL_PROTOCOL_SCAN_OUTPUT_DIR)

# sacnning

WL_SCANNER=wayland-scanner

WL_PROTOCOL_PRIVATE_CODE_OBJS=

WL_PROTOCOL_CLIENT_HEADER=$(addprefix $(WL_PROTOCOL_SCAN_OUTPUT_DIR)/,$(notdir $(WL_PROTOCOLS:.xml=-client-protocol.h)))
WL_PROTOCOL_PRIVATE_CODE=$(addprefix $(WL_PROTOCOL_SCAN_OUTPUT_DIR)/,$(notdir $(WL_PROTOCOLS:.xml=-protocol.c)))

WL_NUM_PROTOCOLS_SEQ!=seq $(words $(WL_PROTOCOLS))

define GEN_RECIPE_WL_PROTOCOL_CLIENT_HEADER
$(1): $(2)
	@mkdir -p $$(dir $$@)
	$(WL_SCANNER) client-header < $$< > $$@
endef

define GEN_RECIPE_WL_PROTOCOL_PRIVATE_CODE
$(1): $(2)
	@mkdir -p $$(dir $$@)
	$(WL_SCANNER) private-code < $$< > $$@
WL_PROTOCOL_PRIVATE_CODE_OBJS+=$(1:.c=.o)
endef

ALL_OBJS+=$(WL_PROTOCOL_PRIVATE_CODE_OBJS)

$(foreach N,$(WL_NUM_PROTOCOLS_SEQ),\
	$(eval $(call GEN_RECIPE_WL_PROTOCOL_CLIENT_HEADER,$(word $(N),$(WL_PROTOCOL_CLIENT_HEADER)),$(word $(N),$(WL_PROTOCOLS)))))
$(foreach N,$(WL_NUM_PROTOCOLS_SEQ),\
	$(eval $(call GEN_RECIPE_WL_PROTOCOL_PRIVATE_CODE,$(word $(N),$(WL_PROTOCOL_PRIVATE_CODE)),$(word $(N),$(WL_PROTOCOLS)))))

scan-protocols: $(WL_PROTOCOL_CLIENT_HEADER) $(WL_PROTOCOL_PRIVATE_CODE)

clean-protocols:
	rm -f $(WL_PROTOCOL_SCAN_OUTPUT_DIR)/*.c
	rm -f $(WL_PROTOCOL_SCAN_OUTPUT_DIR)/*.h
	rm -f $(WL_PROTOCOL_SCAN_OUTPUT_DIR)/*.o
	rm -f $(WL_PROTOCOL_SCAN_OUTPUT_DIR)/*.d
	rmdir --ignore-fail-on-non-empty $(WL_PROTOCOL_SCAN_OUTPUT_DIR)

# ---------------------------------- COMPILE ----------------------------------

CORE_OBJS=
CORE_DEPS=

C_FILES=

NUM_C_DIR_SEQ!=seq $(words $(C_DIR))

define ADD_C_FILE
C_FILES+=$(1)
endef

$(foreach N,$(NUM_C_DIR_SEQ),\
	$(eval $(call ADD_C_FILE,$(shell find $(word $(N),$(C_DIR)) -maxdepth 1 -name "*.c"))))

NUM_C_FILES_SEQ!=seq $(words $(C_FILES))

define GEN_RECIPE_CORE_CODE
-include $(1:.c=.d)
$(1:.c=.o): $(1)
	$(CC) -c $$< -g -Werror $(CFLAGS) -I. -o $$@
CORE_OBJS+=$(1:.c=.o)
CORE_DEPS+=$(1:.c=.d)
endef

$(foreach N,$(NUM_C_FILES_SEQ),\
	$(eval $(call GEN_RECIPE_CORE_CODE,$(word $(N),$(C_FILES)))))

ALL_OBJS+=$(CORE_OBJS)

$(APP_NAME): $(ALL_OBJS)
	$(CC) $^ -g -Werror $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@

clean-code:
	rm -f $(CORE_OBJS)
	rm -f $(CORE_DEPS)

# ---------------------------------- ACTIONS ----------------------------------

all: scan-protocols $(APP_NAME)

bear:
	$(MAKE) -Bn all | bear parse-sh

clean: clean-protocols clean-code

.PHONY:\
	default\
	all\
	scan-protocols\
	clean\
	clean-code\
	clean-protocols\
	bear\
