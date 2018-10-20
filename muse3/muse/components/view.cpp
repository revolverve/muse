//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: view.cpp,v 1.3.2.2 2009/04/06 01:24:55 terminator356 Exp $
//  (C) Copyright 1999 Werner Schweer (ws@seh.de)
//  Additions, modifications (C) Copyright 2011 Tim E. Real (terminator356 on users DOT sourceforge DOT net)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//=========================================================

#include "view.h"
#include "gconfig.h"
#include <cmath>
#include <stdio.h>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
// REMOVE Tim. citem. Added.
#include <QRegion>
#include "tempo.h"

#include "math.h"

#include "al/sig.h"  

// Don't use this, it was just for debugging. 
// It's much slower than muse-1 no matter how hard I tried.
// The left/right pixmap shifters in seXPos setYPos 
//  just ate up all the time no matter what I tried.
//#defines VIEW_USE_DOUBLE_BUFFERING 1

namespace MusEGui {

//---------------------------------------------------------
//   View::View
//    double xMag = (xmag < 0) ? 1.0/-xmag : double(xmag)
//---------------------------------------------------------

View::View(QWidget* w, int xm, int ym, const char* name)
   : QWidget(w)
      {
      setAttribute(Qt::WA_NoSystemBackground);
      setAttribute(Qt::WA_StaticContents);
      // This is absolutely required for speed! Otherwise painfully slow because we get 
      //  full rect paint events even on small scrolls! See help on QPainter::scroll().
      setAttribute(Qt::WA_OpaquePaintEvent);
          
      setObjectName(QString(name));
      xmag  = xm;
      ymag  = ym;
      xpos  = 0;
      ypos  = 0;
      xorg  = 0;
      yorg  = 0;
      _virt = true;
      setBackgroundRole(QPalette::NoRole);
      brush.setStyle(Qt::SolidPattern);
      brush.setColor(Qt::lightGray);
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      pmValid = false;
      #endif
      }

//---------------------------------------------------------
//   setOrigin
//---------------------------------------------------------

void View::setOrigin(int x, int y)
      {
      xorg = x;
      yorg = y;
      redraw();
      }

//---------------------------------------------------------
//   setXMag
//---------------------------------------------------------

void View::setXMag(int xs)
      {
      xmag = xs;
      redraw();
      }

//---------------------------------------------------------
//   seqYMag
//---------------------------------------------------------

void View::setYMag(int ys)
      {
      ymag = ys;
      redraw();
      }

//---------------------------------------------------------
//   setXPos
//    x - phys offset
//---------------------------------------------------------

void View::setXPos(int x)
      {
      int delta  = xpos - x;         // -  -> shift left
      
      // REMOVE Tim. citem. Added.
//       fprintf(stderr, "View::setXPos paint delta:%d x:%d old xpos:%d\n", delta, x, xpos);  
      
      xpos  = x;
      
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      if (pm.isNull())
            return;
      if (!pmValid) {
            //printf("View::setXPos !pmValid x:%d width:%d delta:%d\n", x, width(), delta);
            redraw();
            return;
            }
            
      int w = width();
      int h = height();

      QRect r;
      if (delta >= w || delta <= -w)
            r = QRect(0, 0, w, h);
      else if (delta < 0) {   // shift left
            //bitBlt(&pm,  0, 0, &pm,  -delta, 0, w + delta, h, CopyROP, true);
            QPainter p(&pm);
            p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::HighQualityAntialiasing, false);
            //printf("View::setXPos x:%d w:%d delta:%d r.x:%d r.w:%d\n", 
            //  x, w, delta, r.x(), r.width());                    
            p.drawPixmap(0, 0, pm, -delta, 0, w + delta, h);
            r = QRect(w + delta, 0, -delta, h);
            }
      else {                  // shift right
            //bitBlt(&pm,  delta, 0, &pm,     0, 0, w-delta, h, CopyROP, true);
            QPainter p(&pm);
            p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::HighQualityAntialiasing, false);
            //printf("View::setXPos x:%d w:%d delta:%d r.x:%d r.w:%d\n", 
            //  x, w, delta, r.x(), r.width());                    
            p.drawPixmap(delta, 0, pm,     0, 0, w-delta, h);
            r = QRect(0, 0, delta, h);
            }
      QRect olr = overlayRect();
      QRect olr1(olr);
      olr1.translate(delta, 0);

      r |= olr;
      r |= olr1;
      
      //printf("View::setXPos x:%d w:%d delta:%d r.x:%d r.w:%d\n", x, w, delta, r.x(), r.width());
      //printf("View::setXPos paint delta:%d r.x:%d r.y:%d r.w:%d r.h:%d\n", delta, r.x(), r.y(), r.width(), r.height());  
      
      paint(r);
      update();
      
      #else
      scroll(delta, 0);
      QRect olr = overlayRect();
      // Is there an overlay?
      if(!olr.isNull())
      {
        // Are we shifting right (moving left)?
        if(delta >= 0)
        {
          // Translate not good - need to set x to delta.
          //olr.translate(delta, 0);
          olr.setX(delta);
          olr.setWidth(olr.x() + olr.width() + delta);
        }
        else
        // We are shifting left (moving right).
        {
          // Translate not good - need to limit x to 0.
          //olr.translate(delta, 0);
          olr.setX(olr.x() + delta);
        }
        
        if(olr.x() < 0)
          olr.setX(0);
        if(olr.right() > width())
          olr.setRight(width());
        
        if(olr.y() < 0)
          olr.setY(0);
        if(olr.bottom() > height())
          olr.setBottom(height());
        
        //printf("scroll X update: x:%d y:%d w:%d h:%d\n", olr.x(), olr.y(), olr.width(), olr.height()); 
        update(olr);
      }  
      #endif
      }

//---------------------------------------------------------
//   setYPos
//---------------------------------------------------------

void View::setYPos(int y)
      {
      int delta  = ypos - y;         // -  -> shift up
      ypos  = y;
      
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      if (pm.isNull())
            return;
      if (!pmValid) {
            //printf("View::setYPos !pmValid y:%d height:%d delta:%d\n", y, height(), delta);
            
            redraw();
            return;
            }
      
      int w = width();
      int h = height();
      
      QRect r;
      if (delta >= h || delta <= -h)
            r = QRect(0, 0, w, h);
      else if (delta < 0) {   // shift up
            //bitBlt(&pm,  0, 0, &pm, 0, -delta, w, h + delta, CopyROP, true);
            QPainter p(&pm);
            p.drawPixmap(0, 0, pm, 0, -delta, w, h + delta);
            r = QRect(0, h + delta, w, -delta);
            }
      else {                  // shift down
            //bitBlt(&pm,  0, delta, &pm, 0, 0, w, h-delta, CopyROP, true);
            QPainter p(&pm);
            p.drawPixmap(0, delta, pm, 0, 0, w, h-delta);
            r = QRect(0, 0, w, delta);
            }
      QRect olr = overlayRect();
      QRect olr1(olr);
      olr1.translate(0, delta);

      r |= olr;
      r |= olr1;

      //printf("View::setYPos paint delta:%d r.x:%d r.y:%d r.w:%d r.h:%d\n", delta, r.x(), r.y(), r.width(), r.height());  
      
      paint(r);
      update();
      
      #else
      scroll(0, delta);
      QRect olr = overlayRect();
      // Is there an overlay?
      if(!olr.isNull())
      {
        // Are we shifting down (moving up)?
        if(delta >= 0)
        {
          // Translate not good - need to set y to delta.
          //olr.translate(0, delta);
          olr.setY(delta);
          olr.setHeight(olr.y() + olr.height() + delta);
        }
        else
        // We are shifting up (moving down).
        {
          // Translate not good - need to limit y to 0.
          //olr.translate(0, delta);
          olr.setY(olr.y() + delta);
        }
        
        if(olr.x() < 0)
          olr.setX(0);
        if(olr.right() > width())
          olr.setRight(width());
        
        if(olr.y() < 0)
          olr.setY(0);
        if(olr.bottom() > height())
          olr.setBottom(height());
        
        //printf("scroll Y update: x:%d y:%d w:%d h:%d\n", olr.x(), olr.y(), olr.width(), olr.height()); 
        update(olr);
      }  
      #endif
      }

//---------------------------------------------------------
//   resizeEvent
//---------------------------------------------------------

void View::resizeEvent(QResizeEvent* ev)
      {
      QWidget::resizeEvent(ev);  
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      //pm.resize(ev->size());
      //printf("View::resizeEvent width:%d height:%d\n", 
      //  ev->size().width(), ev->size().height());  
      
      if(pm.isNull())
      {
        //printf("View::resizeEvent pixmap is null\n"); 
        pm = QPixmap(ev->size().width(), ev->size().height());
      }  
      else  
        pm = pm.copy(QRect(QPoint(0, 0), ev->size()));
      pmValid = false;
      #endif
      }

//---------------------------------------------------------
//   paintEvent
//---------------------------------------------------------

void View::paintEvent(QPaintEvent* ev)
      {
      //printf("View::paintEvent x:%d width:%d y:%d height:%d\n", 
      //  ev->rect().x(), ev->rect().width(), ev->rect().y(), ev->rect().height());  
        
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      if (!pmValid)
// REMOVE Tim. citem. Changed.
//             paint(ev->rect());
            paint(ev->rect(), ev->region());
      
      //bitBlt(this, ev->rect().topLeft(), &pm, ev->rect(), CopyROP, true);
      QPainter p(this);
      //p.setCompositionMode(QPainter::CompositionMode_Source);
      p.drawPixmap(ev->rect().topLeft(), pm, ev->rect());
      
      #else
// REMOVE Tim. citem. Changed.
//       paint(ev->rect());
      paint(ev->rect(), ev->region());
      #endif
      }

//---------------------------------------------------------
//   redraw
//---------------------------------------------------------

void View::redraw()
      {
      //printf("View::redraw()\n");  
      
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      QRect r(0, 0, pm.width(), pm.height());
      //printf("View::redraw() r.x:%d r.w:%d\n", r.x(), r.width()); 
      paint(r);
      #endif
      
      update();
      }

//---------------------------------------------------------
//   redraw
//---------------------------------------------------------

void View::redraw(const QRect& r)
      {
      //printf("View::redraw(QRect& r) r.x:%d r.w:%d\n", r.x(), r.width());  
      
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      paint(r);
      #endif
      
      update(r);
      }

// REMOVE Tim. citem. Added.
//---------------------------------------------------------
//   redraw
//---------------------------------------------------------

void View::redraw(const QRegion& r)
      {
      //printf("View::redraw(QRect& r) r.x:%d r.w:%d\n", r.x(), r.width());  
      
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      paint(r);
      #endif
      
      update(r);
      }

//---------------------------------------------------------
//   paint
//    r - phys coord system
//---------------------------------------------------------

// REMOVE Tim. citem. Changed.
// void View::paint(const QRect& r)
void View::paint(const QRect& r, const QRegion& rg)
      {
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      if (pm.isNull())
            return;
      #endif
      
      QRect rr(r);
      
      //printf("View::paint x:%d width:%d y:%d height:%d\n", r.x(), r.width(), r.y(), r.height());   
      
      #ifdef VIEW_USE_DOUBLE_BUFFERING
      if (!pmValid) {
            pmValid = true;
            rr = QRect(0, 0, pm.width(), pm.height());
            }
      
      QPainter p(&pm);
      #else
      QPainter p(this);
      #endif

      // REMOVE Tim. citem. Added. For testing.
//       const int rg_sz = rg.rectCount();
//       int rg_r_cnt = 0;
//       fprintf(stderr, "View::paint: virt:%d rect: x:%d y:%d w:%d h:%d region rect count:%d\n",
//               virt(), r.x(), r.y(), r.width(), r.height(), rg_sz);
//       for(QRegion::const_iterator i = rg.begin(); i != rg.end(); ++i, ++rg_r_cnt)
//       {
//         const QRect& rg_r = *i;
//         fprintf(stderr, "  #%d: x:%d y:%d w:%d h:%d\n", rg_r_cnt, rg_r.x(), rg_r.y(), rg_r.width(), rg_r.height());
//       }
      
      p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::HighQualityAntialiasing, false);
      
      if (bgPixmap.isNull())
            p.fillRect(rr, brush);
      else
            p.drawTiledPixmap(rr, bgPixmap, QPoint(xpos + rmapx(xorg)
               + rr.x(), ypos + rmapy(yorg) + rr.y()));
      
// REMOVE Tim. citem. Changed.
//       p.setClipRegion(rr);
// // Do not set a clip region. Certain drawing routines need to draw outside the update rectangle,
// //  either for efficiency or by design (WaveCanvas selections need full top-to-bottom drawing).
      p.setClipRegion(rg);

      //printf("View::paint r.x:%d w:%d\n", rr.x(), rr.width());
// REMOVE Tim. citem. Changed.
//       pdraw(p, rr);       // draw into pixmap
      pdraw(p, rr, rg);       // draw into pixmap

      p.resetMatrix();      // Q3 support says use resetMatrix instead, but resetMatrix advises resetTransform instead...
      //p.resetTransform();
      
// REMOVE Tim. citem. Changed.
//       drawOverlay(p);
      drawOverlay(p, r, rg);
      }

//---------------------------------------------------------
//   keyPressEvent
//---------------------------------------------------------

void View::keyPressEvent(QKeyEvent* event)
      {
      viewKeyPressEvent(event);
      }

//---------------------------------------------------------
//   keyReleaseEvent
//---------------------------------------------------------

void View::keyReleaseEvent(QKeyEvent* event)
      {
      viewKeyReleaseEvent(event);
      }

//---------------------------------------------------------
//   viewKeyPressEvent
//---------------------------------------------------------

void View::viewKeyPressEvent(QKeyEvent* event)
      {
      event->ignore();
      }

//---------------------------------------------------------
//   viewKeyReleaseEvent
//---------------------------------------------------------

void View::viewKeyReleaseEvent(QKeyEvent* event)
      {
      event->ignore();
      }

//---------------------------------------------------------
//   mousePressEvent
//---------------------------------------------------------

void View::mousePressEvent(QMouseEvent* ev)
      {
      QMouseEvent e(ev->type(), mapDev(ev->pos()),
         ev->globalPos(), ev->button(), ev->buttons(), ev->modifiers());
      viewMousePressEvent(&e);
      }

//---------------------------------------------------------
//   mouseDoubleClickEvent
//---------------------------------------------------------

void View::mouseDoubleClickEvent(QMouseEvent* ev)
      {
      QMouseEvent e(ev->type(), mapDev(ev->pos()),
         ev->globalPos(), ev->button(), ev->buttons(), ev->modifiers());
      viewMouseDoubleClickEvent(&e);
      }

//---------------------------------------------------------
//   mouseMoveEvent
//---------------------------------------------------------

void View::mouseMoveEvent(QMouseEvent* ev)
      {
      QMouseEvent e(ev->type(), mapDev(ev->pos()),
         ev->globalPos(), ev->button(), ev->buttons(), ev->modifiers());
      viewMouseMoveEvent(&e);
      }

//---------------------------------------------------------
//   mouseReleaseEvent
//---------------------------------------------------------

void View::mouseReleaseEvent(QMouseEvent* ev)
      {
      QMouseEvent e(ev->type(), mapDev(ev->pos()),
         ev->globalPos(), ev->button(), ev->buttons(), ev->modifiers());
      viewMouseReleaseEvent(&e);
      }

//---------------------------------------------------------
//   dropEvent
//---------------------------------------------------------

void View::dropEvent(QDropEvent* ev)
      {
      // From Q3 support:
      // "Sets the drop to happen at the given point. You do not normally need to use this 
      //  as it will be set internally before your widget receives the drop event."     
      // But we need to remap it here...
      //ev->setPoint(mapDev(ev->pos())); 
      QDropEvent nev(mapDev(ev->pos()), ev->possibleActions(), ev->mimeData(), ev->mouseButtons(), ev->keyboardModifiers(), ev->type());     
      //viewDropEvent(ev);
      viewDropEvent(&nev);
      }

//---------------------------------------------------------
//   setBg
//---------------------------------------------------------

void View::setBg(const QPixmap& bgpm)
      {
      bgPixmap = bgpm;
      redraw();
      }

// REMOVE Tim. citem. Changed.
// //---------------------------------------------------------
// //   devToVirt
// //---------------------------------------------------------
// 
// QRect View::devToVirt(const QRect& r) const
// {
//   int x = r.x();
//   int y = r.y();
//   int w = r.width();
//   int h = r.height();
//   x = mapxDev(x);
//   y = mapyDev(y);
//   if (xmag <= 0) {
//         // TODO These adjustments are required, otherwise gaps. Tried, unable to remove them for now.  p4.0.30
// // REMOVE Tim. citem. Removed.
// //         x -= 1;   
// //         w += 2;
//         //x = (x + xpos + rmapx(xorg)) * (-xmag);
// //         x = lrint((double(x + xpos) + rmapx_f(xorg)) * double(-xmag));
//         //x = lrint((double(x + xpos) + double(xorg) / double(-xmag)) * double(-xmag));
//         //x = (x + xpos) * -xmag + xorg;
//         //x = mapxDev(x);
//     
//         w = w * (-xmag);
//         }
//   else {
//         //x = (x + xpos + rmapx(xorg)) / xmag;
// //         x = lrint((double(x + xpos) + rmapx_f(xorg)) / double(xmag));
//         //x = lrint((double(x + xpos) + double(xorg) * double(xmag)) / double(xmag));
//         //x = double(x + xpos) / double(xmag) + xorg;
//         //x = mapxDev(x);
//         
//         //w = (w + xmag - 1) / xmag;
//         w = lrint(double(w) / double(xmag));
// // REMOVE Tim. citem. Removed.
// //         x -= 1;
// //         w += 2;
//         }
//   if (ymag <= 0) {
// // REMOVE Tim. citem. Removed.
// //         y -= 1;
// //         h += 2;
//         //y = (y + ypos + rmapy(yorg)) * (-ymag);
// //         y = lrint((double(y + ypos) + rmapy_f(yorg)) * double(-ymag));
//         //y = lrint((double(y + ypos) + (double(yorg) / double(-ymag))) * double(-ymag));
//         //y = (y + ypos) * -ymag + yorg;
//         //y = mapyDev(y);
// 
//         h = h * (-ymag);
//         }
//   else {
//         //y = (y + ypos + rmapy(yorg)) / ymag;
// //         y = lrint((double(y + ypos) + rmapy_f(yorg)) / double(ymag));
//         //y = lrint((double(y + ypos) + (double(yorg) * double(ymag))) / double(ymag));
//         //y = double(y + ypos) / double(ymag) + double(yorg);
//         //y = mapyDev(y);
//         
//         //h = (h + ymag - 1) / ymag;
//         h = lrint(double(h) / double(ymag));
// // REMOVE Tim. citem. Removed.
// //         y -= 1;
// //         h += 2;
//         }
// 
//   if (x < 0)
//         x = 0;
//   if (y < 0)
//         y = 0;
//   
//   return QRect(x, y, w, h);
// }

