/***************************************************************************
                          k3bcdview.h  -  description
                             -------------------
    begin                : Sun Oct 28 2001
    copyright            : (C) 2001 by Sebastian Trueg
    email                : trueg@informatik.uni-freiburg.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef K3BCDVIEW_H
#define K3BCDVIEW_H

#define DEFAULT_ARTIST   "artist"
#define DEFAULT_ALBUM    "album"
#define DEFAULT_TITLE      "title"

#include "../k3bcdcontentsview.h"

#include <qmemarray.h>
#include <qstringlist.h>

//extern "C" {
//#include <cdda_interface.h>
//}

class QString;
//struct cdrom_drive;


struct cdrom_drive;
class K3bCddb;
class K3bCdda;
class K3bCddaCopy;
class K3bPatternParser;
class KListView;
class QListViewItem;
class QPoint;
class KActionCollection;
class K3bDevice;


/**
  *@author Sebastian Trueg
  */
class K3bCdView : public K3bCdContentsView
{
  Q_OBJECT

 public:
  K3bCdView(QWidget *, const char *);
  ~K3bCdView();

  void showCdContent();
  void setFilePatternList(QStringList p){ m_filePatternList = p; };
  void setDirPatternList(QStringList p){ m_dirPatternList = p; };

 public slots:
   /** */
  void showCdView( K3bDevice* dev );
  void reload();

 signals:
  void notSupportedDisc( const QString& );

 private:
  struct cdrom_drive *m_drive;
  K3bCddb *m_cddb;
  K3bCdda *m_cdda;
  KListView *m_listView;
  QListViewItem *m_testItemPattern;
  QString m_device;
  QString m_album;
  QStringList m_titles;
  QMemArray<long> m_size;
  K3bPatternParser *m_parser;
  bool m_initialized;
  //bool m_useFilePattern;
  //bool m_useDirectoryPattern;
  bool m_usePattern;
  QStringList m_filePatternList;
  QStringList m_dirPatternList;

  KActionCollection* m_actionCollection;

  void addItem( int, const QString&, const QString&, const QString&, long, const QString& );
  //QString prepareFilename(QString);
  void setupGUI();
  void applyOptions();
  void checkTitlesOnCd( );
  //void askForView();
  int checkCDType( const QStringList& titles);
  void readSettings();
  //QString prepareDirectory( QListViewItem *item );
  //QString getRealDirectory(int, QListViewItem* );

 private slots:
  // reads title from cddb after cddb is ready, user select of mulit entries
  void slotCheckView();
  /** No descriptions */
  // Toolbar Button actions
  void prepareRipping();
  void changeSelectionMode();
  void play();
  void stop();
  // PopupMenu actions
  void slotMenuActivated(QListViewItem*, const QPoint &, int);
  void slotMenuItemActivated(int itemId);

};


#endif
