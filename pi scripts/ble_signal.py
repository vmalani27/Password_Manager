#!/usr/bin/env python3

import dbus
import dbus.mainloop.glib
from gi.repository import GLib

class Advertisement(dbus.service.Object):
    PATH_BASE = '/org/bluez/example/advertisement'

    def __init__(self, bus, index):
        self.path = self.PATH_BASE + str(index)
        self.bus = bus
        self.ad_type = 'peripheral'
        self.service_uuids = None
        self.manufacturer_data = None
        self.solicit_uuids = None
        self.service_data = None
        self.local_name = 'PiZeroBLE'
        self.include_tx_power = False
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        properties = dict()
        properties['Type'] = self.ad_type
        properties['LocalName'] = self.local_name
        if self.service_uuids:
            properties['ServiceUUIDs'] = dbus.Array(self.service_uuids, signature='s')
        if self.solicit_uuids:
            properties['SolicitUUIDs'] = dbus.Array(self.solicit_uuids, signature='s')
        if self.manufacturer_data:
            properties['ManufacturerData'] = dbus.Dictionary(self.manufacturer_data, signature='qv')
        if self.service_data:
            properties['ServiceData'] = dbus.Dictionary(self.service_data, signature='sv')
        if self.include_tx_power:
            properties['Includes'] = dbus.Array(['tx-power'], signature='s')
        return {'org.bluez.LEAdvertisement1': properties}

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_service_uuid(self, uuid):
        if not self.service_uuids:
            self.service_uuids = []
        self.service_uuids.append(uuid)

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ss', out_signature='v')
    def Get(self, interface, prop):
        return self.get_properties()[interface][prop]

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ssv', out_signature='')
    def Set(self, interface, prop, value):
        pass

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='', out_signature='a{sv}')
    def GetAll(self, interface):
        return self.get_properties()[interface]

    @dbus.service.method('org.bluez.LEAdvertisement1', in_signature='', out_signature='')
    def Release(self):
        print('%s: Released!' % self.path)


class Application(dbus.service.Object):
    def __init__(self, bus):
        self.path = '/'
        self.bus = bus
        dbus.service.Object.__init__(self, bus, self.path)

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_advertisement(self, advertisement):
        self.advertisement = advertisement

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ss', out_signature='v')
    def Get(self, interface, prop):
        return None

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='ssv', out_signature='')
    def Set(self, interface, prop, value):
        pass

    @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='', out_signature='a{sv}')
    def GetAll(self, interface):
        return None


def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    bus = dbus.SystemBus()

    # Initialize Bluetooth Adapter
    adapter = dbus.Interface(
        bus.get_object('org.bluez', '/org/bluez/hci0'),
        'org.freedesktop.DBus.Properties'
    )
    adapter.Set('org.bluez.Adapter1', 'Powered', dbus.Boolean(1))
    print("Bluetooth Adapter Powered On")

    # Advertisement
    advertisement = Advertisement(bus, 0)
    advertisement.add_service_uuid("12345678-1234-5678-1234-56789abcdef0")

    # Register Advertisement
    obj = bus.get_object('org.bluez', '/org/bluez')
    manager = dbus.Interface(obj, 'org.bluez.LEAdvertisingManager1')
    manager.RegisterAdvertisement(advertisement.get_path(), {})
    print("Advertisement Registered")

    # Run Main Loop
    loop = GLib.MainLoop()
    try:
        loop.run()
    except KeyboardInterrupt:
        print("Exiting...")
        manager.UnregisterAdvertisement(advertisement.get_path())
        advertisement.Release()


if __name__ == '__main__':
    main()
