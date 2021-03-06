# The makefile fragment for libraries, perhaps under a project, perhaps not.

include $(OCPI_CDK_DIR)/include/util.mk
# Include project settings for this library, IF it is in a project
# These settings are are available for the project, libraries in the project, and workers
$(OcpiIncludeProject)
# Include library settings for this library, which are available here and for workers
# Thus library settings can depend on project settings
$(call OcpiIncludeLibrary,.,error)

ifndef ProjectPackage
  ProjectPackage:=local
endif
ifdef Package
  ifneq ($(filter .%,$(Package)),)
    Package:=$(ProjectPackage)$(Package)
  endif
else ifeq ($(CwdName),components)
  Package:=$(ProjectPackage)
else
  Package:=$(ProjectPackage).$(CwdName)
endif
include $(OCPI_CDK_DIR)/include/lib.mk

run: $(TestImplementations)
	$(AT)set -e; for i in $(TestImplementations); do \
	  $(MAKE) --no-print-directory -C $i run ; \
	done