// REMOVE Tim. citem. Changed.
//---------------------------------------------------------
//   devToVirt
//---------------------------------------------------------

// void View::devToVirt(const QRegion& rg_in, QRegion& rg_out) const
// {
//   for(QRegion::const_iterator i = rg_in.begin(); i != rg_in.end(); ++i)
//     rg_out += devToVirt(*i);
// }

//---------------------------------------------------------
//   devToVirt
//---------------------------------------------------------

QRect View::devToVirt(const QRect& r) const
{
  return mapDev(r);
}

void View::devToVirt(const QRegion& rg_in, QRegion& rg_out) const
{
  mapDev(rg_in, rg_out);
}

//---------------------------------------------------------
//   pdraw
//    r - phys coords
//---------------------------------------------------------

// REMOVE Tim. citem. Changed.
// void View::pdraw(QPainter& p, const QRect& r)
void View::pdraw(QPainter& p, const QRect& r, const QRegion& rg)
      {
      //printf("View::pdraw virt:%d x:%d width:%d y:%d height:%d\n", virt(), r.x(), r.width(), r.y(), r.height());  
      
      if (virt()) {
            setPainter(p);
// REMOVE Tim. citem. Changed.
//             int x = r.x();
//             int y = r.y();
//             int w = r.width();
//             int h = r.height();
//             if (xmag <= 0) {
//                   // TODO These adjustments are required, otherwise gaps. Tried, unable to remove them for now.  p4.0.30
//                   x -= 1;   
//                   w += 2;
//                   //x = (x + xpos + rmapx(xorg)) * (-xmag);
//                   x = lrint((double(x + xpos) + rmapx_f(xorg)) * double(-xmag));
//                   w = w * (-xmag);
//                   }
//             else {
//                   //x = (x + xpos + rmapx(xorg)) / xmag;
//                   x = lrint((double(x + xpos) + rmapx_f(xorg)) / double(xmag));
//                   //w = (w + xmag - 1) / xmag;
//                   w = lrint(double(w) / double(xmag));
//                   x -= 1;
//                   w += 2;
//                   }
//             if (ymag <= 0) {
//                   y -= 1;
//                   h += 2;
//                   //y = (y + ypos + rmapy(yorg)) * (-ymag);
//                   y = lrint((double(y + ypos) + rmapy_f(yorg)) * double(-ymag));
//                   h = h * (-ymag);
//                   }
//             else {
//                   //y = (y + ypos + rmapy(yorg)) / ymag;
//                   y = lrint((double(y + ypos) + rmapy_f(yorg)) / double(ymag));
//                   //h = (h + ymag - 1) / ymag;
//                   h = lrint(double(h) / double(ymag));
//                   y -= 1;
//                   h += 2;
//                   }
// 
//             if (x < 0)
//                   x = 0;
//             if (y < 0)
//                   y = 0;
            
// REMOVE Tim. citem. Changed.
//             QRect v_r = devToVirt(r);
//             
// // REMOVE Tim. citem. Changed. TODO Needs TESTING
// //             draw(p, QRect(x, y, w, h));
//             QRegion v_rg;
//             for(QRegion::const_iterator i = rg.begin(); i != rg.end(); ++i)
//               v_rg += devToVirt(*i);
//             draw(p, v_r, v_rg);
//             
            draw(p, r, rg);
            }
      else
// REMOVE Tim. citem. Changed.
//             draw(p, r);
            draw(p, r, rg);
      }

//---------------------------------------------------------
//   setPainter
//---------------------------------------------------------

void View::setPainter(QPainter& p)
      {
      p.resetMatrix();      // Q3 support says use resetMatrix instead, but resetMatrix advises resetTransform instead...
      //p.resetTransform();
      
      //p.translate(double(-(xpos+rmapx(xorg))), double(-(ypos+rmapy(yorg))));
      p.translate( -(double(xpos) + rmapx_f(xorg)) , -(double(ypos) + rmapy(yorg)));
      //double xMag = (xmag < 0) ? 1.0/(-xmag) : double(xmag);
      //double yMag = (ymag < 0) ? 1.0/(-ymag) : double(ymag);
      double xMag = (xmag < 0) ? 1.0/double(-xmag) : double(xmag);
      double yMag = (ymag < 0) ? 1.0/double(-ymag) : double(ymag);
      p.scale(xMag, yMag);
      }

// REMOVE Tim. citem. Changed.
// //---------------------------------------------------------
// //   drawTickRaster
// //---------------------------------------------------------
// 
// void View::drawTickRaster(QPainter& p, int x, int y, int w, int h, int raster)
//       {
//       // Changed to draw in device coordinate space instead of virtual, transformed space.     Tim. p4.0.30  
//       
//       //int mx = mapx(x);
//       int my = mapy(y);
//       //int mw = mapx(x + w) - mx;
//       //int mw = mapx(x + w) - mx - 1;
//       //int mh = mapy(y + h) - my;
//       //int mh = mapy(y + h) - my - 1;
//       
//       //p.save();
//       bool wmtxen = p.worldMatrixEnabled();
//       p.setWorldMatrixEnabled(false);
//       
//       int xx,bar1, bar2, beat;
//       int rast_mapx;
//       bool draw_bar;
//       unsigned tick;
//       AL::sigmap.tickValues(x, &bar1, &beat, &tick);
//       AL::sigmap.tickValues(x+w, &bar2, &beat, &tick);
//       ++bar2;
//       ///int y2 = y + h;
//       //int y2 = my + mh;
//       int y2 = mapy(y + h) - 1;
//       //printf("View::drawTickRaster x:%d y:%d w:%d h:%d mx:%d my:%d mw:%d mh:%d y2:%d bar1:%d bar2:%d\n", x, y, w, h, mx, my, mw, mh, y2, bar1, bar2);  
//       //printf("View::drawTickRaster x:%d y:%d w:%d h:%d my:%d mh:%d y2:%d bar1:%d bar2:%d\n", x, y, w, h, my, mh, y2, bar1, bar2);  
//       for (int bar = bar1; bar < bar2; ++bar) {
//             int qq = raster;
//         
//             rast_mapx = rmapx(raster);
//             // grid too dense?
// //             if (rast_mapx <= 1)        
// //                   qq *= 16;
// //             else if (rast_mapx <= 2)
// //                   qq *= 8;
// //             else if (rast_mapx <= 4)
// //                   qq *= 4;
// //             else if (rast_mapx <= 8)
// //                   qq *= 2;
//             draw_bar = true;
//             if (rast_mapx < 64) {
//                   // donï¿½t show beats if measure is this small
//                   int n = 1;
//                   if (rast_mapx < 32)
//                         n = 2;
//                   if (rast_mapx <= 16)
//                         n = 4;
//                   if (rast_mapx < 8)
//                         n = 8;
//                   if (rast_mapx <= 4)
//                         n = 16;
//                   if (rast_mapx <= 2)
//                         n = 32;
//                   if (bar % n)
// //                         continue;
//                         draw_bar = false;
//             }
//             
//             ///unsigned x = AL::sigmap.bar2tick(bar, 0, 0);
//             unsigned xb = AL::sigmap.bar2tick(bar, 0, 0);
//             if(draw_bar)
//             {
//               int xt = mapx(xb);
//               p.setPen(MusEGlobal::config.midiCanvasBarColor);
//               ///p.drawLine(x, y, x, y2);
//               p.drawLine(xt, my, xt, y2);
//             }
//             int z, n;
//             ///AL::sigmap.timesig(x, z, n);
//             AL::sigmap.timesig(xb, z, n);
//             ///int q = p.xForm(QPoint(raster, 0)).x() - p.xForm(QPoint(0, 0)).x();
//             ///int q = p.combinedTransform().map(QPoint(raster, 0)).x() - p.combinedTransform().map(QPoint(0, 0)).x();
//             //int q = rmapx(raster);
// //             int qq = raster;
//             //if (q < 8)        // grid too dense
//             
// // REMOVE Tim. citem. Changed.
// //             if (rmapx(raster) < 8)        // grid too dense
// //                   qq *= 2;
// 
//             p.setPen(MusEGlobal::config.midiCanvasBeatColor);
//             if (raster>=4) {
//                         ///int xx = x + qq;
//                         //int xx = mapx(xb + qq);
//                         xx = xb + qq;
//                         int xxx = AL::sigmap.bar2tick(bar, z, 0);
//                         //int xxx = mapx(AL::sigmap.bar2tick(bar, z, 0));
//                         while (xx <= xxx) {
//                                ///p.drawLine(xx, y, xx, y2);
//                                int x = mapx(xx);
//                                p.drawLine(x, my, x, y2);
//                                xx += qq;
//                                //xx += rmapx(qq);
//                                }
//                         //xx = xxx;
//                         }
// 
//             p.setPen(Qt::darkGray);
//             for (int beat = 1; beat < z; beat++) {
//                         ///int xx = AL::sigmap.bar2tick(bar, beat, 0);
//                         xx = mapx(AL::sigmap.bar2tick(bar, beat, 0));
//                         //printf(" bar:%d z:%d beat:%d xx:%d\n", bar, z, beat, xx);  
//                         ///p.drawLine(xx, y, xx, y2);
//                         p.drawLine(xx, my, xx, y2);
//                         }
// 
//             }
//       //p.setWorldMatrixEnabled(true);
//       p.setWorldMatrixEnabled(wmtxen);
//       //p.restore();      
//       }

// void View::drawBarText(QPainter& p, int tick, int bar, const QRect& vr, const QColor& textColor, const QFont& font) const
// {
//   const int vw1000 = rmapxDev(1000);
//   const int vw2 = rmapxDev(2);
//   const int vy = vr.y();
//   const int vh_m1 = vr.height() - rmapyDev(1);
//   
//   QPen pen;
//   pen.setCosmetic(true);
//   pen.setColor(textColor);
//   p.setPen(pen);
//   
//   QString s;
//   s.setNum(bar + 1);
//   
//   p.setFont(font);
//   int vbrw = rmapxDev(p.fontMetrics().boundingRect(s).width());
//   //const int brh = rmapyDev(br.width());
//   //int w_txt = rmapxDev(1000);
//   //const int h_txt = rmapyDev(height() - 1);
//   if(vbrw > vw1000)
//         vbrw = vw1000;
//   //if(brh < h_txt)
//   //  h_txt = brh;
//   const QRect vbr_txt = QRect(tick + vw2, vy, vbrw, vh_m1);
//   
// //   // REMOVE Tim. citem. Added.
// //   fprintf(stderr,
// //       "drawBarText: Bar text: bar:%d vx:%d vy:%d"
// //       " vw:%d vh:%d tick:%d br x:%d y:%d w:%d h:%d\n",
// //       bar, vr.x(), vr.y(), vr.width(), vr.height(), tick,
// //       br_txt.x(), br_txt.y(), br_txt.width(), br_txt.height());
//   
//   //if(r_txt.intersects(mr))
//   if(vbr_txt.intersects(vr))
//   {
// //     // REMOVE Tim. citem. Added.
// //     fprintf(stderr, "...bar text within range. xorg:%d xmag:%d xpos:%d Drawing bar text at x:%d y:%d w:%d h:%d\n",
// //             xorg, xmag, xpos, map(br_txt).x(), map(br_txt).y(), map(br_txt).width(), map(br_txt).height());
//     
//     //p.drawText(r_txt, Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
//     p.drawText(map(vbr_txt), Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
//   }
// }

// void View::drawBarText(QPainter& p, int tick, int bar, const QRect& mr, const QColor& textColor, const QFont& font) const
// {
//   QPen pen;
//   pen.setCosmetic(true);
//   pen.setColor(textColor);
//   p.setPen(pen);
//   p.setFont(font);
//   QString s;
//   s.setNum(bar + 1);
//     
//   if(xmag <= 0)
//   {
//     const QRect vr = mapDev(mr);
//     const int vw1000 = rmapxDev(1000);
//     const int vw2 = rmapxDev(2);
//     
//     if(ymag <= 0)
//     {
//       const int vy = vr.y();
//       //const int vh_m1 = vr.height() - rmapyDev(1);
//       const int vh_m1 = rmapyDev(mr.height() - 1);
//     }
//     else
//     {
//       const int my = mr.y();
//       //const int vh_m1 = vr.height() - rmapyDev(1);
//       const int mh_m1 = mr.height() - 1;
//     }
//     
//     int vbrw = rmapxDev(p.fontMetrics().boundingRect(s).width());
//     
//     //const int brh = rmapyDev(br.width());
//     //int w_txt = rmapxDev(1000);
//     //const int h_txt = rmapyDev(height() - 1);
//     
//     if(vbrw > vw1000)
//       vbrw = vw1000;
// 
//     //if(brh < h_txt)
//     //  h_txt = brh;
// 
//     const QRect vbr_txt = QRect(tick + vw2, vy, vbrw, vh_m1);
//     
//   //   // REMOVE Tim. citem. Added.
//   //   fprintf(stderr,
//   //       "drawBarText: Bar text: bar:%d vx:%d vy:%d"
//   //       " vw:%d vh:%d tick:%d br x:%d y:%d w:%d h:%d\n",
//   //       bar, vr.x(), vr.y(), vr.width(), vr.height(), tick,
//   //       br_txt.x(), br_txt.y(), br_txt.width(), br_txt.height());
//     
//     if(vbr_txt.intersects(vr))
//     {
//   //     // REMOVE Tim. citem. Added.
//   //     fprintf(stderr, "...bar text within range. xorg:%d xmag:%d xpos:%d Drawing bar text at x:%d y:%d w:%d h:%d\n",
//   //             xorg, xmag, xpos, map(br_txt).x(), map(br_txt).y(), map(br_txt).width(), map(br_txt).height());
//       
//       p.drawText(map(vbr_txt), Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
//     }
//   }  
//   else
//   {
//     const int mw1000 = 1000;
//     const int mw2 = 2;
//     const int my = mr.y();
//     const int mh_m1 = mr.height() - 1;
//     
//     QPen pen;
//     pen.setCosmetic(true);
//     pen.setColor(textColor);
//     p.setPen(pen);
//     
//     QString s;
//     s.setNum(bar + 1);
//     
//     p.setFont(font);
//     
//     int mbrw = p.fontMetrics().boundingRect(s).width();
//     
//     //const int brh = rmapyDev(br.width());
//     //int w_txt = rmapxDev(1000);
//     //const int h_txt = rmapyDev(height() - 1);
//     
//     if(mbrw > mw1000)
//       mbrw = mw1000;
//     
//     //if(brh < h_txt)
//     //  h_txt = brh;
//     
//     const QRect mbr_txt = QRect(mapx(tick) + mw2, my, mbrw, mh_m1);
//     
//   //   // REMOVE Tim. citem. Added.
//   //   fprintf(stderr,
//   //       "drawBarText: Bar text: bar:%d vx:%d vy:%d"
//   //       " vw:%d vh:%d tick:%d br x:%d y:%d w:%d h:%d\n",
//   //       bar, vr.x(), vr.y(), vr.width(), vr.height(), tick,
//   //       br_txt.x(), br_txt.y(), br_txt.width(), br_txt.height());
//     
//     if(mbr_txt.intersects(mr))
//     {
//   //     // REMOVE Tim. citem. Added.
//   //     fprintf(stderr, "...bar text within range. xorg:%d xmag:%d xpos:%d Drawing bar text at x:%d y:%d w:%d h:%d\n",
//   //             xorg, xmag, xpos, map(br_txt).x(), map(br_txt).y(), map(br_txt).width(), map(br_txt).height());
//       
//       p.drawText(mbr_txt, Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
//     }
//   }
// }

