/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <touch_driver.h>
#include "opentx.h"

Window * Window::focusWindow = nullptr;
std::list<Window *> Window::trash;
MainWindow mainWindow;

Window::Window(Window * parent, const rect_t & rect):
  parent(parent),
  innerWidth(rect.w),
  innerHeight(rect.h),
  rect(rect)
{
  if (parent) {
    parent->addChild(this);
    invalidate();
  }
}

Window::~Window()
{
  if (focusWindow == this) {
    focusWindow = nullptr;
  }
}

void Window::detach()
{
  if (parent) {
    parent->removeChild(this);
    parent = nullptr;
  }
}

void Window::deleteLater(bool detach)
{
  if (detach) {
    this->detach();
  }
  if (onClose) {
    onClose();
  }
  trash.push_back(this);
}

void Window::clear()
{
  scrollPositionX = 0;
  scrollPositionY = 0;
  innerWidth = rect.w;
  innerHeight = rect.h;
  deleteChildren();
  invalidate();
}

void Window::deleteChildren()
{
  for (auto window: children) {
    window->deleteLater(false);
  }
  children.clear();
}

void Window::clearFocus()
{
  if (focusWindow) {
    focusWindow->onFocusLost();
    focusWindow = nullptr;
  }
}

void Window::setFocus()
{
  clearFocus();
  focusWindow = this;
  parent->scrollTo(this);
  invalidate();
}

void Window::scrollTo(Window * child)
{
  /*if (child->top() < scrollPositionY) {
    scrollPositionY = child->top();
  }*/
  if (child->bottom() > height() - scrollPositionY) {
    setScrollPositionY(height() - child->bottom());
  }
}

void Window::fullPaint(BitmapBuffer * dc)
{
  paint(dc);
  drawVerticalScrollbar(dc);
  paintChildren(dc);
}

void Window::paintChildren(BitmapBuffer * dc)
{
  coord_t x = dc->getOffsetX();
  coord_t y = dc->getOffsetY();
  coord_t xmin, xmax, ymin, ymax;
  dc->getClippingRect(xmin, xmax, ymin, ymax);
  for (auto child: children) {
    dc->setOffset(x + child->rect.x + child->scrollPositionX, y + child->rect.y + child->scrollPositionY);
    dc->setClippingRect(max(xmin, x + child->rect.left()),
                        min(xmax, x + child->rect.right()),
                        max(ymin, y + child->rect.top()),
                        min(ymax, y + child->rect.bottom()));
    child->fullPaint(dc);
  }
}

void Window::checkEvents()
{
  for (auto child: children) {
    child->checkEvents();
  }
}

bool Window::onTouchStart(coord_t x, coord_t y)
{
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    auto child = *it;
    if (pointInRect(x, y, child->rect)) {
      if (child->onTouchStart(x - child->rect.x - child->scrollPositionX, y - child->rect.y - child->scrollPositionY)) {
        return true;
      }
    }
  }

  return false;
}

bool Window::onTouchEnd(coord_t x, coord_t y)
{
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    auto child = *it;
    if (pointInRect(x, y, child->rect)) {
      if (child->onTouchEnd(x - child->rect.x - child->scrollPositionX, y - child->rect.y - child->scrollPositionY)) {
        return true;
      }
    }
  }

  return false;
}

bool Window::onTouchSlide(coord_t x, coord_t y, coord_t startX, coord_t startY, coord_t slideX, coord_t slideY)
{
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    auto child = *it;
    if (pointInRect(startX, startY, child->rect)) {
      if (child->onTouchSlide(x - child->rect.x, y - child->rect.y, startX - child->rect.x, startY - child->rect.y, slideX, slideY)) {
        return true;
      }
    }
  }

  if (slideY && innerHeight > rect.h) {
    coord_t newScrollPosition = limit<coord_t>(-innerHeight + rect.h, scrollPositionY + slideY, 0);
    if (newScrollPosition != scrollPositionY) {
      scrollPositionY = newScrollPosition;
      invalidate();
    }
    return true;
  }

  if (slideX && innerWidth > rect.w) {
    coord_t newScrollPosition = limit<coord_t>(-innerWidth + rect.w, scrollPositionX + slideX, 0);
    if (newScrollPosition != scrollPositionX) {
      scrollPositionX = newScrollPosition;
      invalidate();
    }
    return true;
  }

  return false;
}

