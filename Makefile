# Root Makefile for sub-project dispatch.
#
# Add a new project by appending one line to PROJECT_SPECS:
#   target_name:path/to/project
#
# Example:
#   ac791n_wifi_story_machine:apps/wifi_story_machine/board/wl82

PROJECT_SPECS := \
	ac791n_wifi_story_machine:apps/wifi_story_machine/board/wl82

TARGETS := $(foreach spec,$(PROJECT_SPECS),$(strip $(word 1,$(subst :, ,$(spec)))))
CLEAN_TARGETS := $(addprefix clean_,$(TARGETS))

DEFAULT_TARGETS := $(TARGETS)
DEFAULT_CLEAN_TARGETS := $(addprefix clean_,$(DEFAULT_TARGETS))

.PHONY: all clean $(TARGETS) $(CLEAN_TARGETS)

all: $(DEFAULT_TARGETS)
	@echo +ALL DONE

clean: $(DEFAULT_CLEAN_TARGETS)
	@echo +CLEAN DONE

define GEN_PROJECT_RULES
$(1):
	$$(MAKE) -C $(2) -f Makefile

clean_$(1):
	$$(MAKE) -C $(2) -f Makefile clean
endef

$(foreach spec,$(PROJECT_SPECS),$(eval $(call GEN_PROJECT_RULES,$(strip $(word 1,$(subst :, ,$(spec)))),$(strip $(word 2,$(subst :, ,$(spec)))))))
