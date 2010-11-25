//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: confmport.h,v 1.3 2004/01/25 11:20:31 wschweer Exp $
//
//  (C) Copyright 2000 Werner Schweer (ws@seh.de)
//=========================================================

#ifndef __CONFMPORT_H__
#define __CONFMPORT_H__

#include <QWidget>
#include <Q3WhatsThis>
#include <QToolTip>
//Added by qt3to4:
//#include <Q3PopupMenu>

#include "ui_synthconfigbase.h"

class QTreeWidget;
class QTableWidget;
class QPoint;
//class Q3PopupMenu;
class QMenu;
class Q3Header;
class Xml;

//----------------------------------------------------------
//   MPHeaderTip
//----------------------------------------------------------

class MPHeaderTip { // : public QToolTip { ddskrjo

   public:
    MPHeaderTip(QWidget *) {} // : QToolTip(parent) {} ddskrjo
      virtual ~MPHeaderTip() {}
   protected:
      void maybeTip(const QPoint &);
      };

//---------------------------------------------------------
//   MPWhatsThis
//---------------------------------------------------------

class MPWhatsThis : public Q3WhatsThis {
      Q3Header* header;

   protected:
      QString text(const QPoint&);

   public:
      MPWhatsThis(QWidget* parent, Q3Header* h) : Q3WhatsThis(parent) {
            header = h;
            }
      };

//---------------------------------------------------------
//   MPConfig
//    Midi Port Config
//---------------------------------------------------------

class MPConfig : public QDialog, Ui::SynthConfigBase {
      MPHeaderTip* _mptooltip;
      //Q3PopupMenu* popup;
      QMenu* instrPopup;
      
      int _showAliases; // -1: None. 0: First aliases. 1: Second aliases etc.
      
      Q_OBJECT

   private slots:
      void rbClicked(QTableWidgetItem*);
      void mdevViewItemRenamed(QTableWidgetItem*);
      void songChanged(int);
      void selectionChanged();
      void addInstanceClicked();
      void removeInstanceClicked();

   public:
      MPConfig(QWidget* parent=0);
      ~MPConfig();
      };

#endif
