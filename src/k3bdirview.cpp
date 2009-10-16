/*
 *
 * Copyright (C) 2003-2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This file is part of the K3b project.
 * Copyright (C) 1998-2008 Sebastian Trueg <trueg@k3b.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file "COPYING" for the exact licensing terms.
 */

// K3B-includes
#include <config-k3b.h>
#include "k3bdirview.h"
#include "k3b.h"
#include "k3bappdevicemanager.h"
#include "k3bapplication.h"
#include "k3bdevice.h"
#include "k3bdevicehandler.h"
#include "k3bdiskinfoview.h"
#include "k3bexternalbinmanager.h"
#include "k3bfileview.h"
#include "k3bfiletreeview.h"
#include "k3bmediacache.h"
#include "k3bpassivepopup.h"
#include "k3bthememanager.h"
#include "rip/k3baudiocdview.h"
#include "rip/k3bvideocdview.h"
#ifdef ENABLE_DVD_RIPPING
#include "rip/videodvd/k3bvideodvdrippingview.h"
#endif

// KDE-includes
#include <KConfig>
#include <KLocale>
#include <KMessageBox>
#include <KUrl>
#include <unistd.h>

// QT-includes
#include <QDir>
#include <QSplitter>
#include <QString>
#include <QStackedWidget>
#include <QVBoxLayout>


class K3b::DirView::Private
{
public:
    QStackedWidget* viewStack;

    AudioCdView* cdView;
    VideoCdView* videoView;
#ifdef ENABLE_DVD_RIPPING
    VideoDVDRippingView* movieView;
#endif
    FileView* fileView;
    DiskInfoView* infoView;

    QSplitter* mainSplitter;
    FileTreeView* fileTreeView;

    bool bViewDiskInfo;
    bool contextMediaInfoRequested;
    
    void setCurrentView( K3b::ContentsView* view );
};


void K3b::DirView::Private::setCurrentView( K3b::ContentsView* view )
{
    if( ContentsView* previous = qobject_cast<ContentsView*>( viewStack->currentWidget() ) ) {
        previous->activate( false );
    }
    
    viewStack->setCurrentWidget( view );
    view->activate( true );
}


K3b::DirView::DirView( K3b::FileTreeView* treeView, QWidget* parent )
    : QWidget(parent),
      d( new Private )
{
    d->fileTreeView = treeView;
    d->bViewDiskInfo = false;
    d->contextMediaInfoRequested = false;

    QVBoxLayout* layout = new QVBoxLayout( this );
    layout->setMargin( 0 );

    if( !d->fileTreeView ) {
        d->mainSplitter = new QSplitter( this );
        d->fileTreeView = new K3b::FileTreeView( d->mainSplitter );
        d->viewStack    = new QStackedWidget( d->mainSplitter );
        layout->addWidget( d->mainSplitter );
    }
    else {
        d->viewStack    = new QStackedWidget( this );
        d->mainSplitter = 0;
        layout->addWidget( d->viewStack );
    }

    d->fileView     = new K3b::FileView( d->viewStack );
    d->viewStack->addWidget( d->fileView );
    d->cdView       = new K3b::AudioCdView( d->viewStack );
    d->viewStack->addWidget( d->cdView );
    d->videoView    = new K3b::VideoCdView( d->viewStack );
    d->viewStack->addWidget( d->videoView );
    d->infoView     = new K3b::DiskInfoView( d->viewStack );
    d->viewStack->addWidget( d->infoView );
#ifdef ENABLE_DVD_RIPPING
    d->movieView    = new K3b::VideoDVDRippingView( d->viewStack );
    d->viewStack->addWidget( d->movieView );
#endif

    d->setCurrentView( d->fileView );

//     d->fileTreeView->setCurrentDevice( k3bappcore->appDeviceManager()->currentDevice() );

    d->fileView->setAutoUpdate( true ); // in case we look at the mounted path

    if( d->mainSplitter ) {
        // split
        QList<int> sizes = d->mainSplitter->sizes();
        int all = sizes[0] + sizes[1];
        sizes[1] = all*2/3;
        sizes[0] = all - sizes[1];
        d->mainSplitter->setSizes( sizes );
    }

    connect( d->fileTreeView, SIGNAL(activated(const KUrl&)),
             this, SLOT(slotDirActivated(const KUrl&)) );
    connect( d->fileTreeView, SIGNAL(activated(K3b::Device::Device*)),
             this, SLOT(showDevice(K3b::Device::Device*)) );
    connect( d->fileTreeView, SIGNAL(activated(K3b::Device::Device*)),
             this, SIGNAL(deviceSelected(K3b::Device::Device*)) );

    connect( d->fileView, SIGNAL(urlEntered(const KUrl&)), d->fileTreeView, SLOT(setSelectedUrl(const KUrl&)) );
    connect( d->fileView, SIGNAL(urlEntered(const KUrl&)), this, SIGNAL(urlEntered(const KUrl&)) );

    connect( k3bappcore->appDeviceManager(), SIGNAL(mountFinished(const QString&)),
             this, SLOT(slotMountFinished(const QString&)) );
    connect( k3bappcore->appDeviceManager(), SIGNAL(unmountFinished(bool)),
             this, SLOT(slotUnmountFinished(bool)) );
}

