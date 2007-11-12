/*
 *
 * $Id$
 * Copyright (C) 2003-2007 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2007 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

#include <config-k3b.h>

#include "k3bdevicemanager.h"
#include "k3bdevice.h"
#include "k3bdeviceglobals.h"
#include "k3bscsicommand.h"
#include "k3bmmc.h"
#include "kdebug.h"

#include <qstring.h>
#include <qstringlist.h>
#include <q3ptrlist.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qregexp.h>

#include <k3process.h>
#include <kapplication.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <ktemporaryfile.h>

#include <Solid/DeviceNotifier>
#include <Solid/DeviceInterface>
#include <Solid/Block>
#include <Solid/Device>

#include <iostream>
#include <limits.h>
#include <assert.h>

#ifdef Q_OS_FREEBSD
#include <sys/param.h>
#include <sys/ucred.h>
#include <osreldate.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#ifdef HAVE_RESMGR
#include <resmgr.h>
#endif

#ifdef Q_OS_LINUX

/* Fix definitions for 2.5 kernels */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,70)
typedef unsigned char u8;
#endif

#undef __STRICT_ANSI__
#include <asm/types.h>
#define __STRICT_ANSI__

#include <scsi/scsi.h>
#include <linux/major.h>


#ifndef SCSI_DISK_MAJOR
#define SCSI_DISK_MAJOR(M) ((M) == SCSI_DISK0_MAJOR ||                  \
                            ((M) >= SCSI_DISK1_MAJOR && (M) <= SCSI_DISK7_MAJOR) || \
                            ((M) >= SCSI_DISK8_MAJOR && (M) <= SCSI_DISK15_MAJOR))
#endif

#ifndef SCSI_BLK_MAJOR
#define SCSI_BLK_MAJOR(M)                       \
    (SCSI_DISK_MAJOR(M)                         \
     || (M) == SCSI_CDROM_MAJOR)
#endif

#ifndef SCSI_GENERIC_MAJOR
#define SCSI_GENERIC_MAJOR 21
#endif

#endif // Q_OS_LINUX


#ifdef Q_OS_FREEBSD
#include <cam/cam.h>
#include <cam/scsi/scsi_pass.h>
#include <camlib.h>
#endif

#ifdef Q_OS_NETBSD
#include <sys/scsiio.h>
#endif



class K3bDevice::DeviceManager::Private
{
public:
    QList<Device*> allDevices;
    QList<Device*> cdReader;
    QList<Device*> cdWriter;
    QList<Device*> dvdReader;
    QList<Device*> dvdWriter;
    QList<Device*> bdReader;
    QList<Device*> bdWriter;

    bool checkWritingModes;
};



K3bDevice::DeviceManager::DeviceManager( QObject* parent )
    : QObject( parent ),
      d( new Private() )
{
    connect( Solid::DeviceNotifier::instance(), SIGNAL( deviceAdded( const QString& ) ),
             this, SLOT( slotSolidDeviceAdded( const QString& ) ) );
    connect( Solid::DeviceNotifier::instance(), SIGNAL( deviceRemoved( const QString& ) ),
             this, SLOT( slotSolidDeviceRemoved( const QString& ) ) );
}


K3bDevice::DeviceManager::~DeviceManager()
{
    qDeleteAll( d->allDevices );
    delete d;
}


void K3bDevice::DeviceManager::setCheckWritingModes( bool b )
{
    d->checkWritingModes = b;
}


K3bDevice::Device* K3bDevice::DeviceManager::deviceByName( const QString& name )
{
    return findDevice( name );
}


K3bDevice::Device* K3bDevice::DeviceManager::findDevice( int bus, int id, int lun )
{
    QListIterator<K3bDevice::Device*> it( d->allDevices );
    while( it.hasNext() ) {
        Device* dev = it.next();
        if( dev->scsiBus() == bus &&
            dev->scsiId() == id &&
            dev->scsiLun() == lun )
            return dev;
    }

    return 0;
}


