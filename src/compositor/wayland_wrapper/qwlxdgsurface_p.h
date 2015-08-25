/****************************************************************************
**
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

#ifndef WLXDGSURFACE_H
#define WLXDGSURFACE_H

#include <QtCompositor/qwaylandexport.h>
#include <QtCompositor/qwaylandsurface.h>
#include <QtCompositor/qwaylandglobalinterface.h>
#include <QtCompositor/qwaylandsurfaceinterface.h>

#include <wayland-server.h>
#include <QHash>
#include <QPoint>
#include <QSet>
#include <private/qwlpointer_p.h>

#include <QtCompositor/private/qwayland-server-xdg-shell.h>

QT_BEGIN_NAMESPACE

class QWaylandSurfaceView;

namespace QtWayland {

class Compositor;
class Surface;
class XDGSurface;
class XDGSurfaceResizeGrabber;
class XDGSurfaceMoveGrabber;
class XDGSurfacePopupGrabber;

class XDGShellGlobal : public QtWaylandServer::xdg_shell
{
public:
    XDGShellGlobal(Compositor *compositor);

private:
    void xdg_shell_get_xdg_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface) Q_DECL_OVERRIDE;
    void xdg_shell_get_xdg_popup(Resource *resource, uint32_t id, struct ::wl_resource *surface, struct ::wl_resource *parent, struct ::wl_resource *seat, uint32_t serial, int32_t x, int32_t y) Q_DECL_OVERRIDE;
};
    

class Q_COMPOSITOR_EXPORT XDGSurface : public QObject, public QWaylandSurfaceInterface, public QtWaylandServer::xdg_surface
{
public:
    XDGSurface(XDGShellGlobal *shell, struct wl_client *client, uint32_t id, Surface *surface);
    ~XDGSurface();
    void sendConfigure(char state, int32_t width, int32_t height);

    void adjustPosInResize();
    void resetResizeGrabber();
    void resetMoveGrabber();

    void setOffset(const QPointF &offset);

    void configure(bool hasBuffer);

    void requestSize(const QSize &size);

protected:
    bool runOperation(QWaylandSurfaceOp *op) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void mapped();

private:
    XDGShellGlobal *m_shell;
    Surface *m_surface;
    QWaylandSurfaceView *m_view;
    QPointF m_window_pos;
    QSizeF m_window_size;
    QByteArray m_state;

    XDGSurfaceResizeGrabber *m_resizeGrabber;
    XDGSurfaceMoveGrabber *m_moveGrabber;

    QSet<quint64> m_configures;

    quint64 m_configure_serial;

    void xdg_surface_destroy(Resource *resource) Q_DECL_OVERRIDE;
    void xdg_surface_set_parent(Resource *resource, struct ::wl_resource *parent) Q_DECL_OVERRIDE;
    void xdg_surface_set_title(Resource *resource, const QString &title) Q_DECL_OVERRIDE;
    void xdg_surface_set_app_id(Resource *resource, const QString &app_id) Q_DECL_OVERRIDE;
    void xdg_surface_show_window_menu(Resource *resource, struct ::wl_resource *seat, uint32_t serial, int32_t x, int32_t y) Q_DECL_OVERRIDE;
    void xdg_surface_move(Resource *resource, struct ::wl_resource *seat, uint32_t serial) Q_DECL_OVERRIDE;
    void xdg_surface_resize(Resource *resource, struct ::wl_resource *seat, uint32_t serial, uint32_t edges) Q_DECL_OVERRIDE;
    void xdg_surface_ack_configure(Resource *resource, uint32_t serial) Q_DECL_OVERRIDE;
    void xdg_surface_set_window_geometry(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) Q_DECL_OVERRIDE;
    void xdg_surface_set_maximized(Resource *resource) Q_DECL_OVERRIDE;
    void xdg_surface_unset_maximized(Resource *resource) Q_DECL_OVERRIDE;
    void xdg_surface_set_fullscreen(Resource *resource, struct ::wl_resource *output) Q_DECL_OVERRIDE;
    void xdg_surface_unset_fullscreen(Resource *resource) Q_DECL_OVERRIDE;
    void xdg_surface_set_minimized(Resource *resource) Q_DECL_OVERRIDE;

    friend class XDGSurfaceMoveGrabber;
};

class XDGSurfaceGrabber : public PointerGrabber
{
public:
    XDGSurfaceGrabber(XDGSurface *shellSurface);
    ~XDGSurfaceGrabber();

    XDGSurface *shell_surface;
};

class XDGSurfaceResizeGrabber : public XDGSurfaceGrabber
{
public:
    XDGSurfaceResizeGrabber(XDGSurface *shellSurface);

    QPointF point;
    enum xdg_surface_resize_edge resize_edges;
    int32_t width;
    int32_t height;

    void focus() Q_DECL_OVERRIDE;
    void motion(uint32_t time) Q_DECL_OVERRIDE;
    void button(uint32_t time, Qt::MouseButton button, uint32_t state) Q_DECL_OVERRIDE;
};

class XDGSurfaceMoveGrabber : public XDGSurfaceGrabber
{
public:
    XDGSurfaceMoveGrabber(XDGSurface *shellSurface, const QPointF &offset);

    void focus() Q_DECL_OVERRIDE;
    void motion(uint32_t time) Q_DECL_OVERRIDE;
    void button(uint32_t time, Qt::MouseButton button, uint32_t state) Q_DECL_OVERRIDE;

private:
    QPointF m_offset;
};

class XDGSurfacePopupGrabber : public PointerGrabber
{
public:
    XDGSurfacePopupGrabber(InputDevice *inputDevice);

    uint32_t grabSerial() const;

    struct ::wl_client *client() const;
    void setClient(struct ::wl_client *client);

    void addPopup(XDGSurface *surface);
    void removePopup(XDGSurface *surface);

    void focus() Q_DECL_OVERRIDE;
    void motion(uint32_t time) Q_DECL_OVERRIDE;
    void button(uint32_t time, Qt::MouseButton button, uint32_t state) Q_DECL_OVERRIDE;

private:
    InputDevice *m_inputDevice;
    struct ::wl_client *m_client;
    QList<XDGSurface *> m_surfaces;
    bool m_initialUp;
};

}

QT_END_NAMESPACE

#endif // WLXDGSURFACE_H
