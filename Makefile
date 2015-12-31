# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# GNU Makefile based on shared rules provided by the Native Client SDK.
# See README.Makefiles for more details.

#pepper_44/toolchain/mac_x86_newlib/x86_64-nacl/usr/lib

VALID_TOOLCHAINS := pnacl newlib glibc clang-newlib mac 

NACL_SDK_ROOT ?= $(abspath $(CURDIR))

TARGET = iguana

EXTRA= -D__PNACL

include $(NACL_SDK_ROOT)/tools/common.mk

CHROME_ARGS += --allow-nacl-socket-api=127.0.0.1

DEPS = nacl_io
LIBS = curl  ssl crypto z glibc-compat nacl_spawn ppapi nacl_io ppapi_simple  #  cli_main ppapi_cpp ppapi_simple

CFLAGS = -Wall -D__PNACL -fno-strict-aliasing $(EXTRA)
LFLAGS = libs

SOURCES = main.c iguana777.c iguana_init.c iguana_json.c iguana_recv.c iguana_chains.c iguana_ramchain.c iguana_rpc.c iguana_bundles.c iguana_pubkeys.c iguana_msg.c iguana_kv.c iguana_blocks.c iguana_utils.c iguana_peers.c curve25519.c curve25519-donna.c inet.c cJSON.c ramcoder.c  InstantDEX/InstantDEX.c #pangea/cards777.c pangea/pangea777.c pangea/pangeafunds.c pangea/poker.c pangea/tourney777.c peggy/peggy777.c peggy/peggytx.c  peggy/txidind777.c peggy/opreturn777.c quotes777.c

# Build rules generated by macros from common.mk:

$(foreach dep,$(DEPS),$(eval $(call DEPEND_RULE,$(dep))))
$(foreach src,$(SOURCES),$(eval $(call COMPILE_RULE,$(src),$(CFLAGS))))

# The PNaCl workflow uses both an unstripped and finalized/stripped binary.
# On NaCl, only produce a stripped binary for Release configs (not Debug).
ifneq (,$(or $(findstring pnacl,$(TOOLCHAIN)),$(findstring Release,$(CONFIG))))

$(eval $(call LINK_RULE,$(TARGET)_unstripped,$(SOURCES) $(LOCALLIBS),$(LIBS),$(DEPS)));
$(eval $(call STRIP_RULE,$(TARGET),$(TARGET)_unstripped))
else
$(eval $(call LINK_RULE,$(TARGET),$(SOURCES),$(LIBS),$(DEPS)))
endif

$(eval $(call NMF_RULE,$(TARGET),))