void View::drawBarText(QPainter& p, int tick, int bar, const QRect& mr, const QColor& textColor, const QFont& font) const
{
  QPen pen;
  pen.setCosmetic(true);
  pen.setColor(textColor);
  p.setPen(pen);
  p.setFont(font);
  QString s;
  s.setNum(bar + 1);
    
//   if(xmag <= 0)
//   {
//     const QRect vr = mapDev(mr);
//     const int vw1000 = rmapxDev(1000);
//     const int vw2 = rmapxDev(2);
//     
//     if(ymag <= 0)
//     {
//       const int vy = vr.y();
//       //const int vh_m1 = vr.height() - rmapyDev(1);
//       const int vh_m1 = rmapyDev(mr.height() - 1);
//     }
//     else
//     {
//       const int my = mr.y();
//       //const int vh_m1 = vr.height() - rmapyDev(1);
//       const int mh_m1 = mr.height() - 1;
//     }
//     
//     int vbrw = rmapxDev(p.fontMetrics().boundingRect(s).width());
//     
//     //const int brh = rmapyDev(br.width());
//     //int w_txt = rmapxDev(1000);
//     //const int h_txt = rmapyDev(height() - 1);
//     
//     if(vbrw > vw1000)
//       vbrw = vw1000;
// 
//     //if(brh < h_txt)
//     //  h_txt = brh;
// 
//     const QRect vbr_txt = QRect(tick + vw2, vy, vbrw, vh_m1);
//     
//   //   // REMOVE Tim. citem. Added.
//   //   fprintf(stderr,
//   //       "drawBarText: Bar text: bar:%d vx:%d vy:%d"
//   //       " vw:%d vh:%d tick:%d br x:%d y:%d w:%d h:%d\n",
//   //       bar, vr.x(), vr.y(), vr.width(), vr.height(), tick,
//   //       br_txt.x(), br_txt.y(), br_txt.width(), br_txt.height());
//     
//     if(vbr_txt.intersects(vr))
//     {
//   //     // REMOVE Tim. citem. Added.
//   //     fprintf(stderr, "...bar text within range. xorg:%d xmag:%d xpos:%d Drawing bar text at x:%d y:%d w:%d h:%d\n",
//   //             xorg, xmag, xpos, map(br_txt).x(), map(br_txt).y(), map(br_txt).width(), map(br_txt).height());
//       
//       p.drawText(map(vbr_txt), Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
//     }
//   }  
//   else
  {
    ViewRect r(mr, true);
    
    const int mw1000 = 1000;
    const int mw2 = 2;
    const int my = mr.y();
    const int mh_m1 = mr.height() - 1;
    
//     QPen pen;
//     pen.setCosmetic(true);
//     pen.setColor(textColor);
//     p.setPen(pen);
//     
//     QString s;
//     s.setNum(bar + 1);
//     
//     p.setFont(font);
    
    int mbrw = p.fontMetrics().boundingRect(s).width();
    
    //const int brh = rmapyDev(br.width());
    //int w_txt = rmapxDev(1000);
    //const int h_txt = rmapyDev(height() - 1);
    
    if(mbrw > mw1000)
      mbrw = mw1000;
    
    //if(brh < h_txt)
    //  h_txt = brh;
    
//     const QRect mbr_txt = QRect(mapx(tick) + mw2, my, mbrw, mh_m1);
    const ViewRect br_txt = ViewRect(
//        mathXCoordinates(ViewXCoordinate(tick, false), ViewXCoordinate(mw2, true), MathAdd),
       mathXCoordinates(ViewXCoordinate(tick, false), ViewWCoordinate(mw2, true), MathAdd),
       ViewYCoordinate(my, true),
       ViewWCoordinate(mbrw, true),
       ViewHCoordinate(mh_m1, true));
    
    // REMOVE Tim. citem. Added.
//     fprintf(stderr,
//         "drawBarText: Bar text: bar:%d vx:%d vy:%d"
//         " vw:%d vh:%d tick:%d br x:%d y:%d w:%d h:%d\n",
//         bar, vr.x(), vr.y(), vr.width(), vr.height(), tick,
//         br_txt.x(), br_txt.y(), br_txt.width(), br_txt.height());
//     fprintf(stderr,
//         "drawBarText: Bar text: bar:%d mx:%d my:%d"
//         " mw:%d mh:%d tick:%d br x:%d y:%d w:%d h:%d\n",
//         bar, mr.x(), mr.y(), mr.width(), mr.height(), tick,
//         br_txt._x._value, br_txt._y._value, br_txt._width._value, br_txt._height._value);
    
//     if(mbr_txt.intersects(mr))
    if(intersects(br_txt, r))
    {
      const QRect br_text_qr = asQRectMapped(br_txt);
      // REMOVE Tim. citem. Added.
//       fprintf(stderr, "...bar text within range. xorg:%d xmag:%d xpos:%d"
//                       " Drawing bar text at x:%d y:%d w:%d h:%d\n",
//               xorg, xmag, xpos, br_text_qr.x(), br_text_qr.y(), br_text_qr.width(), br_text_qr.height());
      
//       p.drawText(mbr_txt, Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
      p.drawText(br_text_qr, Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
    }
  }
}

View::ScaleRetStruct View::scale(bool drawText, int bar, double tpix) const
{
  ScaleRetStruct ret;
  ret._drawBar = true;
  ret._isSmall = drawText ? (tpix < 64) : (tpix < 32);
  
  if(!ret._isSmall)
    return ret;
  
  int n = 1;
  if(drawText)
  {
    //fprintf(stderr, "drawText: tpix < 64:%f\n", tpix);
    if (tpix <= 2)
    {
      //fprintf(stderr, "drawText: tpix <= 2\n");
      n <<= 5;
    }
    else if (tpix <= 4)
    {
      //fprintf(stderr, "drawText: tpix <= 4\n");
      n <<= 4;
    }
    else if (tpix < 8)
    {
      //fprintf(stderr, "drawText: tpix < 8\n");
      n <<= 3;
    }
    else if (tpix <= 16)
    {
      //fprintf(stderr, "drawText: tpix <= 16\n");
      n <<= 2;
    }
    else if (tpix < 32)
    {
      //fprintf(stderr, "drawText: tpix < 32\n");
      n <<= 1;
    }
  }
  else
  {
    //fprintf(stderr, "tpix < 32:%f\n", tpix);
    n = 1;
    if (tpix <= 0.01)
    {
      //fprintf(stderr, "tpix <= 0.01\n");
      n <<= 10;
    }
    else if (tpix <= 0.03125)
    {
      //fprintf(stderr, "tpix <= 0.03125\n");
      n <<= 9;
    }
    else if (tpix <= 0.0625)
    {
      //fprintf(stderr, "tpix <= 0.0625\n");
      n <<= 8;
    }
    else if (tpix <= 0.125)
    {
      //fprintf(stderr, "tpix <= 0.125\n");
      n <<= 7;
    }
    else if (tpix <= 0.25)
    {
      //fprintf(stderr, "tpix <= 0.25\n");
      n <<= 6;
    }
    else if (tpix <= 0.5)
    {
      //fprintf(stderr, "tpix <= 0.5\n");
      n <<= 5;
    }
    else if (tpix <= 1.0)
    {
      //fprintf(stderr, "tpix <= 1.0\n");
      n <<= 4;
    }
    else if (tpix <= 2.0)
    {
      //fprintf(stderr, "tpix <= 2.0\n");
      n <<= 3;
    }
    else if (tpix <= 4.0)
    {
      //fprintf(stderr, "tpix <= 4.0\n");
      n <<= 2;
    }
    else if (tpix <= 8.0)
    {
      //fprintf(stderr, "tpix <= 8.0\n");
      n <<= 1;
    }
  }
  
  if (bar % n)
    ret._drawBar = false;
  
  return ret;
}

// //---------------------------------------------------------
// //   drawTickRaster
// //---------------------------------------------------------
// 
// void View::drawTickRaster(
//   QPainter& p, const QRect& vr, const QRegion& /*vrg*/, int raster,
//   bool waveMode, 
//   bool /*useGivenColors*/,
//   bool drawText,
//   const QColor& bar_color,
//   const QColor& beat_color,
//   const QColor& text_color,
//   const QFont& large_font,
//   const QFont& small_font
//   )
// {
//       // Changed to draw in device coordinate space instead of virtual, transformed space.     Tim. p4.0.30  
//       
//       const int vx = vr.x();
//       const int vy = vr.y();
//       const int vw = vr.width();
//       const int vh = vr.height();
//       
//       if(vw == 0 || vh == 0)
//         return;
//       
//       //int mx = mapx(x);
// //       int my = mapy(y);
// //       int my_1 = mapy(y + 1);
// //       int my_3 = mapy(y + 3);
// //       int my_7 = mapy(y + 7);
//       //int mw = mapx(x + w) - mx;
//       //int mw = mapx(x + w) - mx - 1;
//       //int mh = mapy(y + h) - my;
//       //int mh = mapy(y + h) - my - 1;
//       
//       //p.save();
//       bool wmtxen = p.worldMatrixEnabled();
//       p.setWorldMatrixEnabled(false);
// 
//       const int vx_2 = vx + vw;
// 
//       
//       // Limiter required because AL::sigmap.tickValues takes unsigned only !
//       const int vx_lim = vx < 0 ? 0 : vx;
//       const int vx_2lim = vx_2 < 0 ? 0 : vx_2;
// 
// 
// //       int my2 = mapy(y + h) - 1;
//       //int my2 = mapy(y + h - 1);
// //       int my2_1 = mapy(y + h + 1);
//       
//       const QRect mr = map(vr);
// //       const int mx = mr.x();
//       const int my = mr.y();
// //       const int mw = mr.width();
// //       const int mh = mr.height();
// 
//       const int mbottom = mr.bottom();
// 
// //       int mx2 = x + w - mapx(1);
// 
//       //const int vtop        = mapyDev(0);
//       const int vw2         = rmapxDev(2);
//       const int vw1000      = rmapxDev(1000);
//       const int vh2         = rmapyDev(2);
//       //const int vheight_m1  = rmapyDev(height() - 1);
//       const int vh_m1       = vh - rmapyDev(1);
//       const int vh_m3       = vh - rmapyDev(3);
//       
// //       unsigned ctick;
//       int bar1, bar2, beat;
//       //bool draw_bar, draw_fake_bar;
//       ScaleRetStruct scale_info;
//       //bool is_small;
//       bool is_beat_small;
// //       bool is_raster_small;
//       int beat_start_beat, rast_x, rast_z, rast_n, rast_xx, rast_xxx; //, qq; //, rast_mapx;
//       unsigned tick, rast_xb;
// 
//       QPen pen;
//       pen.setCosmetic(true);
// 
//       // REMOVE Tim. citem. Added.
// //       fprintf(stderr, "View::drawTickRaster_new(): virt:%d drawText:%d x:%d y:%d w:%d h:%d my:%d mh:%d my2:%d raster%d\n",
// //               virt(), drawText, x, y, w, h, my, mh, my2, raster);  
//       
//       //const int rast_mapx = rmapx(raster);
//       const double rast_mapx = rmapx_f(raster);
//       int qq = raster;
//       int qq_shift = 1;
//       
//       // grid too dense?
// //       if (rast_mapx <= 1)
// //             qq *= 16;
// //       else if (rast_mapx <= 2)
// //             qq *= 8;
// //       else if (rast_mapx <= 4)
// //             qq *= 4;
// //       else if (rast_mapx <= 8)
// //             qq *= 2;
// 
//                     if (rast_mapx <= 0.01)
//                     {
//                           //fprintf(stderr, "tpix <= 0.01\n");
//                           qq_shift <<= 11;
//                     }
//                     else if (rast_mapx <= 0.03125)
//                     {
//                           //fprintf(stderr, "tpix <= 0.03125\n");
//                           qq_shift <<= 10;
//                     }
//                     else if (rast_mapx <= 0.0625)
//                     {
//                           //fprintf(stderr, "tpix <= 0.0625\n");
//                           qq_shift <<= 9;
//                     }
//                     else if (rast_mapx <= 0.125)
//                     {
//                           //fprintf(stderr, "tpix <= 0.125\n");
//                           qq_shift <<= 8;
//                     }
//                     else if (rast_mapx <= 0.25)
//                     {
//                           //fprintf(stderr, "tpix <= 0.25\n");
//                           qq_shift <<= 7;
//                     }
//                     else if (rast_mapx <= 0.5)
//                     {
//                           //fprintf(stderr, "tpix <= 0.5\n");
//                           qq_shift <<= 6;
//                     }
//                     else if (rast_mapx <= 1.0)
//                           qq_shift <<= 5;
//                     else if (rast_mapx <= 2)
//                           qq_shift <<= 4;
//                     else if (rast_mapx <= 4)
//                           qq_shift <<= 3;
//                     else if (rast_mapx <= 8)
//                           qq_shift <<= 2;
//                     else if (rast_mapx < 32)
//                           qq_shift <<= 1;
//       
//                     qq *= qq_shift;
//       
//       
// //       is_raster_small = rast_mapx < 64;
// //             if (is_small) {
// //                   // don't show beats if measure is this small
// //                   int n = 1;
// //                   if (rast_mapx <= 2)
// //                         n = 32;
// //                   else if (rast_mapx <= 4)
// //                         n = 16;
// //                   else if (rast_mapx < 8)
// //                         n = 8;
// //                   else if (rast_mapx <= 16)
// //                         n = 4;
// //                   else if (rast_mapx < 32)
// //                         n = 2;
// //               
// //                   if (bar % n)
// // //                         continue;
// //                         draw_bar = false;
// //             }
// 
//       if (waveMode) {
// //             ctick = MusEGlobal::tempomap.frame2tick(mapxDev(x));
// //             ctick = MusEGlobal::tempomap.frame2tick(x);
// //             AL::sigmap.tickValues(ctick, &bar1, &beat, &tick);
// //             AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(mapxDev(x+w)),
// //                &bar2, &beat, &tick);
//             AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(vx_lim),   &bar1, &beat, &tick);
//             AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(vx_2lim), &bar2, &beat, &tick);
// //             AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(mx2), &bar2, &beat, &tick);
//             }
//       else {
// //             ctick = mapxDev(x);
// //             AL::sigmap.tickValues(ctick, &bar1, &beat, &tick);
// //             AL::sigmap.tickValues(mapxDev(x+w), &bar2, &beat, &tick);
// //             ctick = x;
//             AL::sigmap.tickValues(vx_lim,   &bar1, &beat, &tick);
//             AL::sigmap.tickValues(vx_2lim, &bar2, &beat, &tick);
// //             AL::sigmap.tickValues(mx2, &bar2, &beat, &tick);
//             }
// 
//       //printf("bar %d  %d-%d=%d\n", bar, ntick, stick, ntick-stick);
// 
//       int stick = AL::sigmap.bar2tick(bar1, 0, 0);
//       int ntick;
//       ScaleRetStruct prev_scale_info;
//       for (int bar = bar1; bar <= bar2; bar++, stick = ntick) {
// //       for (int bar = bar1; bar < bar2; bar++, stick = ntick) {
//             ntick     = AL::sigmap.bar2tick(bar+1, 0, 0);
// //             int tpix, a, b=0;
//             int a, b=0;
//             double tpix;
//             if (waveMode) {
//                   a = MusEGlobal::tempomap.tick2frame(ntick);
//                   b = MusEGlobal::tempomap.tick2frame(stick);
// //                   tpix  = rmapx(a - b);
//                      tpix  = rmapx_f(a - b);
//                }
//             else {
// //                   tpix  = rmapx(ntick - stick);
//                   tpix  = rmapx_f(ntick - stick);
//                   }
//                   
//             //draw_fake_bar = /*true*/ false;
//             is_beat_small = tpix < 32;
//             
//             scale_info = scale(drawText, bar, tpix);
//             
//             if(drawText && !scale_info._drawBar)
//             {
//               // Only check this on the first starting bar.
//               if(bar == bar1)
//               {
//                 int prev_a, prev_b=0;
//                 double prev_tpix;
//                 int prev_stick;
//                 int prev_ntick = stick;
//                 int prev_bar = bar1 - 1;
//                 for( ; prev_bar >= 0; --prev_bar, prev_ntick = prev_stick)
//                 {
//                   prev_stick = AL::sigmap.bar2tick(prev_bar, 0, 0);
//                   if (waveMode) {
//                         prev_a = MusEGlobal::tempomap.tick2frame(prev_ntick);
//                         prev_b = MusEGlobal::tempomap.tick2frame(prev_stick);
//                         //prev_tpix  = rmapx(b - prev_b);
//                         prev_tpix  = rmapx_f(prev_a - prev_b);
//                     }
//                   else {
//                         //prev_tpix  = rmapx(stick - prev_stick);
//                         prev_tpix  = rmapx_f(prev_ntick - prev_stick);
//                         }
//                   
//                   prev_scale_info = scale(drawText, prev_bar, prev_tpix);
// 
//                   // REMOVE Tim. citem. Added.
// //                   fprintf(stderr,
// //                       "drawTickRaster: Bar check: bar1:%d bar2:%d bar:%d prev_bar:%d stick:%d prev_stick:%d prev_ntick:%d prev_tpix:%f vx:%d vy:%d"
// //                       " vw:%d vh:%d drawBar:%d\n",
// //                       bar1, bar2, bar, prev_bar, stick, prev_stick, prev_ntick, prev_tpix,
// //                       vx, vy, vw, vh, prev_scale_info._drawBar);
// 
//                   if(prev_scale_info._drawBar)
//                     break;
//                 }
//                 
//                 if(prev_bar >= 0)
//                 {
//                   const int prev_tick = waveMode ? prev_b : prev_stick;
//                   drawBarText(p, prev_tick, prev_bar, vr, text_color, large_font);
//                 }
//               }
//                 
//               continue;
//             }
//             
//             if(!drawText || (drawText && scale_info._isSmall))
//             {
//                   const int tick_small = waveMode ? b : stick;
//                   const int x_small = mapx(tick_small);
//                   
//                   if(drawText)
//                     pen.setColor(text_color);
//                   else
//                     pen.setColor(bar_color);
//                   p.setPen(pen);
//                   
//                   //if(scale_info._drawBar || draw_fake_bar)
//                   if(scale_info._drawBar)
//                   {
//                     // REMOVE Tim. citem. Added.
//                     //fprintf(stderr,
//                     //  "...is_small:%d x:%d mx:%d mw:%d x_small:%d w:%d stick:%d ntick:%d bar1:%d bar2:%d bar:%d\n",
//                     //  scale_info._isSmall, x, mx, mw, x_small, w, stick, ntick, bar1, bar2, bar);
//                     
//                     if(tick_small >= vx && tick_small < vx_2)
//                     {
//                       // REMOVE Tim. citem. Added.
//                       //fprintf(stderr, "...Line is within range. Drawing line...\n");
//                       p.drawLine(x_small, my, x_small, mbottom);
//                       //p.drawLine(x_small, 0, x_small, height());
//                     }
//                   }
//                   
//                   if(drawText)
//                   {
// // //                     QRect r_txt = QRect(x_small+2, my, 1000, mh);
// //                     QString s;
// //                     s.setNum(bar + 1);
// //                     
// //                     const QRect br = fm.boundingRect(s);
// //                     int brw = rmapxDev(br.width());
// //                     //const int brh = rmapyDev(br.width());
// //                     //int w_txt = rmapxDev(1000);
// //                     //const int h_txt = rmapyDev(height() - 1);
// //                     if(brw > vw1000)
// //                       brw = vw1000;
// //                     //if(brh < h_txt)
// //                     //  h_txt = brh;
// //                     const QRect br_txt = QRect(tick_small + vw2, vy, brw, vh_m1);
// //                     
// //                     // REMOVE Tim. citem. Added.
// //                     fprintf(stderr,
// //                         "drawTickRaster: Bar text: bar1:%d bar2:%d bar:%d vx:%d vy:%d"
// //                         " vw:%d vh:%d mx:%d my:%d mw:%d mh:%d tick_small:%d x_small:%d br x:%d y:%d w:%d h:%d\n",
// //                         bar1, bar2, bar, vx, vy, vw, vh, mr.x(), my, mr.width(), mr.height(),
// //                         tick_small, x_small, br_txt.x(), br_txt.y(), br_txt.width(), br_txt.height());
// //                     
// //                     
// //                     //if(r_txt.intersects(mr))
// //                     if(br_txt.intersects(vr))
// //                     {
// //                       // REMOVE Tim. citem. Added.
// //                       fprintf(stderr, "...Text is within range. Drawing text...\n");
// //                       fprintf(stderr, "...bar text within range. xorg:%d xmag:%d xpos:%d Drawing bar text at x:%d y:%d w:%d h:%d\n",
// //                               xorg, xmag, xpos, map(br_txt).x(), map(br_txt).y(), map(br_txt).width(), map(br_txt).height());
// //                       p.setFont(large_font);
// //                       //p.drawText(r_txt, Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
// //                       p.drawText(map(br_txt), Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
// //                     }
//                     
//                     drawBarText(p, tick_small, bar, vr, text_color, large_font);
//                   }
//             }
// //             else {
// 
// //             if(!drawText && !scale_info._drawBar && !draw_fake_bar)
// //             if(!drawText && !scale_info._isSmall && !draw_fake_bar)
//             if(!drawText && !scale_info._isSmall)
//             {
//               if (raster>=4) {
//                           pen.setColor(Qt::darkGray);
//                           p.setPen(pen);
//                           rast_xb = AL::sigmap.bar2tick(bar, 0, 0);
//                           AL::sigmap.timesig(rast_xb, rast_z, rast_n);
//                           rast_xx = rast_xb + (scale_info._drawBar ? qq : 0);
//                           rast_xxx = AL::sigmap.bar2tick(bar, rast_z, 0);
//                           while (rast_xx <= rast_xxx) {
//                                 rast_x = mapx(rast_xx);
//                                 p.drawLine(rast_x, my, rast_x, mbottom);
//                                 rast_xx += qq;
//                                 }
//                           }
//             }
//                         
//             //if((!drawText && !is_small) || (drawText && !is_small)) {
//             //if((!drawText && !is_beat_small && !scale_info._drawBar && !draw_fake_bar) || (drawText && !is_small)) {
//             if((!drawText && !is_beat_small) || (drawText && !scale_info._isSmall)) {
//                   int z, n;
//                   AL::sigmap.timesig(stick, z, n);
//                   
//                   //for (int beat = 0; beat < z; beat++) {
// //                   beat_start_beat = (drawText || (!drawText && !scale_info._drawBar && !draw_fake_bar)) ? 0 : 1;
//                   beat_start_beat = (drawText || (!drawText && !scale_info._drawBar)) ? 0 : 1;
//                   for (int beat = beat_start_beat; beat < z; beat++) {
//                         int xx = AL::sigmap.bar2tick(bar, beat, 0);
//                         if (waveMode)
//                               xx = MusEGlobal::tempomap.tick2frame(xx);
//                         
//                         int xx_e = AL::sigmap.bar2tick(bar, beat + 1, 0);
//                         if (waveMode)
//                               xx_e = MusEGlobal::tempomap.tick2frame(xx_e);
//                         
//                         if((xx_e - xx) < raster)
//                           continue;
//                         
//                         int mxx = mapx(xx);
//                         //int xp_e = mapx(xx_e);
//                         
//                         if(drawText)
//                         {
// //                           QRect r(xp+2, y, 1000, h);
//                           //QRect r(xp+2, my, 1000, mh);
//                           int y1;
//                           int num;
//                           int yt = vy;
//                           int h_txt;
//                           
//                           //const QString s = m->second.name();
//                           //const QRect br = fm.boundingRect(s);
//                           //const int brw = rmapxDev(br.width());
//                           //const int brh = rmapyDev(br.width());
//                           //const int h_txt = rmapyDev(12);
//                           //if(brw < w_txt)
//                           //  w_txt = brw;
//                           //if(brh < h_txt)
//                           //  h_txt = brh;
//                           //const QRect br_txt = QRect(xp + rmapxDev(10), y, w_txt, h_txt);
//                           
//                           
//                           if (beat == 0) {
//                                 num = bar + 1;
// //                                 y1  = y + 1;
// //                                 y1  = my_1;
//                                 y1  = my + 1;
//                                 h_txt = vh_m1;
//                                 p.setFont(large_font);
//                                 }
//                           else {
//                                 num = beat + 1;
// //                                 y1  = y + 7;
// //                                 y1  = my_7;
//                                 y1  = my + 6;
// //                                 r.setY(y+3);
// //                                 r.setY(my_3);
// //                                 r.setY(my + 3);
//                                 yt = vy + vh2;
//                                 h_txt = vh_m3;
//                                 p.setFont(small_font);
//                                 }
// 
//                                 
//                           pen.setColor(text_color);
//                           p.setPen(pen);
//                           if(xx >= vx && xx < vx_2)
//                             //p.drawLine(mxx, y1, mxx, my2 + 1);
//                             p.drawLine(mxx, y1, mxx, mbottom);
//                                 
//                           QString s;
//                           s.setNum(num);
//                           int brw = rmapxDev(p.fontMetrics().boundingRect(s).width());
//                           //int w_txt = vw1000;
//                           if(brw > vw1000)
//                             brw = vw1000;
//                           QRect br_txt(xx + vw2, yt, brw, h_txt);
//                                 
//                           // REMOVE Tim. citem. Added.
// //                           fprintf(stderr,
// //                               "drawBarText: Bar text: bar:%d beat:%d vx:%d vy:%d"
// //                               " vw:%d vh:%d tick:%d br x:%d y:%d w:%d h:%d\n",
// //                               bar, beat, vr.x(), vr.y(), vr.width(), vr.height(), tick,
// //                               br_txt.x(), br_txt.y(), br_txt.width(), br_txt.height());
//   
//                           if(br_txt.intersects(vr))
//                           {
//                             // REMOVE Tim. citem. Added.
// //                             fprintf(stderr, "...bar text within range. xorg:%d xmag:%d xpos:%d Drawing bar text at x:%d y:%d w:%d h:%d\n",
// //                                     xorg, xmag, xpos, map(br_txt).x(), map(br_txt).y(), map(br_txt).width(), map(br_txt).height());
//     
//                             //pen.setColor(text_color);
//   //                           p.drawLine(xp, y1, xp, y+1+h);
//   //                           p.drawLine(xp, y1, xp, my2_1);
//   //                           fprintf(stderr, "View::drawTickRaster_new(): beat: my:%d y1:%d\n", my, y1);
//                             //p.setPen(pen);
//                             //p.drawLine(xp, y1, xp, my2 + 1);
//                             //QString s;
//                             //s.setNum(num);
//                             p.drawText(map(br_txt), Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
//                           }
//                         }
//                         else
//                         {
//                           if(xx >= vx && xx < vx_2)
//                           {
//                             pen.setColor(beat_color);
//                             p.setPen(pen);
//                             p.drawLine(mxx, my, mxx, mbottom);
//                           }
//                         }
//                         }
//                   }
//             }
// 
//       //p.setWorldMatrixEnabled(true);
//       p.setWorldMatrixEnabled(wmtxen);
//       //p.restore();      
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
// 
//       
// //       int xx,bar1, bar2, beat;
// //       int rast_mapx;
// //       unsigned tick;
// //       AL::sigmap.tickValues(x, &bar1, &beat, &tick);
// //       AL::sigmap.tickValues(x+w, &bar2, &beat, &tick);
// //       ++bar2;
// //       ///int y2 = y + h;
// //       //int y2 = my + mh;
// //       int y2 = mapy(y + h) - 1;
// //       //printf("View::drawTickRaster x:%d y:%d w:%d h:%d mx:%d my:%d mw:%d mh:%d y2:%d bar1:%d bar2:%d\n", x, y, w, h, mx, my, mw, mh, y2, bar1, bar2);  
// //       //printf("View::drawTickRaster x:%d y:%d w:%d h:%d my:%d mh:%d y2:%d bar1:%d bar2:%d\n", x, y, w, h, my, mh, y2, bar1, bar2);  
// //       for (int bar = bar1; bar < bar2; ++bar) {
// //             ///unsigned x = AL::sigmap.bar2tick(bar, 0, 0);
// //             unsigned xb = AL::sigmap.bar2tick(bar, 0, 0);
// //             int xt = mapx(xb);
// //             p.setPen(MusEGlobal::config.midiCanvasBarColor);
// //             ///p.drawLine(x, y, x, y2);
// //             p.drawLine(xt, my, xt, y2);
// //             int z, n;
// //             ///AL::sigmap.timesig(x, z, n);
// //             AL::sigmap.timesig(xb, z, n);
// //             ///int q = p.xForm(QPoint(raster, 0)).x() - p.xForm(QPoint(0, 0)).x();
// //             ///int q = p.combinedTransform().map(QPoint(raster, 0)).x() - p.combinedTransform().map(QPoint(0, 0)).x();
// //             //int q = rmapx(raster);
// //             int qq = raster;
// //             //if (q < 8)        // grid too dense
// //             
// // // REMOVE Tim. citem. Changed.
// // //             if (rmapx(raster) < 8)        // grid too dense
// // //                   qq *= 2;
// // 
// //             rast_mapx = rmapx(raster);
// //             // grid too dense?
// //             if (rast_mapx <= 1)        
// //                   qq *= 16;
// //             else if (rast_mapx <= 2)
// //                   qq *= 8;
// //             else if (rast_mapx <= 4)
// //                   qq *= 4;
// //             else if (rast_mapx <= 8)
// //                   qq *= 2;
// //             
// //             p.setPen(MusEGlobal::config.midiCanvasBeatColor);
// //             if (raster>=4) {
// //                         ///int xx = x + qq;
// //                         //int xx = mapx(xb + qq);
// //                         xx = xb + qq;
// //                         int xxx = AL::sigmap.bar2tick(bar, z, 0);
// //                         //int xxx = mapx(AL::sigmap.bar2tick(bar, z, 0));
// //                         while (xx <= xxx) {
// //                                ///p.drawLine(xx, y, xx, y2);
// //                                int x = mapx(xx);
// //                                p.drawLine(x, my, x, y2);
// //                                xx += qq;
// //                                //xx += rmapx(qq);
// //                                }
// //                         //xx = xxx;
// //                         }
// // 
// //             p.setPen(Qt::darkGray);
// //             for (int beat = 1; beat < z; beat++) {
// //                         ///int xx = AL::sigmap.bar2tick(bar, beat, 0);
// //                         xx = mapx(AL::sigmap.bar2tick(bar, beat, 0));
// //                         //printf(" bar:%d z:%d beat:%d xx:%d\n", bar, z, beat, xx);  
// //                         ///p.drawLine(xx, y, xx, y2);
// //                         p.drawLine(xx, my, xx, y2);
// //                         }
// // 
// //             }
// //       //p.setWorldMatrixEnabled(true);
// //       p.setWorldMatrixEnabled(wmtxen);
// //       //p.restore();      
// }

//---------------------------------------------------------
//   drawTickRaster
//---------------------------------------------------------

void View::drawTickRaster(
  QPainter& p, const QRect& mr, const QRegion& /*mrg*/, int raster,
  bool waveMode, 
  bool /*useGivenColors*/,
  bool drawText,
  const QColor& bar_color,
  const QColor& beat_color,
  const QColor& text_color,
  const QFont& large_font,
  const QFont& small_font
  )
{
      // Changed to draw in device coordinate space instead of virtual, transformed space.     Tim. p4.0.30  
      
//       const int vx = vr.x();
//       const int vy = vr.y();
//       const int vw = vr.width();
//       const int vh = vr.height();

      const ViewRect r(mr, true);
      const ViewXCoordinate& x = r._x;
//       const ViewYCoordinate& y = r._y;
//       const ViewWCoordinate& w = r._width;
//       const ViewHCoordinate& h = r._height;
      const ViewXCoordinate x_2(mr.x() + mr.width(), true);
      
      
//       const QRect vr = map(mr);
//       const int vx = vr.x();
//       const int vy = vr.y();
//       const int vw = vr.width();
//       const int vh = vr.height();
      
//       if(vw == 0 || vh == 0)
//         return;
      
//       //int mx = mapx(x);
// //       int my = mapy(y);
// //       int my_1 = mapy(y + 1);
// //       int my_3 = mapy(y + 3);
// //       int my_7 = mapy(y + 7);
//       //int mw = mapx(x + w) - mx;
//       //int mw = mapx(x + w) - mx - 1;
//       //int mh = mapy(y + h) - my;
//       //int mh = mapy(y + h) - my - 1;
      
      //p.save();
      bool wmtxen = p.worldMatrixEnabled();
      p.setWorldMatrixEnabled(false);

//       const int vx_2 = vx + vw;

      // Limiter required because AL::sigmap.tickValues takes unsigned only !
//       const int vx_lim = vx < 0 ? 0 : vx;
//       const int vx_2lim = vx_2 < 0 ? 0 : vx_2;
      
      // Limiter required because AL::sigmap.tickValues takes unsigned only !
      const ViewXCoordinate lim_v0(0, false);
      const ViewXCoordinate x_lim = compareXCoordinates(x, lim_v0, CompareLess) ? lim_v0 : x;
      const ViewXCoordinate x_2lim = compareXCoordinates(x_2, lim_v0, CompareLess) ? lim_v0 : x_2;


//       int my2 = mapy(y + h) - 1;
      //int my2 = mapy(y + h - 1);
//       int my2_1 = mapy(y + h + 1);
      
//       const QRect mr = map(vr);
      
//       const int mx = mr.x();
      const int my = mr.y();
//       const int mw = mr.width();
      const int mh = mr.height();

      const int mbottom = mr.bottom();

      const ViewYCoordinate bottom(mr.bottom(), true);

//       int mx2 = x + w - mapx(1);
      
//       const int mx_2 = mx + mw;
      // Limiter required because AL::sigmap.tickValues takes unsigned only !
//       const int mx_lim = mx < 0 ? 0 : mx;
//       const int mx_2lim = mx_2 < 0 ? 0 : mx_2;
      //const int vx_lim = mapxDev(mx_lim);
      
      //const int vtop        = mapyDev(0);
//       const int vw2         = rmapxDev(2);
//       const int vw1000      = rmapxDev(1000);
//       const int vh2         = rmapyDev(2);
//       //const int vheight_m1  = rmapyDev(height() - 1);
//       const int vh_m1       = vh - rmapyDev(1);
//       const int vh_m3       = vh - rmapyDev(3);
//       const int mw2         = 2;
      const int mw1000      = 1000;
      const int mh2         = 2;
      //const int vheight_m1  = rmapyDev(height() - 1);
//       const int mh_m1       = mh - 1;
//       const int mh_m3       = mh - 3;

      const ViewWCoordinate w2(2, true);
      const ViewHCoordinate h2(2, true);
      const ViewHCoordinate h_m1(mh - 1, true);
      const ViewHCoordinate h_m3(mh - 3, true);
      
//       unsigned ctick;
      int bar1, bar2, beat;
      //bool draw_bar, draw_fake_bar;
      ScaleRetStruct scale_info;
      //bool is_small;
      bool is_beat_small;
//       bool is_raster_small;
      int beat_start_beat, rast_x, rast_z, rast_n, rast_xx, rast_xxx; //, qq; //, rast_mapx;
      unsigned tick, rast_xb;

      QPen pen;
      pen.setCosmetic(true);

      // REMOVE Tim. citem. Added.
//       fprintf(stderr, "View::drawTickRaster_new(): virt:%d drawText:%d mx:%d my:%d mw:%d mh:%draster%d\n",
//               virt(), drawText, mx, my, mw, mh, raster);  
      
      //const int rast_mapx = rmapx(raster);
      const double rast_mapx = rmapx_f(raster);
      int qq = raster;
      int qq_shift = 1;
      
      // grid too dense?
//       if (rast_mapx <= 1)
//             qq *= 16;
//       else if (rast_mapx <= 2)
//             qq *= 8;
//       else if (rast_mapx <= 4)
//             qq *= 4;
//       else if (rast_mapx <= 8)
//             qq *= 2;

                    if (rast_mapx <= 0.01)
                    {
                          //fprintf(stderr, "tpix <= 0.01\n");
                          qq_shift <<= 11;
                    }
                    else if (rast_mapx <= 0.03125)
                    {
                          //fprintf(stderr, "tpix <= 0.03125\n");
                          qq_shift <<= 10;
                    }
                    else if (rast_mapx <= 0.0625)
                    {
                          //fprintf(stderr, "tpix <= 0.0625\n");
                          qq_shift <<= 9;
                    }
                    else if (rast_mapx <= 0.125)
                    {
                          //fprintf(stderr, "tpix <= 0.125\n");
                          qq_shift <<= 8;
                    }
                    else if (rast_mapx <= 0.25)
                    {
                          //fprintf(stderr, "tpix <= 0.25\n");
                          qq_shift <<= 7;
                    }
                    else if (rast_mapx <= 0.5)
                    {
                          //fprintf(stderr, "tpix <= 0.5\n");
                          qq_shift <<= 6;
                    }
                    else if (rast_mapx <= 1.0)
                          qq_shift <<= 5;
                    else if (rast_mapx <= 2)
                          qq_shift <<= 4;
                    else if (rast_mapx <= 4)
                          qq_shift <<= 3;
                    else if (rast_mapx <= 8)
                          qq_shift <<= 2;
                    else if (rast_mapx < 32)
                          qq_shift <<= 1;
      
                    qq *= qq_shift;
      
      
//       is_raster_small = rast_mapx < 64;
//             if (is_small) {
//                   // don't show beats if measure is this small
//                   int n = 1;
//                   if (rast_mapx <= 2)
//                         n = 32;
//                   else if (rast_mapx <= 4)
//                         n = 16;
//                   else if (rast_mapx < 8)
//                         n = 8;
//                   else if (rast_mapx <= 16)
//                         n = 4;
//                   else if (rast_mapx < 32)
//                         n = 2;
//               
//                   if (bar % n)
// //                         continue;
//                         draw_bar = false;
//             }

      if (waveMode) {
// //             ctick = MusEGlobal::tempomap.frame2tick(mapxDev(x));
// //             ctick = MusEGlobal::tempomap.frame2tick(x);
// //             AL::sigmap.tickValues(ctick, &bar1, &beat, &tick);
// //             AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(mapxDev(x+w)),
// //                &bar2, &beat, &tick);
// //             AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(mx2), &bar2, &beat, &tick);
            AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(asUnmapped(x_lim)._value), &bar1, &beat, &tick);
            AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(asUnmapped(x_2lim)._value), &bar2, &beat, &tick);
            //AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(mapxDev(mx_lim)), &bar1, &beat, &tick);
            //AL::sigmap.tickValues(MusEGlobal::tempomap.frame2tick(mapxDev(mx_2lim)), &bar2, &beat, &tick);
            }
      else {
// //             ctick = mapxDev(x);
// //             AL::sigmap.tickValues(ctick, &bar1, &beat, &tick);
// //             AL::sigmap.tickValues(mapxDev(x+w), &bar2, &beat, &tick);
// //             ctick = x;
// //             AL::sigmap.tickValues(mx2, &bar2, &beat, &tick);
            AL::sigmap.tickValues(asUnmapped(x_lim)._value, &bar1, &beat, &tick);
            AL::sigmap.tickValues(asUnmapped(x_2lim)._value, &bar2, &beat, &tick);
            }

      //printf("bar %d  %d-%d=%d\n", bar, ntick, stick, ntick-stick);

      int stick = AL::sigmap.bar2tick(bar1, 0, 0);
      int ntick;
      ScaleRetStruct prev_scale_info;
      for (int bar = bar1; bar <= bar2; bar++, stick = ntick) {
//       for (int bar = bar1; bar < bar2; bar++, stick = ntick) {
            ntick     = AL::sigmap.bar2tick(bar+1, 0, 0);
//             int tpix, a, b=0;
            int a, b=0;
            double tpix;
            if (waveMode) {
                  a = MusEGlobal::tempomap.tick2frame(ntick);
                  b = MusEGlobal::tempomap.tick2frame(stick);
//                   tpix  = rmapx(a - b);
                     tpix  = rmapx_f(a - b);
               }
            else {
//                   tpix  = rmapx(ntick - stick);
                  tpix  = rmapx_f(ntick - stick);
                  }
                  
            //draw_fake_bar = /*true*/ false;
            is_beat_small = tpix < 32;
            
            scale_info = scale(drawText, bar, tpix);
            
            if(drawText && !scale_info._drawBar)
            {
              // Only check this on the first starting bar.
              if(bar == bar1)
              {
                int prev_a, prev_b=0;
                double prev_tpix;
                int prev_stick;
                int prev_ntick = stick;
                int prev_bar = bar1 - 1;
                for( ; prev_bar >= 0; --prev_bar, prev_ntick = prev_stick)
                {
                  prev_stick = AL::sigmap.bar2tick(prev_bar, 0, 0);
                  if (waveMode) {
                        prev_a = MusEGlobal::tempomap.tick2frame(prev_ntick);
                        prev_b = MusEGlobal::tempomap.tick2frame(prev_stick);
                        //prev_tpix  = rmapx(b - prev_b);
                        prev_tpix  = rmapx_f(prev_a - prev_b);
                    }
                  else {
                        //prev_tpix  = rmapx(stick - prev_stick);
                        prev_tpix  = rmapx_f(prev_ntick - prev_stick);
                        }
                  
                  prev_scale_info = scale(drawText, prev_bar, prev_tpix);

                  // REMOVE Tim. citem. Added.
//                   fprintf(stderr,
//                       "drawTickRaster: Bar check: bar1:%d bar2:%d bar:%d prev_bar:%d"
//                       " stick:%d prev_stick:%d prev_ntick:%d prev_tpix:%f drawBar:%d\n",
//                       bar1, bar2, bar, prev_bar, stick, prev_stick, prev_ntick, prev_tpix,
//                       prev_scale_info._drawBar);

                  if(prev_scale_info._drawBar)
                    break;
                }
                
                if(prev_bar >= 0)
                {
                  const int prev_tick = waveMode ? prev_b : prev_stick;
//                   drawBarText(p, prev_tick, prev_bar, vr, text_color, large_font);
                  drawBarText(p, prev_tick, prev_bar, mr, text_color, large_font);
                }
              }
                
              continue;
            }
            
            if(!drawText || (drawText && scale_info._isSmall))
            {
                  const int tick_sm = waveMode ? b : stick;
//                   const int x_small = mapx(tick_small);
                  
                  const ViewXCoordinate x_sm(tick_sm, false);
                  const int mx_sm = asMapped(x_sm)._value;
                  
                  if(drawText)
                    pen.setColor(text_color);
                  else
                    pen.setColor(bar_color);
                  p.setPen(pen);
                  
                  //if(scale_info._drawBar || draw_fake_bar)
                  if(scale_info._drawBar)
                  {
                    // REMOVE Tim. citem. Added.
//                     fprintf(stderr,
//                      "...is_small:%d mx:%d mw:%d tick_sm:%d mx_sm:%d stick:%d ntick:%d bar1:%d bar2:%d bar:%d\n",
//                      scale_info._isSmall, mx, mw, tick_sm, mx_sm, stick, ntick, bar1, bar2, bar);
                    
//                     if(tick_small >= vx && tick_small < vx_2)
                    if(isXInRange(x_sm, x, x_2))
                    {
                      // REMOVE Tim. citem. Added.
//                       fprintf(stderr, "...Line is within range."
//                             " Drawing line at mx_sm:%d my:%d mx_sm:%d mbottom:%d...\n",
//                             mx_sm, my, mx_sm, mbottom);
                      
//                       p.drawLine(x_small, my, x_small, mbottom);
//                       p.drawLine(mx_sm, asMapped(y)._value, mx_sm, asMapped(bottom)._value);
                      p.drawLine(mx_sm, my, mx_sm, mbottom);
                      
                      //p.drawLine(x_small, 0, x_small, height());
                    }
                  }
                  
                  if(drawText)
                  {
// //                     QRect r_txt = QRect(x_small+2, my, 1000, mh);
//                     QString s;
//                     s.setNum(bar + 1);
//                     
//                     const QRect br = fm.boundingRect(s);
//                     int brw = rmapxDev(br.width());
//                     //const int brh = rmapyDev(br.width());
//                     //int w_txt = rmapxDev(1000);
//                     //const int h_txt = rmapyDev(height() - 1);
//                     if(brw > vw1000)
//                       brw = vw1000;
//                     //if(brh < h_txt)
//                     //  h_txt = brh;
//                     const QRect br_txt = QRect(tick_small + vw2, vy, brw, vh_m1);
//                     
//                     // REMOVE Tim. citem. Added.
//                     fprintf(stderr,
//                         "drawTickRaster: Bar text: bar1:%d bar2:%d bar:%d vx:%d vy:%d"
//                         " vw:%d vh:%d mx:%d my:%d mw:%d mh:%d tick_small:%d x_small:%d br x:%d y:%d w:%d h:%d\n",
//                         bar1, bar2, bar, vx, vy, vw, vh, mr.x(), my, mr.width(), mr.height(),
//                         tick_small, x_small, br_txt.x(), br_txt.y(), br_txt.width(), br_txt.height());
//                     
//                     
//                     //if(r_txt.intersects(mr))
//                     if(br_txt.intersects(vr))
//                     {
//                       // REMOVE Tim. citem. Added.
//                       fprintf(stderr, "...Text is within range. Drawing text...\n");
//                       fprintf(stderr, "...bar text within range. xorg:%d xmag:%d xpos:%d Drawing bar text at x:%d y:%d w:%d h:%d\n",
//                               xorg, xmag, xpos, map(br_txt).x(), map(br_txt).y(), map(br_txt).width(), map(br_txt).height());
//                       p.setFont(large_font);
//                       //p.drawText(r_txt, Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
//                       p.drawText(map(br_txt), Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
//                     }
                    
//                     drawBarText(p, tick_small, bar, vr, text_color, large_font);
//                     drawBarText(p, tick_small, bar, mr, text_color, large_font);
                    drawBarText(p, tick_sm, bar, mr, text_color, large_font);
                  }
            }
//             else {

//             if(!drawText && !scale_info._drawBar && !draw_fake_bar)
//             if(!drawText && !scale_info._isSmall && !draw_fake_bar)
            if(!drawText && !scale_info._isSmall)
            {
              if (raster>=4) {
                          pen.setColor(Qt::darkGray);
                          p.setPen(pen);
                          rast_xb = AL::sigmap.bar2tick(bar, 0, 0);
                          AL::sigmap.timesig(rast_xb, rast_z, rast_n);
                          rast_xx = rast_xb + (scale_info._drawBar ? qq : 0);
                          rast_xxx = AL::sigmap.bar2tick(bar, rast_z, 0);
                          while (rast_xx <= rast_xxx) {
                                rast_x = mapx(rast_xx);
                                p.drawLine(rast_x, my, rast_x, mbottom);
                                rast_xx += qq;
                                }
                          }
            }
                        
            //if((!drawText && !is_small) || (drawText && !is_small)) {
            //if((!drawText && !is_beat_small && !scale_info._drawBar && !draw_fake_bar) || (drawText && !is_small)) {
            if((!drawText && !is_beat_small) || (drawText && !scale_info._isSmall)) {
                  int z, n;
                  AL::sigmap.timesig(stick, z, n);
                  
                  //for (int beat = 0; beat < z; beat++) {
//                   beat_start_beat = (drawText || (!drawText && !scale_info._drawBar && !draw_fake_bar)) ? 0 : 1;
                  beat_start_beat = (drawText || (!drawText && !scale_info._drawBar)) ? 0 : 1;
                  for (int beat = beat_start_beat; beat < z; beat++) {
                        int xx = AL::sigmap.bar2tick(bar, beat, 0);
                        if (waveMode)
                              xx = MusEGlobal::tempomap.tick2frame(xx);
                        
                        int xx_e = AL::sigmap.bar2tick(bar, beat + 1, 0);
                        if (waveMode)
                              xx_e = MusEGlobal::tempomap.tick2frame(xx_e);
                        
                        if((xx_e - xx) < raster)
                          continue;
                        
//                         int mxx = mapx(xx);
                        const ViewXCoordinate xx_v(xx, false);
                        const int mxx = asMapped(xx_v)._value;
                        //int xp_e = mapx(xx_e);
                        
                        if(drawText)
                        {
//                           QRect r(xp+2, y, 1000, h);
                          //QRect r(xp+2, my, 1000, mh);
                          int y1;
                          int num;
//                           int yt = vy;
                          int m_yt = my;
//                           ViewYCoordinate yt = y;
//                           int h_txt;
                          ViewHCoordinate h_txt;
                          
                          //const QString s = m->second.name();
                          //const QRect br = fm.boundingRect(s);
                          //const int brw = rmapxDev(br.width());
                          //const int brh = rmapyDev(br.width());
                          //const int h_txt = rmapyDev(12);
                          //if(brw < w_txt)
                          //  w_txt = brw;
                          //if(brh < h_txt)
                          //  h_txt = brh;
                          //const QRect br_txt = QRect(xp + rmapxDev(10), y, w_txt, h_txt);
                          
                          
                          if (beat == 0) {
                                num = bar + 1;
//                                 y1  = y + 1;
//                                 y1  = my_1;
                                y1  = my + 1;
//                                 h_txt = vh_m1;
//                                 h_txt = mh_m1;
                                h_txt = h_m1;
                                p.setFont(large_font);
                                }
                          else {
                                num = beat + 1;
//                                 y1  = y + 7;
//                                 y1  = my_7;
                                y1  = my + 6;
//                                 r.setY(y+3);
//                                 r.setY(my_3);
//                                 r.setY(my + 3);
//                                 yt = vy + vh2;
                                m_yt = my + mh2;
//                                 yt = mathYCoordinates(y, h2, MathAdd);
//                                 h_txt = vh_m3;
//                                 h_txt = mh_m3;
                                h_txt = h_m3;
                                p.setFont(small_font);
                                }

                          const ViewYCoordinate yt(m_yt, true);
                          
                          pen.setColor(text_color);
                          p.setPen(pen);
//                           if(xx >= vx && xx < vx_2)
                          if(isXInRange(xx_v, x, x_2))
                            //p.drawLine(mxx, y1, mxx, my2 + 1);
                            p.drawLine(mxx, y1, mxx, mbottom);
                                
                          QString s;
                          s.setNum(num);
//                           int brw = rmapxDev(p.fontMetrics().boundingRect(s).width());
//                           int brw = p.fontMetrics().boundingRect(s).width();
                          int brw = p.fontMetrics().boundingRect(s).width();
                          //int w_txt = vw1000;
//                           if(brw > vw1000)
//                             brw = vw1000;
                          if(brw > mw1000)
                            brw = mw1000;
                          
                          const ViewWCoordinate brw_v(brw, true);
                          
//                           QRect br_txt(xx + vw2, yt, brw, h_txt);
//                           const ViewRect br_txt(
//                             ViewXCoordinate(mxx + mw2, true),
//                             ViewYCoordinate(m_yt, true),
//                             ViewWCoordinate(brw, true),
//                             ViewHCoordinate(h_txt, true));
                          
                          const ViewXCoordinate vxx_v = 
                            mathXCoordinates(xx_v, w2, MathAdd);
                            
                          const ViewRect br_txt(vxx_v, yt, brw_v, h_txt);
                                
                          // REMOVE Tim. citem. Added.
//                           fprintf(stderr,
//                               "drawBarText: Bar text: bar:%d beat:%d vx:%d vy:%d"
//                               " vw:%d vh:%d tick:%d br x:%d y:%d w:%d h:%d\n",
//                               bar, beat, vr.x(), vr.y(), vr.width(), vr.height(), tick,
//                               br_txt.x(), br_txt.y(), br_txt.width(), br_txt.height());
  
//                           if(br_txt.intersects(vr))
                          if(intersects(br_txt, r))
                          {
                            // REMOVE Tim. citem. Added.
//                             fprintf(stderr, "...bar text within range. xorg:%d xmag:%d xpos:%d Drawing bar text at x:%d y:%d w:%d h:%d\n",
//                                     xorg, xmag, xpos, map(br_txt).x(), map(br_txt).y(), map(br_txt).width(), map(br_txt).height());
    
                            //pen.setColor(text_color);
  //                           p.drawLine(xp, y1, xp, y+1+h);
  //                           p.drawLine(xp, y1, xp, my2_1);
  //                           fprintf(stderr, "View::drawTickRaster_new(): beat: my:%d y1:%d\n", my, y1);
                            //p.setPen(pen);
                            //p.drawLine(xp, y1, xp, my2 + 1);
                            //QString s;
                            //s.setNum(num);
//                             p.drawText(map(br_txt), Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
                            p.drawText(asQRectMapped(br_txt), Qt::AlignLeft|Qt::AlignVCenter|Qt::TextDontClip, s);
                          }
                        }
                        else
                        {
//                           if(xx >= vx && xx < vx_2)
                          if(isXInRange(xx_v, x, x_2))
                          {
                            pen.setColor(beat_color);
                            p.setPen(pen);
                            p.drawLine(mxx, my, mxx, mbottom);
                          }
                        }
                        }
                  }
            }

      //p.setWorldMatrixEnabled(true);
      p.setWorldMatrixEnabled(wmtxen);
      //p.restore();      











      
