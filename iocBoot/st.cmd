#!../bin/linux-x86_64/ipmi

#- You may have to change ipmi to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/ipmi.dbd"
ipmi_registerRecordDeviceDriver pdbbase

## Load record instances
#dbLoadRecords("db/xxx.db","user=8w4")

cd "${TOP}/iocBoot"
iocInit

## Start any sequence programs
#seq sncxxx,"user=8w4"