K3bDevice::Device* K3bDevice::DeviceManager::findDevice( const QString& devicename )
{
    if( devicename.isEmpty() ) {
        kDebug() << "(K3bDevice::DeviceManager) request for empty device!" << endl;
        return 0;
    }
    QListIterator<K3bDevice::Device*> it( d->allDevices );
    while( it.hasNext() ) {
        Device* dev = it.next();
        if( dev->deviceNodes().contains(devicename) )
            return dev;
    }

    return 0;
}


QList<K3bDevice::Device*> K3bDevice::DeviceManager::cdWriter() const
{
    return d->cdWriter;
}

QList<K3bDevice::Device*> K3bDevice::DeviceManager::cdReader() const
{
    return d->cdReader;
}

QList<K3bDevice::Device*> K3bDevice::DeviceManager::dvdWriter() const
{
    return d->dvdWriter;
}

QList<K3bDevice::Device*> K3bDevice::DeviceManager::dvdReader() const
{
    return d->dvdReader;
}

QList<K3bDevice::Device*> K3bDevice::DeviceManager::blueRayReader() const
{
    return d->bdReader;
}

QList<K3bDevice::Device*> K3bDevice::DeviceManager::blueRayWriters() const
{
    return d->bdWriter;
}

QList<K3bDevice::Device*> K3bDevice::DeviceManager::burningDevices() const
{
    return cdWriter();
}


QList<K3bDevice::Device*> K3bDevice::DeviceManager::readingDevices() const
{
    return cdReader();
}


QList<K3bDevice::Device*> K3bDevice::DeviceManager::allDevices() const
{
    return d->allDevices;
}


int K3bDevice::DeviceManager::scanBus()
{
    int cnt = 0;

    QList<Solid::Device> dl = Solid::Device::listFromType( Solid::DeviceInterface::OpticalDrive );
    Q_FOREACH( Solid::Device solidDev, dl ) {
        if ( Solid::Block* solidDevInt = solidDev.as<Solid::Block>() ) {
            if ( addDevice( solidDevInt->device() ) )
                ++cnt;
        }
    }

    //
    // Scan the generic devices if we have scsi devices
    //
    kDebug() << "(K3bDevice::DeviceManager) SCANNING FOR GENERIC DEVICES." << endl;
    for( int i = 0; i < 16; i++ ) {
        QString sgDev = resolveSymLink( QString("/dev/sg%1").arg(i) );
        int bus = -1, id = -1, lun = -1;
        if( determineBusIdLun( sgDev, bus, id, lun ) ) {
            if( Device* dev = findDevice( bus, id, lun ) ) {
                dev->m_genericDevice = sgDev;
            }
        }
    }
    // FIXME: also scan /dev/scsi/hostX.... for devfs without symlinks

    return cnt;
}


void K3bDevice::DeviceManager::printDevices()
{
    kDebug() << "Devices:" << endl
             << "------------------------------" << endl;
    Q_FOREACH( Device* dev, d->allDevices ) {
        kDebug() << "Blockdevice:    " << dev->blockDeviceName() << endl
                 << "Generic device: " << dev->genericDevice() << endl
                 << "Vendor:         " << dev->vendor() << endl
                 << "Description:    " << dev->description() << endl
                 << "Version:        " << dev->version() << endl
                 << "Write speed:    " << dev->maxWriteSpeed() << endl
                 << "Profiles:       " << mediaTypeString( dev->supportedProfiles() ) << endl
                 << "Read Cap:       " << mediaTypeString( dev->readCapabilities() ) << endl
                 << "Write Cap:      " << mediaTypeString( dev->writeCapabilities() ) << endl
                 << "Writing modes:  " << writingModeString( dev->writingModes() ) << endl
                 << "Reader aliases: " << dev->deviceNodes().join(", ") << endl
                 << "------------------------------" << endl;
    }
}


void K3bDevice::DeviceManager::clear()
{
    // clear current devices
    d->cdReader.clear();
    d->cdWriter.clear();
    d->dvdReader.clear();
    d->dvdWriter.clear();
    d->bdReader.clear();
    d->bdWriter.clear();

    // to make sure no one crashes lets keep the devices around until the changed
    // signals return
    qDeleteAll( d->allDevices );
    d->allDevices.clear();

    emit changed( this );
    emit changed();
}