void Window::adjustInnerHeight()
{
  innerHeight = 0;
  for (auto child: children) {
    innerHeight = max(innerHeight, child->rect.y + child->rect.h);
  }
}

coord_t Window::adjustHeight()
{
  adjustInnerHeight();
  coord_t old = rect.h;
  rect.h = innerHeight;
  return rect.h - old;
}

void Window::moveWindowsTop(coord_t y, coord_t delta)
{
  for (auto child: children) {
    if (child->rect.y > y) {
      child->rect.y += delta;
    }
  }
}

void Window::invalidate(const rect_t & rect)
{
  if (parent) {
    parent->invalidate({this->rect.x + rect.x + parent->scrollPositionX, this->rect.y + rect.y + parent->scrollPositionY, rect.w, rect.h});
  }
}

void Window::drawVerticalScrollbar(BitmapBuffer * dc)
{
  if (innerHeight > rect.h) {
    coord_t x = rect.w - 3;
    coord_t y = -scrollPositionY + 3;
    coord_t h = rect.h - 6;
    lcd->drawSolidFilledRect(x, y, 1, h, LINE_COLOR);
    coord_t yofs = (-h*scrollPositionY + innerHeight/2) / innerHeight;
    coord_t yhgt = (h*rect.h + innerHeight/2) / innerHeight;
    if (yhgt + yofs > h)
      yhgt = h - yofs;
    dc->drawSolidFilledRect(x-1, y + yofs, 3, yhgt, SCROLLBOX_COLOR);
  }
}

void MainWindow::emptyTrash()
{
  for (auto window: trash) {
    delete window;
  }
  trash.clear();
}

void MainWindow::checkEvents()
{
  if (touchState.Event == TE_DOWN) {
    onTouchStart(touchState.X - scrollPositionX, touchState.Y - scrollPositionY);
    // touchState.Event = TE_NONE;
  }
  else if (touchState.Event == TE_UP) {
    onTouchEnd(touchState.startX - scrollPositionX, touchState.startY - scrollPositionY);
    touchState.Event = TE_NONE;
  }
  else if (touchState.Event == TE_SLIDE) {
    coord_t x = touchState.X - touchState.lastX;
    coord_t y = touchState.Y - touchState.lastY;
    onTouchSlide(touchState.X, touchState.Y, touchState.startX, touchState.startY, x, y);
    touchState.lastX = touchState.X;
    touchState.lastY = touchState.Y;
  }

  Window::checkEvents();

  emptyTrash();
}

void MainWindow::invalidate(const rect_t & rect)
{
  if (invalidatedRect.w) {
    coord_t left = min(invalidatedRect.left(), rect.left());
    coord_t right = max(invalidatedRect.right(), rect.right());
    coord_t top = min(invalidatedRect.top(), rect.top());
    coord_t bottom = max(invalidatedRect.bottom(), rect.bottom());
    invalidatedRect = {left, top, right - left, bottom - top};
  }
  else {
    invalidatedRect = rect;
  }
}

bool MainWindow::refresh()
{
  if (invalidatedRect.w) {
    TRACE("Refresh rect: left=%d top=%d width=%d height=%d", invalidatedRect.left(), invalidatedRect.top(), invalidatedRect.w, invalidatedRect.h);
    if (invalidatedRect.x > 0 || invalidatedRect.y > 0 || invalidatedRect.w < LCD_W || invalidatedRect.h < LCD_H) {
      BitmapBuffer * previous = lcd;
      lcdNextLayer();
      DMACopy(previous->getData(), lcd->getData(), DISPLAY_BUFFER_SIZE);
    }
    else {
      lcdNextLayer();
    }
    lcd->setOffset(0, 0);
    lcd->setClippingRect(invalidatedRect.left(), invalidatedRect.right(), invalidatedRect.top(), invalidatedRect.bottom());
    lcd->clear(TEXT_BGCOLOR);
    fullPaint(lcd);
    lcd->clearClippingRect();
    invalidatedRect.w = 0;
    return true;
  }
  else {
    return false;
  }
}