TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

CXXFLAGS += -g -ggdb -O0 -std=c++17 -fpermissive

# stdc++fs needs to be linked when using the std::filesytem
# library and a gcc version less than 9.
LDLIBS = -lstdc++fs

#=============================
# Build the IOC application

#PROD_IOC = ipmi
# ipmi.dbd will be created and installed
#DBD += ipmi.dbd
DBD += epicsipmi.dbd

# ipmi.dbd will be made up from these files:
#ipmi_DBD += base.dbd

# Include dbd files from all support applications:
#ipmi_DBD += xxx.dbd

# Add all the support libraries needed by this IOC
#ipmi_LIBS += xxx
#ipmi_LIBS += freeipmi
LIB_LIBS += freeipmi
freeipmi_DIR = $(FREE_IPMI)/lib/

SRC_DIRS += $(FREE_IPMI)/include

LIBRARY += epicsipmi
epicsipmi_LIBS += freeipmi
epicsipmi_SRCS += IpmiSdrRec.cpp
epicsipmi_SRCS += IpmiSensorRecComp.cpp
epicsipmi_SRCS += IpmiSensorRecFull.cpp
epicsipmi_SRCS += IpmiFruDevLocRec.cpp
epicsipmi_SRCS += PicmgLed.cpp

epicsipmi_SRCS += epicsshell.cpp
epicsipmi_SRCS += epicsdevice.cpp
epicsipmi_SRCS += common.cpp
epicsipmi_SRCS += print.cpp
epicsipmi_SRCS += dispatcher.cpp
epicsipmi_SRCS += provider.cpp
epicsipmi_SRCS += freeipmiprovider.cpp
epicsipmi_SRCS += ipmisensor.cpp
epicsipmi_SRCS += EntityAddrType.cpp
epicsipmi_SRCS += IpmiException.cpp
epicsipmi_SRCS += IpmiSdrManager.cpp
epicsipmi_SRCS += IpmiConnectionManager.cpp
epicsipmi_SRCS += IpmiSdrInfo.cpp

# ipmi_registerRecordDeviceDriver.cpp derives from ipmi.dbd
##ipmi_SRCS += ipmi_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
##ipmi_SRCS_DEFAULT += ipmiMain.cpp
##ipmi_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#ipmi_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
##ipmi_LIBS += $(EPICS_BASE_IOC_LIBS)

# Read/Report SDR Repository from device
PROD_HOST += report-sdr
report-sdr_SRCS += report-sdr.cpp
report-sdr_SRCS += IpmiSdrRec.cpp
report-sdr_SRCS += IpmiSensorRecComp.cpp
report-sdr_SRCS += IpmiSensorRecFull.cpp
report-sdr_SRCS += IpmiFruDevLocRec.cpp
report-sdr_SRCS += EpRecord.cpp
report-sdr_SRCS += EpAiRecord.cpp
report-sdr_SRCS += EpBiRecord.cpp
report-sdr_SRCS += PicmgLed.cpp
report-sdr_LIBS += freeipmi

#===========================


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