//       int xx,bar1, bar2, beat;
//       int rast_mapx;
//       unsigned tick;
//       AL::sigmap.tickValues(x, &bar1, &beat, &tick);
//       AL::sigmap.tickValues(x+w, &bar2, &beat, &tick);
//       ++bar2;
//       ///int y2 = y + h;
//       //int y2 = my + mh;
//       int y2 = mapy(y + h) - 1;
//       //printf("View::drawTickRaster x:%d y:%d w:%d h:%d mx:%d my:%d mw:%d mh:%d y2:%d bar1:%d bar2:%d\n", x, y, w, h, mx, my, mw, mh, y2, bar1, bar2);  
//       //printf("View::drawTickRaster x:%d y:%d w:%d h:%d my:%d mh:%d y2:%d bar1:%d bar2:%d\n", x, y, w, h, my, mh, y2, bar1, bar2);  
//       for (int bar = bar1; bar < bar2; ++bar) {
//             ///unsigned x = AL::sigmap.bar2tick(bar, 0, 0);
//             unsigned xb = AL::sigmap.bar2tick(bar, 0, 0);
//             int xt = mapx(xb);
//             p.setPen(MusEGlobal::config.midiCanvasBarColor);
//             ///p.drawLine(x, y, x, y2);
//             p.drawLine(xt, my, xt, y2);
//             int z, n;
//             ///AL::sigmap.timesig(x, z, n);
//             AL::sigmap.timesig(xb, z, n);
//             ///int q = p.xForm(QPoint(raster, 0)).x() - p.xForm(QPoint(0, 0)).x();
//             ///int q = p.combinedTransform().map(QPoint(raster, 0)).x() - p.combinedTransform().map(QPoint(0, 0)).x();
//             //int q = rmapx(raster);
//             int qq = raster;
//             //if (q < 8)        // grid too dense
//             
// // REMOVE Tim. citem. Changed.
// //             if (rmapx(raster) < 8)        // grid too dense
// //                   qq *= 2;
// 
//             rast_mapx = rmapx(raster);
//             // grid too dense?
//             if (rast_mapx <= 1)        
//                   qq *= 16;
//             else if (rast_mapx <= 2)
//                   qq *= 8;
//             else if (rast_mapx <= 4)
//                   qq *= 4;
//             else if (rast_mapx <= 8)
//                   qq *= 2;
//             
//             p.setPen(MusEGlobal::config.midiCanvasBeatColor);
//             if (raster>=4) {
//                         ///int xx = x + qq;
//                         //int xx = mapx(xb + qq);
//                         xx = xb + qq;
//                         int xxx = AL::sigmap.bar2tick(bar, z, 0);
//                         //int xxx = mapx(AL::sigmap.bar2tick(bar, z, 0));
//                         while (xx <= xxx) {
//                                ///p.drawLine(xx, y, xx, y2);
//                                int x = mapx(xx);
//                                p.drawLine(x, my, x, y2);
//                                xx += qq;
//                                //xx += rmapx(qq);
//                                }
//                         //xx = xxx;
//                         }
// 
//             p.setPen(Qt::darkGray);
//             for (int beat = 1; beat < z; beat++) {
//                         ///int xx = AL::sigmap.bar2tick(bar, beat, 0);
//                         xx = mapx(AL::sigmap.bar2tick(bar, beat, 0));
//                         //printf(" bar:%d z:%d beat:%d xx:%d\n", bar, z, beat, xx);  
//                         ///p.drawLine(xx, y, xx, y2);
//                         p.drawLine(xx, my, xx, y2);
//                         }
// 
//             }
//       //p.setWorldMatrixEnabled(true);
//       p.setWorldMatrixEnabled(wmtxen);
//       //p.restore();      
}

