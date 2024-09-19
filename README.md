# EPICS-IPMI
Device support module for IPMI protocol devices

## How To
This EPICS-IPMI module requires GNU's FreeIPMI library to work.
### 1. Download GNU FreeIPMI
* Link: https://www.gnu.org/software/freeipmi/index.html
* FYI: EPICS-IPMI was tested with FreeIPMI version 1.6.14
### 2. Build/Install GNU FreeIPMI
* Follow the [README.build](https://www.gnu.org/software/freeipmi/README.build) from FreeIPMI's Documentation tab
* Basic steps: `./configure, make, make install`
```
Example configure flags:
./configure --prefix=/ics/lib/freeipmi_1.6.14/ --exec-prefix=/ics/lib/freeipmi_1.6.14/ --sysconfdir=/ics/etc/freeipmi/ --localstatedir=/ics/var/freeipmi/ --with-systemdsystemunitdir=/ics/lib/freeipmi_1.6.14/services
```
### 3. Download EPICS-IPMI
### 4. Add FreeIPMI to your EPICS-IPMI/configure/RELEASE file
* `FREE_IPMI = /ics/lib/freeipmi-1.6.14`

### 5. Build your EPICS-IPMI module
* `make`
### 6. Test reading the SDR of a device
* The report-sdr tool is located in the bin directory of the module
* `cd bin/linux-x86_64/`
* Run the --help command to see the usage and example usage
* `./report-sdr --help`
* Make sure you know the user-name, password, auth-type, and privilege level before trying to connect to a device
* `./report-sdr -H 192.168.201.205 -u "ADMIN" -p "Password0" --auth-type md5 --privilege-level admin`
* Create an EPICS database from the SDR by passing the --create-db-file flag
```
To create an EPICS database from the SDR:
./report-sdr -H 192.168.201.205 -u "ADMIN" -p "Password0" --auth-type md5 --privilege-level admin --create-db-file /tmp/server.db
```

### 7. Now create an IOC to integrate IPMI devices
* Add EPICS-IPMI to your IOC's configure/RELEASE file: `EPICSIPMI=/ics/epics/7.0.4.1/epics-ipmi`
* Add EPICS-IPMI to your IOC's myIoc/src/Makefile
```
# myIOC/src/Makefile
...
myIoc_DBD += epicsipmi.dbd
...
myIoc_LIBS += epicsipmi
```
* Compile your IOC: `make`
* Create a connection string in your IOC's st.cmd file
```
# myIoc/iocBoot/st.cmd
ipmiConnect ipmidev1 192.168.201.205 "user-name" "password" "admin" "lan"
```
* Create an EPICS ai record by referencing the sensor's entity-id:entity-instance 'sensor-name' in the record's INP field
* **Note**: The EPICS-IPMI module can create epics databases from SDRs automatically. See section **6. Test reading the SDR of a device**
```
record(ai, "FE_MPS:FN0:CU1_TEMP1") {
 field(DTYP, "ipmi")
 field(INP, "@ipmidev1 sensor 30:97 'CU TEMP1'")
 field(SCAN, "Event")
 field(EVNT, "1")
 field(PHAS, "0")
 field(EGU, "C")
 field(PREC, "1")
}
```







