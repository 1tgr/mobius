# $Id: system.pro,v 1.7 2002/08/21 12:05:54 pavlovskii Exp $

key KernelDebug
    # Serial port to use for the kernel debugger
    #Port=0x3f8
    Speed=115200
    Port=0

    # Set to true to sync with gdb on startup and attempt to connect on 
    #	each exception.
    # Set to false to restrict kernel debugging to outputting of kernel console
    #	text on the serial port.
    #SyncGdb=true
    SyncGdb=false
end

key Devices
    # This is a list of device=driver pairs which get loaded at startup.
    0=device,tty,tty0
    1=device,tty,tty1
    2=device,tty,tty2
    3=device,tty,tty3
    4=device,tty,tty4
    5=device,tty,tty5
    6=device,tty,tty6
    7=device,keyboard,keyboard
    8=device,fdc,fdc0
    9=device,ata,ata0
    10=device,ata,ata1
    11=mount,fat,/hd,classes/volume0
    11=mount,ext2,/,classes/volume1
    12=mount,devfs,/System/Devices,
    13=mount,ramfs,/System/Boot,
    14=mount,ext2,/mnb,classes/volume2
    15=device,pci,pci
#    14=mount,fat,/Linux/mnt,classes/volume0
#    13=mount,ext2,/,classes/volume1
#    13=device,ne2000,ne2000
    16=device,ps2mouse,ps2mouse
    17=device,sermouse,sermouse
end

key ISA
    #
    # Each key here has the form:
    #	PNPxxxx
    # where PNP stands for the 3-letter vendor ID, and stands for the device ID
    # Under each key can be several values:
    #	Description=Textual description of device
    #	Driver=Driver to use for such devices
    #	Device=Special name to assign to devices found
    #

    key NBL5016
	Driver=modem
	#Device=modem
    end
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
	Driver=isapnp
    end

    key Vendor8086Device7111Subsystem00000000
	Description=Intel 82371AB/EB/MB PIIX4 EIDE Controller
    end

    key Vendor8086Device7112Subsystem00000000
	Description=Intel 82371AB/EB/MB PIIX4 USB Controller
	Driver=usb
    end

    key Vendor8086Device7113Subsystem00000000
	Description=Intel 82371AB/EB/MB PIIX4 Power Management Controller
    end

    key Vendor5333Device8811Subsystem00000000
	Description=S3 86C732 Trio32, 86C764 Trio64, 86C765 Trio64V+ Rev 01
	Driver=video
    end

    key Vendor10ECDevice8139Subsystem10EC8139
        Description=RT8139 (A/B/C/8130) Fast Ethernet Adapter
        Driver=rtl8139
    end
end

key Network
    key ethernet0
	Bindings=ip,arp
	IpAddress=192.168.0.200
    end
end

# Program to use as the OS shell
#Shell=/System/Boot/desktop.exe
Shell=/Mobius/shell.exe
LibrarySearchPath=/System/Boot,/Mobius,.