//---------------------------------------------------------
//   map
//---------------------------------------------------------

QRect View::mapDev(const QRect& r) const
{
  return QRect(mapxDev(r.x()), mapyDev(r.y()),
// REMOVE Tim. citem. Changed.
//     rmapxDev(r.width()), rmapyDev(r.height()));
    rmapxDev(r.width(), true), rmapyDev(r.height(), true));
}

QPoint View::mapDev(const QPoint& r) const
{
  return QPoint(mapxDev(r.x()), mapyDev(r.y()));
}

void View::mapDev(const QRegion& rg_in, QRegion& rg_out) const
{
  for(QRegion::const_iterator i = rg_in.begin(); i != rg_in.end(); ++i)
    rg_out += mapDev(*i);
}

#if 0
  //
  // Calculations using integer rounding methods...
  //

QRect View::map(const QRect& r) const
      {
      int x, y, w, h;
      //printf("View::map xmag:%d xpos:%d xorg:%d\n", xmag, xpos, xorg);  
      if (xmag < 0) {
            x = r.x()/(-xmag) - (xpos + rmapx(xorg));  // round down
            w = (r.width()-xmag-1)  / (-xmag);  // round up
            }
      else {
            x = r.x()*xmag - (xpos + rmapx(xorg));
            w = r.width() * xmag;
            }
      if (ymag < 0) {
            y = r.y()/-ymag - (ypos + rmapy(yorg));
            h = (r.height()-ymag-1) / (-ymag);
            }
      else {
            y = r.y() * ymag - (ypos + rmapy(yorg));
            h = r.height() * ymag;
            }
      return QRect(x, y, w, h);
      }

