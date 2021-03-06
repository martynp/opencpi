#
#  This file is part of OpenCPI (www.opencpi.org).
#     ____                   __________   ____
#    / __ \____  ___  ____  / ____/ __ \ /  _/ ____  _________ _
#   / / / / __ \/ _ \/ __ \/ /   / /_/ / / /  / __ \/ ___/ __ `/
#  / /_/ / /_/ /  __/ / / / /___/ ____/_/ / _/ /_/ / /  / /_/ /
#  \____/ .___/\___/_/ /_/\____/_/    /___/(_)____/_/   \__, /
#      /_/                                             /____/
#
#  OpenCPI is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as published
#  by the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  OpenCPI is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with OpenCPI.  If not, see <http://www.gnu.org/licenses/>.
#
########################################################################### #
# This makefile is for building assemblies and bitstreams.
# The assemblies get built according to the standard build targets
HdlMode:=assembly
include $(OCPI_CDK_DIR)/include/util.mk
$(OcpiIncludeProject)
include $(OCPI_CDK_DIR)/include/hdl/hdl-make.mk

# Theses next lines are similar to what worker.mk does
ifneq ($(MAKECMDGOALS),clean)
$(if $(wildcard $(CwdName).xml),,\
  $(error The XML for the assembly, $(CwdName).xml, is missing))
endif
override Workers:=$(CwdName)
override Worker:=$(Workers)
Worker_$(Worker)_xml:=$(Worker).xml
Worker_xml:=$(Worker).xml
OcpiLanguage:=verilog
override LibDir=lib/hdl
override HdlLibraries+=platform
#$(info -1.Only:$(OnlyPlatforms),All:$(HdlAllPlatforms),Ex:$(ExcludePlatforms),HP:$(HdlPlatforms))
include $(OCPI_CDK_DIR)/include/hdl/hdl-pre.mk
ifndef HdlSkip
# Add to the component libraries specified in the assembly Makefile,
# and also include those passed down from the "assemblies" level
# THIS LIST IS FOR THE APPLICATION ASSEMBLY NOT FOR THE CONTAINER
override ComponentLibraries+= $(ComponentLibrariesInternal) components adapters
# Override since they may be passed in from assemblies level
override XmlIncludeDirsInternal:=$(XmlIncludeDirs) $(XmlIncludeDirsInternal)
ifdef Container
  ifndef Containers
    Containers:=$(Container)
  endif
endif
HdlContName=$(Worker)_$1_$2
HdlContOutDir=$(OutDir)container-$1
HdlContDir=$(call HdlContOutDir,$(call HdlContName,$1,$2))
HdlStripXml=$(if $(filter .xml,$(suffix $1)),$(basename $1),$1)
ifneq ($(MAKECMDGOALS),clean)
#  $(info 0.Only:$(OnlyPlatforms),All:$(HdlAllPlatforms),Ex:$(ExcludePlatforms),HP:$(HdlPlatforms))
  $(eval $(HdlPrepareAssembly))
  # default the container names
  $(foreach c,$(Containers),$(eval HdlContXml_$c:=$c))
  ## Extract the platforms and targets from the containers that have their own xml
  ## Then we can filter the platforms and targets based on that
  ifdef Containers
    # This procedure is (simply) to extract platform, target, and configuration info from the
    # xml for each explicit container. This allows us to build the list of targets necessary for all the
    # platforms mentioned by all the containers.  This list then is the default targets when
    # targets are not specified.
    define doGetPlatform
      $$(and $$(call DoShell,$(OcpiGen) -X $1,HdlContPfConfig),\
          $$(error Processing container XML $1: $$(HdlContPfConfig)))
      HdlContPlatform:=$$(word 1,$$(HdlContPfConfig))
      HdlContConfig:=$$(word 2,$$(HdlContPfConfig))
      $$(if $$(HdlContPlatform),,$$(error Could not get platform attribute for container $1))
      $$(if $$(HdlContConfig),,$$(error Could not get config attribute for container $1))
      $$(if $$(HdlPart_$$(HdlContPlatform)),,\
        $$(error Platform for container $1, $$(HdlContPlatform), is not defined))
      HdlMyTargets+=$$(call OcpiDbg,HdlPart_$$(HdlContPlatform) is $$(HdlPart_$$(HdlContPlatform)))$$(call HdlGetFamily,$$(HdlPart_$$(HdlContPlatform)))
      ContName:=$(Worker)_$$(HdlContPlatform)_$$(HdlContConfig)_$1
      HdlPlatform_$$(ContName):=$$(HdlContPlatform)
      HdlTarget_$$(ContName):=$$(call HdlGetFamily,$$(HdlPart_$$(HdlContPlatform)))
      HdlConfig_$$(ContName):=$$(HdlContConfig)
      HdlContXml_$$(ContName):=$$(call HdlContOutDir,$$(ContName))/gen/$$(ContName).xml
      $$(shell mkdir -p $$(call HdlContOutDir,$$(ContName))/gen; \
               if ! test -e  $$(HdlContXml_$$(ContName)); then \
                 ln -s ../../$1.xml $$(HdlContXml_$$(ContName)); \
               fi)
      HdlContainers:=$$(HdlContainers) $$(ContName)
    endef
    $(foreach c,$(Containers),$(eval $(call doGetPlatform,$(call HdlStripXml,$c))))
  endif
  # Create the default container directories and files
  # $(call doDefaultContainer,<platform>,<config>)
  HdlDefContXml=<HdlContainer platform='$1/$2' default='true'/>
