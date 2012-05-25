# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := iondtn

MY_ICI		:= ../../../ici

MY_ICISOURCES := \
	$(MY_ICI)/library/platform.c    \
	$(MY_ICI)/library/platform_sm.c \
	$(MY_ICI)/library/memmgr.c      \
	$(MY_ICI)/library/llcv.c        \
	$(MY_ICI)/library/lyst.c        \
	$(MY_ICI)/library/psm.c         \
	$(MY_ICI)/library/smlist.c      \
	$(MY_ICI)/library/smrbt.c       \
	$(MY_ICI)/library/ion.c         \
	$(MY_ICI)/library/rfx.c         \
	$(MY_ICI)/library/zco.c         \
	$(MY_ICI)/sdr/sdrtable.c        \
	$(MY_ICI)/sdr/sdrhash.c         \
	$(MY_ICI)/sdr/sdrxn.c           \
	$(MY_ICI)/sdr/sdrmgt.c          \
	$(MY_ICI)/sdr/sdrstring.c       \
	$(MY_ICI)/sdr/sdrlist.c         \
	$(MY_ICI)/sdr/sdrcatlg.c        \
	$(MY_ICI)/daemon/rfxclock.c     \
	$(MY_ICI)/utils/ionadmin.c      \
	$(MY_ICI)/utils/sdrmend.c       \
	$(MY_ICI)/library/ionsec.c      \
	$(MY_ICI)/utils/ionsecadmin.c 

#	$(MY_ICI)/utils/ionexit.c      \

MY_DGR		:= ../../../dgr

MY_DGRSOURCES :=     \
	$(MY_DGR)/library/libdgr.c    \

#	MY_LTP		:= ../../../ltp

#	MY_LTPSOURCES :=     \
#		$(MY_LTP)/library/libltp.c    \
#		$(MY_LTP)/library/libltpP.c   \
#		$(MY_LTP)/daemon/ltpclock.c   \
#		$(MY_LTP)/daemon/ltpmeter.c   \
#		$(MY_LTP)/udp/udplsi.c        \
#		$(MY_LTP)/udp/udplso.c        \
#		$(MY_LTP)/utils/ltpadmin.c

MY_BP		:= ../../../bp

MY_BPSOURCES :=      \
	$(MY_BP)/library/libbp.c      \
	$(MY_BP)/library/libbpP.c     \
	$(MY_BP)/daemon/bpclock.c     \
	$(MY_BP)/utils/bpadmin.c      \
	$(MY_BP)/utils/bpstats.c      \
	$(MY_BP)/utils/bptrace.c      \
	$(MY_BP)/utils/bplist.c       \
	$(MY_BP)/utils/lgagent.c      \
	$(MY_BP)/cgr/libcgr.c         \
	$(MY_BP)/ipn/ipnadmin.c       \
	$(MY_BP)/ipn/ipnfw.c          \
	$(MY_BP)/ipn/ipnadminep.c     \
	$(MY_BP)/ipn/libipnfw.c       \
	$(MY_BP)/udp/udpcli.c         \
	$(MY_BP)/udp/udpclo.c         \
	$(MY_BP)/udp/libudpcla.c      \
	$(MY_BP)/tcp/tcpcli.c         \
	$(MY_BP)/tcp/tcpclo.c         \
	$(MY_BP)/tcp/libtcpcla.c      \
	$(MY_BP)/dgr/dgrcla.c         \
	$(MY_BP)/library/bei.c        \
	$(MY_BP)/library/ext/ecos/ecos.c

#		$(MY_BP)/ltp/ltpcli.c         \
#		$(MY_BP)/ltp/ltpclo.c         \

MY_BSP		:= $(MY_BP)/library/ext/bsp

MY_BSPSOURCES :=                      \
	$(MY_BSP)/extbsputil.c        \
	$(MY_BSP)/extbspbab.c         \
	$(MY_BSP)/extbsppib.c         \
	$(MY_BSP)/extbsppcb.c         \
	$(MY_BP)/library/crypto/NULL_SUITES/crypto.c          

MY_DTN2		:= $(MY_BP)/dtn2

MY_DTN2SOURCES :=    \
	$(MY_DTN2)/dtn2admin.c       \
	$(MY_DTN2)/dtn2fw.c          \
	$(MY_DTN2)/dtn2adminep.c     \
	$(MY_DTN2)/libdtn2fw.c

#	MY_TEST		:= $(MY_BP)/test

#	MY_TESTSOURCES =     \
#		$(MY_TEST)/bpsource.c        \
#		$(MY_TEST)/bpsink.c

#	MY_CFDP		:= ../../../cfdp

#	MY_CFDPSOURCES :=    \
#		$(MY_CFDP)/library/libcfdp.c    \
#		$(MY_CFDP)/library/libcfdpP.c   \
#		$(MY_CFDP)/library/libcfdpops.c \
#		$(MY_CFDP)/bp/bputa.c           \
#		$(MY_CFDP)/daemon/cfdpclock.c   \
#		$(MY_CFDP)/utils/cfdpadmin.c    \

LOCAL_C_INCLUDES := $(MY_ICI)/include $(MY_ICI)/library $(MY_DGR)/include $(MY_BP)/include $(MY_BP)/library $(MY_BP)/ipn $(MY_BP)/dtn2 $(MY_BP)/library/crypto $(MY_BP)/library/ext/bsp $(MY_BP)/library/ext/ecos

#	$(MY_LTP)/include $(MY_LTP)/library $(MY_LTP)/udp 
#	$(MY_CFDP)/include $(MY_CFDP)/library

LOCAL_CFLAGS = -g -Wall -Werror -Dbionic -DBP_EXTENDED -DGDSSYMTAB -DGDSLOGGER -DUSING_SDR_POINTERS -DNO_SDR_TRACE -DNO_PSM_TRACE
#	-DNO_PROXY -DNO_DIRLIST

LOCAL_SRC_FILES := iondtn.c $(MY_ICISOURCES) $(MY_DGRSOURCES) $(MY_BPSOURCES) $(MY_BSPSOURCES) $(MY_DTN2SOURCES)

#	$(MY_LTPSOURCES) $(MY_TESTSOURCES) $(MY_CFDPSOURCES)

include $(BUILD_SHARED_LIBRARY)