QPoint View::map(const QPoint& p) const
      {
      int x, y;
      if (xmag < 0) {
            x = p.x()/(-xmag) - (xpos + rmapx(xorg));  // round down
            }
      else {
            x = p.x()*xmag - (xpos + rmapx(xorg));
            }
      if (ymag < 0) {
            y = p.y()/-ymag - (ypos + rmapy(yorg));
            }
      else {
            y = p.y() * ymag - (ypos + rmapy(yorg));
            }
      return QPoint(x, y);
      }

int View::mapx(int x) const
      {
      if (xmag < 0) {
            return (x-xmag/2)/(-xmag) - (xpos + rmapx(xorg));  // round
            }
      else {
            return (x * xmag) - (xpos + rmapx(xorg));
            }
      }
int View::mapy(int y) const
      {
      if (ymag < 0) {
            return (y-ymag/2)/(-ymag) - (ypos + rmapy(yorg));  // round
            }
      else {
            return (y * ymag) - (ypos + rmapy(yorg));
            }
      }
int View::mapxDev(int x) const
      {
      int val;
      if (xmag <= 0)
            val = (x + xpos + rmapx(xorg)) * (-xmag);
      else
            val = (x + xpos + rmapx(xorg) + xmag / 2) / xmag;
      if (val < 0)            // DEBUG
            val = 0;
      return val;
      }

int View::mapyDev(int y) const
      {
      if (ymag <= 0)
            return (y + ypos + rmapy(yorg)) * (-ymag);
      else
            return (y + ypos + rmapy(yorg) + ymag / 2) / ymag;
      }

// r == relative conversion
int View::rmapx(int x) const
      {
      if (xmag < 0)
            return (x-xmag/2) / (-xmag);
      else
            return x * xmag;
      }
int View::rmapy(int y) const
      {
      if (ymag < 0)
            return (y-ymag/2) / (-ymag);
      else
            return y * ymag;
      }
int View::rmapxDev(int x) const
      {
      if (xmag <= 0)
            return x * (-xmag);
      else
            return (x + xmag/2) / xmag;
      }
int View::rmapyDev(int y) const
      {
      if (ymag <= 0)
            return y * (-ymag);
      else
            return (y + ymag/2) / ymag;
      }


#else
  //
  // Calculations using more accurate methods...  p4.0.29 Tim.
  //

// REMOVE Tim. citem. Changed.
// QRect View::map(const QRect& r) const
//       {
//       //int x, y, w, h;
// //       double x, y, w, h;
// //       double w, h;
// //       int w, h;
//       int xx, xx2, yy, yy2, ww, hh;
//       xx = mapx(r.x());
//       xx2 = mapx(r.x() + r.width());
//       yy = mapy(r.y());
//       yy2 = mapy(r.y() + r.height());
//       //ww = rmapx(r.width());
//       //hh = rmapy(r.height());
//       ww = xx2 - xx;
//       hh = yy2 - yy;
// //       //printf("View::map xmag:%d xpos:%d xorg:%d\n", xmag, xpos, xorg);  
// //       if (xmag < 0) {
// //             //x = lrint(double(r.x())/double(-xmag) - rmapx_f(xorg)) - xpos;  
// //             // REMOVE Tim. citem. Changed.
// // //             x = double(r.x())/double(-xmag) - rmapx_f(xorg) - xpos;  
// //             //x = double(r.x())/double(-xmag) - double(xorg) / double(-xmag) - xpos;  
// //             //x = double(r.x() - xorg) / double(-xmag) - xpos;  
// //             //xx = mapx(r.x());
// //             
// //             //w = lrint(double(r.width()) / double(-xmag));  
// //             w = floor(double(r.width()) / double(-xmag));
// // //             xx = lrint(x);
// // //             ww = lrint(x + w) - xx;
// // //             ww = xx + lrint(w) - xx;
// //             ww = xx + w - xx;
// //             }
// //       else {
// //             // REMOVE Tim. citem. Changed.
// // //             xx = r.x()*xmag - xpos - lrint(rmapx_f(xorg));
// //             //xx = r.x() * xmag - xpos - lrint(double(xorg) * double(xmag));
// //             //xx = r.x() * xmag - xpos - lrint(double(xorg) * double(xmag));
// //             //xx = (r.x() - xorg) * xmag - xpos;
// //             //xx = mapx(r.x());
// // 
// //             ww = r.width() * xmag;
// //             }
// //       if (ymag < 0) {
// //             //y = lrint(double(r.y())/double(-ymag) - rmapy_f(yorg)) - ypos;
// //             // REMOVE Tim. citem. Changed. 
// // //             y = double(r.y())/double(-ymag) - rmapy_f(yorg) - ypos;
// //             //y = double(r.y()) / double(-ymag) - double(yorg) / double(-ymag) - ypos;
// //             //y = double(r.y() - yorg) / double(-ymag) - ypos;
// //             //yy = mapy(r.y());
// //             
// //             //h = lrint(double(r.height()) / double(-ymag));
// //             h = floor(double(r.height()) / double(-ymag));
// // //             yy = lrint(y);
// // //             hh = lrint(y + h) - yy;
// // //             hh = yy + lrint(h) - yy;
// //             hh = yy + h - yy;
// //             }
// //       else {
// //             // REMOVE Tim. citem. Changed.
// // //             yy = r.y()*ymag - ypos - lrint(rmapy_f(yorg));
// //             //yy = r.y() * ymag - ypos - lrint(double(yorg * double(ymag)));
// //             //yy = (r.y() - yorg) * ymag - ypos;
// //             //yy = mapy(r.y());
// // 
// //             hh = r.height() * ymag;
// //             }
//       return QRect(xx, yy, ww, hh);
//       }

      
QRect View::map(const QRect& r) const
{
  return QRect(mapx(r.x()), mapy(r.y()),
    rmapx(r.width(), true), rmapy(r.height(), true));
//     rmapx(r.width(), false), rmapy(r.height(), false));
}

QPoint View::map(const QPoint& p) const
      {
      /*
      int x, y;
      if (xmag < 0) {
            x = lrint(double(p.x())/double(-xmag) - rmapx_f(xorg)) - xpos;  
            }
      else {
            x = p.x()*xmag - xpos - lrint(rmapx_f(xorg));
            }
      if (ymag < 0) {
            y = lrint(double(p.y())/double(-ymag) - rmapy_f(yorg)) - ypos;
            }
      else {
            y = p.y()*ymag - ypos - lrint(rmapy_f(yorg));
            }
      return QPoint(x, y);
      */
      return QPoint(mapx(p.x()), mapy(p.y()));
      }

void View::map(const QRegion& rg_in, QRegion& rg_out) const
{
  for(QRegion::const_iterator i = rg_in.begin(); i != rg_in.end(); ++i)
    rg_out += map(*i);
}
      
int View::mapx(int x) const
      {
      if (xmag < 0)
            // REMOVE Tim. citem. Changed.
//             return lrint(double(x)/double(-xmag) - rmapx_f(xorg)) - xpos;  
            //return lrint(double(x) / double(-xmag) - double(xorg) / double(-xmag)) - xpos;  
            return floor(double(x - xorg) / double(-xmag)) - xpos;

      else 
//             return x*xmag - xpos - lrint(rmapx_f(xorg));
            //return x*xmag - xpos - lrint(double(xorg) * double(xmag));
            //return x * xmag - xpos - xorg * xmag;
            return (x - xorg) * xmag - xpos;

      }
int View::mapy(int y) const
      {
      if (ymag < 0)
            // REMOVE Tim. citem. Changed.
//             return lrint(double(y)/double(-ymag) - rmapy_f(yorg)) - ypos;  
            //return lrint(double(y) / double(-ymag) - (double(yorg) / double(-ymag))) - ypos;
            return floor(double(y - yorg) / double(-ymag)) - ypos;
      else
//             return y*ymag - ypos - lrint(rmapy_f(yorg));
            //return y*ymag - ypos - lrint(double(yorg) * double(ymag));
            //return y * ymag - ypos - yorg * ymag;
            return (y - yorg) * ymag - ypos;
      }
int View::mapxDev(int x) const
      {
      if (xmag <= 0)
            // REMOVE Tim. citem. Changed.
//             val = lrint((double(x + xpos) + rmapx_f(xorg)) * double(-xmag));
            //val = lrint((double(x + xpos) + double(xorg) / double(-xmag)) * double(-xmag));
            return  (x + xpos) * -xmag + xorg;
      else
            // REMOVE Tim. citem. Changed.
//             val = lrint((double(x + xpos) + rmapx_f(xorg)) / double(xmag));  
            //val = lrint((double(x + xpos) + double(xorg) * double(xmag)) / double(xmag));  
            //val = lrint((double(x + xpos) + double(xorg) * double(xmag)) / double(xmag));  
            return floor(double(x + xpos) / double(xmag)) + xorg;
            
// REMOVE Tim. citem. Removed.
//       if (val < 0)            // DEBUG
//             val = 0;
      }

int View::mapyDev(int y) const
      {
      if (ymag <= 0)
            // REMOVE Tim. citem. Changed.
//             return lrint((double(y + ypos) + rmapy_f(yorg)) * double(-ymag));
            //return lrint((double(y + ypos) + double(yorg) / double(-ymag)) * double(-ymag));
            return (y + ypos) * -ymag + yorg;
      else
            // REMOVE Tim. citem. Changed.
//             return lrint((double(y + ypos) + rmapy_f(yorg)) / double(ymag));
            //return lrint((double(y + ypos) + double(yorg) * double(ymag)) / double(ymag));
            //return lrint((double(y + ypos) + double(yorg) * double(ymag)) / double(ymag));
            return floor(double(y + ypos) / double(ymag)) + yorg;
      }

//-----------------------------
// r == relative conversion
//-----------------------------

// REMOVE Tim. citem. Changed.
// QRect View::rmap(const QRect& r, bool round) const
// {
//      //int x, y, w, h;
// //       double x, y, w, h;
// //       double w, h;
// //       int w, h;
//       int xx, xx2, yy, yy2, ww, hh;
//       xx = rmapx(r.x());
//       xx2 = rmapx(r.x() + r.width(), round);
//       yy = rmapy(r.y());
//       yy2 = rmapy(r.y() + r.height(), round);
//       ww = xx2 - xx;
//       hh = yy2 - yy;
// //       //printf("View::map xmag:%d xpos:%d xorg:%d\n", xmag, xpos, xorg);  
// //       if (xmag < 0) {
// //             //x = lrint(double(r.x())/double(-xmag) - rmapx_f(xorg)) - xpos;  
// //             // REMOVE Tim. citem. Changed.
// // //             x = double(r.x())/double(-xmag) - rmapx_f(xorg) - xpos;  
// //             //x = double(r.x())/double(-xmag) - double(xorg) / double(-xmag) - xpos;  
// //             //x = double(r.x() - xorg) / double(-xmag) - xpos;  
// //             //xx = mapx(r.x());
// //             
// //             //w = lrint(double(r.width()) / double(-xmag));  
// //             w = floor(double(r.width()) / double(-xmag));
// // //             xx = lrint(x);
// // //             ww = lrint(x + w) - xx;
// // //             ww = xx + lrint(w) - xx;
// //             ww = xx + w - xx;
// //             }
// //       else {
// //             // REMOVE Tim. citem. Changed.
// // //             xx = r.x()*xmag - xpos - lrint(rmapx_f(xorg));
// //             //xx = r.x() * xmag - xpos - lrint(double(xorg) * double(xmag));
// //             //xx = r.x() * xmag - xpos - lrint(double(xorg) * double(xmag));
// //             //xx = (r.x() - xorg) * xmag - xpos;
// //             //xx = mapx(r.x());
// // 
// //             ww = r.width() * xmag;
// //             }
// //       if (ymag < 0) {
// //             //y = lrint(double(r.y())/double(-ymag) - rmapy_f(yorg)) - ypos;
// //             // REMOVE Tim. citem. Changed. 
// // //             y = double(r.y())/double(-ymag) - rmapy_f(yorg) - ypos;
// //             //y = double(r.y()) / double(-ymag) - double(yorg) / double(-ymag) - ypos;
// //             //y = double(r.y() - yorg) / double(-ymag) - ypos;
// //             //yy = mapy(r.y());
// //             
// //             //h = lrint(double(r.height()) / double(-ymag));
// //             h = floor(double(r.height()) / double(-ymag));
// // //             yy = lrint(y);
// // //             hh = lrint(y + h) - yy;
// // //             hh = yy + lrint(h) - yy;
// //             hh = yy + h - yy;
// //             }
// //       else {
// //             // REMOVE Tim. citem. Changed.
// // //             yy = r.y()*ymag - ypos - lrint(rmapy_f(yorg));
// //             //yy = r.y() * ymag - ypos - lrint(double(yorg * double(ymag)));
// //             //yy = (r.y() - yorg) * ymag - ypos;
// //             //yy = mapy(r.y());
// // 
// //             hh = r.height() * ymag;
// //             }
//       return QRect(xx, yy, ww, hh);
// }

//QRect View::rmap(const QRect& r, bool round) const
QRect View::rmap(const QRect& r) const
{
  return QRect(rmapx(r.x()), rmapy(r.y()),
    rmapx(r.width(), true), rmapy(r.height(), true));
}

int View::rmapx(int x, bool round) const
      {
      if (xmag < 0)
      {
            // REMOVE Tim. citem. Changed.
//             return lrint(double(x) / double(-xmag));
            if(round)
              return ceil(double(x) / double(-xmag));
            else
              return floor(double(x) / double(-xmag));
      }
      else
            return x * xmag;
      }
int View::rmapy(int y, bool round) const
      {
      if (ymag < 0)
      {
            // REMOVE Tim. citem. Changed.
//             return lrint(double(y) / double(-ymag));
            if(round)
              return ceil(double(y) / double(-ymag));
            else
              return floor(double(y) / double(-ymag));
      }
      else
            return y * ymag;
      }
int View::rmapxDev(int x, bool round) const
      {
      if (xmag <= 0)
            return x * (-xmag);
      else
      {
            // REMOVE Tim. citem. Changed.
//             return lrint(double(x) / double(xmag));
            if(round)
              return ceil(double(x) / double(xmag));
            else
              return floor(double(x) / double(xmag));
      }
      }
int View::rmapyDev(int y, bool round) const
      {
      if (ymag <= 0)
            return y * (-ymag);
      else
      {
            // REMOVE Tim. citem. Changed.
//             return lrint(double(y) / double(ymag));
            if(round)
              return ceil(double(y) / double(ymag));
            else
              return floor(double(y) / double(ymag));
      }
      }
#endif

double View::rmapx_f(double x) const
      {
      if (xmag < 0)
            return x / double(-xmag);
      else
            return x * double(xmag);
      }
double View::rmapy_f(double y) const
      {
      if (ymag < 0)
            return y / double(-ymag);
      else
            return y * double(ymag);
      }
double View::rmapxDev_f(double x) const
      {
      if (xmag <= 0)
            return x * double(-xmag);
      else
            return x / double(xmag);
      }
double View::rmapyDev_f(double y) const
      {
      if (ymag <= 0)
            return y * double(-ymag);
      else
            return y / double(ymag);
      }

//--------------------------------------------------
// Lossless intersections (in x and/or y, depending)
//  for any magnification.
//--------------------------------------------------

void ViewRect::dump(const char* header) const
{
  if(header)
    fprintf(stderr, "%s\n", header);
  else
    fprintf(stderr, "ViewRect dump:\n");
  
  fprintf(stderr, "x:%8d  map:%d rel:%d\t  y:%8d  map:%d rel:%d\t  w:%8d  map:%d rel:%d\t  h:%8d  map:%d rel:%d\n\n",
          _x._value, _x.isMapped(), _x.isRelative(),
          _y._value, _y.isMapped(), _y.isRelative(),
          _width._value, _width.isMapped(), _width.isRelative(),
          _height._value, _height.isMapped(), _height.isRelative());
}


// ViewXCoordinate::ViewXCoordinate(const ViewWCoordinate& w) : ViewCoordinate(w._value, w._flags) { _flags &= ~IsRelative; } 
// ViewYCoordinate::ViewYCoordinate(const ViewHCoordinate& h) : ViewCoordinate(h._value, h._flags) { _flags &= ~IsRelative; } 
// ViewWCoordinate::ViewWCoordinate(const ViewXCoordinate& x) : ViewXCoordinate(x._value, x._flags) { _flags |= IsRelative; } 
// ViewHCoordinate::ViewHCoordinate(const ViewYCoordinate& y) : ViewYCoordinate(y._value, y._flags) { _flags |= IsRelative; } 

