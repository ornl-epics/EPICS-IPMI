# EPICS-IPMI
Device support module for IPMI protocol devices

## How To
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

### 7. Now 