bool K3bDevice::DeviceManager::readConfig( KConfig* c_ )
{
    //
    // New configuration format since K3b 0.11.94
    // for details see saveConfig()
    //

    if( !c_->hasGroup( "Devices" ) )
        return false;

    KConfigGroup c = c_->group( "Devices" );

    QStringList deviceSearchPath = c.readEntry( "device_search_path", QStringList() );
    for( QStringList::const_iterator it = deviceSearchPath.constBegin();
         it != deviceSearchPath.constEnd(); ++it )
        addDevice( *it );

    //
    // Iterate over all devices and check if we have a config entry
    //
    for( QList<K3bDevice::Device*>::iterator it = d->allDevices.begin(); it != d->allDevices.end(); ++it ) {
        K3bDevice::Device* dev = *it;

        QString configEntryName = dev->vendor() + " " + dev->description();
        QStringList list = c.readEntry( configEntryName, QStringList() );
        if( !list.isEmpty() ) {
            kDebug() << "(K3bDevice::DeviceManager) found config entry for devicetype: " << configEntryName << endl;

            dev->setMaxReadSpeed( list[0].toInt() );
            if( list.count() > 1 )
                dev->setMaxWriteSpeed( list[1].toInt() );
            if( list.count() > 2 )
                dev->setCdrdaoDriver( list[2] );
            if( list.count() > 3 )
                dev->setCdTextCapability( list[3] == "yes" );
        }
    }

    return true;
}


bool K3bDevice::DeviceManager::saveConfig( KConfig* c_ )
{
    //
    // New configuration format since K3b 0.11.94
    //
    // We save a device search path which contains all device nodes
    // where devices could be found including the old search path.
    // This way also for example a manually added USB device will be
    // found between sessions.
    // Then we do not save the device settings (writing speed, cdrdao driver)
    // for every single device but for every device type.
    // This also makes sure device settings are kept between sessions
    //

    KConfigGroup c = c_->group( "Devices" );

    QStringList deviceSearchPath = c.readEntry( "device_search_path", QStringList() );
    // remove duplicate entries (caused by buggy old implementations)
    QStringList saveDeviceSearchPath;
    for( QStringList::const_iterator it = deviceSearchPath.constBegin(); it != deviceSearchPath.constEnd(); ++it )
        if( !saveDeviceSearchPath.contains( *it ) )
            saveDeviceSearchPath.append( *it );

    Q_FOREACH( Device* dev, d->allDevices ) {

        // update device search path
        if( !saveDeviceSearchPath.contains( dev->blockDeviceName() ) )
            saveDeviceSearchPath.append( dev->blockDeviceName() );

        // save the device type settings
        QString configEntryName = dev->vendor() + " " + dev->description();
        QStringList list;
        list << QString::number(dev->maxReadSpeed())
             << QString::number(dev->maxWriteSpeed())
             << dev->cdrdaoDriver();

        if( dev->cdrdaoDriver() != "auto" )
            list << ( dev->cdTextCapable() == 1 ? "yes" : "no" );
        else
            list << "auto";

        c.writeEntry( configEntryName, list );
    }

    c.writeEntry( "device_search_path", saveDeviceSearchPath );

    c.sync();

    return true;
}


bool K3bDevice::DeviceManager::testForCdrom( const QString& devicename )
{
#ifdef Q_OS_FREEBSD
    Q_UNUSED(devicename);
    return true;
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_NETBSD)
    bool ret = false;
    int cdromfd = K3bDevice::openDevice( QFile::encodeName( devicename ).data() );
    if (cdromfd < 0) {
        kDebug() << "could not open device " << devicename << " (" << strerror(errno) << ")" << endl;
        return ret;
    }

    // stat the device
    struct stat cdromStat;
    if ( ::fstat( cdromfd, &cdromStat ) ) {
        return false;
    }

    if( !S_ISBLK( cdromStat.st_mode) ) {
        kDebug() << devicename << " is no block device" << endl;
    }
    else {
        kDebug() << devicename << " is block device (" << (int)(cdromStat.st_rdev & 0xFF) << ")" << endl;
#if defined(Q_OS_NETBSD)
    }
    {
#endif
        // inquiry
        // use a 36 bytes buffer since not all devices return the full inquiry struct
        unsigned char buf[36];
        struct inquiry* inq = (struct inquiry*)buf;
        ::memset( buf, 0, sizeof(buf) );

        ScsiCommand cmd( cdromfd );
        cmd[0] = MMC_INQUIRY;
        cmd[4] = sizeof(buf);
        cmd[5] = 0;

        if( cmd.transport( TR_DIR_READ, buf, sizeof(buf) ) ) {
            kDebug() << "(K3bDevice::Device) Unable to do inquiry. " << devicename << " is not a cdrom device" << endl;
        }
        else if( (inq->p_device_type&0x1f) != 0x5 ) {
            kDebug() << devicename << " seems not to be a cdrom device: " << strerror(errno) << endl;
        }
        else {
            ret = true;
            kDebug() << devicename << " seems to be cdrom" << endl;
        }
    }

    ::close( cdromfd );
    return ret;