K3b::DirView::~DirView()
{
    delete d;
}


void K3b::DirView::showUrl( const KUrl& url )
{
    kDebug() << url;
    slotDirActivated( url );
}


void K3b::DirView::showDevice( K3b::Device::Device* dev )
{
    d->contextMediaInfoRequested = true;
    d->fileTreeView->setSelectedDevice( dev );
    showMediumInfo( k3bappcore->mediaCache()->medium( dev ) );
}


void K3b::DirView::showDiskInfo( K3b::Device::Device* dev )
{
    d->contextMediaInfoRequested = false;
    d->fileTreeView->setSelectedDevice( dev );
    showMediumInfo( k3bappcore->mediaCache()->medium( dev ) );
}


void K3b::DirView::showMediumInfo( const K3b::Medium& medium )
{
    if( !d->contextMediaInfoRequested ||
        medium.diskInfo().diskState() == K3b::Device::STATE_EMPTY ||
        medium.diskInfo().diskState() == K3b::Device::STATE_NO_MEDIA ) {

        // show cd info
        d->setCurrentView( d->infoView );
        d->infoView->reload( medium );
        return;
    }

#ifdef ENABLE_DVD_RIPPING
    else if( medium.content() & K3b::Medium::ContentVideoDVD ) {
        KMessageBox::ButtonCode r = KMessageBox::Yes;
        if( KMessageBox::shouldBeShownYesNo( "videodvdripping", r ) ) {
            r = (KMessageBox::ButtonCode)
                KMessageBox::questionYesNoCancel( this,
                                                  i18n("<p>You have selected the K3b Video DVD ripping tool."
                                                       "<p>It is intended to <em>rip single titles</em> from a video DVD "
                                                       "into a compressed format such as XviD. Menu structures are completely ignored."
                                                       "<p>If you intend to copy the plain Video DVD vob files from the DVD "
                                                       "(including decryption) for further processing with another application, "
                                                       "please use the following link to access the Video DVD file structure: "
                                                       "<a href=\"videodvd:/\">videodvd:/</a>"
                                                       "<p>If you intend to make a copy of the entire Video DVD including all menus "
                                                       "and extras it is recommended to use the K3b DVD Copy tool."),
                                                  i18n("Video DVD ripping"),
                                                  KGuiItem( i18n("Continue") ),
                                                  KGuiItem( i18n("Open DVD Copy Dialog") ),
                                                  KStandardGuiItem::cancel(),
                                                  "videodvdripping",
                                                  KMessageBox::AllowLink );
        }
        else { // if we do not show the dialog we always continue with the ripping. Everything else would be confusing
            r = KMessageBox::Yes;
        }

        if( r == KMessageBox::Cancel ) {
            //      d->viewStack->raiseWidget( d->fileView );
        }
        else if( r == KMessageBox::No ) {
            d->setCurrentView( d->fileView );
            static_cast<K3b::MainWindow*>( kapp->activeWindow() )->slotMediaCopy();
        }
        else {
            d->movieView->reload( medium );
            d->setCurrentView( d->movieView );
        }

        return;
    }
#endif

    else if( medium.content() & K3b::Medium::ContentData ) {
        bool mount = true;
        if( medium.content() & K3b::Medium::ContentVideoCD ) {
            if( !k3bcore ->externalBinManager() ->foundBin( "vcdxrip" ) ) {
                KMessageBox::sorry( this,
                                    i18n("K3b uses vcdxrip from the vcdimager package to rip Video CDs. "
                                         "Please make sure it is installed.") );
            }
            else {
                if( KMessageBox::questionYesNo( this,
                                                i18n("Found %1. Do you want K3b to mount the data part "
                                                     "or show all the tracks?", i18n("Video CD") ),
                                                i18n("Video CD"),
                                                KGuiItem(i18n("Mount CD")),
                                                KGuiItem(i18n("Show Video Tracks")) ) == KMessageBox::No ) {
                    mount = false;
                    d->setCurrentView( d->videoView );
                    d->videoView->reload( medium );
                }
            }
        }
        else if( medium.content() & K3b::Medium::ContentAudio ) {
            if( KMessageBox::questionYesNo( this,
                                            i18n("Found %1. Do you want K3b to mount the data part "
                                                 "or show all the tracks?", i18n("Audio CD") ),
                                            i18n("Audio CD"),
                                            KGuiItem(i18n("Mount CD")),
                                            KGuiItem(i18n("Show Audio Tracks")) ) == KMessageBox::No ) {
                mount = false;
                d->setCurrentView( d->cdView );
                d->cdView->reload( medium );
            }
        }

        if( mount )
            k3bappcore->appDeviceManager()->mountDisk( medium.device() );
    }

    else if( medium.content() & K3b::Medium::ContentAudio ) {
        d->setCurrentView( d->cdView );
        d->cdView->reload( medium );
    }

    else {
        // show cd info
        d->setCurrentView( d->infoView );
        d->infoView->reload( medium );
    }

    d->contextMediaInfoRequested = false;
}


