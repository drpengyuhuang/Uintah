#
#  The MIT License
#
#  Copyright (c) 1997-2017 The University of Utah
# 
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to
#  deal in the Software without restriction, including without limitation the
#  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
#  sell copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
# 
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
#  IN THE SOFTWARE.
# 
# 
# 
# 
# 
# Makefile fragment for this subdirectory 


# include $(SCIRUN_SCRIPTS)/smallso_prologue.mk

SRCDIR  := CCA/Components

# The following variables are used by the Fake* scripts... please
# do not modify...
#

# MPM
ifeq ($(BUILD_MPM),yes)
  MPM := $(SRCDIR)/MPM
endif

# ICE
ifeq ($(BUILD_ICE),yes)
  ICE := $(SRCDIR)/ICE
endif

# MPM-ICE
ifeq ($(BUILD_MPM)$(BUILD_ICE),yesyes)
  MPMICE := $(SRCDIR)/MPMICE
endif

# Arches
ifeq ($(BUILD_ARCHES),yes)
  ARCHES   := $(SRCDIR)/Arches
endif

# MPM-Arches
ifeq ($(BUILD_MPM)$(BUILD_ARCHES),yesyes)
  MPMARCHES := $(SRCDIR)/MPMArches
endif

# FVM
ifeq ($(BUILD_FVM),yes)
  FVM :=$(SRCDIR)/FVM
endif

# MPM-FVM
ifeq ($(BUILD_MPM)$(BUILD_FVM),yesyes)
  MPMFVM := $(SRCDIR)/MPMFVM
endif

# Wasatch
ifeq ($(BUILD_WASATCH),yes)
  WASATCH := $(SRCDIR)/Wasatch
endif

# Heat
ifeq ($(BUILD_HEAT),yes)
  HEAT := $(SRCDIR)/Heat
endif

# PhaseField
ifeq ($(BUILD_PHASEFIELD),yes)
  PHASEFIELD := $(SRCDIR)/PhaseField
endif

SUBDIRS := \
        $(MPM)                         \
        $(ICE)                         \
        $(MPMICE)                      \
        $(ARCHES)                      \
        $(MPMARCHES)                   \
        $(FVM)                         \
        $(MPMFVM)                      \
        $(WASATCH)                     \
        $(HEAT)                        \
        $(PHASEFIELD)                  \
        $(SRCDIR)/Application          \
        $(SRCDIR)/DataArchiver         \
        $(SRCDIR)/Examples             \
        $(SRCDIR)/LoadBalancers        \
        $(SRCDIR)/Models               \
        $(SRCDIR)/OnTheFlyAnalysis     \
        $(SRCDIR)/Parent               \
        $(SRCDIR)/ProblemSpecification \
        $(SRCDIR)/PostProcessUda       \
        $(SRCDIR)/Regridder            \
        $(SRCDIR)/Schedulers           \
        $(SRCDIR)/SimulationController \
        $(SRCDIR)/Solvers              \
        $(SRCDIR)/SwitchingCriteria    

include $(SCIRUN_SCRIPTS)/recurse.mk
