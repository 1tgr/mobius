# $Id: system.pro,v 1.5 2002/04/04 00:08:42 pavlovskii Exp $

key KernelDebug
    # Serial port to use for the kernel debugger
    Port=0x3f8
    #Port=0

    # Set to true to sync with gdb on startup and attempt to connect on 
    #	each exception.
    # Set to false to restrict kernel debugging to outputting of kernel console
    #	text on the serial port.
    #SyncGdb=true
    SyncGdb=false
end

key Devices
    # This is a list of device=driver pairs which get loaded at startup.
    fdc0=fdc
    keyboard=keyboard
    tty0=tty
    tty1=tty
    tty2=tty
    tty3=tty
    tty4=tty
    tty5=tty
    tty6=tty
    pci=pci
    cmos=cmos
    ps2mouse=ps2mouse
end

key PCI
    #
    # Each key here has the form:
    #	VendorXXXXDeviceYYYYSubsystemZZZZZZZZ
    # Under each key can be several values:
    #	Description=Textual description of device
    #	Driver=Driver to use for such devices
    #	Device=Special name to assign to devices found
    #

    key Vendor8086Device7190Subsystem00000000
	Description=Intel 82443BX/ZX 440BX/ZX CPU to PCI Bridge (AGP Implemented)
    end

    key Vendor8086Device7191Subsystem00000000
	Description=Intel 82443BX/ZX 440BX/ZX PCI to AGP Bridge
    end

    key Vendor8086Device7110Subsystem00000000
	Description=Intel 82371AB/EB/MB PIIX4 ISA Bridge
    end

    key Vendor8086Device7111Subsystem00000000
	Description=Intel 82371AB/EB/MB PIIX4 EIDE Controller
	Driver=ata
    end

    key Vendor8086Device7112Subsystem00000000
	Description=Intel 82371AB/EB/MB PIIX4 USB Controller
    end
    
    key Vendor8086Device7113Subsystem00000000
	Description=Intel 82371AB/EB/MB PIIX4 Power Management Controller
    end

    key Vendor5333Device8811Subsystem00000000
	Description=S3 86C732 Trio32, 86C764 Trio64, 86C765 Trio64V+ Rev 01
	Driver=video
	Device=video
    end
end

# Program to use as the OS shell
Shell=/System/Boot/desktop.exe