// Returns the intersection of the rectangles. The coordinate x, y, and w, h are mapped
//  if the view's xmag > 0, and unmapped if not. The coordinate w, h are mapped if the
//  view's ymag > 0, and unmapped if not.
ViewRect View::intersected(const ViewRect& r1, const ViewRect& r2) const
{
  int fx_1, fw, fy_1, fh;
  const bool xunmap = xmag <= 0;
  const bool yunmap = ymag <= 0;
  if (xunmap)
  {
    const int vr1x = r1._x.isMapped() ? mapxDev(r1._x._value) : r1._x._value;
    const int vr1x_2 = vr1x + (r1._width.isMapped() ? rmapxDev(r1._width._value) : r1._width._value);
    
    const int vr2x = r2._x.isMapped() ? mapxDev(r2._x._value) : r2._x._value;
    const int vr2x_2 = vr2x + (r2._width.isMapped() ? rmapxDev(r2._width._value) : r2._width._value);
    
    fx_1 = vr1x > vr2x ? vr1x : vr2x;
    const int fx_2 = vr1x_2 < vr2x_2 ? vr1x_2 : vr2x_2;
    fw = fx_2 - fx_1;
    
    // REMOVE Tim. citem. Added.
//     fprintf(stderr, "View::intersected: xmag:%d vr1x:%d vr1x_2:%d vr2x:%d vr2x_2:%d fx_1:%d fx_2:%d\n",
//             xmag, vr1x, vr1x_2, vr2x, vr2x_2, fx_1, fx_2);
  }
  else
  {
    const int mr1x = r1._x.isMapped() ? r1._x._value : mapx(r1._x._value);
    const int mr1x_2 = mr1x + (r1._width.isMapped() ? r1._width._value : rmapx(r1._width._value));
    
    const int mr2x = r2._x.isMapped() ? r2._x._value : mapx(r2._x._value);
    const int mr2x_2 = mr2x + (r2._width.isMapped() ? r2._width._value : rmapx(r2._width._value));
    
    fx_1 = mr1x > mr2x ? mr1x : mr2x;
    const int fx_2 = mr1x_2 < mr2x_2 ? mr1x_2 : mr2x_2;
    fw = fx_2 - fx_1;
    
    // REMOVE Tim. citem. Added.
//     fprintf(stderr, "View::intersected: xmag:%d mr1x:%d mr1x_2:%d mr2x:%d mr2x_2:%d fx_1:%d fx_2:%d\n",
//             xmag, mr1x, mr1x_2, mr2x, mr2x_2, fx_1, fx_2);
  }
  
  if (ymag <= 0)
  {
    const int vr1y = r1._y.isMapped() ? mapyDev(r1._y._value) : r1._y._value;
    const int vr1y_2 = vr1y + (r1._height.isMapped() ? rmapyDev(r1._height._value) : r1._height._value);
    
    const int vr2y = r2._y.isMapped() ? mapyDev(r2._y._value) : r2._y._value;
    const int vr2y_2 = vr2y + (r2._height.isMapped() ? rmapyDev(r2._height._value) : r2._height._value);
    
    fy_1 = vr1y > vr2y ? vr1y : vr2y;
    const int fy_2 = vr1y_2 < vr2y_2 ? vr1y_2 : vr2y_2;
    fh = fy_2 - fy_1;
    
    // REMOVE Tim. citem. Added.
//     fprintf(stderr, "View::intersected: ymag:%d vr1y:%d vr1y_2:%d vr2y:%d vr2y_2:%d fy_1:%d fy_2:%d\n",
//             ymag, vr1y, vr1y_2, vr2y, vr2y_2, fy_1, fy_2);
  }
  else
  {
    const int mr1y = r1._y.isMapped() ? r1._y._value : mapy(r1._y._value);
    const int mr1y_2 = mr1y + (r1._height.isMapped() ? r1._height._value : rmapy(r1._height._value));
    
    const int mr2y = r2._y.isMapped() ? r2._y._value : mapy(r2._y._value);
    const int mr2y_2 = mr2y + (r2._height.isMapped() ? r2._height._value : rmapy(r2._height._value));
    
    fy_1 = mr1y > mr2y ? mr1y : mr2y;
    const int fy_2 = mr1y_2 < mr2y_2 ? mr1y_2 : mr2y_2;
    fh = fy_2 - fy_1;
    
    // REMOVE Tim. citem. Added.
//     fprintf(stderr, "View::intersected: ymag:%d mr1y:%d mr1y_2:%d mr2y:%d mr2y_2:%d fy_1:%d fy_2:%d\n",
//             ymag, mr1y, mr1y_2, mr2y, mr2y_2, fy_1, fy_2);
  }
  
  return ViewRect(ViewXCoordinate(fx_1, !xunmap),
                  ViewYCoordinate(fy_1, !yunmap),
                  ViewWCoordinate(fw,   !xunmap),
                  ViewHCoordinate(fh,   !yunmap));
}

// Returns the mapped intersection of the mapped rectangle and the unmapped rectangle.
QRect View::intersectedMap(const QRect& mapped_r, const QRect& unmapped_r) const
{
  int mfx_1, mfw, mfy_1, mfh;
  if (xmag <= 0)
  {
    const int vmx = mapxDev(mapped_r.x());
    const int vmx_2 = vmx + rmapxDev(mapped_r.width());
    const int vx = unmapped_r.x();
    const int vx_2 = vx + unmapped_r.width();
    const int vfx_1 = vmx > vx ? vmx : vx;
    const int vfx_2 = vmx_2 < vx_2 ? vmx_2 : vx_2;
    mfx_1 = mapx(vfx_1);
    mfw = rmapx(vfx_2 - vfx_1);
  }
  else
  {
    const int mx = mapped_r.x();
    const int mx_2 = mx + mapped_r.width();
    const int mvx = mapx(unmapped_r.x());
    const int mvx_2 = mvx + rmapx(unmapped_r.width());
    mfx_1 = mx > mvx ? mx : mvx;
    const int mfx_2 = mx_2 < mvx_2 ? mx_2 : mvx_2;
    mfw = mfx_2 - mfx_1;
  }
  
  if (ymag <= 0)
  {
    const int vmy = mapyDev(mapped_r.y());
    const int vmy_2 = vmy + rmapyDev(mapped_r.height());
    const int vy = unmapped_r.y();
    const int vy_2 = vy + unmapped_r.height();
    const int vfy_1 = vmy > vy ? vmy : vy;
    const int vfy_2 = vmy_2 < vy_2 ? vmy_2 : vy_2;
    mfy_1 = mapy(vfy_1);
    mfh = rmapy(vfy_2 - vfy_1);
  }
  else
  {
    const int my = mapped_r.y();
    const int my_2 = my + mapped_r.height();
    const int mvy = mapy(unmapped_r.y());
    const int mvy_2 = mvy + rmapy(unmapped_r.height());
    
    mfy_1 = my > mvy ? my : mvy;
    const int mfy_2 = my_2 < mvy_2 ? my_2 : mvy_2;
    mfh = mfy_2 - mfy_1;
  }
  
  return QRect(mfx_1, mfy_1, mfw, mfh);
}

// Returns the unmapped intersection of the mapped rectangle and the unmapped rectangle.
QRect View::intersectedUnmap(const QRect& mapped_r, const QRect& unmapped_r) const
{
  int vfx_1, vfw, vfy_1, vfh;
  if (xmag <= 0)
  {
    const int vmx = mapxDev(mapped_r.x());
    const int vmx_2 = vmx + rmapxDev(mapped_r.width());
    const int vx = unmapped_r.x();
    const int vx_2 = vx + unmapped_r.width();
    
    vfx_1 = vmx > vx ? vmx : vx;
    const int vfx_2 = vmx_2 < vx_2 ? vmx_2 : vx_2;
    vfw = vfx_2 - vfx_1;
  }
  else
  {
    const int mx = mapped_r.x();
    const int mx_2 = mx + mapped_r.width();
    const int mvx = mapx(unmapped_r.x());
    const int mvx_2 = mvx + rmapx(unmapped_r.width());
    const int mfx_1 = mx > mvx ? mx : mvx;
    const int mfx_2 = mx_2 < mvx_2 ? mx_2 : mvx_2;
    vfx_1 = mapxDev(mfx_1);
    vfw = rmapxDev(mfx_2 - mfx_1);
  }
  
  if (ymag <= 0)
  {
    const int vmy = mapyDev(mapped_r.y());
    const int vmy_2 = vmy + rmapyDev(mapped_r.height());
    const int vy = unmapped_r.y();
    const int vy_2 = vy + unmapped_r.height();

    vfy_1 = vmy > vy ? vmy : vy;
    const int vfy_2 = vmy_2 < vy_2 ? vmy_2 : vy_2;
    vfh = vfy_2 - vfy_1;
  }
  else
  {
    const int my = mapped_r.y();
    const int my_2 = my + mapped_r.height();
    const int mvy = mapy(unmapped_r.y());
    const int mvy_2 = mvy + rmapy(unmapped_r.height());
    
    const int mfy_1 = my > mvy ? my : mvy;
    const int mfy_2 = my_2 < mvy_2 ? my_2 : mvy_2;
    vfy_1 = mapyDev(mfy_1);
    vfh = rmapyDev(mfy_2 - mfy_1);
  }
  
  return QRect(vfx_1, vfy_1, vfw, vfh);
}

// Returns true if the mapped rectangle intersects the unmapped rectangle.
bool View::intersects(const QRect& mapped_r, const QRect& unmapped_r) const
{
  int vfx_1, vfx_2, vfy_1, vfy_2;
  if (xmag <= 0)
  {
    const int vmx = mapxDev(mapped_r.x());
    const int vmx_2 = vmx + rmapxDev(mapped_r.width());
    const int vx = unmapped_r.x();
    const int vx_2 = vx + unmapped_r.width();
    
    vfx_1 = vmx > vx ? vmx : vx;
    vfx_2 = vmx_2 < vx_2 ? vmx_2 : vx_2;
  }
  else
  {
    const int mx = mapped_r.x();
    const int mx_2 = mx + mapped_r.width();
    const int mvx = mapx(unmapped_r.x());
    const int mvx_2 = mvx + rmapx(unmapped_r.width());
    
    vfx_1 = mx > mvx ? mx : mvx;
    vfx_2 = mx_2 < mvx_2 ? mx_2 : mvx_2;
  }
  
  if (ymag <= 0)
  {
    const int vmy = mapyDev(mapped_r.y());
    const int vmy_2 = vmy + rmapyDev(mapped_r.height());
    const int vy = unmapped_r.y();
    const int vy_2 = vy + unmapped_r.height();

    vfy_1 = vmy > vy ? vmy : vy;
    vfy_2 = vmy_2 < vy_2 ? vmy_2 : vy_2;
  }
  else
  {
    const int my = mapped_r.y();
    const int my_2 = my + mapped_r.height();
    const int mvy = mapy(unmapped_r.y());
    const int mvy_2 = mvy + rmapy(unmapped_r.height());
    
    vfy_1 = my > mvy ? my : mvy;
    vfy_2 = my_2 < mvy_2 ? my_2 : mvy_2;
  }
  
  return vfx_1 < vfx_2 && vfy_1 < vfy_2;
}
      
// Returns true if the rectangles intersect.
bool View::intersects(const ViewRect& r1, const ViewRect& r2) const
{
  int fx_1, fx_2, fy_1, fy_2;
  if (xmag <= 0)
  {
    const int vr1x = r1._x.isMapped() ? mapxDev(r1._x._value) : r1._x._value;
    const int vr1x_2 = vr1x + (r1._width.isMapped() ? rmapxDev(r1._width._value) : r1._width._value);
    
    const int vr2x = r2._x.isMapped() ? mapxDev(r2._x._value) : r2._x._value;
    const int vr2x_2 = vr2x + (r2._width.isMapped() ? rmapxDev(r2._width._value) : r2._width._value);
    
    fx_1 = vr1x > vr2x ? vr1x : vr2x;
    fx_2 = vr1x_2 < vr2x_2 ? vr1x_2 : vr2x_2;
    
    // REMOVE Tim. citem. Added.
//     fprintf(stderr, "View::intersects: xmag:%d vr1x:%d vr1x_2:%d vr2x:%d vr2x_2:%d fx_1:%d fx_2:%d\n",
//             xmag, vr1x, vr1x_2, vr2x, vr2x_2, fx_1, fx_2);
  }
  else
  {
    const int mr1x = r1._x.isMapped() ? r1._x._value : mapx(r1._x._value);
    const int mr1x_2 = mr1x + (r1._width.isMapped() ? r1._width._value : rmapx(r1._width._value));
    
    const int mr2x = r2._x.isMapped() ? r2._x._value : mapx(r2._x._value);
    const int mr2x_2 = mr2x + (r2._width.isMapped() ? r2._width._value : rmapx(r2._width._value));
    
    fx_1 = mr1x > mr2x ? mr1x : mr2x;
    fx_2 = mr1x_2 < mr2x_2 ? mr1x_2 : mr2x_2;
    
    // REMOVE Tim. citem. Added.
//     fprintf(stderr, "View::intersects: xmag:%d mr1x:%d mr1x_2:%d mr2x:%d mr2x_2:%d fx_1:%d fx_2:%d\n",
//             xmag, mr1x, mr1x_2, mr2x, mr2x_2, fx_1, fx_2);
  }
  
  if (ymag <= 0)
  {
    const int vr1y = r1._y.isMapped() ? mapyDev(r1._y._value) : r1._y._value;
    const int vr1y_2 = vr1y + (r1._height.isMapped() ? rmapyDev(r1._height._value) : r1._height._value);
    
    const int vr2y = r2._y.isMapped() ? mapyDev(r2._y._value) : r2._y._value;
    const int vr2y_2 = vr2y + (r2._height.isMapped() ? rmapyDev(r2._height._value) : r2._height._value);
    
    fy_1 = vr1y > vr2y ? vr1y : vr2y;
    fy_2 = vr1y_2 < vr2y_2 ? vr1y_2 : vr2y_2;
    
    // REMOVE Tim. citem. Added.
//     fprintf(stderr, "View::intersects: ymag:%d vr1y:%d vr1y_2:%d vr2y:%d vr2y_2:%d fy_1:%d fy_2:%d\n",
//             ymag, vr1y, vr1y_2, vr2y, vr2y_2, fy_1, fy_2);
  }
  else
  {
    const int mr1y = r1._y.isMapped() ? r1._y._value : mapy(r1._y._value);
    const int mr1y_2 = mr1y + (r1._height.isMapped() ? r1._height._value : rmapy(r1._height._value));
    
    const int mr2y = r2._y.isMapped() ? r2._y._value : mapy(r2._y._value);
    const int mr2y_2 = mr2y + (r2._height.isMapped() ? r2._height._value : rmapy(r2._height._value));
    
    fy_1 = mr1y > mr2y ? mr1y : mr2y;
    fy_2 = mr1y_2 < mr2y_2 ? mr1y_2 : mr2y_2;
    
    // REMOVE Tim. citem. Added.
//     fprintf(stderr, "View::intersects: ymag:%d mr1y:%d mr1y_2:%d mr2y:%d mr2y_2:%d fy_1:%d fy_2:%d\n",
//             ymag, mr1y, mr1y_2, mr2y, mr2y_2, fy_1, fy_2);
  }
  
  return fx_1 < fx_2 && fy_1 < fy_2;
}

//--------------------------------------------
// Lossless comparisons for any magnification.
//--------------------------------------------

// Compares x1 and x2. x1 is the left operand and x2 is the right. For example x1 < x2.
bool View::compareXCoordinates(const ViewXCoordinate& x1, const ViewXCoordinate& x2, const CoordinateCompareMode& mode) const
{
  int xx1, xx2;
  if (xmag <= 0)
  {
    xx1 = x1.isMapped() ? mapxDev(x1._value) : x1._value;
    xx2 = x2.isMapped() ? mapxDev(x2._value) : x2._value;
  }
  else
  {
    xx1 = x1.isMapped() ? x1._value : mapx(x1._value);
    xx2 = x2.isMapped() ? x2._value : mapx(x2._value);
  }
  switch(mode)
  {
    case CompareLess:
      return xx1 < xx2;
    case CompareGreater:
      return xx1 > xx2;
    case CompareLessEqual:
      return xx1 <= xx2;
    case CompareGreaterEqual:
      return xx1 >= xx2;
    case CompareEqual:
      return xx1 == xx2;
  }
  return false;
}

// Compares y1 and y2. y1 is the left operand and y2 is the right. For example y1 < y2.
bool View::compareYCoordinates(const ViewYCoordinate& y1, const ViewYCoordinate& y2, const CoordinateCompareMode& mode) const
{
  int yy1, yy2;
  if (ymag <= 0)
  {
    yy1 = y1.isMapped() ? mapyDev(y1._value) : y1._value;
    yy2 = y2.isMapped() ? mapyDev(y2._value) : y2._value;
  }
  else
  {
    yy1 = y1.isMapped() ? y1._value : mapy(y1._value);
    yy2 = y2.isMapped() ? y2._value : mapy(y2._value);
  }
  switch(mode)
  {
    case CompareLess:
      return yy1 < yy2;
    case CompareGreater:
      return yy1 > yy2;
    case CompareLessEqual:
      return yy1 <= yy2;
    case CompareGreaterEqual:
      return yy1 >= yy2;
    case CompareEqual:
      return yy1 == yy2;
  }
  return false;
}

// Compares w1 and w2. w1 is the left operand and w2 is the right. For example w1 < w2.
bool View::compareWCoordinates(const ViewWCoordinate& w1, const ViewWCoordinate& w2, const CoordinateCompareMode& mode) const
{
  int ww1, ww2;
  if (xmag <= 0)
  {
    ww1 = w1.isMapped() ? rmapxDev(w1._value) : w1._value;
    ww2 = w2.isMapped() ? rmapxDev(w2._value) : w2._value;
  }
  else
  {
    ww1 = w1.isMapped() ? w1._value : rmapx(w1._value);
    ww2 = w2.isMapped() ? w2._value : rmapx(w2._value);
  }
  switch(mode)
  {
    case CompareLess:
      return ww1 < ww2;
    case CompareGreater:
      return ww1 > ww2;
    case CompareLessEqual:
      return ww1 <= ww2;
    case CompareGreaterEqual:
      return ww1 >= ww2;
    case CompareEqual:
      return ww1 == ww2;
  }
  return false;
}