#  $(info 1.Only:$(OnlyPlatforms),All:$(HdlAllPlatforms),Ex:$(ExcludePlatforms),HP:$(HdlPlatforms))
  define doDefaultContainer
    $(call OcpiDbg,In doDefaultContainer for $1/$2 and HdlPlatforms: $(HdlPlatforms) [filtered=$(filter $1,$(HdlPlatforms))])
    ifneq (,$(if $(HdlPlatforms),$(filter $1,$(HdlPlatforms)),yes))
      # Create this default container's directory and xml file
      ContName:=$(call HdlContName,$1,$2)
      $(foreach d,$(call HdlContDir,$1,$2)/gen,\
        $$(foreach x,$d/$$(ContName).xml,\
          $$(shell \
               mkdir -p $d; \
               if test ! -f $$x || test "`cat $$x`" != "$(call HdlDefContXml,$1,$2)"; then \
                 echo "$(call HdlDefContXml,$1,$2)"> $$x; \
               fi)))
      HdlPlatform_$$(ContName):=$1
      HdlTarget_$$(ContName):=$(call HdlGetFamily,$(HdlPart_$1))
      HdlConfig_$$(ContName):=$2
      HdlContXml_$$(ContName):=$$(call HdlContOutDir,$$(ContName))/gen/$$(ContName).xml
      HdlContainers:=$$(HdlContainers) $$(ContName)
    endif
  endef
  ifeq ($(origin DefaultContainers),undefined)
    $(call OcpiDbg,No Default Containers: HdlPlatforms: $(HdlPlatforms))
    # If undefined, we determine the default containers based on HdlPlatforms
    $(foreach p,$(filter $(or $(OnlyPlatforms),$(HdlAllPlatforms)),\
                 $(filter-out $(ExcludePlatforms),$(HdlPlatforms))),\
                $(eval $(call doDefaultContainer,$p,base)))
    $(call OcpiDbg,No Default Containers: Containers: $(Containers) (Container=$(Container)))
  else
    $(foreach d,$(DefaultContainers),\
       $(if $(findstring /,$d),\
         $(eval \
           $(call doDefaultContainer $(word 1,$(subst /, ,$d)),$(word 2,$(subst /, ,$d)))),\
         $(if $(filter $d,$(filter $(or $(OnlyPlatforms),$(HdlAllPlatforms)),$(filter-out $(ExcludePlatforms),$(HdlAllPlatforms)))),\
              $(eval $(call doDefaultContainer,$d,base)),\
              $(error In DefaultContainers, $d is not a defined HDL platform.))))
  endif
  HdlContXml=$(or $($1_xml),$(if $(filter .xml,$(suffix $1)),$1,$1.xml))
  ifdef HdlContainers
    HdlMyPlatforms:=$(foreach c,$(HdlContainers),$(HdlPlatform_$c))
    ifdef HdlPlatforms
      override HdlPlatforms:=$(call Unique,$(filter $(HdlMyPlatforms),$(HdlPlatforms)))
    else
      override HdlPlatforms:=$(call Unique,$(HdlMyPlatforms))
    endif
    override HdlPlatforms:=$(filter $(or $(OnlyPlatforms),$(HdlAllPlatforms)),$(filter-out $(ExcludePlatforms),$(HdlPlatforms)))
    ifdef HdlTargets
      HdlTargets:=$(call Unique,$(filter $(HdlMyTargets),$(HdlTargets)))
    else
      HdlTargets:=$(call Unique,$(foreach p,$(HdlPlatforms),$(call HdlGetFamily,$(HdlPart_$p))))
    endif
  endif # for ifdef Containers
else # for "clean" goal
  HdlTargets:=all
endif
# Due to our filtering, we might have no targets to build
ifeq ($(filter $(or $(OnlyPlatforms),$(HdlAllPlatforms)),$(filter-out $(ExcludePlatforms),$(HdlPlatforms))),)
#  $(info 2.Only:$(OnlyPlatforms),All:$(HdlAllPlatforms),Ex:$(ExcludePlatforms),HP:$(HdlPlatforms))
  $(info Not building assembly $(Worker) for platform(s): $(HdlPlatforms) in "$(shell pwd)")
#  $(info No targets or platforms to build for this "$(Worker)" assembly in "$(shell pwd)")
else
  include $(OCPI_CDK_DIR)/include/hdl/hdl-worker.mk
  ifndef HdlSkip
    ifndef HdlContainers
      ifneq ($(MAKECMDGOALS),clean)
        $(info No containers will be built since none match the specified platforms.)
      endif
    else
      # We have containers to build locally.
      # We already build the directories for default containers, and we already
      # did a first pass parse of the container XML for these containers
      HdlContResult=$(call HdlContOutDir,$1)/target-$(HdlTarget_$1)/$1.bitz
      define doContainer
         .PHONY: $(call HdlContOutDir,$1)
         all: $(call HdlContResult,$1)$(infox DEPEND:$(call HdlContResult,$1))
        $(call HdlContResult,$1): links
	  $(AT)mkdir -p $(call HdlContOutDir,$1)
	  $(AT)$(MAKE) -C $(call HdlContOutDir,$1) -f $(OCPI_CDK_DIR)/include/hdl/hdl-container.mk \
               HdlAssembly=../../$(CwdName) \
	       ComponentLibrariesInternal="$(call OcpiAdjustLibraries,$(ComponentLibraries))" \
	       HdlLibrariesInternal="$(call OcpiAdjustLibraries,$(HdlMyLibraries))" \
               XmlIncludeDirsInternal="$(call AdjustRelative,$(XmlIncludeDirsInternal))"
      endef
      $(foreach c,$(HdlContainers),$(and $(filter $(HdlPlatform_$c),$(HdlPlatforms)),$(eval $(call doContainer,$c))))
    endif # containers
  endif # of skip
endif # check for no targets
endif
clean::
	$(AT) rm -r -f container-* lib