#endif
}

K3bDevice::Device* K3bDevice::DeviceManager::addDevice( const QString& devicename )
{
#ifdef Q_OS_FREEBSD
    return 0;
#endif

    K3bDevice::Device* device = 0;

    // resolve all symlinks
    QString resolved = resolveSymLink( devicename );
    kDebug() << devicename << " resolved to " << resolved << endl;

    if ( K3bDevice::Device* oldDev = findDevice(resolved) ) {
        kDebug() << "(K3bDevice::DeviceManager) dev " << resolved  << " already found" << endl;
        oldDev->addDeviceNode( devicename );
        return 0;
    }

    if( !testForCdrom(resolved) ) {
#ifdef HAVE_RESMGR
        // With resmgr we might only be able to open the symlink name.
        if( testForCdrom(devicename) ) {
            resolved = devicename;
        }
        else {
            return 0;
        }
#else
        return 0;
#endif
    }

    int bus = -1, target = -1, lun = -1;
    bool scsi = determineBusIdLun( resolved, bus, target, lun );
    if(scsi) {
        if ( K3bDevice::Device* oldDev = findDevice(bus, target, lun) ) {
            kDebug() << "(K3bDevice::DeviceManager) dev " << resolved  << " already found" << endl;
            oldDev->addDeviceNode( devicename );
            return 0;
        }
    }

    device = new K3bDevice::Device(resolved);
    if( scsi ) {
        device->m_bus = bus;
        device->m_target = target;
        device->m_lun = lun;
    }

    return addDevice(device);
}


K3bDevice::Device* K3bDevice::DeviceManager::addDevice( K3bDevice::Device* device )
{
    const QString devicename = device->devicename();

    if( !device->init() ) {
        kDebug() << "Could not initialize device " << devicename << endl;
        delete device;
        return 0;
    }

    if( device ) {
        d->allDevices.append( device );

        // not every drive is able to read CDs
        // there are some 1st generation DVD writer that cannot
        if( device->type() & K3bDevice::DEVICE_CD_ROM )
            d->cdReader.append( device );
        if( device->readsDvd() )
            d->dvdReader.append( device );
        if( device->writesCd() )
            d->cdWriter.append( device );
        if( device->writesDvd() )
            d->dvdWriter.append( device );
        if( device->readCapabilities() & MEDIA_BD_ALL )
            d->bdReader.append( device );
        if( device->writeCapabilities() & MEDIA_BD_ALL )
            d->bdWriter.append( device );

        if( device->writesCd() ) {
            // default to max write speed
            kDebug() << "(K3bDevice::DeviceManager) setting current write speed of device "
                     << device->blockDeviceName()
                     << " to " << device->maxWriteSpeed() << endl;
            device->setCurrentWriteSpeed( device->maxWriteSpeed() );
        }

        emit changed( this );
        emit changed();
    }

    return device;
}


void K3bDevice::DeviceManager::removeDevice( const QString& dev )
{
    if( Device* device = findDevice( dev ) ) {
        d->cdReader.removeAll( device );
        d->dvdReader.removeAll( device );
        d->bdReader.removeAll( device );
        d->cdWriter.removeAll( device );
        d->dvdWriter.removeAll( device );
        d->bdWriter.removeAll( device );
        d->allDevices.removeAll( device );

        emit changed( this );
        emit changed();

        delete device;
    }
}


