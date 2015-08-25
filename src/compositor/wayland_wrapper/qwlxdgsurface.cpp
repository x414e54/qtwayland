/****************************************************************************
**
** Copyright (C) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwlxdgsurface_p.h"

#include "qwlcompositor_p.h"
#include "qwlsurface_p.h"
#include "qwloutput_p.h"
#include "qwlinputdevice_p.h"
#include "qwlsubsurface_p.h"
#include "qwlpointer_p.h"
#include "qwlextendedsurface_p.h"

#include "qwaylandoutput.h"
#include "qwaylandsurfaceview.h"

#include <QtCore/qglobal.h>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

namespace QtWayland {

XDGShellGlobal::XDGShellGlobal(Compositor *compositor)
    : QtWaylandServer::xdg_shell(compositor->wl_display(), 1)
{
}

void XDGShellGlobal::xdg_shell_get_xdg_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface_res)
{
    Surface *surface = Surface::fromResource(surface_res);
    new XDGSurface(this, resource->client(), id, surface);
}

void XDGShellGlobal::xdg_shell_get_xdg_popup(Resource *resource, uint32_t id, struct ::wl_resource *surface, struct ::wl_resource *parent, struct ::wl_resource *seat, uint32_t serial, int32_t x, int32_t y)
{
}


XDGSurface::XDGSurface(XDGShellGlobal *shell, wl_client *client, uint32_t id, Surface *surface)
    : QWaylandSurfaceInterface(surface->waylandSurface())
    , xdg_surface(client, id, 1)
    , m_shell(shell)
    , m_surface(surface)
    , m_window_pos(0.0, 0.0)
    , m_window_size(0.0, 0.0)
    , m_resizeGrabber(0)
    , m_moveGrabber(0)
{
    m_view = surface->compositor()->waylandCompositor()->createView(surface->waylandSurface());
    connect(surface->waylandSurface(), &QWaylandSurface::configure, this, &XDGSurface::configure);
    connect(surface->waylandSurface(), &QWaylandSurface::mapped, this, &XDGSurface::mapped);
}

XDGSurface::~XDGSurface()
{
    delete m_view;
}

void XDGSurface::sendConfigure(char state, int32_t width, int32_t height)
{
    m_state.clear();
    m_state.push_back(state);
    quint64 serial = wl_display_next_serial(m_surface->compositor()->wl_display());
    m_configures.insert(serial);
    send_configure(width, height, m_state, serial);
}

void XDGSurface::adjustPosInResize()
{
    if (m_surface->transientParent())
        return;
    if (!m_resizeGrabber || !(m_resizeGrabber->resize_edges & WL_SHELL_SURFACE_RESIZE_TOP_LEFT))
        return;

    int bottomLeftX = m_resizeGrabber->point.x() + m_resizeGrabber->width;
    int bottomLeftY = m_resizeGrabber->point.y() + m_resizeGrabber->height;
    qreal x = m_view->pos().x();
    qreal y = m_view->pos().y();
    if (m_resizeGrabber->resize_edges & WL_SHELL_SURFACE_RESIZE_TOP)
        y = bottomLeftY - m_view->surface()->size().height();
    if (m_resizeGrabber->resize_edges & WL_SHELL_SURFACE_RESIZE_LEFT)
        x = bottomLeftX - m_view->surface()->size().width();
    QPointF newPos(x,y);
    m_view->setPos(newPos);
}

void XDGSurface::resetResizeGrabber()
{
    m_resizeGrabber = 0;
}

void XDGSurface::resetMoveGrabber()
{
    m_moveGrabber = 0;
}

void XDGSurface::setOffset(const QPointF &offset)
{
    m_surface->setTransientOffset(offset.x(), offset.y());
}

void XDGSurface::configure(bool hasBuffer)
{
    m_surface->setMapped(hasBuffer);
}

bool XDGSurface::runOperation(QWaylandSurfaceOp *op)
{
    switch (op->type()) {
        case QWaylandSurfaceOp::Resize:
            requestSize(static_cast<QWaylandSurfaceResizeOp *>(op)->size());
            return true;
        default:
            break;
    }
    return false;
}

void XDGSurface::mapped()
{
}

void XDGSurface::requestSize(const QSize &size)
{
    sendConfigure(m_state[0], size.width(), size.height());
}

void XDGSurface::xdg_surface_destroy(Resource *)
{
    delete this;
}

void XDGSurface::xdg_surface_set_parent(Resource *resource,
                      struct wl_resource *parent_surface_resource)
{

    Q_UNUSED(resource);
    Surface *parent_surface = Surface::fromResource(parent_surface_resource);
    m_surface->setTransientParent(parent_surface);

    setSurfaceType(QWaylandSurface::Transient);
}

void XDGSurface::xdg_surface_set_title(Resource *resource,
                             const QString &title)
{
    Q_UNUSED(resource);
    m_surface->setTitle(title);
}

void XDGSurface::xdg_surface_set_app_id(Resource *resource, const QString &app_id)
{
    Q_UNUSED(resource);
    Q_UNUSED(app_id);
}

void XDGSurface::xdg_surface_show_window_menu(Resource *resource,
                  struct ::wl_resource *seat, uint32_t serial, int32_t x, int32_t y)
{
    Q_UNUSED(resource);
    Q_UNUSED(seat);
    Q_UNUSED(serial);
    Q_UNUSED(x);
    Q_UNUSED(y);
}

void XDGSurface::xdg_surface_move(Resource *resource,
                struct wl_resource *input_device_super,
                uint32_t time)
{
    Q_UNUSED(resource);
    Q_UNUSED(time);

    if (m_resizeGrabber || m_moveGrabber) {
        qDebug() << "invalid state";
        return;
    }

    InputDevice *input_device = InputDevice::fromSeatResource(input_device_super);
    Pointer *pointer = input_device->pointerDevice();

    m_moveGrabber = new XDGSurfaceMoveGrabber(this, pointer->position() - m_view->pos());

    pointer->startGrab(m_moveGrabber);
}

void XDGSurface::xdg_surface_resize(Resource *resource,
                  struct wl_resource *input_device_super,
                  uint32_t time,
                  uint32_t edges)
{
    Q_UNUSED(resource);
    Q_UNUSED(time);
    Q_UNUSED(edges);

    if (m_moveGrabber || m_resizeGrabber) {
        qDebug() << "invalid state2";
        return;
    }

    m_resizeGrabber = new XDGSurfaceResizeGrabber(this);

    InputDevice *input_device = InputDevice::fromSeatResource(input_device_super);
    Pointer *pointer = input_device->pointerDevice();

    m_resizeGrabber->point = pointer->position();
    m_resizeGrabber->resize_edges = static_cast<xdg_surface_resize_edge>(edges);
    m_resizeGrabber->width = m_view->surface()->size().width();
    m_resizeGrabber->height = m_view->surface()->size().height();

    pointer->startGrab(m_resizeGrabber);
}

void XDGSurface::xdg_surface_ack_configure(Resource *resource, uint32_t serial)
{
    Q_UNUSED(resource);
    if (m_configures.remove(serial))
        emit m_surface->waylandSurface()->pong();
    else
        qWarning("Received an unexpected ack!");
}

void XDGSurface::xdg_surface_set_window_geometry(Resource *resource,
                  int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(resource);
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void XDGSurface::xdg_surface_set_maximized(Resource *resource)
{
    Q_UNUSED(resource);

    QWaylandOutput *output = Q_NULLPTR;

    if (!output) {
        // Look for an output that can contain this surface
        Q_FOREACH (QWaylandOutput *curOutput, m_surface->compositor()->outputs()) {
            if (curOutput->geometry().size().width() >= m_surface->size().width() &&
                    curOutput->geometry().size().height() >= m_surface->size().height()) {
                output = curOutput;
                break;
            }
        }
    }
    if (!output) {
        qWarning() << "Unable to maximize surface, cannot determine output";
        return;
    }
    QSize outputSize = output->availableGeometry().size();

    m_window_pos = m_view->pos();
    m_window_size = m_view->surface()->size();

    m_view->setPos(output->availableGeometry().topLeft());
    sendConfigure(XDG_SURFACE_STATE_MAXIMIZED, outputSize.width(), outputSize.height());
}

void XDGSurface::xdg_surface_unset_maximized(Resource *resource)
{
    Q_UNUSED(resource);

    m_view->setPos(m_window_pos);
    sendConfigure(XDG_SURFACE_STATE_ACTIVATED, m_window_size.width(), m_window_size.height());
}

void XDGSurface::xdg_surface_set_fullscreen(Resource *resource,
                       struct wl_resource *output_resource)
{
    Q_UNUSED(resource);

    QWaylandOutput *output = output_resource
            ? QWaylandOutput::fromResource(output_resource)
            : Q_NULLPTR;
    if (!output) {
        // Look for an output that can contain this surface
        Q_FOREACH (QWaylandOutput *curOutput, m_surface->compositor()->outputs()) {
            if (curOutput->geometry().size().width() >= m_surface->size().width() &&
                    curOutput->geometry().size().height() >= m_surface->size().height()) {
                output = curOutput;
                break;
            }
        }
    }
    if (!output) {
        qWarning() << "Unable to resize surface full screen, cannot determine output";
        return;
    }
    QSize outputSize = output->geometry().size();

    m_window_pos = m_view->pos();
    m_window_size = m_view->surface()->size();

    m_view->setPos(output->geometry().topLeft());
    sendConfigure(XDG_SURFACE_STATE_FULLSCREEN, outputSize.width(), outputSize.height());
}

void XDGSurface::xdg_surface_unset_fullscreen(Resource *resource)
{
    Q_UNUSED(resource);

    m_view->setPos(m_window_pos);

    sendConfigure(XDG_SURFACE_STATE_ACTIVATED, m_window_size.width(), m_window_size.height());
}

void XDGSurface::xdg_surface_set_minimized(Resource *resource)
{
    Q_UNUSED(resource);
}

XDGSurfaceGrabber::XDGSurfaceGrabber(XDGSurface *shellSurface)
    : PointerGrabber()
    , shell_surface(shellSurface)
{
}

XDGSurfaceGrabber::~XDGSurfaceGrabber()
{
}

XDGSurfaceResizeGrabber::XDGSurfaceResizeGrabber(XDGSurface *shellSurface)
    : XDGSurfaceGrabber(shellSurface)
{
}

void XDGSurfaceResizeGrabber::focus()
{
}

void XDGSurfaceResizeGrabber::motion(uint32_t time)
{
    Q_UNUSED(time);

    int width_delta = point.x() - m_pointer->position().x();
    int height_delta = point.y() - m_pointer->position().y();

    int new_height = height;
    if (resize_edges & WL_SHELL_SURFACE_RESIZE_TOP)
        new_height = qMax(new_height + height_delta, 1);
    else if (resize_edges & WL_SHELL_SURFACE_RESIZE_BOTTOM)
        new_height = qMax(new_height - height_delta, 1);

    int new_width = width;
    if (resize_edges & WL_SHELL_SURFACE_RESIZE_LEFT)
        new_width = qMax(new_width + width_delta, 1);
    else if (resize_edges & WL_SHELL_SURFACE_RESIZE_RIGHT)
        new_width = qMax(new_width - width_delta, 1);

    shell_surface->sendConfigure(XDG_SURFACE_STATE_RESIZING, new_width, new_height);
}

void XDGSurfaceResizeGrabber::button(uint32_t time, Qt::MouseButton button, uint32_t state)
{
    Q_UNUSED(time)

    if (button == Qt::LeftButton && !state) {
        m_pointer->endGrab();
        shell_surface->resetResizeGrabber();
        delete this;
    }
}

XDGSurfaceMoveGrabber::XDGSurfaceMoveGrabber(XDGSurface *shellSurface, const QPointF &offset)
    : XDGSurfaceGrabber(shellSurface)
    , m_offset(offset)
{
}

void XDGSurfaceMoveGrabber::focus()
{
}

void XDGSurfaceMoveGrabber::motion(uint32_t time)
{
    Q_UNUSED(time);

    QPointF pos(m_pointer->position() - m_offset);
    shell_surface->m_view->setPos(pos);
    if (shell_surface->m_surface->transientParent()) {
        QWaylandSurfaceView *view = shell_surface->m_surface->transientParent()->waylandSurface()->views().first();
        if (view)
            shell_surface->setOffset(pos - view->pos());
    }

}

void XDGSurfaceMoveGrabber::button(uint32_t time, Qt::MouseButton button, uint32_t state)
{
    Q_UNUSED(time)

    if (button == Qt::LeftButton && !state) {
        m_pointer->setFocus(0, QPointF());
        m_pointer->endGrab();
        shell_surface->resetMoveGrabber();
        delete this;
    }
}

}

QT_END_NAMESPACE