// Compares h1 and h2. h1 is the left operand and h2 is the right. For example h1 < h2.
bool View::compareHCoordinates(const ViewHCoordinate& h1, const ViewHCoordinate& h2, const CoordinateCompareMode& mode) const
{
  int hh1, hh2;
  if (ymag <= 0)
  {
    hh1 = h1.isMapped() ? rmapyDev(h1._value) : h1._value;
    hh2 = h2.isMapped() ? rmapyDev(h2._value) : h2._value;
  }
  else
  {
    hh1 = h1.isMapped() ? h1._value : rmapy(h1._value);
    hh2 = h2.isMapped() ? h2._value : rmapy(h2._value);
  }
  switch(mode)
  {
    case CompareLess:
      return hh1 < hh2;
    case CompareGreater:
      return hh1 > hh2;
    case CompareLessEqual:
      return hh1 <= hh2;
    case CompareGreaterEqual:
      return hh1 >= hh2;
    case CompareEqual:
      return hh1 == hh2;
  }
  return false;
}

// Returns true if x is >= x1 and x < x2.
bool View::isXInRange(ViewXCoordinate x, ViewXCoordinate x1, ViewXCoordinate x2) const
{
  return compareXCoordinates(x, x1, CompareGreaterEqual) &&
         compareXCoordinates(x, x2, CompareLess);
}

// Returns true if y is >= y1 and y < y2.
bool View::isYInRange(ViewYCoordinate y, ViewYCoordinate y1, ViewYCoordinate y2) const
{
  return compareYCoordinates(y, y1, CompareGreaterEqual) &&
         compareYCoordinates(y, y2, CompareLess);
}

//--------------------------------------------
// Lossless math for any magnification.
//--------------------------------------------

// // Returns a ViewXCoordinate of the math operation on x1 and x2. The result is mapped if the view's xmag > 0, and unmapped if not.
// ViewXCoordinate View::mathXCoordinates(const ViewXCoordinate& x1, const ViewXCoordinate& x2,
//                                        const CoordinateMathMode& mode) const
// {
//   int xx1, xx2;
//   const bool xunmap = xmag <= 0;
//   if(xunmap)
//   {
//     xx1 = x1.isMapped() ? (x1.isRelative() ? rmapxDev(x1._value) : mapxDev(x1._value)) : x1._value;
//     xx2 = x2.isMapped() ? (x2.isRelative() ? rmapxDev(x2._value) : mapxDev(x2._value)) : x2._value;
//   }
//   else
//   {
//     xx1 = x1.isMapped() ? x1._value : (x1.isRelative() ? rmapx(x1._value) : mapx(x1._value));
//     xx2 = x2.isMapped() ? x2._value : (x2.isRelative() ? rmapx(x2._value) : mapx(x2._value));
//   }
//   return ViewXCoordinate(doCoordinateMath(xx1, xx2, mode), !xunmap);
// }

// // Returns math operation on y1 and y2. The result is mapped if the view's ymag > 0, and unmapped if not.
// ViewYCoordinate View::mathYCoordinates(const ViewYCoordinate& y1, const ViewYCoordinate& y2,
//                                        const CoordinateMathMode& mode) const
// {
//   int yy1, yy2, res;
//   const bool yunmap = ymag <= 0;
//   if(yunmap)
//   {
//     yy1 = y1.isMapped() ? (y1.isRelative() ? rmapyDev(y1._value) : mapyDev(y1._value)) : y1._value;
//     yy2 = y2.isMapped() ? (y2.isRelative() ? rmapyDev(y2._value) : mapyDev(y2._value)) : y2._value;
//   }
//   else
//   {
//     yy1 = y1.isMapped() ? y1._value : (y1.isRelative() ? rmapy(y1._value) : mapy(y1._value));
//     yy2 = y2.isMapped() ? y2._value : (y2.isRelative() ? rmapy(y2._value) : mapy(y2._value));
//   }
//   switch(mode)
//   {
//     case MathAdd:
//       res = yy1 + yy2;
//     break;
//     case MathSubtract:
//       res = yy1 - yy2;
//     break;
//     case MathMultiply:
//       res = yy1 * yy2;
//     break;
//     case MathDivide:
//       res = yy1 / yy2;
//     break;
//   }
//   return ViewYCoordinate(res, !yunmap);
// }

//--------------------------------------------
// Lossless math for any magnification.
//--------------------------------------------

// Returns a ViewXCoordinate of the math operation on x1 and x2. The result is mapped if the view's xmag > 0, and unmapped if not.
ViewXCoordinate View::mathXCoordinates(const ViewXCoordinate& x1, const ViewXCoordinate& x2,
                                       const CoordinateMathMode& mode) const
{
  if(xmag <= 0)
    return ViewXCoordinate(doCoordinateMath(asIntUnmapped(x1), asIntUnmapped(x2), mode), false);
  return ViewXCoordinate(doCoordinateMath(asIntMapped(x1), asIntMapped(x2), mode), true);
}

// Returns a ViewXCoordinate of the math operation on x1 and w1. The result is mapped if the view's xmag > 0, and unmapped if not.
ViewXCoordinate View::mathXCoordinates(const ViewXCoordinate& x1, const ViewWCoordinate& w1,
                                       const CoordinateMathMode& mode) const
{
  if(xmag <= 0)
    return ViewXCoordinate(doCoordinateMath(asIntUnmapped(x1), asIntUnmapped(w1), mode), false);
  return ViewXCoordinate(doCoordinateMath(asIntMapped(x1), asIntMapped(w1), mode), true);
}

// Returns a ViewWCoordinate of the math operation on w1 and w2. The result is mapped if the view's xmag > 0, and unmapped if not.
ViewWCoordinate View::mathXCoordinates(const ViewWCoordinate& w1, const ViewWCoordinate& w2,
                                       const CoordinateMathMode& mode) const
{
  if(xmag <= 0)
    return ViewWCoordinate(doCoordinateMath(asIntUnmapped(w1), asIntUnmapped(w2), mode), false);
  return ViewWCoordinate(doCoordinateMath(asIntMapped(w1), asIntMapped(w2), mode), true);
}



// Returns a ViewYCoordinate of the math operation on y1 and y2. The result is mapped if the view's ymag > 0, and unmapped if not.
ViewYCoordinate View::mathYCoordinates(const ViewYCoordinate& y1, const ViewYCoordinate& y2,
                                       const CoordinateMathMode& mode) const
{
  if(ymag <= 0)
    return ViewYCoordinate(doCoordinateMath(asIntUnmapped(y1), asIntUnmapped(y2), mode), false);
  return ViewYCoordinate(doCoordinateMath(asIntMapped(y1), asIntMapped(y2), mode), true);
}

// Returns a ViewYCoordinate of the math math operation on y1 and h1. The result is mapped if the view's ymag > 0, and unmapped if not.
ViewYCoordinate View::mathYCoordinates(const ViewYCoordinate& y1, const ViewHCoordinate& h1,
                                       const CoordinateMathMode& mode) const
{
  if(ymag <= 0)
    return ViewYCoordinate(doCoordinateMath(asIntUnmapped(y1), asIntUnmapped(h1), mode), false);
  return ViewYCoordinate(doCoordinateMath(asIntMapped(y1), asIntMapped(h1), mode), true);
}

// Returns a ViewHCoordinate of the math math operation on h1 and h2. The result is mapped if the view's ymag > 0, and unmapped if not.
ViewHCoordinate View::mathYCoordinates(const ViewHCoordinate& h1, const ViewHCoordinate& h2,
                                       const CoordinateMathMode& mode) const
{
  if(ymag <= 0)
    return ViewHCoordinate(doCoordinateMath(asIntUnmapped(h1), asIntUnmapped(h2), mode), false);
  return ViewHCoordinate(doCoordinateMath(asIntMapped(h1), asIntMapped(h2), mode), true);
}



// Performs math operation on x1. The result is mapped if the view's xmag > 0, and unmapped if not.
// Returns a reference to x1.
ViewXCoordinate& View::mathRefXCoordinates(ViewXCoordinate& x1, const ViewXCoordinate& x2,
                                  const CoordinateMathMode& mode) const
{
  if(xmag <= 0)
    x1 = ViewXCoordinate(doCoordinateMath(asIntUnmapped(x1), asIntUnmapped(x2), mode), false);
  else
    x1 = ViewXCoordinate(doCoordinateMath(asIntMapped(x1), asIntMapped(x2), mode), true);
  return x1;
}

// Performs math operation on x1. The result is mapped if the view's xmag > 0, and unmapped if not.
// Returns a reference to x1.
ViewXCoordinate& View::mathRefXCoordinates(ViewXCoordinate& x1, const ViewWCoordinate& w1,
                                  const CoordinateMathMode& mode) const
{
  if(xmag <= 0)
    x1 = ViewXCoordinate(doCoordinateMath(asIntUnmapped(x1), asIntUnmapped(w1), mode), false);
  else
    x1 = ViewXCoordinate(doCoordinateMath(asIntMapped(x1), asIntMapped(w1), mode), true);
  return x1;
}

// Performs math operation on w1. The result is mapped if the view's xmag > 0, and unmapped if not.
// Returns a reference to w1.
ViewWCoordinate& View::mathRefXCoordinates(ViewWCoordinate& w1, const ViewWCoordinate& w2,
                                  const CoordinateMathMode& mode) const
{
  if(xmag <= 0)
    w1 = ViewWCoordinate(doCoordinateMath(asIntUnmapped(w1), asIntUnmapped(w2), mode), false);
  else
    w1 = ViewWCoordinate(doCoordinateMath(asIntMapped(w1), asIntMapped(w2), mode), true);
  return w1;
}


// Performs math operation on y1. The result is mapped if the view's ymag > 0, and unmapped if not.
// Returns a reference to y1.
ViewYCoordinate& View::mathRefYCoordinates(ViewYCoordinate& y1, const ViewYCoordinate& y2,
                                  const CoordinateMathMode& mode) const
{
  if(ymag <= 0)
    y1 = ViewYCoordinate(doCoordinateMath(asIntUnmapped(y1), asIntUnmapped(y2), mode), false);
  else
    y1 = ViewYCoordinate(doCoordinateMath(asIntMapped(y1), asIntMapped(y2), mode), true);
  return y1;
}

// Performs math operation on y1. The result is mapped if the view's ymag > 0, and unmapped if not.
// Returns a reference to y1.
ViewYCoordinate& View::mathRefYCoordinates(ViewYCoordinate& y1, const ViewHCoordinate& h1,
                                  const CoordinateMathMode& mode) const
{
  if(ymag <= 0)
    y1 = ViewYCoordinate(doCoordinateMath(asIntUnmapped(y1), asIntUnmapped(h1), mode), false);
  else
    y1 = ViewYCoordinate(doCoordinateMath(asIntMapped(y1), asIntMapped(h1), mode), true);
  return y1;
}

// Performs math operation on h1. The result is mapped if the view's ymag > 0, and unmapped if not.
// Returns a reference to h1.
ViewHCoordinate& View::mathRefYCoordinates(ViewHCoordinate& h1, const ViewHCoordinate& h2,
                                  const CoordinateMathMode& mode) const
{
  if(ymag <= 0)
    h1 = ViewHCoordinate(doCoordinateMath(asIntUnmapped(h1), asIntUnmapped(h2), mode), false);
  else
    h1 = ViewHCoordinate(doCoordinateMath(asIntMapped(h1), asIntMapped(h2), mode), true);
  return h1;
}

// Adjusts the x, y, w and h of the rectangle. Returns a reference to the rectangle.
ViewRect& View::adjustRect(ViewRect& r, const ViewWCoordinate& dx, const ViewHCoordinate& dy,
                      const ViewWCoordinate& dw, const ViewHCoordinate& dh) const
{
    mathRefXCoordinates(r._x, dx, MathAdd);
    mathRefYCoordinates(r._y, dy, MathAdd);
    mathRefXCoordinates(r._width, dw, MathAdd);
    mathRefYCoordinates(r._height, dh, MathAdd);
  return r;
}

// Returns a rectangle with the x, y, w and h adjusted.
ViewRect View::adjustedRect(const ViewRect& r, const ViewWCoordinate& dx, const ViewHCoordinate& dy,
                      const ViewWCoordinate& dw, const ViewHCoordinate& dh) const
{
  return ViewRect(mathXCoordinates(r._x, dx, MathAdd),
                  mathYCoordinates(r._y, dy, MathAdd),
                  mathXCoordinates(r._width, dw, MathAdd),
                  mathYCoordinates(r._height, dh, MathAdd));
}

// // Returns true if the mapped value is less than the unmapped value.
// bool View::isXLess(int mapped_x, int unmapped_x) const
// {
//   if (xmag <= 0)
//     return mapxDev(mapped_x) < unmapped_x;
//   else
//     return mapped_x < mapx(unmapped_x);
// }
// 
// // Returns true if the mapped value is greater than or equal to the unmapped value.
// bool View::isXGreaterOrEqual(int mapped_x, int unmapped_x) const
// {
//   if (xmag <= 0)
//     return mapxDev(mapped_x) >= unmapped_x;
//   else
//     return mapped_x >= mapx(unmapped_x);
// }
// 
// // Returns true if the mapped value is equal to the unmapped value.
// bool View::isXEqual(int mapped_x, int unmapped_x) const
// {
//   if (xmag <= 0)
//     return mapxDev(mapped_x) == unmapped_x;
//   else
//     return mapped_x == mapx(unmapped_x);
// }
// 
// // Returns true if the mapped value is less than the unmapped value.
// bool View::isYLess(int mapped_y, int unmapped_y) const
// {
//   if (ymag <= 0)
//     return mapyDev(mapped_y) < unmapped_y;
//   else
//     return mapped_y < mapy(unmapped_y);
// }
// 
// // Returns true if the mapped value is greater than or equal to the unmapped value.
// bool View::isYGreaterOrEqual(int mapped_y, int unmapped_y) const
// {
//   if (ymag <= 0)
//     return mapyDev(mapped_y) >= unmapped_y;
//   else
//     return mapped_y >= mapy(unmapped_y);
// }
// 
// // Returns true if the mapped value is equal to the unmapped value.
// bool View::isYEqual(int mapped_y, int unmapped_y) const
// {
//   if (ymag <= 0)
//     return mapyDev(mapped_y) == unmapped_y;
//   else
//     return mapped_y == mapy(unmapped_y);
// }

//--------------------------------------------
// Conversions.
//--------------------------------------------

// // Returns a mapped version of the x coordinate.
// ViewXCoordinate View::asMapped(const ViewXCoordinate& x) const
// {
//   return ViewXCoordinate(x.isMapped() ? x._value : mapx(x._value), true);
// }
// 
// // Returns an unmapped version of the y coordinate.
// ViewYCoordinate View::asMapped(const ViewYCoordinate& y) const
// {
//   return ViewYCoordinate(y.isMapped() ? y._value : mapy(y._value), true);
// }
//       
// // Returns a relative mapped version of the w coordinate.
// ViewWCoordinate View::asMapped(const ViewWCoordinate& w) const
// {
//   return ViewWCoordinate(w.isMapped() ? w._value : rmapx(w._value), true);
// }
// 
// // Returns a relative mapped version of the h coordinate.
// ViewHCoordinate View::asMapped(const ViewHCoordinate& h) const
// {
//   return ViewHCoordinate(h.isMapped() ? h._value : rmapy(h._value), true);
// }
// 
// // Returns an unmapped version of the x coordinate.
// ViewXCoordinate View::asUnmapped(const ViewXCoordinate& x) const
// {
//   return ViewXCoordinate(x.isMapped() ? mapxDev(x._value) : x._value, false);
// }
// 
// // Returns an unmapped version of the y coordinate.
// ViewYCoordinate View::asUnmapped(const ViewYCoordinate& y) const
// {
//   return ViewYCoordinate(y.isMapped() ? mapyDev(y._value) : y._value, false);
// }
//       
// // Returns a relative unmapped version of the w coordinate.
// ViewWCoordinate View::asUnmapped(const ViewWCoordinate& w) const
// {
//   return ViewWCoordinate(w.isMapped() ? rmapxDev(w._value) : w._value, false);
// }
// 
// // Returns a relative unmapped version of the h coordinate.
// ViewHCoordinate View::asUnmapped(const ViewHCoordinate& h) const
// {
//   return ViewHCoordinate(h.isMapped() ? rmapyDev(h._value) : h._value, false);
// }
//       
// // Returns a mapped version of the rectangle.
// ViewRect View::asMapped(const ViewRect& r) const
// {
//   return ViewRect(ViewXCoordinate(r._x.isMapped() ? r._x._value : mapx(r._x._value), true),
//                   ViewYCoordinate(r._y.isMapped() ? r._y._value : mapy(r._y._value), true),
//                   ViewWCoordinate(r._width.isMapped() ? r._width._value : rmapx(r._width._value), true),
//                   ViewHCoordinate(r._height.isMapped() ? r._height._value : rmapy(r._height._value), true));
// }
// 
// // Returns an unmapped version of the rectangle.
// ViewRect View::asUnmapped(const ViewRect& r) const
// {
//   return ViewRect(ViewXCoordinate(r._x.isMapped() ? mapxDev(r._x._value) : r._x._value, false),
//                   ViewYCoordinate(r._y.isMapped() ? mapyDev(r._y._value) : r._y._value, false),
//                   ViewWCoordinate(r._width.isMapped() ? rmapxDev(r._width._value) : r._width._value, false),
//                   ViewHCoordinate(r._height.isMapped() ? rmapyDev(r._height._value) : r._height._value, false));
// }
//       
// // Returns a mapped QRect version of the rectangle.
// QRect View::asMappedQRect(const ViewRect& r) const
// {
//   return QRect(r._x.isMapped() ? r._x._value : mapx(r._x._value),
//                r._y.isMapped() ? r._y._value : mapy(r._y._value),
//                r._width.isMapped() ? r._width._value : rmapx(r._width._value),
//                r._height.isMapped() ? r._height._value : rmapy(r._height._value));
// }
// 
// // Returns an unmapped QRect version of the rectangle.
// QRect View::asUnmappedQRect(const ViewRect& r) const
// {
//   return QRect(r._x.isMapped() ? mapxDev(r._x._value) : r._x._value,
//                r._y.isMapped() ? mapyDev(r._y._value) : r._y._value,
//                r._width.isMapped() ? rmapxDev(r._width._value) : r._width._value,
//                r._height.isMapped() ? rmapyDev(r._height._value) : r._height._value);
// }
      
} // namespace MusEGui