void K3b::DirView::slotMountFinished( const QString& mp )
{
    if( !mp.isEmpty() ) {
        slotDirActivated( mp );
        d->fileView->reload(); // HACK to get the contents shown... FIXME
    }
    else {
        d->setCurrentView( d->fileView );
        K3b::PassivePopup::showPopup( i18n("<p>K3b was unable to mount medium <b>%1</b> in device <em>%2 - %3</em>"
                                         ,k3bappcore->mediaCache()->medium( k3bappcore->appDeviceManager()->currentDevice() ).shortString()
                                         ,k3bappcore->appDeviceManager()->currentDevice()->vendor()
                                         ,k3bappcore->appDeviceManager()->currentDevice()->description() ),
                                    i18n("Mount Failed"),
                                    K3b::PassivePopup::Warning );
    }
}


void K3b::DirView::slotUnmountFinished( bool success )
{
    if( success ) {
        // TODO: check if the fileview is still displaying a folder from the medium
    }
    else {
        K3b::PassivePopup::showPopup( i18n("<p>K3b was unable to unmount medium <b>%1</b> in device <em>%2 - %3</em>"
                                         ,k3bappcore->mediaCache()->medium( k3bappcore->appDeviceManager()->currentDevice() ).shortString()
                                         ,k3bappcore->appDeviceManager()->currentDevice()->vendor()
                                         ,k3bappcore->appDeviceManager()->currentDevice()->description() ),
                                    i18n("Unmount Failed"),
                                    K3b::PassivePopup::Warning );
    }
}


void K3b::DirView::slotDirActivated( const QString& url )
{
    slotDirActivated( KUrl(url) );
}


void K3b::DirView::slotDirActivated( const KUrl& url )
{
    kDebug() << url;
    d->fileView->setUrl( url, true );
    d->setCurrentView( d->fileView );
}


void K3b::DirView::home()
{
    slotDirActivated( QDir::homePath() );
}


void K3b::DirView::saveConfig( KConfigGroup grp )
{
    d->fileView->saveConfig(grp);
}


void K3b::DirView::readConfig( const KConfigGroup &grp )
{
    d->fileView->readConfig(grp);
}

#include "k3bdirview.moc"
