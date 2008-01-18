/*
 *
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

#include <config-k3b.h>

#include "k3blameencoder.h"
#include "k3blamemanualsettingsdialog.h"

#include <k3bcore.h>

#include <klocale.h>
#include <kconfig.h>
#include <kdebug.h>
#include <knuminput.h>
#include <kcombobox.h>
#include <kdialog.h>

#include <qlayout.h>
#include <q3cstring.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qspinbox.h>
#include <q3groupbox.h>
#include <q3buttongroup.h>
#include <qtextcodec.h>
#include <qfile.h>
#include <qslider.h>
#include <qlabel.h>
#include <qpushbutton.h>
//Added by qt3to4:
#include <Q3HBoxLayout>

#include <stdio.h>
#include <lame/lame.h>


static const int s_lame_bitrates[] = {
    32,
    40,
    48,
    56,
    64,
    80,
    96,
    112,
    128,
    160,
    192,
    224,
    256,
    320,
    0 // just used for the loops below
};


static const int s_lame_presets[] = {
    56, // ABR for Voice, Radio, Mono, etc.
    90, //

    V6, // ~115 kbps
    V5, // ~130 kbps  | Portable - small size
    V4, // ~160 kbps

    V3, // ~175 kbps
    V2, // ~190 kbps  | HiFi - for home or quite listening
    V1, // ~210 kbps  |
    V0, // ~230 kbps

    320 // ABR 320 neary lossless for archiving (not recommended, use flac instead)
};


static const int s_lame_preset_approx_bitrates[] = {
    56,
    90,
    115,
    130,
    160,
    175,
    190,
    210,
    230,
    320
};


static const char* s_lame_preset_strings[] = {
    I18N_NOOP("Low quality (56 kbps)"),
    I18N_NOOP("Low quality (90 kbps)"),

    I18N_NOOP("Portable (average 115 kbps)"),
    I18N_NOOP("Portable (average 130 kbps)"),
    I18N_NOOP("Portable (average 160 kbps)"),

    I18N_NOOP("HiFi (average 175 kbps)"),
    I18N_NOOP("HiFi (average 190 kbps)"),
    I18N_NOOP("HiFi (average 210 kbps)"),
    I18N_NOOP("HiFi (average 230 kbps)"),

    I18N_NOOP("Archiving (320 kbps)"),
};


static const char* s_lame_mode_strings[] = {
    I18N_NOOP("Stereo"),
    I18N_NOOP("Joint Stereo"),
    I18N_NOOP("Mono")
};



class K3bLameEncoder::Private
{
public:
    Private()
        : flags(0),
          fid(0) {
    }

    lame_global_flags* flags;

    char buffer[8000];

    QString filename;
    FILE* fid;
};




K3bLameEncoder::K3bLameEncoder( QObject* parent, const QVariantList& )
    : K3bAudioEncoder( parent )
{
    d = new Private();
}


K3bLameEncoder::~K3bLameEncoder()
{
    closeFile();

    delete d;
}


bool K3bLameEncoder::openFile( const QString& extension, const QString& filename, const K3b::Msf& length )
{
    closeFile();

    d->filename = filename;
    d->fid = ::fopen( QFile::encodeName( filename ), "w+" );
    if( d->fid )
        return initEncoder( extension, length );
    else
        return false;
}


bool K3bLameEncoder::isOpen() const
{
    return ( d->fid != 0 );
}


void K3bLameEncoder::closeFile()
{
    if( isOpen() ) {
        finishEncoder();
        ::fclose( d->fid );
        d->fid = 0;
        d->filename.truncate(0);
    }
}


const QString& K3bLameEncoder::filename() const
{
    return d->filename;
}


bool K3bLameEncoder::initEncoderInternal( const QString&, const K3b::Msf& length )
{
    KConfig* c = k3bcore->config();
    KConfigGroup grp(c, "K3bLameEncoderPlugin" );

    d->flags = lame_init();

    if( d->flags == 0 ) {
        kDebug() << "(K3bLameEncoder) lame_init failed.";
        return false;
    }

    //
    // set the format of the input data
    //
    lame_set_num_samples( d->flags, length.lba()*588 );
    lame_set_in_samplerate( d->flags, 44100 );
    lame_set_num_channels( d->flags, 2 );

    //
    // Lame by default determines the samplerate based on the bitrate
    // since we have no option for the user to influence this yet
    // we just keep to the good old 44.1 khz
    //
    lame_set_out_samplerate( d->flags, 44100 );

    //
    // Choose the quality level
    //
    if( grp.readEntry( "Manual Bitrate Settings", false ) ) {
        //
        // Mode
        //
        QString mode = grp.readEntry( "Mode", "stereo" );
        if( mode == "stereo" )
            lame_set_mode( d->flags, STEREO );
        else if( mode == "joint" )
            lame_set_mode( d->flags, JOINT_STEREO );
        else // mono
            lame_set_mode( d->flags, MONO );

        //
        // Variable Bitrate
        //
        if( grp.readEntry( "VBR", false ) ) {
            // we use the default algorithm here
            lame_set_VBR( d->flags, vbr_default );

            if( grp.readEntry( "Use Maximum Bitrate", false ) ) {
                lame_set_VBR_max_bitrate_kbps( d->flags, grp.readEntry( "Maximum Bitrate", 224 ) );
            }
            if( grp.readEntry( "Use Minimum Bitrate", false ) ) {
                lame_set_VBR_min_bitrate_kbps( d->flags, grp.readEntry( "Minimum Bitrate", 32 ) );

                // TODO: lame_set_hard_min
            }
            if( grp.readEntry( "Use Average Bitrate", true ) ) {
                lame_set_VBR( d->flags, vbr_abr );
                lame_set_VBR_mean_bitrate_kbps( d->flags, grp.readEntry( "Average Bitrate", 128 ) );
            }
        }

        //
        // Constant Bitrate
        //
        else {
            lame_set_VBR( d->flags, vbr_off );
            lame_set_brate( d->flags, grp.readEntry( "Constant Bitrate", 128 ) );
        }
    }

    else {
        //
        // In lame 0 is the highest quality. Since that is just confusing for the user
        // if we call the setting "Quality" we simply invert the value.
        //
        int q = grp.readEntry( "Quality Level", 5 );
        if( q < 0 ) q = 0;
        if( q > 9 ) q = 9;

        kDebug() << "(K3bLameEncoder) setting preset encoding value to " << q;

        if ( q < 2 || q > 8 ) {
            lame_set_VBR( d->flags, vbr_abr );
        }
        else {
            lame_set_VBR( d->flags, vbr_default );
        }
        lame_set_preset( d->flags, s_lame_presets[q] );

        if( q < 2 )
            lame_set_mode( d->flags, MONO );
    }


    //
    // file options
    //
    lame_set_copyright( d->flags, grp.readEntry( "Copyright", false ) );
    lame_set_original( d->flags, grp.readEntry( "Original", true ) );
    lame_set_strict_ISO( d->flags, grp.readEntry( "ISO compliance", false ) );
    lame_set_error_protection( d->flags, grp.readEntry( "Error Protection", false ) );


    //
    // Used Algorithm
    //
    // default to 2 which is the same as the -h lame option
    // THIS HAS NO INFLUENCE ON THE SIZE OF THE FILE!
    //
    //
    // In lame 0 is the highest quality. Since that is just confusing for the user
    // if we call the setting "Quality" we simply invert the value.
    //
    int q = grp.readEntry( "Encoder Quality", 7 );
    if( q < 0 ) q = 0;
    if( q > 9 ) q = 9;
    lame_set_quality( d->flags, 9-q );

    //
    // ID3 settings
    //
    // for now we default to both v1 and v2 tags
    id3tag_add_v2( d->flags );
    id3tag_pad_v2( d->flags );

    return( lame_init_params( d->flags ) != -1 );
}


long K3bLameEncoder::encodeInternal( const char* data, Q_ULONG len )
{
    // FIXME: we may have to swap data here
    int size = lame_encode_buffer_interleaved( d->flags,
                                               (short int*)data,
                                               len/4,
                                               (unsigned char*)d->buffer,
                                               8000 );
    if( size < 0 ) {
        kDebug() << "(K3bLameEncoder) lame_encode_buffer_interleaved failed.";
        return -1;
    }

    return ::fwrite( d->buffer, 1, size, d->fid );
}


void K3bLameEncoder::finishEncoderInternal()
{
    int size = lame_encode_flush( d->flags,
                                  (unsigned char*)d->buffer,
                                  8000 );
    if( size > 0 )
        ::fwrite( d->buffer, 1, size, d->fid );

    lame_mp3_tags_fid( d->flags, d->fid );

    lame_close( d->flags );
    d->flags = 0;
}


void K3bLameEncoder::setMetaDataInternal( K3bAudioEncoder::MetaDataField f, const QString& value )
{
    // let's not use UTF-8 here since I don't know how to tell lame...
    // FIXME: when we use the codec we only get garbage. Why?
    QTextCodec* codec = 0;//QTextCodec::codecForName( "ISO8859-1" );
//  if( !codec )
//    kDebug() << "(K3bLameEncoder) could not find codec ISO8859-1.";

    switch( f ) {
    case META_TRACK_TITLE:
        id3tag_set_title( d->flags, codec ? codec->fromUnicode(value).data() : value.latin1() );
        break;
    case META_TRACK_ARTIST:
        id3tag_set_artist( d->flags, codec ? codec->fromUnicode(value).data() : value.latin1() );
        break;
    case META_ALBUM_TITLE:
        id3tag_set_album( d->flags, codec ? codec->fromUnicode(value).data() : value.latin1() );
        break;
    case META_ALBUM_COMMENT:
        id3tag_set_comment( d->flags, codec ? codec->fromUnicode(value).data() : value.latin1() );
        break;
    case META_YEAR:
        id3tag_set_year( d->flags, codec ? codec->fromUnicode(value).data() : value.latin1() );
        break;
    case META_TRACK_NUMBER:
        id3tag_set_track( d->flags, codec ? codec->fromUnicode(value).data() : value.latin1() );
        break;
    case META_GENRE:
        if( id3tag_set_genre( d->flags, codec ? codec->fromUnicode(value).data() : value.latin1() ) )
            kDebug() << "(K3bLameEncoder) unable to set genre.";
        break;
    default:
        return;
    }

    if( lame_init_params( d->flags ) < 0 )
        kDebug() << "(K3bLameEncoder) lame_init_params failed.";
}





K3bLameEncoderSettingsWidget::K3bLameEncoderSettingsWidget( QWidget* parent )
    : K3bPluginConfigWidget( parent )
{
    setupUi( this );
    m_sliderQuality->setRange( 0, 9 );
    m_spinEncoderQuality->setRange( 0, 9, true );

    m_manualSettingsDialog = new K3bLameManualSettingsDialog( this );
    for( int i = 0; s_lame_bitrates[i]; ++i )
        m_manualSettingsDialog->m_comboMaximumBitrate->insertItem( i18n("%1 kbps" , s_lame_bitrates[i]) );

    for( int i = 0; s_lame_bitrates[i]; ++i )
        m_manualSettingsDialog->m_comboMinimumBitrate->insertItem( i18n("%1 kbps" , s_lame_bitrates[i]) );

    for( int i = 0; s_lame_bitrates[i]; ++i )
        m_manualSettingsDialog->m_comboConstantBitrate->insertItem( i18n("%1 kbps" , s_lame_bitrates[i]) );


    // TODO: add whatsthis help for the quality level.
    //  QString qualityLevelWhatsThis = i18n("<p>");

    connect( m_buttonManualSettings, SIGNAL(clicked()),
             this, SLOT(slotShowManualSettings()) );
    connect( m_sliderQuality, SIGNAL(valueChanged(int)),
             this, SLOT(slotQualityLevelChanged(int)) );

    updateManualSettingsLabel();
    slotQualityLevelChanged( 5 );
}


K3bLameEncoderSettingsWidget::~K3bLameEncoderSettingsWidget()
{
}


void K3bLameEncoderSettingsWidget::slotShowManualSettings()
{
    // save current settings for proper cancellation
    bool constant = m_manualSettingsDialog->m_radioConstantBitrate->isChecked();
    int constBitrate = m_manualSettingsDialog->m_comboConstantBitrate->currentItem();
    int max = m_manualSettingsDialog->m_comboMaximumBitrate->currentItem();
    int min = m_manualSettingsDialog->m_comboMinimumBitrate->currentItem();
    int av = m_manualSettingsDialog->m_spinAverageBitrate->value();
    int mode = m_manualSettingsDialog->m_comboMode->currentItem();

    if( m_manualSettingsDialog->exec() == QDialog::Rejected ) {
        m_manualSettingsDialog->m_radioConstantBitrate->setChecked( constant );
        m_manualSettingsDialog->m_comboConstantBitrate->setCurrentIndex( constBitrate );
        m_manualSettingsDialog->m_comboMaximumBitrate->setCurrentIndex( max );
        m_manualSettingsDialog->m_comboMinimumBitrate->setCurrentIndex( min );
        m_manualSettingsDialog->m_spinAverageBitrate->setValue( av );
        m_manualSettingsDialog->m_comboMode->setCurrentIndex( mode );
    }
    else
        updateManualSettingsLabel();
}


void K3bLameEncoderSettingsWidget::updateManualSettingsLabel()
{
    if( m_manualSettingsDialog->m_radioConstantBitrate->isChecked() )
        m_labelManualSettings->setText( i18n("Constant Bitrate: %1 kbps (%2)")
                                        .arg(s_lame_bitrates[m_manualSettingsDialog->m_comboConstantBitrate->currentItem()])
                                        .arg(i18n(s_lame_mode_strings[m_manualSettingsDialog->m_comboMode->currentItem()])) );
    else
        m_labelManualSettings->setText( i18n("Variable Bitrate (%1)")
                                        .arg(i18n(s_lame_mode_strings[m_manualSettingsDialog->m_comboMode->currentItem()])) );
}


void K3bLameEncoderSettingsWidget::slotQualityLevelChanged( int val )
{
    m_labelQualityLevel->setText( i18n(s_lame_preset_strings[val]) );
}


void K3bLameEncoderSettingsWidget::loadConfig()
{
    KConfig* c = k3bcore->config();
    KConfigGroup grp(c, "K3bLameEncoderPlugin" );

    QString mode = grp.readEntry( "Mode", "stereo" );
    if( mode == "stereo" )
        m_manualSettingsDialog->m_comboMode->setCurrentIndex( 0 );
    else if( mode == "joint" )
        m_manualSettingsDialog->m_comboMode->setCurrentIndex( 1 );
    else // mono
        m_manualSettingsDialog->m_comboMode->setCurrentIndex( 2 );

    bool manual = grp.readEntry( "Manual Bitrate Settings", false );
    if( manual )
        m_radioManual->setChecked(true);
    else
        m_radioQualityLevel->setChecked(true);

    if(grp.readEntry( "VBR", false ) )
        m_manualSettingsDialog->m_radioVariableBitrate->setChecked( true );
    else
        m_manualSettingsDialog->m_radioConstantBitrate->setChecked( true );

    m_manualSettingsDialog->m_comboConstantBitrate->setCurrentItem( i18n("%1 kbps",grp.readEntry( "Constant Bitrate", 128 )) );
    m_manualSettingsDialog->m_comboMaximumBitrate->setCurrentItem( i18n("%1 kbps",grp.readEntry( "Maximum Bitrate", 224 )) );
    m_manualSettingsDialog->m_comboMinimumBitrate->setCurrentItem( i18n("%1 kbps",grp.readEntry( "Minimum Bitrate", 32 )) );
    m_manualSettingsDialog->m_spinAverageBitrate->setValue( grp.readEntry( "Average Bitrate", 128) );

    m_manualSettingsDialog->m_checkBitrateMaximum->setChecked( grp.readEntry( "Use Maximum Bitrate", false ) );
    m_manualSettingsDialog->m_checkBitrateMinimum->setChecked( grp.readEntry( "Use Minimum Bitrate", false ) );
    m_manualSettingsDialog->m_checkBitrateAverage->setChecked( grp.readEntry( "Use Average Bitrate", true ) );

    m_sliderQuality->setValue( grp.readEntry( "Quality Level", 5 ) );

    m_checkCopyright->setChecked( grp.readEntry( "Copyright", false ) );
    m_checkOriginal->setChecked( grp.readEntry( "Original", true ) );
    m_checkISO->setChecked( grp.readEntry( "ISO compliance", false ) );
    m_checkError->setChecked( grp.readEntry( "Error Protection", false ) );

    // default to 2 which is the same as the -h lame option
    m_spinEncoderQuality->setValue( grp.readEntry( "Encoder Quality", 7 ) );

    updateManualSettingsLabel();
}


void K3bLameEncoderSettingsWidget::saveConfig()
{
    KConfig* c = k3bcore->config();
    KConfigGroup grp(c, "K3bLameEncoderPlugin" );

    QString mode;
    switch( m_manualSettingsDialog->m_comboMode->currentItem() ) {
    case 0:
        mode = "stereo";
        break;
    case 1:
        mode = "joint";
        break;
    case 2:
        mode = "mono";
        break;
    }
    grp.writeEntry( "Mode", mode );

    grp.writeEntry( "Manual Bitrate Settings", m_radioManual->isChecked() );

    grp.writeEntry( "VBR", !m_manualSettingsDialog->m_radioConstantBitrate->isChecked() );
    grp.writeEntry( "Constant Bitrate", m_manualSettingsDialog->m_comboConstantBitrate->currentText().left(3).toInt() );
    grp.writeEntry( "Maximum Bitrate", m_manualSettingsDialog->m_comboMaximumBitrate->currentText().left(3).toInt() );
    grp.writeEntry( "Minimum Bitrate", m_manualSettingsDialog->m_comboMinimumBitrate->currentText().left(3).toInt() );
    grp.writeEntry( "Average Bitrate", m_manualSettingsDialog->m_spinAverageBitrate->value() );

    grp.writeEntry( "Use Maximum Bitrate", m_manualSettingsDialog->m_checkBitrateMaximum->isChecked() );
    grp.writeEntry( "Use Minimum Bitrate", m_manualSettingsDialog->m_checkBitrateMinimum->isChecked() );
    grp.writeEntry( "Use Average Bitrate", m_manualSettingsDialog->m_checkBitrateAverage->isChecked() );

    grp.writeEntry( "Quality Level", m_sliderQuality->value() );

    grp.writeEntry( "Copyright", m_checkCopyright->isChecked() );
    grp.writeEntry( "Original", m_checkOriginal->isChecked() );
    grp.writeEntry( "ISO compliance", m_checkISO->isChecked() );
    grp.writeEntry( "Error Protection", m_checkError->isChecked() );

    // default to 2 which is the same as the -h lame option
    grp.writeEntry( "Encoder Quality", m_spinEncoderQuality->value() );
}



QStringList K3bLameEncoder::extensions() const
{
    return QStringList( "mp3" );
}


QString K3bLameEncoder::fileTypeComment( const QString& ) const
{
    return "MPEG1 Layer III (mp3)";
}


long long K3bLameEncoder::fileSize( const QString&, const K3b::Msf& msf ) const
{
    KConfig* c = k3bcore->config();
    KConfigGroup grp(c, "K3bLameEncoderPlugin" );
    int bitrate = 0;
    if( grp.readEntry( "Manual Bitrate Settings", false ) ) {
        if( grp.readEntry( "VBR", false ) ) {
            if( grp.readEntry( "Use Maximum Bitrate", false ) )
                bitrate = grp.readEntry( "Maximum Bitrate", 224 );
            if( grp.readEntry( "Use Minimum Bitrate", false ) )
                bitrate = ( bitrate > 0
                            ? (bitrate - grp.readEntry( "Minimum Bitrate", 32 )) / 2
                            : grp.readEntry( "Minimum Bitrate", 32 ) );
            if( grp.readEntry( "Use Average Bitrate", true ) )
                bitrate = grp.readEntry( "Average Bitrate", 128 );
        }
        else {
            bitrate = grp.readEntry( "Constant Bitrate", 128 );
        }
    }
    else {
        int q = grp.readEntry( "Quality Level", 5 );
        if( q < 0 ) q = 0;
        if( q > 9 ) q = 9;
        bitrate = s_lame_preset_approx_bitrates[q];
    }

    return (msf.totalFrames()/75 * bitrate * 1000)/8;
}


K3bPluginConfigWidget* K3bLameEncoder::createConfigWidget( QWidget* parent ) const
{
    return new K3bLameEncoderSettingsWidget( parent );
}


#include "k3blameencoder.moc"
