# Makefile fragment for this subdirectory

SRCDIR := testprograms/Malloc

PSELIBS :=
LIBS := 

PROGRAM := $(SRCDIR)/test1
SRCS := $(SRCDIR)/test1.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test2
SRCS := $(SRCDIR)/test2.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test3
SRCS := $(SRCDIR)/test3.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test4
SRCS := $(SRCDIR)/test4.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test5
SRCS := $(SRCDIR)/test5.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test6
SRCS := $(SRCDIR)/test6.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test7
SRCS := $(SRCDIR)/test7.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test8
SRCS := $(SRCDIR)/test8.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test9
SRCS := $(SRCDIR)/test9.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test10
SRCS := $(SRCDIR)/test10.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test11
SRCS := $(SRCDIR)/test11.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test12
SRCS := $(SRCDIR)/test12.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test13
SRCS := $(SRCDIR)/test13.cc
include $(SRCTOP)/scripts/program.mk

PROGRAM := $(SRCDIR)/test14
SRCS := $(SRCDIR)/test14.cc
include $(SRCTOP)/scripts/program.mk