bool K3bDevice::DeviceManager::determineBusIdLun( const QString& dev, int& bus, int& id, int& lun )
{
#ifdef Q_OS_FREEBSD
    Q_UNUSED(dev);
    Q_UNUSED(bus);
    Q_UNUSED(id);
    Q_UNUSED(lun);
    return false;
    /* NOTREACHED */
#endif

#ifdef Q_OS_NETBSD
    int cdromfd = K3bDevice::openDevice ( dev.ascii() );
    if (cdromfd < 0) {
        kDebug() << "could not open device " << dev << " (" << strerror(errno) << ")" << endl;
        return false;
    }

    struct scsi_addr my_addr;

    if (::ioctl(cdromfd, SCIOCIDENTIFY, &my_addr))
    {
        kDebug() << "ioctl(SCIOCIDENTIFY) failed on device " << dev << " (" << strerror(errno) << ")" << endl;

        ::close(cdromfd);
        return false;
    }

    if (my_addr.type == TYPE_ATAPI)
    {
        // XXX Re-map atapibus, so it doesn't conflict with "real" scsi
        // busses

        bus = 15;
        id  = my_addr.addr.atapi.drive + 2 * my_addr.addr.atapi.atbus;
        lun = 0;
    }
    else
    {
        bus = my_addr.addr.scsi.scbus;
        id  = my_addr.addr.scsi.target;
        lun = my_addr.addr.scsi.lun;
    }

    ::close(cdromfd);

    return true;
#endif

#ifdef Q_OS_LINUX
    int ret = false;
    int cdromfd = K3bDevice::openDevice( QFile::encodeName( dev ).data() );
    if (cdromfd < 0) {
        return false;
    }

    struct stat cdromStat;
    if ( ::fstat( cdromfd, &cdromStat ) ) {
        return false;
    }

    if( SCSI_BLK_MAJOR( cdromStat.st_rdev>>8 ) ||
        SCSI_GENERIC_MAJOR == (cdromStat.st_rdev>>8) ) {
        struct ScsiIdLun
        {
            int id;
            int lun;
        };
        ScsiIdLun idLun;

        // in kernel 2.2 SCSI_IOCTL_GET_IDLUN does not contain the bus id
        if ( (::ioctl( cdromfd, SCSI_IOCTL_GET_IDLUN, &idLun ) < 0) ||
             (::ioctl( cdromfd, SCSI_IOCTL_GET_BUS_NUMBER, &bus ) < 0) ) {
            kDebug() << "(K3bDevice::DeviceManager) no SCSI device: " << dev << endl;
            ret = false;
        }
        else {
            id  = idLun.id & 0xff;
            lun = (idLun.id >> 8) & 0xff;
            kDebug() << "bus: " << bus << ", id: " << id << ", lun: " << lun << endl;
            ret = true;
        }
    }

    ::close(cdromfd);
    return ret;
#endif
}


QString K3bDevice::DeviceManager::resolveSymLink( const QString& path )
{
    char resolved[PATH_MAX];
    if( !realpath( QFile::encodeName(path), resolved ) )
    {
        kDebug() << "Could not resolve " << path << endl;
        return path;
    }

    return QString::fromLatin1( resolved );
}


void K3bDevice::DeviceManager::slotSolidDeviceAdded( const QString& udi )
{
    Solid::Device solidDev( udi );
    if ( solidDev.isDeviceInterface( Solid::DeviceInterface::OpticalDrive ) ) {
        if ( Solid::Block* solidDevInt = solidDev.as<Solid::Block>() ) {
            addDevice( solidDevInt->device() );
        }
    }
}


void K3bDevice::DeviceManager::slotSolidDeviceRemoved( const QString& udi )
{
    Solid::Device solidDev( udi );
    if ( solidDev.isDeviceInterface( Solid::DeviceInterface::OpticalDrive ) ) {
        if ( Solid::Block* solidDevInt = solidDev.as<Solid::Block>() ) {
            removeDevice( solidDevInt->device() );
        }
    }
}

#include "k3bdevicemanager.moc"
