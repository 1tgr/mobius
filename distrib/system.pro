# $Id: system.pro.bochs,v 1.5 2002/09/01 16:24:38 pavlovskii Exp $

key ISA
    #
    # Each key here has the form:
    #        PNPxxxx
    # where PNP stands for the 3-letter vendor ID, and stands for the device ID
    # Under each key can be several values:
    #        Description=Textual description of device
    #        Driver=Driver to use for such devices
    #        Device=Special name to assign to devices found
    #

    key NBL5016
        Driver=modem
        Device=modem
    end

    key video
        SuppressModeSwitch=no
        Description=VGA Adaptor
        0=vga.kll

        # Comment this line if you have problems with VESA
        1=vesa.kll
    end

    key keyboard
		Description=PS/2 or AT Keyboard

		# Keyboard maps available in /System/Boot:
		#  british, for Tim's keyboard
		#  us, so Americans can type \
		#  russian, to test Unicode features
		DefaultMap=/System/Boot/russian.kbd
	end

	key ps2mouse
		Description=PS/2 Mouse
	end

	key sermouse
		Description=Serial Mouse
	end

	key fdc
		Description=Floppy Disk Drive
	end

	key ata
		Description=ATA/ATAPI Drive
	end
end

key PCI
    #
    # Each key here has the form:
    #        VendorXXXXDeviceYYYYSubsystemZZZZZZZZ
    # Under each key can be several values:
    #        Description=Textual description of device
    #        Driver=Driver to use for such devices
    #        Device=Special name to assign to devices found
    #

    key Vendor8086Device7190Subsystem00000000
        Description=Intel 82443BX/ZX 440BX/ZX CPU to PCI Bridge (AGP Implemented)
    end

    # Virtual PC chipset
    key Vendor8086Device7192Subsystem00000000
        Description=Intel 82443BX/ZX 440BX/ZX CPU to PCI Bridge (AGP Not Implemented)
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
    end

    key Vendor8086Device7113Subsystem00000000
        Description=Intel 82371AB/EB/MB PIIX4 Power Management Controller
    end

    key Vendor5333Device8811Subsystem00000000
        Description=S3 86C732 Trio32, 86C764 Trio64, 86C765 Trio64V+ Rev 01
        Driver=video
        0=vga.kll
        1=s3.kll
    end

    key Vendor1274Device1371Subsystem12741371
        Description=Ensoniq ES1371, ES1373 AudioPCI
        Driver=sound
    end

    # Virtual PC network card
    key Vendor1011Device0009Subsystem0a002114
        Description=Digital DecChip 21140 Fast Ethernet Adapter
    end
end

key ATA
	key Volume
		Description=Hard Disk Volume
	end

	key AtaDrive
		Description=Hard Disk
	end

	key AtapiDrive
		Description=CD-ROM Drive
	end
end

LibrarySearchPath=/System/Boot,/Mobius,.
